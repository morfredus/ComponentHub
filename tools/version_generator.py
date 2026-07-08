#!/usr/bin/env python3
"""
version_generator.py — Incrémente la version du projet (fichier VERSION,
format semver MAJOR.MINOR.PATCH) et l'écrit pour build_info.py.

Usage :
  python tools/version_generator.py patch   # 1.0.6 -> 1.0.7 (défaut)
  python tools/version_generator.py minor   # 1.0.6 -> 1.1.0
  python tools/version_generator.py major   # 1.0.6 -> 2.0.0
"""

import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
VERSION_FILE = PROJECT_ROOT / "VERSION"


def read_version() -> tuple[int, int, int]:
    if not VERSION_FILE.exists():
        return (0, 0, 0)
    text = VERSION_FILE.read_text(encoding="utf-8").strip()
    parts = text.split(".")
    return tuple(int(p) for p in parts[:3]) if len(parts) == 3 else (0, 0, 0)


def write_version(major: int, minor: int, patch: int):
    VERSION_FILE.write_text(f"{major}.{minor}.{patch}\n", encoding="utf-8")


def main():
    bump = sys.argv[1] if len(sys.argv) > 1 else "patch"
    major, minor, patch = read_version()

    if bump == "major":
        major, minor, patch = major + 1, 0, 0
    elif bump == "minor":
        minor, patch = minor + 1, 0
    elif bump == "patch":
        patch += 1
    else:
        print(f"Type de bump inconnu : {bump} (attendu : major|minor|patch)")
        sys.exit(1)

    write_version(major, minor, patch)
    print(f"Version mise à jour : {major}.{minor}.{patch}")


if __name__ == "__main__":
    main()
