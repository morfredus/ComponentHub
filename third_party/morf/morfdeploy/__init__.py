"""One orchestration core, native mechanisms per operating system.

Officially supported: Windows x64, Linux x64, Linux ARM64 (Raspberry Pi).
macOS is architecturally accommodated but not supported -- see backends/launchd.py.
"""

from .core import DeployError, Deployer
from .manifest import Manifest, ManifestError

__all__ = ["Deployer", "DeployError", "Manifest", "ManifestError"]
