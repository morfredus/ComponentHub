#!/usr/bin/env python3
"""
release.py — Pipeline de release complet :
  1. version_generator.py (bump de version)
  2. pio run (build firmware — régénère automatiquement l'interface web
     embarquée en PROGMEM via extra_scripts, voir platformio.ini)

Un seul binaire (.pio/build/<env>/firmware.bin) contient désormais le
firmware ET l'interface web : plus d'image LittleFS séparée à construire
pour l'UI (LittleFS ne sert plus qu'aux données de l'inventaire, jamais
régénérées par ce pipeline).

Usage :
  python tools/release.py [patch|minor|major]
"""

import subprocess
import sys
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent


def step(description: str, cmd: list) -> bool:
    print(f"\n=== {description} ===")
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(f"ÉCHEC : {description}")
        return False
    return True


def main():
    bump = sys.argv[1] if len(sys.argv) > 1 else "patch"

    steps = [
        ("Incrémentation de version", [sys.executable, str(TOOLS_DIR / "version_generator.py"), bump]),
        ("Compilation du firmware (UI embarquée automatiquement)", ["pio", "run"]),
    ]

    for description, cmd in steps:
        if not step(description, cmd):
            sys.exit(1)

    print("\nRelease prête : .pio/build/<env>/firmware.bin — `pio run --target upload` suffit "
          "à mettre à jour firmware ET interface web.")


if __name__ == "__main__":
    main()
