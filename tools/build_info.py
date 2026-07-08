#!/usr/bin/env python3
"""
build_info.py — Génère BUILD_DATE / BUILD_TIME / GIT_COMMIT / PROJECT_VERSION
injectés via build_flags avant chaque compilation PlatformIO.

Lit la version depuis platformio.ini (build_flags existant) si présente,
sinon retombe sur version_generator.py / VERSION.
"""

import datetime
import subprocess
from pathlib import Path

env = None
if "Import" in globals():
    # "Import" est injecté par PlatformIO/SCons dans le contexte exec() ;
    # absent en exécution standalone (python tools/build_info.py).
    globals()["Import"]("env")

# Exécuté par PlatformIO via exec() dans SConscript : __file__ n'existe pas.
# env["PROJECT_DIR"] est alors la seule source fiable du chemin du projet.
if env is not None:
    PROJECT_ROOT = Path(env.get("PROJECT_DIR"))
else:
    PROJECT_ROOT = Path(__file__).resolve().parent.parent


def git_commit() -> str:
    try:
        out = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=PROJECT_ROOT, capture_output=True, text=True, timeout=5,
        )
        if out.returncode == 0:
            return out.stdout.strip()
    except Exception:
        pass
    return "unknown"


def project_version() -> str:
    version_file = PROJECT_ROOT / "VERSION"
    if version_file.exists():
        return version_file.read_text(encoding="utf-8").strip()
    return "0.0.0-dev"


def main():
    now = datetime.datetime.now()
    build_date = now.strftime("%Y-%m-%d")
    build_time = now.strftime("%H:%M:%S")
    commit = git_commit()
    version = project_version()

    flags = [
        f'-D BUILD_DATE=\\"{build_date}\\"',
        f'-D BUILD_TIME=\\"{build_time}\\"',
        f'-D GIT_COMMIT=\\"{commit}\\"',
        f'-D PROJECT_VERSION=\\"{version}\\"',
    ]

    print(f"[build_info] version={version} commit={commit} date={build_date} time={build_time}")

    if env is not None:
        env.Append(BUILD_FLAGS=flags)


if __name__ == "__main__" or env is not None:
    main()
