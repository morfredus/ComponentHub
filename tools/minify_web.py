#!/usr/bin/env python3
"""
minify_web.py — Minifie les sources web de web_src/ (.html, .css, .js) et
écrit le résultat dans web/. web/ est ensuite embarqué dans le firmware en
PROGMEM par tools/generate_web_assets.py (voir ce script pour la suite du
pipeline) — plus aucun fichier web n'est servi depuis LittleFS. LittleFS ne
contient plus que les données réelles de l'inventaire, jamais touchées par
une mise à jour de l'interface web.

Exécuté automatiquement avant chaque compilation (extra_scripts dans
platformio.ini, via generate_web_assets.py) : il n'y a rien à lancer à la
main en usage courant, ce script reste utilisable isolément pour inspecter
le résultat de la minification sans repasser par une compilation complète.

Usage :
  python tools/minify_web.py

Idempotent : peut être exécuté plusieurs fois sans dégrader le résultat
(à partir des fichiers déjà minifiés, il n'y a simplement plus rien à gagner).

Requirements (optionnels — fallback regex intégré) :
    pip install rcssmin rjsmin
"""

import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = PROJECT_ROOT / "web_src"
WEB_DIR = PROJECT_ROOT / "web"

try:
    import rcssmin
except ImportError:
    rcssmin = None

try:
    import rjsmin
except ImportError:
    rjsmin = None


def minify_css(css: str) -> str:
    if rcssmin is not None:
        return str(rcssmin.cssmin(css))
    css = re.sub(r"/\*.*?\*/", "", css, flags=re.DOTALL)
    css = re.sub(r"\s*([{}:;,>~+])\s*", r"\1", css)
    return re.sub(r"\s+", " ", css).strip()


def minify_js(js: str) -> str:
    if rjsmin is not None:
        return str(rjsmin.jsmin(js))
    js = re.sub(r"//[^\n]*", "", js)
    return re.sub(r"\s+", " ", js).strip()


def minify_html(html: str) -> str:
    html = re.sub(r"<!--(?!\[if).*?-->", "", html, flags=re.DOTALL)
    html = re.sub(r">\s+<", "><", html)
    return html.strip()


def run() -> bool:
    if not SRC_DIR.exists():
        print("[minify_web] web_src/ absent — rien à faire")
        return True

    WEB_DIR.mkdir(parents=True, exist_ok=True)

    total_before = total_after = 0
    expected_names = set()
    for src_path in sorted(SRC_DIR.glob("*")):
        if not src_path.is_file():
            continue
        if src_path.suffix not in (".css", ".js", ".html"):
            continue
        raw = src_path.read_text(encoding="utf-8", errors="ignore")
        if src_path.suffix == ".css":
            minified = minify_css(raw)
        elif src_path.suffix == ".js":
            minified = minify_js(raw)
        else:
            minified = minify_html(raw)

        total_before += len(raw)
        total_after += len(minified)
        expected_names.add(src_path.name)
        (WEB_DIR / src_path.name).write_text(minified, encoding="utf-8")

    # web/ est un dossier entièrement généré : tout fichier qui n'a plus de
    # source correspondante dans web_src/ (page supprimée/renommée) doit
    # disparaître, sinon il resterait embarqué indéfiniment dans le firmware
    # via generate_web_assets.py, sans plus aucun rapport avec web_src/.
    removed = 0
    for stale_path in WEB_DIR.glob("*"):
        if stale_path.is_file() and stale_path.name not in expected_names:
            stale_path.unlink()
            removed += 1

    if total_before:
        ratio = 100 * (1 - total_after / total_before)
        print(f"[minify_web] web_src/ -> web/ : {total_before:,} -> {total_after:,} octets (gain {ratio:.1f}%)")
    if removed:
        print(f"[minify_web] {removed} fichier(s) obsolète(s) supprimé(s) de web/")
    return True


if __name__ == "__main__":
    sys.exit(0 if run() else 1)
