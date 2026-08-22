"""morfdeploy's own reader of `morfproject.json` (schema v1).

Autonomous on purpose: morfdeploy is vendored into each project and must not
depend on morfTools to understand a manifest it is itself a consumer of. This
reader parses only what morfdeploy needs to PACKAGE a target -- schema version,
project id/type, the default provider, and each target's platform, build preset
and package format (with its optional provider override).

It applies the SAME observable contract as morfTools' reader (same required
fields, same normalised values, same provider inheritance, same rejection of an
unknown schema version, same explicit errors naming the invalid field). The two
readers stay separate copies -- one per consumer, as the parc vendors its shared
code -- but must never diverge on what a valid manifest is.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

PROJECT_FILE = "morfproject.json"
SCHEMA_VERSION = 1

TYPES = ("service", "application", "firmware",
         "library", "tool", "documentation", "meta", "template")
PROVIDERS = ("none", "morfdeploy", "project")


class MorfProjectError(ValueError):
    """A `morfproject.json` present but not conforming to schema v1."""


def _norm_arch(arch):
    """Internal architecture names: x86_64 and arm64, whatever the spelling."""
    if arch is None:
        return None
    a = str(arch).lower()
    if a in ("x86_64", "amd64"):
        return "x86_64"
    if a in ("arm64", "aarch64"):
        return "arm64"
    return a


@dataclass
class Target:
    name: str
    platform: dict
    build: dict
    package: dict
    default_provider: str

    @property
    def provider(self) -> str:
        return self.package.get("provider") or self.default_provider

    @property
    def os(self):
        return self.platform.get("os")

    @property
    def arch(self):
        return self.platform.get("arch")

    @property
    def build_preset(self):
        return self.build.get("preset")

    @property
    def format(self):
        return self.package.get("format")


@dataclass
class MorfProject:
    id: str
    type: str
    provider: str
    targets: dict = field(default_factory=dict)
    path: Path | None = None

    @property
    def is_morfdeploy(self) -> bool:
        """The default provider is morfdeploy (a target may still override)."""
        return self.provider == "morfdeploy"

    def morfdeploy_targets(self) -> list:
        """Targets whose EFFECTIVE provider is morfdeploy -- what this tool packages."""
        return [t for t in self.targets.values() if t.provider == "morfdeploy"]


def load(project_path: Path) -> MorfProject | None:
    """Read `morfproject.json` at a project root, or None when absent.

    Absent is not an error (rolled out project by project). Present-but-invalid
    IS: a manifest acted upon must be trustworthy. Errors name the invalid field.
    """
    file = Path(project_path) / PROJECT_FILE
    if not file.is_file():
        return None
    try:
        data = json.loads(file.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError) as exc:
        raise MorfProjectError(f"{file}: cannot read ({exc})")

    version = data.get("schema_version")
    if version != SCHEMA_VERSION:
        raise MorfProjectError(
            f"{file}: schema_version {version!r} unknown (this reader speaks v{SCHEMA_VERSION})")

    project = data.get("project") or {}
    ptype = project.get("type")
    if ptype not in TYPES:
        raise MorfProjectError(
            f"{file}: project.type must be one of {', '.join(TYPES)} (got {ptype!r})")

    pack = data.get("packaging") or {}
    provider = pack.get("provider")
    if provider not in PROVIDERS:
        raise MorfProjectError(
            f"{file}: packaging.provider must be one of {', '.join(PROVIDERS)} "
            f"(got {provider!r})")

    raw = pack.get("targets") or {}
    if not isinstance(raw, dict):
        raise MorfProjectError(f"{file}: packaging.targets must be an object")

    targets = {}
    for name, spec in raw.items():
        spec = spec or {}
        platform = dict(spec.get("platform") or {})
        if "arch" in platform:
            platform["arch"] = _norm_arch(platform["arch"])
        tpkg = dict(spec.get("package") or {})
        tprov = tpkg.get("provider")
        if tprov is not None and tprov not in PROVIDERS:
            raise MorfProjectError(
                f"{file}: packaging.targets.{name}.package.provider must be one of "
                f"{', '.join(PROVIDERS)} (got {tprov!r})")
        targets[name] = Target(
            name=name,
            platform=platform,
            build=dict(spec.get("build") or {}),
            package=tpkg,
            default_provider=provider,
        )

    return MorfProject(
        id=project.get("id") or file.parent.name,
        type=ptype,
        provider=provider,
        targets=targets,
        path=file.parent,
    )
