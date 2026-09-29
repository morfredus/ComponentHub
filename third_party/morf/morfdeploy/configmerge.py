"""Bring an installed configuration up to the version being deployed.

An update must guarantee three things: install the new binary, preserve the
user's settings, and make the configuration match the new version. The first two
were always covered. The third was only half done: new keys were added at the
top level, but never INSIDE a module the user already had, and a key a version
removed stayed in the file forever. A service could run for weeks with a stale
setting nobody knew was dead (morfAnalytics' `altitude_m`, 2026-09-29).

The rules, applied on install, update and the explicit `config` action alike:

  - a key present in the reference but missing from the installed file is added
    with its default value -- at any depth, including inside the objects of a
    list matched by their `id` (a module the user already has). A reference
    entry the installed list does not hold is NEVER added: an update adds
    options, it never switches on a module, probe or service nobody asked for;
  - a value the user set is kept, untouched, always;
  - documentation comments (`_comment*` keys) belong to the reference, not to
    the user: they are refreshed from it, and dropped when it no longer has
    them. The file then documents the version that is actually running;
  - a key a version REMOVED is deleted only when the project declares it
    (`removed_keys` of the config entry in service.json). Deleting a setting is
    a decision; the project that retired it is the one entitled to make it;
  - any other key the reference does not know is REPORTED and left in place:
    it may be something the user added on purpose, and this step never guesses.

This logic lives here, in morfdeploy, because making an existing installation
evolve is a lifecycle responsibility -- like install, update, uninstall. A
project declares its reference configuration (and what it retired); morfdeploy
owns how an installed one catches up.
"""

from __future__ import annotations

import json
import os
import shutil
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path


def _is_comment(key: str) -> bool:
    return key.startswith("_comment")


@dataclass
class MergeReport:
    """What a merge did, as dotted key paths (lists shown as `list[id]`)."""

    added: list = field(default_factory=list)     # new options, default applied
    removed: list = field(default_factory=list)   # declared retired, deleted
    obsolete: list = field(default_factory=list)  # unknown to the reference, kept
    comments: int = 0                             # comments added / refreshed / dropped

    @property
    def changed(self) -> bool:
        return bool(self.added or self.removed or self.comments)


def _load(path: Path) -> dict:
    # utf-8-sig: a file edited on Windows may carry a BOM.
    return json.loads(path.read_text(encoding="utf-8-sig"))


def _remove(current, segments: list, prefix: str, report: MergeReport) -> None:
    """Delete the declared path `segments` from `current`.

    A segment ending in `[]` walks every object of that list: `modules[].x`
    removes `x` from each module, whatever its id (ids are the user's).
    """
    if not isinstance(current, dict) or not segments:
        return
    head, rest = segments[0], segments[1:]
    if head.endswith("[]"):
        items = current.get(head[:-2])
        if isinstance(items, list) and rest:
            for item in items:
                if isinstance(item, dict):
                    label = item.get("id", "?")
                    _remove(item, rest, f"{prefix}{head[:-2]}[{label}].", report)
        return
    if not rest:
        if head in current:
            del current[head]
            report.removed.append(prefix + head)
        return
    _remove(current.get(head), rest, prefix + head + ".", report)


def _merge(reference: dict, current: dict, prefix: str, report: MergeReport) -> None:
    """Recursively bring `current` up to `reference` (mutated in place)."""
    for key, ref_value in reference.items():
        path = prefix + key
        if _is_comment(key):
            if current.get(key) != ref_value:
                current[key] = ref_value
                report.comments += 1
        elif key not in current:
            current[key] = ref_value
            report.added.append(path)
        elif isinstance(ref_value, dict) and isinstance(current[key], dict):
            _merge(ref_value, current[key], path + ".", report)
        elif isinstance(ref_value, list) and isinstance(current[key], list):
            _merge_list(ref_value, current[key], path, report)
        # else: the key exists and is a plain value -- the user's value stands.

    for key in list(current):
        if key in reference:
            continue
        if _is_comment(key):
            # The reference no longer documents this: the comment would describe
            # an option that moved or disappeared.
            del current[key]
            report.comments += 1
        else:
            report.obsolete.append(prefix + key)


def _merge_list(ref_list: list, cur_list: list, path: str, report: MergeReport) -> None:
    """Merge two lists of objects by matching their `id`.

    Only objects the installed list ALREADY holds are brought up to date; a
    reference entry the installed list lacks is deliberately NOT added, so this
    never enables something the user did not choose. Items without an `id`, or
    non-object items, are left exactly as they are -- there is no reliable way
    to pair them, and guessing would be worse than doing nothing.
    """
    current_by_id = {
        item["id"]: item
        for item in cur_list
        if isinstance(item, dict) and "id" in item
    }
    for ref_item in ref_list:
        if not isinstance(ref_item, dict) or ref_item.get("id") is None:
            continue
        target = current_by_id.get(ref_item["id"])
        if target is not None:
            _merge(ref_item, target, f"{path}[{ref_item['id']}].", report)


def merge_config(reference: Path, installed: Path, removed_keys=(),
                 backup: bool = True) -> MergeReport:
    """Bring `installed` up to `reference`; see the module docstring.

    `removed_keys`: dotted paths this version retired (`a.b`, `modules[].x`).
    The file is rewritten only when something changed, after a timestamped
    backup, and atomically (temp file + one move), so an interrupted write
    never leaves a truncated configuration the service cannot read.
    """
    report = MergeReport()
    ref = _load(reference)
    cur = _load(installed)
    if not isinstance(ref, dict) or not isinstance(cur, dict):
        # A configuration that is not a JSON object is outside what this merge
        # understands; leave it exactly as it is rather than guess.
        return report

    # Removals first: a retired key must not come back as "obsolete".
    for declared in removed_keys:
        _remove(cur, str(declared).split("."), "", report)
    _merge(ref, cur, "", report)

    if report.changed:
        if backup:
            stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
            shutil.copy2(installed, installed.with_name(f"{installed.name}.bak-{stamp}"))
        tmp = installed.with_name(f".{installed.name}.tmp")
        tmp.write_text(json.dumps(cur, ensure_ascii=False, indent=2) + "\n",
                       encoding="utf-8")
        os.replace(tmp, installed)

    return report
