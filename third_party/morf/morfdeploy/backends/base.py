"""The contract a platform must satisfy to host a morfSystem service.

The orchestration around a service install is identical everywhere: find or
build the binary, stop whatever runs, copy the binary and the configurations
into a fixed directory, then hand the result to the system's service manager.
Only that last step is platform business, and it is the only thing this
interface describes.

Keeping the interface this narrow is what makes a new platform cheap. A
contributor adding macOS writes one module implementing these methods; they
touch no orchestration, no configuration handling, and no project. If a method
here ever needs to know *which* platform it runs on, the boundary has been
drawn in the wrong place.
"""

from __future__ import annotations

import abc
from pathlib import Path

from ..manifest import Manifest


class ServiceBackend(abc.ABC):
    """A system service manager: systemd, Windows SCM, launchd."""

    #: Human-readable name, used in messages and diagnostics.
    name: str = "unknown"

    #: False for platforms whose backend exists but has never been validated.
    #: The parc supports what it can test; see `supported_note`.
    supported: bool = True

    #: Why a backend is not supported, shown verbatim to whoever hits it.
    supported_note: str = ""

    # -- Interrogation ----------------------------------------------------

    @abc.abstractmethod
    def is_installed(self, manifest: Manifest) -> bool:
        """True when the service is registered with the system."""

    def is_active(self, manifest: Manifest) -> bool:
        """True when the service is not merely installed but currently RUNNING.

        The purge guard uses it: erasing data a live service is still writing to
        can corrupt it. The default is False on purpose -- a backend that cannot
        tell must not block a purge on a guess. The guard is a net for the clear
        "it is running" case, and --force overrides it when the caller knows the
        service is stopped (or accepts the risk).
        """
        return False

    def can_query_installation(self, manifest: Manifest) -> bool:
        """True when this process may ask the system what is registered.

        Default True: on most platforms, listing units is a read anyone can do
        -- `systemctl list-unit-files` answers a plain user perfectly well.

        It exists because Windows is different, and the difference is
        dangerous: `schtasks /Query` on a task registered to run as SYSTEM
        answers "access denied" to a non-elevated caller, with an exit status
        indistinguishable from "no such task". A caller would conclude "not
        installed" from what is only a lack of rights, skip the service, and
        report success. Saying "I cannot tell" is the honest third answer.
        """
        return True

    @abc.abstractmethod
    def status(self, manifest: Manifest) -> str:
        """A short human-readable status, for display only.

        Never parsed: callers that need a decision use `is_installed`. Status
        text is the most volatile thing a service manager produces, and code
        that reads it breaks on a system upgrade rather than on a change of
        ours.
        """

    # -- Lifecycle --------------------------------------------------------

    @abc.abstractmethod
    def install(self, manifest: Manifest, app_dir: Path, run_user: str) -> None:
        """Register the service so it starts on boot, and start it now.

        Must be idempotent: reinstalling over an existing service is the normal
        update path, not an error.
        """

    @abc.abstractmethod
    def stop(self, manifest: Manifest) -> None:
        """Stop the service if it runs; do nothing if it does not.

        Called before the binary is replaced, so a service that is absent, or
        already stopped, is a success and not a failure -- the goal is "not
        running", and it is already met.
        """

    @abc.abstractmethod
    def start(self, manifest: Manifest) -> None:
        """Start the service."""

    @abc.abstractmethod
    def uninstall(self, manifest: Manifest) -> None:
        """Deregister the service.

        Leaves the application directory alone. It holds configurations the
        person edited by hand, and an uninstall that silently discards them is
        the kind of surprise no message can undo afterwards.
        """

    # -- Runtime dependencies ---------------------------------------------

    def install_runtime(self, installed_binary: Path, source_binary: Path) -> None:
        """Place beside the binary any shared libraries it needs to start.

        `installed_binary` is the copy in the application directory; the runtime
        libraries go next to it. `source_binary` is the freshly built one still
        in the build tree, kept as a parameter because the tools that find those
        libraries (windeployqt) are located from the build's CMake cache, which
        sits beside it -- not beside the installed copy.

        A no-op by default, and that is correct for every platform whose shared
        libraries come from a system-wide location: a Linux service finds Qt
        where the package manager installed it, so the binary alone runs.

        Windows has no such place. A Qt program copied without Qt6Core.dll, the
        platform plugin and the compiler runtime starts to a 'DLL not found'
        error that the Service Control Manager reports only as a failed start,
        with nothing pointing at the missing files. The Windows backend
        overrides this to gather them; keeping it a no-op here means no other
        platform pays for a problem it does not have.
        """

    @abc.abstractmethod
    def has_privileges(self) -> bool:
        """True when the current process holds those rights."""

    def privilege_hint(self) -> str:
        """How to re-run with the required rights, in this platform's terms."""
        return "Re-run with administrator privileges."
