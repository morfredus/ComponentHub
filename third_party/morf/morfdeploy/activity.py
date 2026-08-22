"""Emission (optionnelle) d'une activite de build vers morfAnalytics Monitor.

morfDeploy sait ce qu'il compile (projet, preset, debut/fin, resultat) : c'est la
source naturelle et exacte des evenements de compilation, sans rien faire deviner a
personne (cf. domaine Monitor de morfAnalytics). Il les signale ici.

Principe : best-effort et sans dependance. Si l'URL d'ingestion n'est pas
configuree, rien n'est emis ; si morfAnalytics est injoignable, on n'en fait pas une
erreur -- une telemetrie muette ne doit jamais faire echouer un build.
"""
from __future__ import annotations

import json
import os
import socket
import time
import urllib.request
from pathlib import Path

# Ou trouver l'URL d'ingestion des activites du domaine Monitor de morfAnalytics.
# Deux sources, dans l'ordre :
#   1. la variable d'environnement MORFANALYTICS_ACTIVITY_URL (ex.
#      "http://pi4fred:8799/api/monitor/activity") -- pratique pour `morf build`,
#      qui tourne sous le compte utilisateur ;
#   2. a defaut, un fichier /etc/morfsystem/monitor-activity-url (une ligne).
#      Necessaire car `sudo service.py update` efface l'environnement : un fichier
#      admin, lu quel que soit l'appelant, est le moyen robuste et pose une fois.
# Aucune des deux => aucune emission. morfDeploy ne depend jamais de morfAnalytics.
_ENV_URL = "MORFANALYTICS_ACTIVITY_URL"
_FILE_URL = Path("/etc/morfsystem/monitor-activity-url")

# Tampon ecrit par le lanceur de compilateur (premier g++), lu a la fin du build.
_STAMP_NAME = ".morf-compile-t0"
_SENT_NAME = ".morf-activity-sent"
_BUILD_DIRS = ("build-arm64", "build-mingw", "build")


def _target_url() -> str | None:
    env = os.environ.get(_ENV_URL)
    if env and env.strip():
        return env.strip()
    try:
        if _FILE_URL.is_file():
            text = _FILE_URL.read_text(encoding="utf-8").strip()
            return text or None
    except OSError:
        pass
    return None


def _project_name(repo_root: Path) -> str:
    name = Path(repo_root).name
    # Le nom de projet ne porte jamais le suffixe du bac a sable de developpement.
    if name.endswith("_travail"):
        name = name[: -len("_travail")]
    return name


def _find_in_build(repo_root: Path, filename: str) -> Path | None:
    root = Path(repo_root)
    for d in _BUILD_DIRS:
        p = root / d / filename
        if p.is_file():
            return p
    return None


def emit_build_activity(repo_root: Path, preset: str, start: float, end: float,
                        ok: bool, duration_s: float | None = None) -> None:
    """Signale une compilation a morfAnalytics, si configure. Ne leve jamais."""
    url = _target_url()
    if not url:
        return
    # Duree = difference des instants reels, pas int(start) et int(end) separes
    # (un build de 1,4 s devenait 0 ou 1 s selon la seconde Unix).
    wall = float(end) - float(start)
    dur = float(duration_s) if duration_s is not None else wall
    if dur < 0:
        dur = 0.0
    payload = {
        "type": "compile",
        "project": _project_name(repo_root),
        "machine": socket.gethostname(),
        "start": float(start),
        "end": float(end),
        "duration_s": round(dur),
        "status": "success" if ok else "failed",
        "metadata": {"preset": preset},
    }
    try:
        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(
            url, data=data, headers={"Content-Type": "application/json"})
        urllib.request.urlopen(req, timeout=3).read()
    except Exception:  # noqa: BLE001 - injoignable, timeout, DNS... jamais bloquant
        pass


def complete_build_activity(repo_root: Path, preset: str, start_wall: float,
                            ok: bool) -> None:
    """Apres cmake --build : preferer l'horloge du premier compilateur.

    Un `cmake --build` sans fichier a recompiler rend la main en ~1 s. Ce n'est
    pas une duree de compile. Si un tampon a ete pose par le lanceur (premier
    g++), on mesure de ce moment jusqu'ici. Si POST_BUILD a deja emis, on se
    tait. Sinon, on n'emet le chrono du processus que s'il a dure au moins 2 s
    (configure + ninja reel, pas un no-op).
    """
    end = time.time()
    sent = _find_in_build(repo_root, _SENT_NAME)
    if sent is not None:
        try:
            sent.unlink()
        except OSError:
            pass
        return
    stamp = _find_in_build(repo_root, _STAMP_NAME)
    if stamp is not None:
        try:
            start = float(stamp.read_text(encoding="utf-8").strip())
        except (OSError, ValueError):
            start = start_wall
        try:
            stamp.unlink()
        except OSError:
            pass
        emit_build_activity(repo_root, preset, start, end, ok)
        return
    if (end - start_wall) >= 2.0:
        emit_build_activity(repo_root, preset, start_wall, end, ok)
