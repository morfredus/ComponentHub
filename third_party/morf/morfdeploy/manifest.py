"""What a project must declare to be installable.

Every install-service.sh in the parc carried the same four steps and differed
only by a service name and a directory. Those two values, plus the handful of
paths around them, are what this manifest holds -- so the algorithm can live in
one place and the project keeps stating its own facts.

The manifest is JSON for the same reason the shared parc configuration is:
neither C++ nor Python is privileged, and a project written in either declares
itself the same way.
"""

from __future__ import annotations

import json
import os
import platform
import sys
from dataclasses import dataclass, field
from pathlib import Path

MANIFEST_NAME = "service.json"

#: Single environment override for the install directory, across the whole
#: parc. The per-project prefixes it replaces (MT_, MN_, MS_) had already
#: drifted -- morfAnalytics carried morfMonitor's MT_APP_DIR verbatim, and
#: morfSync had none at all, so its documented override silently did nothing.
APP_DIR_ENV = "MORF_APP_DIR"

#: Still honoured, in the order written. Each was one project's own spelling of
#: the same idea, which is how morfAnalytics ended up reading morfMonitor's
#: MT_APP_DIR and morfSync documented an override nothing read.
LEGACY_APP_DIR_ENV = ("MORF_MONITOR_APP_DIR", "MT_APP_DIR", "MN_APP_DIR", "MS_APP_DIR")


class ManifestError(RuntimeError):
    """The manifest is missing, malformed, or incomplete."""


@dataclass(frozen=True)
class ConfigFile:
    """A configuration to place at install time.

    `overwrite` is false by default, and that default matters: these files hold
    settings edited by hand on the machine. An install that overwrites them
    destroys local state to deliver a default nobody asked for.
    """

    source: str
    dest: object          # str, or {"linux": ..., "windows": ...}
    overwrite: bool = False

    #: Places this configuration used to live. On install, the first one found
    #: is adopted rather than replaced by the example -- a settings file
    #: someone edited by hand outlives the directory convention it was written
    #: under. morfSync has already been moved once, from /etc/homeserverhub to
    #: /etc/morfsync, and that migration was hard-coded into its install script
    #: where nothing else could see it or learn from it.
    migrate_from: tuple = ()

    def find_predecessor(self) -> Path | None:
        """An earlier home of this configuration that still holds a file."""
        for candidate in self.migrate_from:
            path = Path(os.path.expandvars(candidate))
            if path.is_file():
                return path
        return None

    def resolved_dest(self, config_dir: Path) -> Path:
        """Absolute destinations are honoured as-is; relative ones sit in config_dir.

        The shared parc configuration lives outside any single application's
        directory -- it is read by morfMonitor and by morfDashboard -- so
        it must be expressible as an absolute path.

        And that path is not the same everywhere. `dest` may therefore be an
        object keyed by platform, exactly as `app_dir` is. A single string
        "/etc/morfsystem/morfsystem.json" would resolve on Windows to \\etc\\...
        on whatever drive happens to be current: a real directory, silently
        created, that nothing ever reads.
        """
        raw = self.dest
        if isinstance(raw, dict):
            key = {"Windows": "windows", "Darwin": "darwin"}.get(
                platform.system(), "linux")
            raw = raw.get(key) or raw.get("linux") or ""
            if not raw:
                raise ManifestError(
                    f"Configuration destination not declared for this platform: {self.source}")

        dest = Path(os.path.expandvars(raw))
        return dest if dest.is_absolute() else config_dir / dest


#: The three directories a service owns, named the same way in service.json's
#: `base` field as they are in the manifest. A purge category says which of them
#: its data sits under rather than repeating an absolute path that would then
#: have to track the platform.
PURGE_BASES = ("state", "config", "app")


@dataclass(frozen=True)
class PurgeCategory:
    """One named class of data a project knows how to erase.

    The identifier is FREE, not an enum: a project may announce `database`,
    `cache`, `thumbnails`, `sync-state` or anything else tomorrow without
    morfdeploy or morfTools changing. The contract is only that the project
    declares, for each category, a human label, whether erasing it loses real
    data (`destructive`), and HOW it is erased -- so morfdeploy executes a
    stated intention and never has to know where a service keeps its SQLite.

    Two kinds of erasure, because the parc has both:

      - "path"    remove files or directories, resolved under `base`
                  (state / config / app). morfdeploy does the removal; a
                  dry-run lists the real paths. Always simulatable.
      - "command" run the project's OWN purge entry point (a category that is
                  part of a shared database, say, which a path cannot express).
                  morfdeploy only forwards the intention. It can be simulated
                  only if the project says its command honours --dry-run
                  (`dry_run: true`); otherwise a dry-run reports that this
                  category cannot be simulated rather than pretending it can.
    """

    id: str
    label: str
    destructive: bool = False
    kind: str = "path"          # "path" | "command" (JSON key: "type")
    paths: tuple = ()           # kind == "path": default location, under `base`
    base: str = "state"         # kind == "path": state | config | app
    #: kind == "path": a dotted key into the DEPLOYED config whose value, when
    #: set, OVERRIDES the default location -- for data a project lets the admin
    #: relocate (morfCollector's vault_root / storage_root). When the key is
    #: absent or empty, the default `base`/`paths` applies. This is how a purge
    #: stays honest for a configurable path: the project declares where to read
    #: the real location, morfdeploy reads it, and neither has to guess.
    from_config: str = ""
    #: How to read the from_config value:
    #:   "path" (default) -- the value IS the target (a full path that replaces
    #:           base/paths). Fits a key that names the data itself (a vault dir).
    #:   "dir"  -- the value is a PARENT directory; `paths` are joined onto it,
    #:           and the fallback directory is base/`default_dir`. Fits a key that
    #:           names a directory holding named files (a cache dir with one
    #:           sqlite per history).
    from_config_kind: str = "path"
    #: kind == "path", from_config_kind == "dir": the fallback directory under
    #: `base` when the config key is absent (e.g. the default of a cache_dir).
    default_dir: str = ""
    command: tuple = ()         # kind == "command"
    dry_run: bool = False       # kind == "command": does the command support --dry-run?


@dataclass(frozen=True)
class SystemDependency:
    """A system package a project needs, declared as a NEED, not a mechanism.

    The project states what a capability requires ("the LD2410C driver needs Qt
    SerialPort") and which package provides it per distribution family. It never
    states how to install it -- that is morfDeploy's job, which detects the
    platform's package manager and resolves the right package. `required` False
    means an optional capability: its absence disables that capability but never
    blocks the rest (morfSensor's radar driver, morfPhoto's exiftool). `required`
    True means the component cannot run correctly without it.
    """

    id: str
    label: str
    required: bool = False
    required_for: tuple = ()          # capabilities this dependency enables
    #: Packages per family. Two shapes are accepted, both understood here:
    #:   legacy flat list  {"debian": ["libssh2-1-dev"]}          -> BUILD only;
    #:   split object      {"debian": {"build": [...], "runtime": [...]}}.
    #: BUILD prepares the compilation machine; RUNTIME is what a .deb's `Depends`
    #: draws from -- never inferred from a `-dev` name. A linked library is found
    #: from the binary itself (dpkg-shlibdeps), so RUNTIME lists only what the
    #: linkage cannot reveal: a subprocess (exiftool), a plugin, a system tool.
    packages: dict = field(default_factory=dict)

    def build_packages(self, family: str) -> tuple:
        """Packages needed to COMPILE, for this family (legacy flat list = build)."""
        value = self.packages.get(family)
        if value is None:
            return ()
        if isinstance(value, dict):
            return tuple(value.get("build") or ())
        return tuple(value)   # a flat list is, historically, the build packages

    def runtime_packages(self, family: str) -> tuple:
        """Packages needed to RUN, for this family -- what feeds a .deb's Depends.

        Empty for a legacy flat list: it declared build packages, and a runtime
        need must be stated explicitly, never guessed from a `-dev` name.
        """
        value = self.packages.get(family)
        if isinstance(value, dict):
            return tuple(value.get("runtime") or ())
        return ()


@dataclass(frozen=True)
class BuildDependency:
    """A library a project needs to COMPILE, declared as a logical need.

    Distinct from SystemDependency (which a service needs to RUN): this one is
    required at build time (headers/libs a `find_package` looks for). The project
    names the need by a logical id -- "openssl", "libssh2" -- never a package or a
    package manager. morfDeploy maps the id to the right package for the platform
    (see builddeps.py) and resolves it BEFORE the build, so a missing library is a
    clear stop rather than a cryptic CMake `find_package` failure fifteen projects
    deep.
    """

    id: str
    required: bool = True


@dataclass(frozen=True)
class Manifest:
    """A project's deployment facts."""

    repo_root: Path
    service_name: str
    display_name: str
    binary: str
    # Optional root-only companion used by the rare services that must perform
    # one narrowly defined privileged action while their HTTP process remains
    # unprivileged. It is deliberately not part of the normal application dir.
    helper_binary: str = ""
    app_dirs: dict = field(default_factory=dict)
    config_dirs: dict = field(default_factory=dict)
    state_dirs: dict = field(default_factory=dict)
    configs: tuple = ()
    description: str = ""
    status_url: str = ""

    #: Purgeable data categories this project announces. Empty is the norm and
    #: means exactly what it did before this field existed: nothing to purge
    #: selectively. A category here is what makes `service.py purge <id>`
    #: possible at all.
    purge_categories: tuple = ()

    #: System packages this project needs, declared as needs. Empty means no
    #: declared system dependency: install/deploy behave exactly as before.
    system_dependencies: tuple = ()

    #: Libraries this project needs to COMPILE, by logical id. Empty means none
    #: declared: the build behaves exactly as before.
    build_dependencies: tuple = ()

    #: Places an earlier convention installed this binary. Reported at install,
    #: never deleted: removing an executable nobody asked us to remove is not a
    #: tidy-up, and a stale copy left unmentioned is how someone ends up
    #: debugging the version they are not running.
    legacy_binaries: tuple = ()

    # -- Derived paths ----------------------------------------------------

    def app_dir(self) -> Path:
        """Where the binary and configurations are installed.

        The environment override wins over the manifest, which wins over a
        conventional default. Resolution order is fixed here rather than in
        each backend so that the answer cannot depend on the platform for any
        reason other than the path itself.
        """
        override = os.environ.get(APP_DIR_ENV)
        if override:
            return Path(override)

        # The prefixes MORF_APP_DIR replaces are still honoured, and say so.
        # Someone who wrote MT_APP_DIR in a note a year ago should get the
        # directory they asked for, not silently get the default while
        # believing otherwise -- which is what a straight rename would deliver.
        for legacy in LEGACY_APP_DIR_ENV:
            value = os.environ.get(legacy)
            if value:
                print(
                    f"[note] {legacy} is honoured but superseded by {APP_DIR_ENV}, "
                    f"which is the same variable for every service.",
                    file=sys.stderr,
                )
                return Path(value)

        key = {"Windows": "windows", "Darwin": "darwin"}.get(platform.system(), "linux")
        declared = self.app_dirs.get(key)
        if declared:
            return Path(os.path.expandvars(declared))

        if key == "windows":
            base = os.environ.get("ProgramData", r"C:\ProgramData")
            return Path(base) / self.service_name
        return Path("/opt") / self.service_name

    def config_dir(self) -> Path:
        """Where THIS service's configuration lives -- separate from its binary.

        /etc is where a Linux administrator looks first, and the FHS is explicit
        that host configuration belongs there rather than beside the program.
        Keeping it out of app_dir also means wiping /opt/<service> to reinstall
        cleanly no longer takes the settings with it.

        The parc previously put both in /opt, self-contained-bundle style. Only
        morfSync was doing it the conventional way, and it got normalised onto
        the twelve that were not -- the right goal, the wrong reference.
        """
        key = {"Windows": "windows", "Darwin": "darwin"}.get(platform.system(), "linux")
        declared = self.config_dirs.get(key)
        if declared:
            return Path(os.path.expandvars(declared))
        if key == "windows":
            base = os.environ.get("ProgramData", r"C:\ProgramData")
            return Path(base) / self.service_name
        return Path("/etc") / self.service_name

    def state_dir(self) -> Path:
        """Where THIS service's PERSISTENT STATE lives -- separate from both its
        binary and its configuration.

        The FHS is explicit: variable data a service generates itself (generated
        keys, encrypted vaults, sync sequences, collection cursors, dedup
        indexes, last-run timestamps) belongs under /var/lib, not in /etc (which
        is the administrator's read-only reference) nor in /opt (which is the
        program). Putting state in /etc is exactly what left morfCollector's
        vault unwritable under a root-owned /etc/morfsystem: a service running as
        its own user could never create its key.

        On Linux this pairs with the unit's `StateDirectory=morfsystem/<service>`,
        which systemd creates owned by the service User with the right mode and
        exposes as $STATE_DIRECTORY. This method reports the same path so the
        install summary and any explicit __STATE_DIR__ substitution agree with it.
        """
        key = {"Windows": "windows", "Darwin": "darwin"}.get(platform.system(), "linux")
        declared = self.state_dirs.get(key)
        if declared:
            return Path(os.path.expandvars(declared))
        if key == "windows":
            base = os.environ.get("ProgramData", r"C:\ProgramData")
            return Path(base) / "morfsystem" / self.service_name / "state"
        return Path("/var/lib/morfsystem") / self.service_name

    def binary_name(self) -> str:
        """The executable's file name, with the platform's extension."""
        if platform.system() == "Windows" and not self.binary.endswith(".exe"):
            return self.binary + ".exe"
        return self.binary

    def helper_binary_name(self) -> str:
        if platform.system() == "Windows" and self.helper_binary and not self.helper_binary.endswith(".exe"):
            return self.helper_binary + ".exe"
        return self.helper_binary

    def installed_binary(self) -> Path:
        return self.app_dir() / self.binary_name()

    # -- Purge ------------------------------------------------------------

    def base_dir(self, base: str) -> Path:
        """Resolve a purge category's `base` to the directory it names.

        The three bases are the three directories a service owns, so a category
        never repeats an absolute path that would then have to track the
        platform on its own. Anything else is a manifest error, caught here
        rather than as a mysterious deletion under the wrong root.
        """
        if base == "state":
            return self.state_dir()
        if base == "config":
            return self.config_dir()
        if base == "app":
            return self.app_dir()
        raise ManifestError(
            f"purge category base '{base}' is not one of {', '.join(PURGE_BASES)}.")

    def purge_ids(self) -> tuple:
        """The identifiers, in declaration order, for messages and validation."""
        return tuple(category.id for category in self.purge_categories)

    def deployed_config_value(self, dotted_key: str):
        """Read a value from the service's DEPLOYED config, or None if unavailable.

        Used to resolve a purge path a project lets the admin relocate: the
        project names the key, morfdeploy reads the config actually on this
        machine. Returns None when the config is not deployed, the key is absent,
        or the value is empty -- every "fall back to the default location" case,
        which the caller treats identically. Reads the service's own (first)
        config; the shared parc file, listed after it, is never the source of a
        service's data path.
        """
        if not self.configs:
            return None
        dest = self.configs[0].resolved_dest(self.config_dir())
        if not dest.is_file():
            return None
        try:
            data = json.loads(dest.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError):
            return None
        cursor = data
        for part in dotted_key.split("."):
            if not isinstance(cursor, dict) or part not in cursor:
                return None
            cursor = cursor[part]
        if isinstance(cursor, str) and cursor.strip():
            return cursor.strip()
        return None

    def resolve_purge_token(self, token: str) -> str:
        """Substitute the placeholders a `command` category may use.

        A project states its purge command without hard-coding this machine's
        paths: __BINARY__ becomes the installed executable, and the three owned
        directories are available too. The service keeps the knowledge of what
        to run; the manifest only fills in where things landed on this host.
        """
        replacements = {
            "__BINARY__": str(self.installed_binary()),
            "__STATE_DIR__": str(self.state_dir()),
            "__CONFIG_DIR__": str(self.config_dir()),
            "__APP_DIR__": str(self.app_dir()),
        }
        return replacements.get(token, token)

    # -- Loading ----------------------------------------------------------

    @classmethod
    def load(cls, repo_root: Path) -> "Manifest":
        path = repo_root / MANIFEST_NAME
        if not path.is_file():
            raise ManifestError(
                f"No {MANIFEST_NAME} in {repo_root}.\n"
                f"A project states its own deployment facts there; see "
                f"morfTemplateService/{MANIFEST_NAME} for the reference."
            )
        try:
            raw = json.loads(path.read_text(encoding="utf-8-sig"))
        except json.JSONDecodeError as exc:
            raise ManifestError(f"{path} is not valid JSON: {exc}") from exc

        missing = [k for k in ("service_name", "binary") if not raw.get(k)]
        if missing:
            raise ManifestError(f"{path} declares no {' and no '.join(missing)}.")

        configs = tuple(
            ConfigFile(
                source=entry["source"],
                dest=entry["dest"],
                overwrite=bool(entry.get("overwrite", False)),
                migrate_from=tuple(entry.get("migrate_from", ())),
            )
            for entry in raw.get("configs", [])
            if entry.get("source") and entry.get("dest")
        )

        purge_categories = cls._parse_purge(path, raw.get("purge", []))
        system_dependencies = cls._parse_system_deps(
            path, raw.get("system_dependencies", []))
        build_dependencies = cls._parse_build_deps(
            path, raw.get("build_dependencies", []))

        return cls(
            repo_root=repo_root,
            service_name=raw["service_name"],
            display_name=raw.get("display_name") or raw["service_name"],
            binary=raw["binary"],
            helper_binary=raw.get("helper_binary", ""),
            app_dirs=raw.get("app_dir") or {},
            config_dirs=raw.get("config_dir") or {},
            state_dirs=raw.get("state_dir") or {},
            configs=configs,
            description=raw.get("description", ""),
            status_url=raw.get("status_url", ""),
            legacy_binaries=tuple(raw.get("legacy_binaries", ())),
            purge_categories=purge_categories,
            system_dependencies=system_dependencies,
            build_dependencies=build_dependencies,
        )

    @staticmethod
    def _parse_build_deps(path: Path, raw_list: object) -> tuple:
        """Read and validate build_dependencies: a list of {id, required?}.

        Only the logical id and whether it is required live here; the package
        that provides it per platform is morfDeploy's business, not the project's.
        """
        if not isinstance(raw_list, list):
            raise ManifestError(f"{path}: 'build_dependencies' must be a list.")
        deps = []
        seen = set()
        for entry in raw_list:
            if not isinstance(entry, dict):
                raise ManifestError(
                    f"{path}: each 'build_dependencies' entry must be an object.")
            did = entry.get("id")
            if not did or not isinstance(did, str):
                raise ManifestError(
                    f"{path}: a 'build_dependencies' entry has no 'id'.")
            if did in seen:
                raise ManifestError(f"{path}: duplicate build dependency '{did}'.")
            seen.add(did)
            deps.append(BuildDependency(
                id=did, required=bool(entry.get("required", True))))
        return tuple(deps)

    @staticmethod
    def _parse_system_deps(path: Path, raw_list: object) -> tuple:
        """Read and validate the system_dependencies block.

        A malformed declaration is rejected at load time with the service named,
        so the resolver can trust it. `packages` must map a distribution family
        to a list of package names; an entry that declares none is refused rather
        than silently doing nothing.
        """
        if not isinstance(raw_list, list):
            raise ManifestError(
                f"{path}: 'system_dependencies' must be a list.")
        deps = []
        seen = set()
        for entry in raw_list:
            if not isinstance(entry, dict):
                raise ManifestError(
                    f"{path}: each 'system_dependencies' entry must be an object.")
            did = entry.get("id")
            if not did or not isinstance(did, str):
                raise ManifestError(
                    f"{path}: a 'system_dependencies' entry has no 'id'.")
            if did in seen:
                raise ManifestError(
                    f"{path}: duplicate system dependency '{did}'.")
            seen.add(did)
            packages = entry.get("packages", {})
            if not isinstance(packages, dict) or not packages:
                raise ManifestError(
                    f"{path}: system dependency '{did}' declares no 'packages'.")
            normalised = {}
            for family, names in packages.items():
                # Two accepted shapes per family: the legacy flat list (BUILD), or
                # the split object {build, runtime}. A build-only or runtime-only
                # split is valid (exiftool is runtime-only); an entry that lists
                # neither is refused, as an empty list always was.
                if isinstance(names, list):
                    if not names:
                        raise ManifestError(
                            f"{path}: system dependency '{did}', family '{family}' "
                            "lists no package.")
                    normalised[family] = tuple(names)
                elif isinstance(names, dict):
                    build = names.get("build", [])
                    runtime = names.get("runtime", [])
                    if not isinstance(build, list) or not isinstance(runtime, list):
                        raise ManifestError(
                            f"{path}: system dependency '{did}', family '{family}': "
                            "'build' and 'runtime' must be lists.")
                    if not build and not runtime:
                        raise ManifestError(
                            f"{path}: system dependency '{did}', family '{family}' "
                            "declares neither a build nor a runtime package.")
                    normalised[family] = {"build": tuple(build),
                                          "runtime": tuple(runtime)}
                else:
                    raise ManifestError(
                        f"{path}: system dependency '{did}', family '{family}' "
                        "must be a list of packages, or {build, runtime}.")
            deps.append(SystemDependency(
                id=did,
                label=entry.get("label") or did,
                required=bool(entry.get("required", False)),
                required_for=tuple(entry.get("required_for", ())),
                packages=normalised,
            ))
        return tuple(deps)

    @staticmethod
    def _parse_purge(path: Path, raw_list: object) -> tuple:
        """Read and VALIDATE the purge block, refusing an unusable declaration.

        A malformed purge category is rejected at load time, with the service
        named, rather than surfacing later as a wrong deletion or a silent
        no-op. The identifier is free, but everything the executor relies on --
        a kind it understands, the paths or command that kind needs, a base it
        can resolve -- is checked here so `purge` can trust the manifest.
        """
        if not isinstance(raw_list, list):
            raise ManifestError(f"{path}: 'purge' must be a list of categories.")

        categories = []
        seen = set()
        for entry in raw_list:
            if not isinstance(entry, dict):
                raise ManifestError(f"{path}: each 'purge' entry must be an object.")
            cid = entry.get("id")
            if not cid or not isinstance(cid, str):
                raise ManifestError(f"{path}: a 'purge' category has no 'id'.")
            if cid in seen:
                raise ManifestError(f"{path}: duplicate purge category '{cid}'.")
            seen.add(cid)

            label = entry.get("label") or cid
            kind = entry.get("type", "path")
            destructive = bool(entry.get("destructive", False))

            if kind == "path":
                paths = tuple(entry.get("paths", ()))
                if not paths:
                    raise ManifestError(
                        f"{path}: purge category '{cid}' is type 'path' but lists no 'paths'.")
                base = entry.get("base", "state")
                if base not in PURGE_BASES:
                    raise ManifestError(
                        f"{path}: purge category '{cid}' has base '{base}', "
                        f"not one of {', '.join(PURGE_BASES)}.")
                fc_kind = entry.get("from_config_kind", "path")
                if fc_kind not in ("path", "dir"):
                    raise ManifestError(
                        f"{path}: purge category '{cid}' has from_config_kind "
                        f"'{fc_kind}', expected 'path' or 'dir'.")
                categories.append(PurgeCategory(
                    id=cid, label=label, destructive=destructive,
                    kind="path", paths=paths, base=base,
                    from_config=entry.get("from_config", ""),
                    from_config_kind=fc_kind,
                    default_dir=entry.get("default_dir", "")))
            elif kind == "command":
                command = tuple(entry.get("command", ()))
                if not command:
                    raise ManifestError(
                        f"{path}: purge category '{cid}' is type 'command' but "
                        f"lists no 'command'.")
                categories.append(PurgeCategory(
                    id=cid, label=label, destructive=destructive,
                    kind="command", command=command,
                    dry_run=bool(entry.get("dry_run", False))))
            else:
                raise ManifestError(
                    f"{path}: purge category '{cid}' has unknown type '{kind}' "
                    f"(expected 'path' or 'command').")

        return tuple(categories)
