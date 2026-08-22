#!/usr/bin/env python3
"""Lanceur de compilateur + cloture d'activite Monitor.

Deux modes :

1. Lanceur (CMAKE_CXX_COMPILER_LAUNCHER) :
   python record_compile.py <stamp> <compilateur> [args...]
   Au premier appel, ecrit l'heure de debut dans <stamp>, puis execute le
   compilateur. Ainsi la duree couvre le vrai travail g++/clang, pas un
   cmake --preset deja a jour.

2. Cloture (POST_BUILD) :
   python record_compile.py --finish --stamp <stamp> --project <nom> --preset <p>
   Emet l'activite si le tampon existe encore, puis pose un marqueur pour que
   morfDeploy ne renvoie pas un deuxieme evenement.
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path


def _launch(stamp: Path, cmd: list[str]) -> int:
    if not cmd:
        return 1
    if not stamp.exists():
        try:
            stamp.parent.mkdir(parents=True, exist_ok=True)
            stamp.write_text(str(time.time()), encoding="utf-8")
        except OSError:
            pass
    if os.name == "nt":
        return subprocess.call(cmd)
    os.execvp(cmd[0], cmd)
    return 1


def _finish(stamp: Path, project: str, preset: str, repo: Path) -> None:
    sent = stamp.with_name(".morf-activity-sent")
    if not stamp.is_file():
        return
    try:
        start = float(stamp.read_text(encoding="utf-8").strip())
    except (OSError, ValueError):
        return
    try:
        stamp.unlink()
    except OSError:
        pass
    # Import relatif au paquet vendore (third_party/morf/morfdeploy/...).
    here = Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(here))
    from morfdeploy.activity import emit_build_activity  # noqa: WPS433

    emit_build_activity(repo, preset, start, time.time(), True)
    try:
        sent.write_text("1", encoding="utf-8")
    except OSError:
        pass


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if argv and argv[0] == "--finish":
        p = argparse.ArgumentParser()
        p.add_argument("--finish", action="store_true")
        p.add_argument("--stamp", required=True)
        p.add_argument("--project", default="")
        p.add_argument("--preset", default="")
        p.add_argument("--repo", default="")
        ns = p.parse_args(argv)
        repo = Path(ns.repo) if ns.repo else Path.cwd()
        _finish(Path(ns.stamp), ns.project, ns.preset, repo)
        return 0
    if len(argv) < 2:
        return 1
    return _launch(Path(argv[0]), argv[1:])


if __name__ == "__main__":
    sys.exit(main())
