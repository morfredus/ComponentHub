#!/usr/bin/env python3
"""Maintain the ComponentHub translation catalogs (translations/*.ts).

Why not lupdate: every string goes through chui::tr("...") (or CHUI_NOOP("...")
for static tables) with a single context, "chui". lupdate does not follow these
helpers without extra setup, so this script reads the string literals directly.
It only needs Python 3.

Usage, from the repository root:
    python scripts/i18n/update-catalogs.py              update every .ts file and the template
    python scripts/i18n/update-catalogs.py --check      list what is still untranslated (exit 1 if any)
    python scripts/i18n/update-catalogs.py --new de_DE  create translations/componenthub_de.ts

The source text of every string is ENGLISH. Existing translations are kept,
new strings arrive empty ("unfinished"), strings that left the code are
dropped. No line numbers are written, so git diffs stay readable.

A string may carry a second literal argument, tr("Import", "title"): it is the
disambiguation comment Qt uses to tell two meanings of the same English word
apart (noun or verb, window title or button). It is shown to translators.
"""
import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
TR_DIR = ROOT / "translations"
CONTEXT = "chui"
# The vendored morfUpdate library has its own context: its texts are English too and go
# through mt("...") (QCoreApplication::translate("morfupdate", ...)). They live in the
# same catalogs, in a second <context>, so a language needs no extra file.
LIB_SRC = ROOT / "third_party" / "morf" / "update" / "src"
LIB_CONTEXT = "morfupdate"

CALL_RE = re.compile(r'(?<![A-Za-z0-9_])(?:chui::)?(?:tr|CHUI_NOOP)\(\s*(?=")')
LIB_CALL_RE = re.compile(r'(?<![A-Za-z0-9_])mt\(\s*(?=")')
LITERAL_RE = re.compile(r'\s*"((?:[^"\\\n]|\\.)*)"')
COMMENT_RE = re.compile(r'\s*,\s*"((?:[^"\\\n]|\\.)*)"')
ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", '"': '"', "'": "'", "0": "\0"}


def decode_literal(body):
    """Decode the inside of a C++ string literal (without the quotes)."""
    out, i = [], 0
    while i < len(body):
        c = body[i]
        if c == "\\":
            i += 1
            if body[i] not in ESCAPES:
                raise ValueError("unsupported escape: \\" + body[i])
            out.append(ESCAPES[body[i]])
        else:
            out.append(c)
        i += 1
    return "".join(out)


def encode_literal(text):
    """Write a text as a C++ string literal, quotes included."""
    return '"' + (text.replace("\\", "\\\\").replace('"', '\\"')
                  .replace("\n", "\\n").replace("\t", "\\t")) + '"'


def iter_calls(code, call_re=CALL_RE):
    """Yield (start, text_end, text, comment) for each tr("...") / CHUI_NOOP("...").

    Adjacent literals ("a" "b") are joined, as the compiler does.
    """
    for m in call_re.finditer(code):
        pos, parts = m.end(), []
        while True:
            lit = LITERAL_RE.match(code, pos)
            if not lit:
                break
            parts.append(decode_literal(lit.group(1)))
            pos = lit.end()
        c = COMMENT_RE.match(code, pos)
        yield m.start(), pos, "".join(parts), decode_literal(c.group(1)) if c else ""


def scan_sources():
    """Ordered list, without duplicates, of (text, comment) found in the code."""
    seen, order = set(), []
    for path in sorted(SRC.rglob("*")):
        if path.suffix not in (".cpp", ".h") or "third_party" in path.parts:
            continue
        for _, _, text, comment in iter_calls(path.read_text(encoding="utf-8")):
            if (text, comment) not in seen:
                seen.add((text, comment))
                order.append((text, comment))
    return order


def scan_lib_sources():
    """Same as scan_sources() for the morfUpdate library (context "morfupdate")."""
    seen, order = set(), []
    for path in sorted(LIB_SRC.rglob("*.cpp")):
        for _, _, text, comment in iter_calls(path.read_text(encoding="utf-8"), LIB_CALL_RE):
            if (text, comment) not in seen:
                seen.add((text, comment))
                order.append((text, comment))
    return order


def _read(path, context):
    root = ET.parse(path).getroot()
    tr = {}
    for ctx in root.iter("context"):
        if ctx.findtext("name") != context:
            continue
        for m in ctx.iter("message"):
            key = (m.findtext("source") or "", m.findtext("comment") or "")
            t = m.find("translation")
            if t is not None and t.get("type") != "unfinished" and (t.text or "").strip():
                tr[key] = t.text
    return root.get("language", ""), tr


def read_catalog(path):
    """Return (language, {(source, comment): translation}) for finished "chui" entries."""
    return _read(path, CONTEXT)


def read_lib_catalog(path):
    """Same for the "morfupdate" context."""
    return _read(path, LIB_CONTEXT)[1]


def write_catalog(path, language, keys, translations, lib_keys=(), lib_translations=None):
    lines = ['<?xml version="1.0" encoding="utf-8"?>', "<!DOCTYPE TS>",
             f'<TS version="2.1" language="{language}">']
    _write_context(lines, CONTEXT, keys, translations)
    _write_context(lines, LIB_CONTEXT, lib_keys, lib_translations or {})
    lines += ["</TS>", ""]
    path.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def _write_context(lines, context, keys, translations):
    lines += ["<context>", f"    <name>{context}</name>"]
    for key in keys:
        src, comment = key
        t = translations.get(key, "")
        lines += ["    <message>", f"        <source>{escape(src)}</source>"]
        if comment:
            lines.append(f"        <comment>{escape(comment)}</comment>")
        if t:
            lines.append(f"        <translation>{escape(t)}</translation>")
        else:
            lines.append('        <translation type="unfinished"></translation>')
        lines.append("    </message>")
    lines.append("</context>")


def catalog_files():
    return sorted(p for p in TR_DIR.glob("componenthub_*.ts") if p.stem != "componenthub_template")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="write nothing, list what is missing")
    ap.add_argument("--new", metavar="LANG", help="create a catalog (e.g. de_DE, zh_CN)")
    args = ap.parse_args()

    keys = scan_sources()
    known = set(keys)
    lib_keys = scan_lib_sources()

    if args.new:
        code = args.new.split("_")[0]
        target = TR_DIR / f"componenthub_{code}.ts"
        if target.exists():
            sys.exit(f"{target.name} already exists")
        write_catalog(target, args.new, keys, {}, lib_keys)
        print(f"created: {target.relative_to(ROOT)} ({len(keys) + len(lib_keys)} strings to translate)")
        return

    incomplete = False
    for path in catalog_files():
        language, tr = read_catalog(path)
        lib_tr = read_lib_catalog(path)
        missing = [k for k in keys if k not in tr] + [k for k in lib_keys if k not in lib_tr]
        obsolete = [k for k in tr if k not in known]
        total = len(keys) + len(lib_keys)
        print(f"{path.name}: {total - len(missing)}/{total} translated, "
              f"{len(obsolete)} obsolete")
        if args.check:
            for text, comment in missing:
                print("   to translate:", text.replace("\n", "\\n")[:100],
                      f"[{comment}]" if comment else "")
            incomplete = incomplete or bool(missing)
        else:
            write_catalog(path, language, keys, tr, lib_keys, lib_tr)
    if not args.check:
        write_catalog(TR_DIR / "componenthub_template.ts", "", keys, {}, lib_keys)
        print("template: translations/componenthub_template.ts")
    sys.exit(1 if incomplete else 0)


if __name__ == "__main__":
    main()
