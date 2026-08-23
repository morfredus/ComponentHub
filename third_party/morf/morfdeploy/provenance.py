"""Build provenance: a `build-info.json` written beside a freshly built artifact.

The proof that a given binary came from a given commit. Packaging (a later
chantier) reads it to refuse a stale or dirty build rather than ship a binary
nobody can trace back to a source state.

Two rules shape this module:

  - it is written only after a SUCCESSFUL build, and beside the artifact ITSELF
    -- the provenance is about that exact binary, not merely about a build
    directory that may hold several;
  - packaging never regenerates it. This module writes; the distribution chain
    only reads. A binary with no `build-info.json`, or one that disagrees with
    the current `HEAD`, is treated as unproven by whoever reads it -- not fixed
    up here.

Platform and architecture are DETECTED, never declared: the file must describe
the machine that actually built, in the parc's canonical target names
(`windows-x86_64`, `linux-amd64`, `linux-arm64`).
"""

from __future__ import annotations

import json
import platform
import subprocess
from datetime import datetime, timezone
from pathlib import Path

#: The provenance file, written next to the artifact it describes.
BUILD_INFO_NAME = "build-info.json"


def _git(repo_root: Path, *args: str) -> str | None:
    """A git command's stdout, or None when git is absent or the call fails.

    Best-effort: a build outside a repository still produces a binary, it just
    cannot be traced -- and that is exactly what a null commit records.
    """
    try:
        out = subprocess.run(
            ["git", "-C", str(repo_root), *args],
            capture_output=True, text=True, check=False,
        )
    except (OSError, ValueError):
        return None
    if out.returncode != 0:
        return None
    return out.stdout.strip()


def _read_version(repo_root: Path) -> str | None:
    """The project's version, from its root `VERSION` file (the parc convention)."""
    try:
        first = (repo_root / "VERSION").read_text(encoding="utf-8").strip().splitlines()
    except OSError:
        return None
    return first[0].strip() if first else None


def detect_platform_arch() -> tuple[str, str]:
    """(platform, architecture) in the parc's canonical target names.

    Detected from the running machine, never taken from a caller's claim: the
    provenance must not be able to lie about where it was produced.
    """
    system = platform.system()
    machine = platform.machine().lower()
    if system == "Windows":
        return "windows", "x86_64"
    # Internal, normalised architecture names (x86_64 / arm64) -- the SAME the
    # morfproject reader stores for a target's platform.arch, so the packaging
    # barrier compares like with like. The Debian name (amd64/arm64) is a
    # packaging-time detail, carried by a target's package.architecture.
    if machine in ("aarch64", "arm64"):
        arch = "arm64"
    elif machine in ("x86_64", "amd64"):
        arch = "x86_64"
    else:
        arch = machine or "unknown"
    plat = "linux" if system == "Linux" else system.lower()
    return plat, arch


def write_build_info(repo_root: Path, artifact: Path, *, project: str,
                     preset: str | None = None) -> Path:
    """Write `build-info.json` beside `artifact`, right after a successful build.

    Captures the commit and whether the working tree was dirty AT BUILD TIME, so
    a later packaging step can prove the binary matches `HEAD`. `project` is the
    display name (e.g. "morfCollector"); the version is read from the repo's
    `VERSION` file. Returns the path written.
    """
    # The FULL sha is the provenance and the conflict key: two builds can share a
    # short prefix, and a distribution chain must compare on the whole identifier.
    # The short form is kept alongside, for display only.
    commit = _git(repo_root, "rev-parse", "HEAD")
    commit_short = _git(repo_root, "rev-parse", "--short", "HEAD")
    status = _git(repo_root, "status", "--porcelain")
    # `dirty` stays null when git could not be asked -- an unknown state, not a
    # clean one; a reader must not mistake "could not tell" for "clean". The
    # packaging barrier refuses both `true` AND `null` (provenance unprovable).
    dirty = None if status is None else bool(status.strip())
    plat, arch = detect_platform_arch()

    info = {
        "project": project,
        "version": _read_version(repo_root),
        "commit": commit,
        "commit_short": commit_short,
        "dirty": dirty,
        "platform": plat,
        "architecture": arch,
        "target": f"{plat}-{arch}",
        "preset": preset,
        "artifact": artifact.name,
        "built_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }
    dest = artifact.parent / BUILD_INFO_NAME
    payload = json.dumps(info, indent=2, ensure_ascii=False) + "\n"
    try:
        dest.write_text(payload, encoding="utf-8")
    except PermissionError:
        # Dossier user, fichier resté root après un cmake/ninja en sudo :
        # os.access(parent, W_OK) est vrai, write_text non. Unlink puis réécrire.
        dest.unlink(missing_ok=True)
        dest.write_text(payload, encoding="utf-8")
    return dest
