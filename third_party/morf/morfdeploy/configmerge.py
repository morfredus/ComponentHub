"""Enrich an installed configuration with keys a new version introduced.

An update must guarantee three things: install the new binary, preserve the
user's settings, and make the new version's options available. The first two
were already covered; this is the third. A key added to the reference
configuration by a new release did not reach users who already had a
personalised file.

The rule is strict and one-directional: **enriching, never destructive.**

  - a key present in the reference but missing from the installed file is added
    with its default value;
  - a value the user already set is kept, untouched, always;
  - a key the installed file has and the reference no longer does is REPORTED
    as possibly obsolete, and left in place -- removing it might discard
    something the user added on purpose, and this step never decides that.

Lists are values, not containers to merge. A key whose value is a list is added
whole when missing and kept whole when present -- the parc's standing rule that
an update adds keys, never list entries (a supervised service, a probe), so
that monitoring is never switched on that nobody asked for.

This logic lives here, in morfdeploy, because making an existing installation
evolve is a lifecycle responsibility -- like install, update, uninstall. A
project declares its reference configuration; morfdeploy owns how an installed
one catches up. It replaces a merge-config.py that used to sit, duplicated, in
every service.
"""

from __future__ import annotations

import json
import os
import shutil
from datetime import datetime
from pathlib import Path


def _load(path: Path) -> dict:
    # utf-8-sig: a file edited on Windows may carry a BOM.
    return json.loads(path.read_text(encoding="utf-8-sig"))


def _merge(reference: dict, current: dict, prefix: str,
           added: list, obsolete: list, deep_lists: bool = False) -> None:
    """Recursively add missing keys into `current`; record what changed.

    `current` is mutated in place. Existing values are never touched. Nested
    objects recurse; every other type is atomic.

    Lists are atomic by default (`deep_lists=False`) -- the standing rule of the
    `update` action, which adds keys but never list entries. When `deep_lists`
    is set (the explicit `config` action), a list of objects is matched item by
    item on its `id` and the SAME rule is applied inside each matching object:
    missing keys are added, existing values are kept, and NO new entry is ever
    appended. This is what lets a new module parameter (say `morfsync_url`)
    reach a module the user already has, without ever switching on a module,
    probe or service nobody asked for.
    """
    for key, ref_value in reference.items():
        path = prefix + key
        if key not in current:
            current[key] = ref_value
            added.append(path)
        elif isinstance(ref_value, dict) and isinstance(current[key], dict):
            _merge(ref_value, current[key], path + ".", added, obsolete, deep_lists)
        elif deep_lists and isinstance(ref_value, list) and isinstance(current[key], list):
            _merge_list(ref_value, current[key], path, added, obsolete)
        # else: the key exists and is not a sub-object (nor a list we deep-merge)
        # -- the user's value stands, whatever it is.

    for key in current:
        if key not in reference:
            # Reported at this level only; nested obsoletes surface through the
            # recursion above when both sides are objects.
            obsolete.append(prefix + key)


def _merge_list(ref_list: list, cur_list: list, path: str,
                added: list, obsolete: list) -> None:
    """Deep-merge two lists of objects by matching their `id`.

    Only objects the installed list ALREADY holds are enriched (matched on
    `id`); a reference entry the installed list lacks is deliberately NOT added,
    so this never enables something the user did not choose. Items without an
    `id`, or non-object items, are left exactly as they are -- there is no
    reliable way to pair them, and guessing would be worse than doing nothing.
    """
    current_by_id = {
        item["id"]: item
        for item in cur_list
        if isinstance(item, dict) and "id" in item
    }
    for ref_item in ref_list:
        if not isinstance(ref_item, dict):
            continue
        item_id = ref_item.get("id")
        if item_id is None:
            continue
        target = current_by_id.get(item_id)
        if target is not None:
            _merge(ref_item, target, f"{path}[{item_id}].", added, obsolete,
                   deep_lists=True)


def merge_config(reference: Path, installed: Path, backup: bool = True,
                 deep_lists: bool = False) -> tuple:
    """Add to `installed` the keys `reference` has and it lacks.

    Returns (added, obsolete): dotted key paths added, and dotted key paths the
    installed file carries that the reference no longer mentions. The file is
    rewritten only when something was added, and a timestamped backup is made
    first -- non-destructive by construction, but a config is worth the belt.

    `deep_lists` extends the merge into lists of objects, matched by `id` (see
    `_merge`). The `update` action leaves it off (lists atomic, the parc rule);
    the explicit `config` action turns it on to propagate new keys INSIDE a
    module the user already has -- still never adding a list entry.
    """
    ref = _load(reference)
    cur = _load(installed)
    if not isinstance(ref, dict) or not isinstance(cur, dict):
        # A configuration that is not a JSON object is outside what this merge
        # understands; leave it exactly as it is rather than guess.
        return [], []

    added: list = []
    obsolete: list = []
    _merge(ref, cur, "", added, obsolete, deep_lists)

    if added:
        if backup:
            stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
            shutil.copy2(installed, installed.with_name(f"{installed.name}.bak-{stamp}"))
        # Atomic write: a full temp file replaced in one move, so an interrupted
        # write never leaves a truncated configuration the service cannot read.
        tmp = installed.with_name(f".{installed.name}.tmp")
        tmp.write_text(json.dumps(cur, ensure_ascii=False, indent=2) + "\n",
                       encoding="utf-8")
        os.replace(tmp, installed)

    return added, obsolete
