"""System-dependency resolution: detect, check, plan -- never install silently.

A project declares system packages it needs as needs (see manifest.py's
SystemDependency); this module answers, for the current machine, which are
already present and which are missing, and how the platform's package manager
would install the missing ones. It does not decide to install anything: that is
the Deployer's job, behind the shared guards (dry-run, explicit confirmation).

The design rule mirrors the rest of morfdeploy: the project declares the need,
this module resolves it on the platform actually running, and nothing global is
ever touched -- only the declared packages.
"""

from __future__ import annotations

import platform
import shutil
import subprocess
from dataclasses import dataclass


def detect_package_manager() -> tuple:
    """(family, manager) for this machine, or (None, None) if none is supported.

    Family is the key a project uses in `packages` (debian/fedora/arch); manager
    is how we query and install. Detection starts from the tools actually on the
    machine, and only for the platforms morfSystem really targets today (Debian
    family first). Anything else -- Windows, macOS, an unknown Linux -- returns
    (None, None): the resolver then reports rather than guesses, and never
    pretends it could install.
    """
    if platform.system() != "Linux":
        return (None, None)
    if shutil.which("apt-get"):
        return ("debian", "apt")
    if shutil.which("dnf"):
        return ("fedora", "dnf")
    if shutil.which("pacman"):
        return ("arch", "pacman")
    return (None, None)


def _is_installed(manager: str, package: str) -> bool:
    """True when the package is installed, asked of the native database.

    A read-only query, no privileges needed. An error or an unknown manager
    answers False -- treated as "not clearly present", so the resolver reports
    it as missing rather than skipping it.
    """
    try:
        if manager == "apt":
            probe = subprocess.run(["dpkg", "-s", package],
                                   capture_output=True, check=False)
            return probe.returncode == 0
        if manager == "dnf":
            probe = subprocess.run(["rpm", "-q", package],
                                   capture_output=True, check=False)
            return probe.returncode == 0
        if manager == "pacman":
            probe = subprocess.run(["pacman", "-Q", package],
                                   capture_output=True, check=False)
            return probe.returncode == 0
    except OSError:
        return False
    return False


def install_command(manager: str, packages: list) -> list | None:
    """The command that installs exactly these packages -- and nothing else.

    Never a global upgrade: only the declared packages are named. Returns None
    for an unsupported manager, so the caller reports "cannot install here"
    instead of running something wrong.
    """
    if manager == "apt":
        return ["apt-get", "install", "-y", *packages]
    if manager == "dnf":
        return ["dnf", "install", "-y", *packages]
    if manager == "pacman":
        return ["pacman", "-S", "--needed", "--noconfirm", *packages]
    return None


@dataclass
class DepStatus:
    """One dependency resolved against the current machine."""

    dep: object                 # SystemDependency
    packages: tuple = ()        # packages for this platform (may be empty)
    missing: tuple = ()         # of those, the ones not installed
    resolvable: bool = True     # False when no packages are declared for here

    @property
    def satisfied(self) -> bool:
        return self.resolvable and not self.missing


def resolve(dependencies, family: str, manager: str) -> list:
    """Status of every declared dependency on this machine.

    A dependency with no package for this platform family is marked
    unresolvable (packages empty): it is neither checked nor installable here,
    and the caller decides what that means (a notice, never a silent success).
    """
    statuses = []
    for dep in dependencies:
        # BUILD packages: this resolver serves the pre-compilation check
        # (ensure_dependencies). Runtime packages belong to the .deb's Depends and
        # are read separately at packaging time (dep.runtime_packages).
        packages = dep.build_packages(family) if family else ()
        if not packages:
            statuses.append(DepStatus(dep=dep, packages=(), missing=(),
                                      resolvable=False))
            continue
        missing = tuple(p for p in packages if not _is_installed(manager, p))
        statuses.append(DepStatus(dep=dep, packages=packages, missing=missing))
    return statuses
