"""Backend selection.

The only place in morfdeploy that asks which platform it runs on. Everything
downstream receives a ServiceBackend and never enquires further -- that is what
keeps `platform.system()` from spreading back through the orchestration, one
special case at a time, until the single core exists in name only.
"""

from __future__ import annotations

import platform

from .base import ServiceBackend
from .launchd import LaunchdBackend
from .systemd import SystemdBackend
from .windows import WindowsBackend

BACKENDS = {
    "Linux": SystemdBackend,
    "Windows": WindowsBackend,
    "Darwin": LaunchdBackend,
}


def select(system: str | None = None) -> ServiceBackend:
    """Return the backend for this machine.

    An unknown system is an error rather than a fallback to systemd: guessing
    would run `systemctl` on a platform that has none and report whatever that
    failure happens to look like.
    """
    system = system or platform.system()
    backend_class = BACKENDS.get(system)
    if backend_class is None:
        raise RuntimeError(
            f"No service backend for '{system}'.\n"
            f"Supported: {', '.join(sorted(BACKENDS))}."
        )
    return backend_class()


__all__ = ["ServiceBackend", "select", "BACKENDS"]
