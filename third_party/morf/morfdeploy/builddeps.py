"""Build-dependency resolution: the libraries a project needs to COMPILE.

A project declares a logical need ("openssl"); this module maps it to the right
package for the current platform and answers whether it is present, so the build
can be preceded by a clear report rather than followed by a cryptic CMake
`find_package` failure. It never installs silently -- the Deployer drives that
behind the shared guards (dry-run, explicit confirmation).

The mapping lives HERE, centrally, not in each project: common build libraries
(OpenSSL, zlib) have well-known packages, and a project should not have to learn
`apt install libssl-dev` vs `pacman -S mingw-w64-x86_64-openssl`. It names the
need; morfDeploy knows the packages.

Where no package manager is available for the current toolchain (the official Qt
MinGW toolchain on Windows, say), this module does not pretend: it reports that
the dependency cannot be verified or installed here and leaves the build's own
`find_package` as the honest last word.
"""

from __future__ import annotations

from dataclasses import dataclass

from .sysdeps import _is_installed


#: Logical build-dependency id -> packages per distribution family. Extend as
#: real projects and platforms require it, not ahead of need.
BUILD_DEPENDENCY_REGISTRY = {
    "openssl": {
        "debian": ["libssl-dev"],
        "fedora": ["openssl-devel"],
        "arch": ["openssl"],
    },
    "libssh2": {
        "debian": ["libssh2-1-dev"],
        "fedora": ["libssh2-devel"],
        "arch": ["libssh2"],
    },
    "nlohmann-json": {
        "debian": ["nlohmann-json3-dev"],
        "fedora": ["nlohmann-json-devel"],
        "arch": ["nlohmann-json"],
    },
    "zlib": {
        "debian": ["zlib1g-dev"],
        "fedora": ["zlib-devel"],
        "arch": ["zlib"],
    },
}


def packages_for(dep_id: str, family: str) -> list:
    """Packages providing this logical dependency on the given family, or []."""
    if not family:
        return []
    return list(BUILD_DEPENDENCY_REGISTRY.get(dep_id, {}).get(family, []))


@dataclass
class BuildDepStatus:
    """One build dependency resolved against the current machine."""

    dep: object                 # BuildDependency
    packages: tuple = ()        # packages for this family (empty if unmapped)
    missing: tuple = ()         # of those, the ones not installed
    known: bool = True          # False when the id is not in the registry

    @property
    def resolvable(self) -> bool:
        return self.known and bool(self.packages)

    @property
    def satisfied(self) -> bool:
        return self.resolvable and not self.missing


def resolve(dependencies, family: str, manager: str) -> list:
    """Status of every declared build dependency on this machine.

    A dependency the registry does not know, or one with no package for this
    family, is marked not-resolvable: it can be neither verified nor installed
    here, and the caller reports that honestly rather than guessing.
    """
    statuses = []
    for dep in dependencies:
        if dep.id not in BUILD_DEPENDENCY_REGISTRY:
            statuses.append(BuildDepStatus(dep=dep, known=False))
            continue
        packages = tuple(packages_for(dep.id, family))
        if not packages:
            statuses.append(BuildDepStatus(dep=dep, packages=()))
            continue
        missing = tuple(p for p in packages if not _is_installed(manager, p))
        statuses.append(BuildDepStatus(dep=dep, packages=packages, missing=missing))
    return statuses
