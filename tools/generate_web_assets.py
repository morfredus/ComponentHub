#!/usr/bin/env python3
"""
generate_web_assets.py — Génère src/generated/web_assets_data.cpp (tableaux
d'octets PROGMEM + table de routage chemin -> asset) à partir de web/
(minifié depuis web_src/ par minify_web.py, appelé automatiquement en
première étape de ce script).

Fichier généré, jamais édité à la main — voir
src/services/web_assets/web_assets.h pour l'API exposée aux modules
(WebAssets::find/send), et docs/ARCHITECTURE.md pour le principe général
(l'interface web est embarquée dans le firmware, LittleFS ne sert plus qu'à
l'inventaire).

Exécuté automatiquement avant chaque compilation (extra_scripts dans
platformio.ini) : aucune étape manuelle n'est nécessaire en usage courant.

Usage :
  python tools/generate_web_assets.py
"""

import sys
from pathlib import Path

env = None
if "Import" in globals():
    # "Import" est injecté par PlatformIO/SCons dans le contexte exec() ;
    # absent en exécution standalone (python tools/generate_web_assets.py).
    globals()["Import"]("env")

if env is not None:
    PROJECT_ROOT = Path(env.get("PROJECT_DIR"))
    TOOLS_DIR = PROJECT_ROOT / "tools"
else:
    PROJECT_ROOT = Path(__file__).resolve().parent.parent
    TOOLS_DIR = Path(__file__).resolve().parent

sys.path.insert(0, str(TOOLS_DIR))
import minify_web  # noqa: E402

WEB_DIR = PROJECT_ROOT / "web"
OUTPUT_DIR = PROJECT_ROOT / "src" / "generated"
OUTPUT_FILE = OUTPUT_DIR / "web_assets_data.cpp"

CONTENT_TYPES = {
    ".html": "text/html",
    ".css": "text/css",
    ".js": "application/javascript",
    ".json": "application/json",
    ".svg": "image/svg+xml",
    ".png": "image/png",
}


def content_type_for(path: Path) -> str:
    return CONTENT_TYPES.get(path.suffix, "text/plain")


def emit_byte_array(name: str, data: bytes) -> str:
    lines = [f"static const uint8_t {name}[] PROGMEM = {{"]
    hex_bytes = [f"0x{b:02x}" for b in data]
    for i in range(0, len(hex_bytes), 20):
        lines.append("    " + ",".join(hex_bytes[i:i + 20]) + ",")
    lines.append("};")
    return "\n".join(lines)


def run() -> bool:
    if not minify_web.run():
        return False

    if not WEB_DIR.exists():
        print("[generate_web_assets] web/ absent — rien à embarquer")
        return True

    files = sorted(p for p in WEB_DIR.glob("*") if p.is_file())
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    arrays = []
    entries = []
    total_bytes = 0
    for i, path in enumerate(files):
        data = path.read_bytes()
        total_bytes += len(data)
        var_name = f"asset_{i}_data"
        arrays.append(f"// {path.name}\n" + emit_byte_array(var_name, data))
        entries.append(
            f'    {{ "/{path.name}", {var_name}, sizeof({var_name}), "{content_type_for(path)}" }},'
        )

    content = (
        "// Fichier généré par tools/generate_web_assets.py — NE PAS ÉDITER À LA MAIN.\n"
        "// Régénéré à chaque compilation à partir de web/ (voir tools/minify_web.py).\n"
        '#include "../services/web_assets/web_assets.h"\n\n'
        + "\n\n".join(arrays)
        + "\n\nconst WebAsset kWebAssets[] = {\n"
        + "\n".join(entries)
        + "\n};\n\n"
        "const size_t kWebAssetCount = sizeof(kWebAssets) / sizeof(kWebAssets[0]);\n"
    )
    OUTPUT_FILE.write_text(content, encoding="utf-8")

    print(f"[generate_web_assets] web/ -> {OUTPUT_FILE.relative_to(PROJECT_ROOT)} : "
          f"{len(files)} fichier(s), {total_bytes:,} octets embarqués")
    return True


if __name__ == "__main__" or env is not None:
    ok = run()
    if __name__ == "__main__":
        sys.exit(0 if ok else 1)
