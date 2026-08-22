"""launchd backend — macOS. Architecturally provided, not supported.

The parc supports what it can test. There is no macOS development or validation
machine, so no claim is made that this works: the methods raise rather than run
plausible-looking commands whose failure modes nobody has ever observed.

That is a deliberate policy, not an oversight. A backend that half-works is
worse than one that refuses, because it fails somewhere downstream -- on a
machine its author cannot reach -- and the person hitting it has no way to know
whether they misconfigured something or ran into untested code.

What a contributor needs to do is bounded and stated: implement the methods of
ServiceBackend, generate a .plist in ~/Library/LaunchAgents (per-user) or
/Library/LaunchDaemons (system-wide), drive it with `launchctl`, and flip
`supported` to True once it has been exercised on a real machine. Nothing
outside this file needs to change -- the orchestration, the manifests and the
projects are already platform-agnostic.
"""

from __future__ import annotations

from pathlib import Path

from ..manifest import Manifest
from .base import ServiceBackend

NOTE = (
    "macOS is not an officially supported platform for morfSystem.\n"
    "The architecture accommodates it -- one module implementing ServiceBackend,\n"
    "generating a .plist and driving launchctl -- but no development or\n"
    "validation environment exists, so no support is promised for what cannot\n"
    "be tested. Contributions are welcome; see morfTools/lib/morfdeploy/\n"
    "backends/launchd.py."
)


class LaunchdBackend(ServiceBackend):
    name = "launchd"
    supported = False
    supported_note = NOTE

    def _unsupported(self):
        return NotImplementedError(NOTE)

    def is_installed(self, manifest: Manifest) -> bool:
        raise self._unsupported()

    def status(self, manifest: Manifest) -> str:
        raise self._unsupported()

    def install(self, manifest: Manifest, app_dir: Path, run_user: str) -> None:
        raise self._unsupported()

    def stop(self, manifest: Manifest) -> None:
        raise self._unsupported()

    def start(self, manifest: Manifest) -> None:
        raise self._unsupported()

    def uninstall(self, manifest: Manifest) -> None:
        raise self._unsupported()

    def requires_privileges(self) -> bool:
        return True

    def has_privileges(self) -> bool:
        return False
