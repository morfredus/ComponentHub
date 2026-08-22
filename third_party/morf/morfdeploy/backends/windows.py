"""Windows backend — x64.

A note on what a Windows service actually requires, because it decides the
shape of this file.

The Service Control Manager does not merely launch a program: it expects the
process to connect back, declare a service entry point and report its state
within about thirty seconds. A binary that does not do this is registered
without complaint by `sc.exe create` and then fails at start with error 1053,
"the service did not respond in a timely fashion". Nothing in that message
mentions the missing SCM handshake.

The morfSystem services are ordinary Qt console programs. They are SCM-aware on
no platform, and making them so would mean adding Windows-specific startup code
to every one of them -- inside programs whose whole point is to be identical
everywhere.

So there are two strategies here, and the manifest chooses:

  "scm"       real Windows service. Requires either an SCM-aware binary or a
              wrapper (WinSW, NSSM) that speaks the protocol on its behalf.
  "task"      scheduled task at boot. What the PowerShell scripts did. Not a
              service: no dependency ordering, no automatic restart on crash,
              and it does not appear in services.msc.

The default is "task" because it is what works today with an unmodified binary.
Declaring "scm" without a wrapper is refused at install time rather than
producing a service registered to fail.
"""

from __future__ import annotations

import ctypes
import os
import shutil
import subprocess
import time
from pathlib import Path

from ..activity import complete_build_activity
from ..manifest import Manifest
from .base import ServiceBackend


def _openssl_cmake_flags(prefix: Path) -> list:
    """Chemins MinGW explicites : CMake 4.x FindOpenSSL cherche souvent des .lib
    MSVC. Apres un reset de cache (changement de g++), OPENSSL_ROOT_DIR seul
    ne retrouve plus libcrypto.dll.a.
    """
    flags = [f"-DOPENSSL_ROOT_DIR={prefix}"]
    include = prefix / "include"
    if include.is_dir():
        flags.append(f"-DOPENSSL_INCLUDE_DIR={include}")
    crypto = prefix / "lib" / "libcrypto.dll.a"
    if not crypto.is_file():
        crypto = prefix / "lib" / "libcrypto.a"
    ssl = prefix / "lib" / "libssl.dll.a"
    if not ssl.is_file():
        ssl = prefix / "lib" / "libssl.a"
    if crypto.is_file():
        flags.append(f"-DOPENSSL_CRYPTO_LIBRARY={crypto}")
    if ssl.is_file():
        flags.append(f"-DOPENSSL_SSL_LIBRARY={ssl}")
    return flags


def _mingw_toolchain_overrides() -> list:
    """CMake -D flags for the MinGW/Qt toolchain actually present on this machine.

    A project's `mingw` preset pins one box's MSYS2 paths (ninja, g++, the Qt
    prefix) as absolute cache variables. On another Windows machine -- the
    official Qt toolchain under C:/Qt, MSYS2 absent -- those paths do not exist
    and the pinned `ninja.exe` fails with "no such file". Detect what is on the
    PATH/env here and override the pins. Empty when nothing is found: the build
    then proceeds on the preset's own values, no worse than before, so this is a
    pure improvement rather than a new failure mode.
    """
    overrides = []
    prefixes: list[str] = []
    ninja = shutil.which("ninja")
    if ninja:
        overrides.append(f"-DCMAKE_MAKE_PROGRAM={ninja}")
    cxx = shutil.which("g++") or shutil.which("c++")
    if cxx:
        overrides.append(f"-DCMAKE_CXX_COMPILER={cxx}")
        cc = shutil.which("gcc") or shutil.which("cc")
        if cc:
            overrides.append(f"-DCMAKE_C_COMPILER={cc}")
    if not (os.environ.get("CMAKE_PREFIX_PATH") or os.environ.get("Qt6_DIR")):
        qmake = shutil.which("qmake6") or shutil.which("qmake")
        if qmake:
            prefixes.append(str(Path(qmake).resolve().parent.parent))
    if not os.environ.get("OPENSSL_ROOT_DIR"):
        openssl = shutil.which("openssl")
        if openssl:
            # OpenSSL installed with the active MinGW toolchain exposes its
            # executable under <prefix>/bin and headers under <prefix>/include.
            # Deriving the prefix keeps a portable preset free of one machine's
            # MSYS2 installation path.
            prefix = Path(openssl).resolve().parent.parent
            if (prefix / "include" / "openssl" / "ssl.h").is_file():
                overrides.extend(_openssl_cmake_flags(prefix))
                ossl = str(prefix)
                if ossl not in prefixes:
                    prefixes.append(ossl)
    if prefixes and not os.environ.get("CMAKE_PREFIX_PATH"):
        overrides.append("-DCMAKE_PREFIX_PATH=" + ";".join(prefixes))
    return overrides

#: Wrappers that implement the SCM handshake for an ordinary executable.
WRAPPERS = ("winsw", "nssm")


class WindowsBackend(ServiceBackend):
    name = "windows"
    supported = True

    # -- Strategy ---------------------------------------------------------

    def _strategy(self, manifest: Manifest) -> str:
        declared = (manifest.app_dirs.get("windows_strategy") or "").lower()
        return declared if declared in ("scm", "task") else "task"

    def _wrapper(self) -> str | None:
        for name in WRAPPERS:
            found = shutil.which(name)
            if found:
                return found
        return None

    # -- Interrogation ----------------------------------------------------

    def can_query_installation(self, manifest: Manifest) -> bool:
        # A scheduled task runs as SYSTEM (/RU SYSTEM), and schtasks refuses to
        # describe it to a non-elevated caller -- "Acces refuse", exit status 1,
        # exactly what "no such task" returns. Without this, an unelevated
        # sweep would silently decide nothing is installed. `sc query` has no
        # such restriction, so the scm strategy can always answer.
        if self._strategy(manifest) == "scm":
            return True
        return self.has_privileges()

    def is_installed(self, manifest: Manifest) -> bool:
        if self._strategy(manifest) == "scm":
            result = subprocess.run(
                ["sc.exe", "query", manifest.service_name],
                capture_output=True, text=True, check=False,
            )
            return result.returncode == 0
        result = subprocess.run(
            ["schtasks", "/Query", "/TN", manifest.service_name],
            capture_output=True, text=True, check=False,
        )
        return result.returncode == 0

    def is_active(self, manifest: Manifest) -> bool:
        # "Clearly running", for the purge guard. An SCM service reports STATE
        # RUNNING; a scheduled task reports Status "Running" while it executes.
        # Anything else -- stopped, ready, access denied, absent -- is treated as
        # not clearly running, so the guard never blocks on a doubt.
        if self._strategy(manifest) == "scm":
            result = subprocess.run(
                ["sc.exe", "query", manifest.service_name],
                capture_output=True, text=True, check=False,
            )
            return result.returncode == 0 and "RUNNING" in result.stdout
        result = subprocess.run(
            ["schtasks", "/Query", "/TN", manifest.service_name, "/V", "/FO", "LIST"],
            capture_output=True, text=True, check=False,
        )
        return result.returncode == 0 and "Running" in result.stdout

    def status(self, manifest: Manifest) -> str:
        if self._strategy(manifest) == "scm":
            args = ["sc.exe", "query", manifest.service_name]
        else:
            args = ["schtasks", "/Query", "/TN", manifest.service_name, "/V", "/FO", "LIST"]
        result = subprocess.run(args, capture_output=True, text=True, check=False)
        return (result.stdout or result.stderr).strip()

    # -- Lifecycle --------------------------------------------------------

    def install(self, manifest: Manifest, app_dir: Path, run_user: str) -> None:
        target = app_dir / manifest.binary_name()

        if self._strategy(manifest) == "scm":
            wrapper = self._wrapper()
            if wrapper is None:
                raise RuntimeError(
                    f"{manifest.display_name} declares the 'scm' strategy, but no service\n"
                    f"wrapper ({', '.join(WRAPPERS)}) is on PATH.\n\n"
                    "A Qt console program cannot be a Windows service on its own: the\n"
                    "Service Control Manager expects it to report its state within about\n"
                    "thirty seconds, and a binary that does not will be registered\n"
                    "successfully and then fail to start with error 1053.\n\n"
                    "Install WinSW or NSSM, or use the 'task' strategy."
                )
            self._run(["sc.exe", "stop", manifest.service_name], check=False)
            self._run(["sc.exe", "delete", manifest.service_name], check=False)
            self._run([
                "sc.exe", "create", manifest.service_name,
                f"binPath= {wrapper} {target}",
                "start= auto",
                f"DisplayName= {manifest.display_name}",
            ])
            self._run(["sc.exe", "start", manifest.service_name])
            print(f"  Windows service registered via {Path(wrapper).name}")
            return

        # Scheduled task: recreated wholesale, /F overwriting any previous one.
        self._run([
            "schtasks", "/Create", "/F",
            "/TN", manifest.service_name,
            "/TR", f'"{target}"',
            "/SC", "ONSTART",
            "/RL", "HIGHEST",
            "/RU", "SYSTEM",
        ])
        self._run(["schtasks", "/Run", "/TN", manifest.service_name], check=False)
        print("  scheduled task registered (not a service: no restart on crash)")

    def stop(self, manifest: Manifest) -> None:
        """Disable, stop, and wait until the binary is actually released.

        Three steps where Linux needs one, because Windows differs twice.

        **Disable first.** A stop that something can undo is not a stop: an SCM
        wrapper (WinSW, NSSM) restarts a service it believes crashed, and it
        would come back holding the very files about to be replaced. Disabling
        is always undone by whoever called us -- install and update re-register
        the service wholesale, uninstall removes it -- so it never leaks.

        **Then wait.** Windows refuses to overwrite a file a process holds open,
        and stopping is not instantaneous: `schtasks /End` returns as soon as
        the request is made. Copying immediately fails with a permission error
        that says nothing about its real cause -- the previous instance still
        running. Linux has no equivalent step: `systemctl stop` returns once the
        unit has actually stopped.
        """
        if self._strategy(manifest) == "scm":
            self._run(["sc.exe", "config", manifest.service_name,
                       "start=", "disabled"], check=False)
            self._run(["sc.exe", "stop", manifest.service_name], check=False)
        else:
            self._run(["schtasks", "/Change", "/TN", manifest.service_name,
                       "/DISABLE"], check=False)
            self._run(["schtasks", "/End", "/TN", manifest.service_name], check=False)
        self._await_release(manifest)

    def _await_release(self, manifest: Manifest, timeout_s: float = 15.0) -> None:
        """Block until the installed binary can be written, or say why not.

        Opening the file for append is the test: Windows locks a running
        image against writing, and grants the handle the moment the process is
        gone. Nothing is written -- the handle is opened and closed.

        A timeout warns rather than fails: the copy that follows will produce
        the real error if the file is genuinely stuck, and this message is what
        makes that error readable.
        """
        target = manifest.app_dir() / manifest.binary_name()
        if not target.exists():
            return                      # first install: nothing holds anything

        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            try:
                with open(target, "ab"):
                    return
            except OSError:
                time.sleep(0.3)

        print(f"  WARNING: {target.name} is still held by a running process after "
              f"{timeout_s:.0f}s.")
        print("  Replacing it will fail. Stop it by hand, then run this again:")
        print(f"      schtasks /End /TN {manifest.service_name}")

    def start(self, manifest: Manifest) -> None:
        if self._strategy(manifest) == "scm":
            self._run(["sc.exe", "start", manifest.service_name])
        else:
            self._run(["schtasks", "/Run", "/TN", manifest.service_name])

    def uninstall(self, manifest: Manifest) -> None:
        self.stop(manifest)
        if self._strategy(manifest) == "scm":
            self._run(["sc.exe", "delete", manifest.service_name], check=False)
        else:
            self._run(["schtasks", "/Delete", "/F", "/TN", manifest.service_name], check=False)

    # -- Runtime dependencies ---------------------------------------------

    def install_runtime(self, installed_binary: Path, source_binary: Path) -> None:
        """Bring the Qt and MinGW DLLs the service needs next to its binary.

        The reason the base method is a no-op and this one is not: Linux
        resolves Qt from where the package manager put it, so a copied binary
        just runs. Windows has no system-wide Qt. An executable installed on its
        own starts to a 'Qt6Core.dll not found' dialog -- which, for a service,
        the SCM reports only as a start failure, mentioning none of the missing
        files. windeployqt, shipped with Qt, reads the executable's imports and
        places exactly the Qt libraries and plugins it needs beside it.

        windeployqt covers Qt and, with --compiler-runtime, the MinGW runtime
        (libgcc, libstdc++, libwinpthread). A second, best-effort pass with ldd
        widens the net to any remaining third-party DLL the binaries still
        resolve from the toolchain -- the belt-and-suspenders ComponentHub
        settled on -- and is simply skipped where no MSYS2 shell is present.
        """
        tool = self._find_windeployqt(source_binary)
        if tool is None:
            raise RuntimeError(
                "windeployqt could not be found, so the Qt runtime cannot be placed\n"
                f"beside {installed_binary.name}. Installed alone, the service starts\n"
                "to a missing-DLL error the Service Control Manager reports only as a\n"
                "failed start, naming none of the absent files.\n\n"
                "It is normally located automatically from the build's CMake cache.\n"
                "If Qt was moved since the build, rebuild (service.py install --rebuild)\n"
                "or add Qt's bin directory to PATH."
            )

        # windeployqt is run with Qt's own bin directory prepended to PATH, so it
        # finds objdump for its dependency walk and the compiler-runtime DLLs to
        # copy -- neither of which is on PATH when the install runs from an
        # ordinary PowerShell rather than the MSYS2 shell that built the binary.
        # This is what lets the install work from any terminal.
        env = dict(os.environ)
        env["PATH"] = str(Path(tool).parent) + os.pathsep + env.get("PATH", "")

        result = subprocess.run(
            [tool, "--no-translations", "--compiler-runtime", str(installed_binary)],
            env=env, text=True, capture_output=True, check=False,
        )
        detail = "\n".join(part for part in (result.stdout, result.stderr) if part).strip()
        if detail:
            print(detail)
        if result.returncode != 0:
            # Some services are plain MinGW executables. Asking windeployqt to
            # inspect one is expected to fail, but the toolchain dependency pass
            # below is still required for it.
            if "does not seem to be a qt executable" not in detail.lower():
                raise RuntimeError(
                    f"windeployqt failed ({result.returncode}) for {installed_binary.name}."
                )
            print(f"  no Qt runtime required beside {installed_binary.name}")
        else:
            print(f"  Qt runtime deployed beside {installed_binary.name} (windeployqt)")

        copied = self._copy_toolchain_dlls(installed_binary.parent, Path(tool).parent)
        if copied:
            print(f"  {copied} toolchain DLL(s) copied (transitive dependencies)")

    def _find_windeployqt(self, source_binary: Path) -> str | None:
        """Locate windeployqt without relying on it being on PATH.

        1. On PATH -- the MSYS2/MinGW shell that built the binary has it there.
        2. Beside Qt, found from the build's CMakeCache.txt: `Qt6_DIR` points at
           <qt>/lib/cmake/Qt6, so windeployqt sits three levels up, in <qt>/bin.
           This is the same anchor ComponentHub's CMake uses, and it makes the
           install work from an ordinary terminal, where PATH has no Qt.
        """
        for name in ("windeployqt", "windeployqt6", "windeployqt-qt6"):
            found = shutil.which(name)
            if found:
                return found

        cache = self._find_cmake_cache(source_binary)
        if cache is not None:
            for qt_dir in self._qt_dirs_from_cache(cache):
                # <qt>/lib/cmake/Qt6[Core] -> up three -> <qt>, then /bin.
                qt_bin = qt_dir.parents[2] / "bin"
                for name in ("windeployqt.exe", "windeployqt6.exe"):
                    candidate = qt_bin / name
                    if candidate.is_file():
                        return str(candidate)
        return None

    @staticmethod
    def _find_cmake_cache(source_binary: Path) -> Path | None:
        """The build's CMakeCache.txt, searched upward from the built binary.

        The binary may sit in build-*/service/ (the C++ services) or in the
        build root (the simpler ones), so the cache is one or two levels up.
        Bounded to a few levels: past that, we have left the build tree.
        """
        directory = source_binary.parent
        for _ in range(4):
            cache = directory / "CMakeCache.txt"
            if cache.is_file():
                return cache
            if directory == directory.parent:
                break
            directory = directory.parent
        return None

    @staticmethod
    def _qt_dirs_from_cache(cache: Path) -> list:
        """The Qt cmake-package directories recorded in a CMakeCache.txt.

        Both Qt6_DIR and Qt6Core_DIR are read: a cache may carry either, and
        both resolve to the same <qt>/bin three levels up.
        """
        found = []
        try:
            text = cache.read_text(encoding="utf-8", errors="replace")
        except OSError:
            return found
        for line in text.splitlines():
            for key in ("Qt6_DIR", "Qt6Core_DIR"):
                prefix = f"{key}:"
                if line.startswith(prefix) and "=" in line:
                    value = line.split("=", 1)[1].strip()
                    if value:
                        found.append(Path(value))
        return found

    def _copy_toolchain_dlls(self, app_dir: Path, toolchain_bin: Path) -> int:
        """Copy the third-party DLLs Qt itself depends on but windeployqt omits.

        windeployqt deploys the Qt libraries and, with --compiler-runtime, the
        compiler runtime -- but NOT the third-party libraries those Qt DLLs link
        against (brotli, double-conversion, ICU, pcre2...). Left out, the service
        stops at the first of them: 'libbrotlidec.dll introuvable', one dialog at
        a time. The old fix walked these with ldd from an MSYS2 shell; run from an
        ordinary PowerShell that shell is absent, and exactly those DLLs went
        missing.

        objdump replaces it. It ships in the same MinGW bin as windeployqt -- so
        it is present whenever windeployqt was found -- and needs no shell. We
        read each binary's import table, and for every imported DLL that exists
        in the toolchain bin (i.e. is a MinGW/Qt library, not a system one like
        kernel32), copy it and follow its own imports. The transitive closure
        settles when nothing new is pulled in.

        Returns the number of DLLs copied, for the caller to report.
        """
        objdump = toolchain_bin / "objdump.exe"
        if not objdump.is_file():
            found = shutil.which("objdump")
            objdump = Path(found) if found else None
        if objdump is None:
            # windeployqt covered Qt and the compiler runtime; without objdump we
            # cannot widen to the third-party libraries. Say so rather than fail:
            # a Core-only binary may need nothing more.
            print("  note: objdump not found; third-party DLLs (if any) not resolved")
            return 0

        # What the toolchain can supply, by lowercase name -> real path (its
        # actual case preserved for the copy). System DLLs are simply absent
        # here, which is how they get skipped.
        available = {}
        for entry in toolchain_bin.iterdir():
            if entry.is_file() and entry.suffix.lower() == ".dll":
                available.setdefault(entry.name.lower(), entry)

        present = {p.name.lower() for p in app_dir.iterdir() if p.is_file()}
        queue = [app_dir / n for n in present if n.endswith((".exe", ".dll"))]
        copied = 0
        while queue:
            binary = queue.pop()
            for dep in self._imports(objdump, binary):
                key = dep.lower()
                if key in present:
                    continue
                source = available.get(key)
                if source is None:
                    continue          # a system DLL, or not from this toolchain
                dest = app_dir / source.name
                shutil.copy2(source, dest)
                present.add(key)
                queue.append(dest)
                copied += 1
        return copied

    @staticmethod
    def _imports(objdump: Path, binary: Path) -> list:
        """The DLL names a PE binary imports, read from `objdump -p`.

        objdump prints one 'DLL Name: <x>.dll' line per import table entry; we
        need nothing else from its output.
        """
        result = subprocess.run([str(objdump), "-p", str(binary)],
                                capture_output=True, text=True, check=False)
        names = []
        for line in result.stdout.splitlines():
            stripped = line.strip()
            if stripped.startswith("DLL Name:"):
                names.append(stripped.split(":", 1)[1].strip())
        return names

    # -- Privileges -------------------------------------------------------

    def requires_privileges(self) -> bool:
        return True

    def has_privileges(self) -> bool:
        try:
            return bool(ctypes.windll.shell32.IsUserAnAdmin())
        except (AttributeError, OSError):
            return False

    def privilege_hint(self) -> str:
        return "Re-run from a terminal opened with 'Run as administrator'."

    # -- Build ------------------------------------------------------------

    def build_as_user(self, repo_root: Path, preset: str, run_user: str) -> None:
        """No privilege drop: Windows has no sudo, and the elevated shell is
        the same user, so the build tree keeps ordinary ownership."""
        # Signale la compilation au domaine Monitor de morfAnalytics (best-effort ;
        # emise meme sur echec, puis l'exception remonte normalement).
        # Adapt the pinned mingw preset to this machine's real toolchain (see
        # _mingw_toolchain_overrides). List form, no shell: paths with spaces need
        # no quoting, and there is nothing for a shell to mis-parse.
        overrides = _mingw_toolchain_overrides() if preset in ("mingw", "windows") else []
        start = time.time()
        ok = False
        try:
            subprocess.run(["cmake", "--preset", preset, *overrides],
                           cwd=repo_root, check=True)
            subprocess.run(["cmake", "--build", "--preset", preset],
                           cwd=repo_root, check=True)
            ok = True
        finally:
            complete_build_activity(repo_root, preset, start, ok)

    # -- Internals --------------------------------------------------------

    def _run(self, args: list, check: bool = True) -> None:
        result = subprocess.run(args, check=False)
        if check and result.returncode != 0:
            raise RuntimeError(f"{args[0]} {args[1]} failed ({result.returncode})")
