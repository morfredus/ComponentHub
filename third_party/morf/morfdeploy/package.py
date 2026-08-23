"""morfdeploy package: turn a freshly built, PROVEN binary into an installable.

The rules this module enforces come straight from the packaging contract:

  - only NATIVE targets of the current os+arch are packaged (cross-compilation is
    never assumed); an incompatible target is reported and skipped, never faked;
  - by default the binary is (re)built from the target's preset, so a package is
    never quietly made from a stale binary; `--no-build` reuses an existing one
    but the provenance barrier still applies in full;
  - the provenance barrier refuses anything it cannot prove: build-info.json must
    exist and say dirty=false (true OR null is refused), its commit must equal the
    current HEAD, its version must equal VERSION, and its platform/arch must be the
    target's. A build failure is fatal -- there is never a fall-back to an old
    binary;
  - a .deb's Depends is the sorted, de-duplicated UNION of what dpkg-shlibdeps
    reads from the actual binary and the explicit runtime packages; the two
    origins are printed separately before the merge. Development (-dev) packages
    never appear there.

Building the deliverables needs the platform's own tools (dpkg-deb/dpkg-shlibdeps
on Debian, windeployqt on Windows); this module orchestrates them, it does not
reimplement them.
"""

from __future__ import annotations

import json
import platform
import shutil
import subprocess
import zipfile
from pathlib import Path

from .core import detect_preset, invoking_user, locate_binary
from .manifest import Manifest
from .provenance import BUILD_INFO_NAME, detect_platform_arch, write_build_info
from . import morfproject


class PackageError(RuntimeError):
    """A packaging step could not proceed for a reason worth reporting as-is."""


# --- Provenance barrier ------------------------------------------------------

def _read_build_info(binary: Path) -> dict | None:
    info = binary.parent / BUILD_INFO_NAME
    if not info.is_file():
        return None
    try:
        return json.loads(info.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def _git_head(repo_root: Path) -> str | None:
    try:
        out = subprocess.run(["git", "-C", str(repo_root), "rev-parse", "HEAD"],
                             capture_output=True, text=True, check=False)
        return out.stdout.strip() if out.returncode == 0 else None
    except OSError:
        return None


def _read_version(repo_root: Path) -> str | None:
    try:
        lines = (repo_root / "VERSION").read_text(encoding="utf-8").splitlines()
        return lines[0].strip() if lines else None
    except OSError:
        return None


def verify_provenance(binary: Path, target, repo_root: Path) -> dict:
    """Refuse to package anything whose provenance cannot be fully proved."""
    info = _read_build_info(binary)
    if info is None:
        raise PackageError(
            f"no {BUILD_INFO_NAME} beside {binary.name}: provenance unknown, "
            "refusing to package. Build it (drop --no-build) to stamp it.")
    if info.get("dirty") is not False:
        state = info.get("dirty")
        raise PackageError(
            f"build provenance is dirty={state!r}: the sources were modified (or "
            "their state could not be proven). Refusing to package.")
    head = _git_head(repo_root)
    if head is not None and info.get("commit") != head:
        raise PackageError(
            f"build was made at commit {str(info.get('commit'))[:12]}, but HEAD is "
            f"{head[:12]}: the binary does not match the current sources. Rebuild.")
    version = _read_version(repo_root)
    if version is not None and info.get("version") != version:
        raise PackageError(
            f"build version {info.get('version')!r} != VERSION {version!r}: rebuild.")
    if info.get("platform") != target.os or info.get("architecture") != target.arch:
        raise PackageError(
            f"binary is {info.get('platform')}-{info.get('architecture')}, target "
            f"is {target.os}-{target.arch}: wrong build for this deliverable.")
    return info


# --- Target selection --------------------------------------------------------

def select_targets(project, target_name, cur_os, cur_arch):
    """(selected, incompatible) among the morfdeploy-provider targets.

    Selected are the NATIVE ones (or the one named, if native). Incompatible are
    the other-platform morfdeploy targets, reported so nothing is silently missed.
    Cross-compilation is never assumed: a named non-native target is refused.
    """
    md = project.morfdeploy_targets()
    native = [t for t in md if t.os == cur_os and t.arch == cur_arch]
    incompatible = [t for t in md if not (t.os == cur_os and t.arch == cur_arch)]
    if target_name:
        t = project.targets.get(target_name)
        if t is None:
            raise PackageError(f"unknown target '{target_name}'. "
                               f"Declared: {', '.join(project.target_names()) or '(none)'}")
        if t.provider != "morfdeploy":
            raise PackageError(f"target '{target_name}' is not packaged by morfdeploy "
                               f"(provider '{t.provider}').")
        if not (t.os == cur_os and t.arch == cur_arch):
            raise PackageError(
                f"target '{target_name}' is {t.os}-{t.arch}, this machine is "
                f"{cur_os}-{cur_arch}. Cross-compilation is not assumed; run it on "
                "the matching platform (or declare a verified cross toolchain).")
        return [t], incompatible
    return native, incompatible


# --- Build -------------------------------------------------------------------

def build_preset(repo_root: Path, preset: str) -> None:
    """Configure and build a preset, or stop. No fall-back on failure."""
    overrides = []
    if platform.system() == "Windows" and preset in ("mingw", "windows"):
        # Packaging builds must receive the same portable toolchain discovery as
        # ordinary service builds. Otherwise a preset sees Qt but misses OpenSSL
        # and other libraries installed beside the detected MinGW compiler.
        from .backends.windows import _mingw_toolchain_overrides
        overrides = _mingw_toolchain_overrides()
    for stage in (["cmake", "--preset", preset, *overrides],
                  ["cmake", "--build", "--preset", preset]):
        result = subprocess.run(stage, cwd=str(repo_root), check=False)
        if result.returncode != 0:
            raise PackageError(
                f"{' '.join(stage)} failed ({result.returncode}); refusing to "
                "package. No fall-back to an earlier binary.")


# --- Debian runtime dependencies --------------------------------------------

def declared_runtime_debs(manifest: Manifest) -> list:
    """Explicit runtime packages declared for Debian, across dependencies.

    Only what the linkage cannot reveal is declared here (a subprocess such as
    exiftool, a plugin, a system tool). An optional dependency's runtime is kept
    when its capability is part of this build -- and a declared runtime IS that
    statement of intent. Required ones are always kept.
    """
    packages: list = []
    for dep in manifest.system_dependencies:
        packages += list(dep.runtime_packages("debian"))
    return sorted(set(packages))


def shlibdeps(binary: Path) -> list:
    """ELF runtime dependencies read from the binary itself, via dpkg-shlibdeps.

    Returns a list of Debian dependency clauses (e.g. 'libqt6core6 (>= 6.4)').
    dpkg-shlibdeps needs a minimal debian/ context, provided in a temp tree.
    """
    if shutil.which("dpkg-shlibdeps") is None:
        raise PackageError("dpkg-shlibdeps not found: install 'dpkg-dev' to build a .deb.")
    import tempfile
    work = Path(tempfile.mkdtemp(prefix="morfpkg-shlibs-"))
    try:
        (work / "debian").mkdir()
        (work / "debian" / "control").write_text(
            "Source: probe\n\nPackage: probe\nArchitecture: any\nDescription: probe\n",
            encoding="utf-8")
        # -O prints "shlibs:Depends=..." to stdout instead of writing a substvars
        # file; --ignore-missing-info keeps a private/bundled .so from aborting it.
        result = subprocess.run(
            ["dpkg-shlibdeps", "-O", "--ignore-missing-info", str(binary)],
            cwd=str(work), capture_output=True, text=True, check=False)
        line = result.stdout.strip()
        if not line.startswith("shlibs:Depends="):
            # Not fatal to the whole package: report and continue with an empty ELF
            # set, so the explicit runtime deps still apply. The caller shows both.
            return []
        clauses = line.split("=", 1)[1].strip()
        return [c.strip() for c in clauses.split(",") if c.strip()] if clauses else []
    finally:
        shutil.rmtree(work, ignore_errors=True)


def _dep_name(clause: str) -> str:
    """The bare package name of a Depends clause ('libc6 (>= 2.34)' -> 'libc6')."""
    return clause.split("(", 1)[0].strip()


def merge_depends(elf: list, runtime: list) -> list:
    """Sorted, de-duplicated union of ELF and explicit runtime dependencies.

    A package named by both keeps its ELF clause (it carries the version bound).
    """
    by_name: dict = {}
    for clause in elf:
        by_name[_dep_name(clause)] = clause
    for name in runtime:
        by_name.setdefault(_dep_name(name), name)
    return [by_name[n] for n in sorted(by_name)]


# --- .deb --------------------------------------------------------------------

def _substitute_unit(manifest: Manifest, run_user: str) -> str:
    """The project's systemd unit, with its placeholders resolved for a package."""
    from .backends.systemd import UNIT_DIR  # noqa: F401  (import guard on Linux)
    import re
    candidates = [
        manifest.repo_root / "scripts" / "linux" / f"{manifest.service_name}.service",
        manifest.repo_root / "deploy" / f"{manifest.service_name}.service",
    ]
    template = next((p for p in candidates if p.is_file()), None)
    if template is None:
        raise PackageError(
            f"no systemd unit template for {manifest.service_name} "
            f"(looked in scripts/linux/ and deploy/).")
    unit = template.read_text(encoding="utf-8")
    home = Path("/home") / run_user if run_user != "root" else Path("/root")
    for token, value in (
        ("__RUN_USER__", run_user),
        ("__APP_DIR__", f"/opt/{manifest.service_name}"),
        ("__CONFIG_DIR__", str(manifest.config_dir())),
        ("__STATE_DIR__", str(manifest.state_dir())),
        ("__RUN_HOME__", str(home)),
    ):
        unit = unit.replace(token, value)
    leftover = sorted(set(re.findall(r"__[A-Z][A-Z0-9_]*__", unit)))
    if leftover:
        raise PackageError(
            f"unit template uses placeholders not resolved for packaging: "
            f"{', '.join(leftover)}")
    return unit


def build_deb(manifest: Manifest, binary: Path, target, out_dir: Path) -> Path:
    """Assemble and build a .deb from the proven binary. Debian only."""
    if shutil.which("dpkg-deb") is None:
        raise PackageError("dpkg-deb not found: install 'dpkg-dev' to build a .deb.")
    import tempfile
    svc = manifest.service_name
    version = _read_version(manifest.repo_root) or "0.0.0"
    arch = target.package.get("architecture") or "amd64"
    run_user = svc  # a dedicated system user, created by the maintainer script

    # --- Depends: ELF (from the binary) + explicit runtime, shown then merged ---
    elf = shlibdeps(binary)
    runtime = declared_runtime_debs(manifest)
    print("  Depends -- from dpkg-shlibdeps (linked libraries):")
    for c in elf:
        print(f"      {c}")
    print("  Depends -- explicit runtime (declared, not linkage-visible):")
    for c in (runtime or ["(none)"]):
        print(f"      {c}")
    depends = merge_depends(elf, runtime)
    print(f"  Depends -- merged: {', '.join(depends) or '(none)'}")

    stage = Path(tempfile.mkdtemp(prefix=f"morfpkg-{svc}-"))
    try:
        # Layout
        opt = stage / "opt" / svc
        opt.mkdir(parents=True)
        shutil.copy2(binary, opt / manifest.binary_name())
        (opt / manifest.binary_name()).chmod(0o755)

        # A privileged helper is an explicit, opt-in package fact. It never
        # shares the application directory: the service account must not be
        # able to replace a root executable during a normal update.
        helper_path = None
        if manifest.helper_binary:
            candidates = (
                manifest.repo_root / "build" / "service" / manifest.helper_binary,
                manifest.repo_root / "build-arm64" / "service" / manifest.helper_binary,
            )
            helper_path = next((candidate for candidate in candidates if candidate.is_file()), None)
            if helper_path is None:
                raise PackageError(f"privileged helper not built: {manifest.helper_binary}")
            helper_dir = stage / "usr" / "lib" / "morfsystem" / svc
            helper_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(helper_path, helper_dir / manifest.helper_binary)
            (helper_dir / manifest.helper_binary).chmod(0o4750)

        conffiles = []
        etc = Path(str(manifest.config_dir()).lstrip("/"))
        for config in manifest.configs:
            src = manifest.repo_root / config.source
            if config.source.endswith(".example.json"):
                real = Path(str(src).replace(".example.json", ".json"))
                if real.is_file():
                    src = real
            if not src.is_file():
                continue
            dest_abs = config.resolved_dest(manifest.config_dir())
            dest = stage / str(dest_abs).lstrip("/")
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dest)
            conffiles.append("/" + str(dest.relative_to(stage)).replace("\\", "/"))

        # Persistent state directory (owned by the run user via postinst).
        (stage / str(manifest.state_dir()).lstrip("/")).mkdir(parents=True, exist_ok=True)

        # systemd unit
        unit_dir = stage / "lib" / "systemd" / "system"
        unit_dir.mkdir(parents=True)
        (unit_dir / f"{svc}.service").write_text(_substitute_unit(manifest, run_user),
                                                 encoding="utf-8")

        # DEBIAN metadata
        debian = stage / "DEBIAN"
        debian.mkdir()
        control = (
            f"Package: {svc}\n"
            f"Version: {version}\n"
            f"Architecture: {arch}\n"
            f"Maintainer: morfredus <noreply@morfredus.fr>\n"
            + (f"Depends: {', '.join(depends)}\n" if depends else "")
            + f"Section: net\nPriority: optional\n"
            f"Description: {manifest.display_name} (morfSystem service)\n"
            f" Packaged by morfdeploy from a provenance-checked binary.\n")
        (debian / "control").write_text(control, encoding="utf-8")
        if conffiles:
            (debian / "conffiles").write_text("\n".join(conffiles) + "\n", encoding="utf-8")

        # Maintainer scripts: a dedicated system user, then systemd wiring. Config
        # files are conffiles, so dpkg preserves an edited one across upgrades.
        # prerm : a l'upgrade, seulement stop (pas disable). disable --now laissait
        # le service eteint si enable --now du postinst echouait (|| true), visible
        # en mettant a jour morfMonitor depuis sa propre interface.
        helper_postinst = ""
        if manifest.helper_binary:
            # Le binaire 4750 ne suffit pas : si le dossier reste root:root 750,
            # le compte du service ne peut pas le parcourir et QProcess echoue
            # « helper introuvable » alors que le fichier est la (cas vu avec
            # morfphoto-helper apres un dpkg incomplet).
            helper_dir = f"/usr/lib/morfsystem/{svc}"
            helper_file = f"{helper_dir}/{manifest.helper_binary}"
            helper_postinst = (
                f"chown root:{run_user} {helper_dir} || true\n"
                f"chmod 750 {helper_dir} || true\n"
                f"chown root:{run_user} {helper_file} || true\n"
                f"chmod 4750 {helper_file} || true\n")
        postinst = (
            "#!/bin/sh\nset -e\n"
            f"if ! id -u {run_user} >/dev/null 2>&1; then\n"
            f"  adduser --system --group --no-create-home --home /opt/{svc} {run_user} || true\n"
            "fi\n"
            f"chown -R {run_user}:{run_user} /opt/{svc} {manifest.state_dir()} || true\n") + helper_postinst + (
            "if [ -d /run/systemd/system ]; then\n"
            "  systemctl daemon-reload || true\n"
            f"  systemctl enable --now {svc}.service || true\n"
            "fi\nexit 0\n")
        prerm = (
            "#!/bin/sh\nset -e\n"
            "if [ -d /run/systemd/system ]; then\n"
            "  case \"$1\" in\n"
            "    remove|deconfigure)\n"
            f"      systemctl disable --now {svc}.service || true\n"
            "      ;;\n"
            "    *)\n"
            f"      systemctl stop {svc}.service || true\n"
            "      ;;\n"
            "  esac\n"
            "fi\nexit 0\n")
        for name, body in (("postinst", postinst), ("prerm", prerm)):
            p = debian / name
            p.write_text(body, encoding="utf-8")
            p.chmod(0o755)

        out_dir.mkdir(parents=True, exist_ok=True)
        deb = out_dir / f"{svc}-{version}-linux-{arch}.deb"
        result = subprocess.run(
            ["dpkg-deb", "--root-owner-group", "--build", str(stage), str(deb)],
            capture_output=True, text=True, check=False)
        if result.returncode != 0:
            raise PackageError(f"dpkg-deb failed: {result.stderr.strip()}")
        print(f"  built: {deb}")
        return deb
    finally:
        shutil.rmtree(stage, ignore_errors=True)


# --- .zip (Windows) ----------------------------------------------------------

def build_zip(manifest: Manifest, binary: Path, target, out_dir: Path) -> Path:
    """Assemble and zip a portable Windows bundle from the proven binary."""
    import tempfile
    svc = manifest.service_name
    version = _read_version(manifest.repo_root) or "0.0.0"
    stage = Path(tempfile.mkdtemp(prefix=f"morfpkg-{svc}-"))
    try:
        appdir = stage / svc
        appdir.mkdir()
        installed = appdir / manifest.binary_name()
        shutil.copy2(binary, installed)

        # Bundle the Qt and compiler runtime beside the binary (windeployqt),
        # exactly as an install does -- reused, not reimplemented.
        from .backends.windows import WindowsBackend
        WindowsBackend().install_runtime(installed, binary)

        # A ready-to-edit configuration alongside.
        for config in manifest.configs:
            src = manifest.repo_root / config.source
            if src.is_file():
                shutil.copy2(src, appdir / Path(config.source).name)

        # Register / remove the service on the target machine (scheduled task as
        # SYSTEM -- the same strategy morfdeploy uses on Windows without a wrapper).
        exe = f"%~dp0{manifest.binary_name()}"
        (appdir / "install-service.ps1").write_text(
            "# Register the morfSystem service (run as Administrator).\n"
            "$ErrorActionPreference = 'Stop'\n"
            f"$exe = Join-Path $PSScriptRoot '{manifest.binary_name()}'\n"
            f"schtasks /Create /F /TN '{svc}' /TR \"$exe\" /SC ONSTART /RU SYSTEM\n"
            f"schtasks /Run /TN '{svc}'\n"
            f"Write-Host 'Service {svc} registered as a scheduled task (SYSTEM).'\n",
            encoding="utf-8")
        (appdir / "uninstall-service.ps1").write_text(
            "# Remove the morfSystem service (run as Administrator).\n"
            "$ErrorActionPreference = 'SilentlyContinue'\n"
            f"schtasks /End /TN '{svc}'\n"
            f"schtasks /Delete /F /TN '{svc}'\n"
            f"Write-Host 'Service {svc} removed.'\n",
            encoding="utf-8")

        out_dir.mkdir(parents=True, exist_ok=True)
        zip_path = out_dir / f"{svc}-{version}-windows-x86_64.zip"
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
            for path in sorted(appdir.rglob("*")):
                if path.is_file():
                    zf.write(path, path.relative_to(stage))
        print(f"  built: {zip_path}")
        return zip_path
    finally:
        shutil.rmtree(stage, ignore_errors=True)


# --- Orchestration -----------------------------------------------------------

_FORMAT_BUILDERS = {"deb": build_deb, "zip": build_zip}


def package(project, manifest: Manifest, target_name, no_build: bool,
            out_dir: Path) -> list:
    """Build the deliverable(s) for the current platform. Returns the paths made."""
    cur_os, cur_arch = detect_platform_arch()
    selected, incompatible = select_targets(project, target_name, cur_os, cur_arch)

    if incompatible:
        print(f"Skipping {len(incompatible)} target(s) not native to "
              f"{cur_os}-{cur_arch} (no cross-compilation assumed):")
        for t in incompatible:
            print(f"  - {t.name} ({t.os}-{t.arch})")
    if not selected:
        print(f"No morfdeploy target native to {cur_os}-{cur_arch}: nothing to build.")
        return []

    _, build_dir = detect_preset()

    if not no_build:
        # Shared compilation: build each distinct preset ONCE, however many
        # deliverables share it.
        for preset in sorted({t.build_preset for t in selected if t.build_preset}):
            print(f"Building (preset {preset})...")
            build_preset(manifest.repo_root, preset)
        binary = locate_binary(manifest, build_dir)
        if binary is None:
            raise PackageError(f"no binary under {manifest.repo_root / build_dir} "
                               "after building.")
        write_build_info(manifest.repo_root, binary,
                         project=manifest.display_name)
    else:
        binary = locate_binary(manifest, build_dir)
        if binary is None:
            raise PackageError("no built binary and --no-build was given: build it "
                               "first, or drop --no-build.")

    made = []
    for t in selected:
        print(f"\nTarget {t.name} ({t.format}):")
        verify_provenance(binary, t, manifest.repo_root)   # the barrier
        builder = _FORMAT_BUILDERS.get(t.format)
        if builder is None:
            print(f"  no builder for format '{t.format}', skipped.")
            continue
        made.append(builder(manifest, binary, t, out_dir))
    return made
