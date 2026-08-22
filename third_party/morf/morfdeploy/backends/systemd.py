"""systemd backend — Linux x64 and ARM64.

Reproduces what the install-service.sh scripts did, including the details that
look incidental and are not: the unit is generated from a template committed in
the project, and `daemon-reload` is issued before enabling, because systemd
otherwise acts on the unit it read at boot.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import time
from pathlib import Path

from ..activity import complete_build_activity
from ..manifest import Manifest
from .base import ServiceBackend

UNIT_DIR = Path("/etc/systemd/system")


class SystemdBackend(ServiceBackend):
    name = "systemd"
    supported = True

    # -- Helpers ----------------------------------------------------------

    def _unit_path(self, manifest: Manifest) -> Path:
        return UNIT_DIR / f"{manifest.service_name}.service"

    def _systemctl(self, *args: str, check: bool = True) -> int:
        result = subprocess.run(["systemctl", *args], check=False)
        if check and result.returncode != 0:
            raise RuntimeError(f"systemctl {' '.join(args)} failed ({result.returncode})")
        return result.returncode

    def _unit_template(self, manifest: Manifest) -> Path:
        """The unit committed in the project.

        Kept in the project rather than generated here on purpose: ExecStart
        arguments, Restart policy and hardening options are that service's
        business, and a template generated from this side would force every
        service to want the same thing.
        """
        candidates = [
            manifest.repo_root / "scripts" / "linux" / f"{manifest.service_name}.service",
            manifest.repo_root / "deploy" / f"{manifest.service_name}.service",
        ]
        for path in candidates:
            if path.is_file():
                return path
        raise RuntimeError(
            f"No systemd unit template found for {manifest.service_name}.\n"
            "Looked in:\n  " + "\n  ".join(str(p) for p in candidates)
        )

    # -- Interrogation ----------------------------------------------------

    def is_installed(self, manifest: Manifest) -> bool:
        return self._unit_path(manifest).is_file()

    def is_active(self, manifest: Manifest) -> bool:
        # `systemctl is-active --quiet` exits 0 only when the unit is running.
        # Any other outcome (inactive, failed, unknown, or not queryable) is
        # treated as "not clearly running", so the purge guard never blocks on a
        # doubt -- it only catches the unambiguous case.
        result = subprocess.run(
            ["systemctl", "is-active", "--quiet", manifest.service_name],
            capture_output=True, check=False,
        )
        return result.returncode == 0

    def status(self, manifest: Manifest) -> str:
        result = subprocess.run(
            ["systemctl", "--no-pager", "--lines=0", "status", manifest.service_name],
            capture_output=True,
            text=True,
            check=False,
        )
        return (result.stdout or result.stderr).strip()

    # -- Lifecycle --------------------------------------------------------

    def install(self, manifest: Manifest, app_dir: Path, run_user: str) -> None:
        template = self._unit_template(manifest)
        unit = template.read_text(encoding="utf-8")

        # __RUN_HOME__ exists because a SYSTEM unit with User= is not always
        # given $HOME by systemd, and %h would expand to /root. morfSync
        # aborts outright when it cannot resolve its data directory, so an
        # unsubstituted placeholder here is not cosmetic.
        home = Path.home() if run_user == "root" else Path("/home") / run_user
        for token, value in (
            ("__RUN_USER__", run_user),
            ("__APP_DIR__", str(app_dir)),
            # Configuration no longer sits beside the binary, so a unit needs
            # both paths: __APP_DIR__ for what it executes, __CONFIG_DIR__ for
            # what it reads.
            ("__CONFIG_DIR__", str(manifest.config_dir())),
            # Persistent state lives under /var/lib, never in /etc. A unit that
            # passes its state root explicitly (rather than reading systemd's
            # $STATE_DIRECTORY) gets it here; both resolve to the same path.
            ("__STATE_DIR__", str(manifest.state_dir())),
            ("__RUN_HOME__", str(home)),
        ):
            unit = unit.replace(token, value)

        # Anything still unsubstituted would be written into /etc/systemd as a
        # literal. The service would start, or fail, on a path spelled
        # __SOMETHING__, and the unit file would look deliberate. Refuse
        # instead: an unknown placeholder means this backend does not yet know
        # something the project is asking for.
        leftover = sorted(set(re.findall(r"__[A-Z][A-Z0-9_]*__", unit)))
        if leftover:
            raise RuntimeError(
                f"Unit template {template} uses placeholders this backend does not "
                f"substitute: {', '.join(leftover)}"
            )

        dest = self._unit_path(manifest)
        previous = dest.read_text(encoding="utf-8") if dest.is_file() else None
        if previous == unit:
            print(f"  unit unchanged: {dest}")
        else:
            dest.write_text(unit, encoding="utf-8")
            dest.chmod(0o644)
            print(f"  unit {'updated' if previous else 'installed'}: {dest}")

        # Before enable, always: systemd otherwise acts on the definition it
        # last read, and a unit edited in the repository would never take
        # effect while nothing reported anything wrong.
        self._systemctl("daemon-reload")
        self._systemctl("enable", "--now", manifest.service_name)

    def stop(self, manifest: Manifest) -> None:
        # A service that is absent or already stopped satisfies the goal, so
        # nothing here is an error. The unit is checked first rather than
        # stopped unconditionally: on a first install systemd would otherwise
        # print "Failed to stop ...: Unit not loaded" in the middle of a
        # successful run, which reads as a failure and is not one.
        if not self.is_installed(manifest):
            return
        subprocess.run(
            ["systemctl", "stop", manifest.service_name],
            capture_output=True,
            check=False,
        )

    def start(self, manifest: Manifest) -> None:
        self._systemctl("start", manifest.service_name)

    def uninstall(self, manifest: Manifest) -> None:
        self._systemctl("disable", "--now", manifest.service_name, check=False)
        unit = self._unit_path(manifest)
        if unit.is_file():
            unit.unlink()
        self._systemctl("daemon-reload", check=False)

    # -- Privileges -------------------------------------------------------

    def requires_privileges(self) -> bool:
        return True

    def has_privileges(self) -> bool:
        return os.geteuid() == 0

    def privilege_hint(self) -> str:
        return "Re-run with sudo."

    # -- Build ------------------------------------------------------------

    def build_as_user(self, repo_root: Path, preset: str, run_user: str) -> None:
        """Build as the invoking user, never as root.

        Running cmake under sudo leaves a build tree owned by root inside the
        clone; the next ordinary build then fails on permissions, in a place
        that says nothing about the install that caused it.
        """
        command = f"cd {repo_root!s} && cmake --preset {preset} && cmake --build --preset {preset}"
        if run_user != "root" and shutil.which("sudo"):
            args = ["sudo", "-u", run_user, "bash", "-lc", command]
        else:
            args = ["bash", "-lc", command]
        # Signale la compilation au domaine Monitor de morfAnalytics (best-effort).
        # `finally` : on emet meme sur echec (status=failed), puis l'exception
        # remonte normalement -- un build casse reste un build casse.
        start = time.time()
        ok = False
        try:
            subprocess.run(args, check=True)
            ok = True
        finally:
            complete_build_activity(repo_root, preset, start, ok)
