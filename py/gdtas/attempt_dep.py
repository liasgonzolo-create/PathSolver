"""Does a level depend on which attempt it is, and on which attempt did a run clear it.

A level can start differently on its first attempt: an Item Persistence trigger (id 3641) keeps items across
attempts, and an Item Edit / Item Compare (3619 / 3620) can read the layer's attempt count (item type 5, the
trigger table's i1mode / i2mode). The mod prints the same warning in the play menu (level_warnings.hpp,
kAttemptDependent); the drivers read it off the dumps the mod leaves beside a run (objrects.txt, triggers.txt).
A clear of such a level counts only if the plan clears from a fresh game (py/seqify.py).
"""
from __future__ import annotations

import csv
import re
from pathlib import Path

PERSIST_ID = 3641
ITEM_TRIGGER_IDS = {"3619", "3620"}
ATTEMPT_COUNT_MODE = "5"


def attempt_dependent(kit: Path) -> tuple[bool, str]:
    """(dependent, why) from <kit>/objrects.txt and <kit>/triggers.txt. A kit without them reads as (False, '')."""
    why: list[str] = []
    objr = Path(kit) / "objrects.txt"
    if objr.exists():
        with open(objr, encoding="utf-8", errors="replace", newline="") as f:
            n = sum(1 for row in csv.DictReader(f) if row.get("id") == str(PERSIST_ID))
        if n:
            why.append(f"{n}x Item Persistence (3641)")
    trig = Path(kit) / "triggers.txt"
    if trig.exists():
        with open(trig, encoding="utf-8", errors="replace", newline="") as f:
            n = sum(1 for row in csv.DictReader(f)
                    if row.get("id") in ITEM_TRIGGER_IDS
                    and ATTEMPT_COUNT_MODE in (row.get("i1mode"), row.get("i2mode")))
        if n:
            why.append(f"{n}x item trigger reading the attempt count")
    return bool(why), "; ".join(why)


def cleared_attempt(result_text: str) -> int | None:
    """The layer's attempt number on which the run's last `complete:` line was written."""
    last = None
    for m in re.finditer(r"^complete: attempt=(\d+)", result_text, re.M):
        last = int(m.group(1))
    return last


def cleared_plan(kit: Path, result_text: str) -> tuple[Path | None, dict]:
    """The plan the run filed as its solution, as the kit holds it, or None; and notes for meta.json.

    The session names it on its `dpsolve: solution saved -> <file>` line, and the driver copies that file into
    the kit. It is NOT dp_plan.txt: that is the plan from before the clearing call's tail was spliced on, so a
    replay of it dies where the session re-anchored (a custom level, 10-09: 284 inputs in dp_plan.txt, 310 in
    the solution). The input counts of the kit's copy and of the last splice the log reports (`... = N
    inputs`) are noted side by side."""
    saves = re.findall(r"^dpsolve: solution saved -> (\S+)", result_text, re.M)
    if not saves:
        return None, {"seqify_error": "the run filed no solution (no `dpsolve: solution saved` line)"}
    name = Path(saves[-1].replace("\\", "/")).name
    p = Path(kit) / name
    if not p.exists():
        return None, {"seqify_error": f"the kit has no {name}, the plan the run filed as its solution"}
    head = result_text[:result_text.rfind("dpsolve: solution saved")]
    spliced = re.findall(r"= (\d+) inputs$", head, re.M)
    have = sum(1 for line in p.read_text(encoding="utf-8", errors="replace").splitlines()
               if line.startswith("input="))
    return p, {"judged_plan": name, "judged_plan_inputs": have,
               "last_splice_inputs": int(spliced[-1]) if spliced else None}


def judge_clear(kit: Path, level_file: Path | str, worker: int, outcome: str, result_text: str,
                find_prelude=None, mod_file: Path | None = None, extra_cfg: list[str] | None = None,
                max_prelude: int = 3, timeout_s: float = 480.0) -> tuple[str, dict]:
    """What a driver should file for a run that says `outcome`, given that a clear of an attempt-dependent level
    counts only from a fresh game. Returns (outcome, notes for meta.json).

    - not "cleared", or the level is not attempt dependent: unchanged.
    - dependent and some prelude of 0..max empty attempts clears the filed plan (cleared_plan: the solution file
      the run saved, never dp_plan.txt) from a fresh game, replayed on the SAME package (mod_file) and cfg
      (extra_cfg) as the solve: "cleared", with `seq_prelude` = that k.
    - dependent and none clears: "mid_session_only" (not a clear), with the attempt it cleared on.
    - the check itself failed (no solution file in the kit, an exception): "unverified" -- never "cleared"; the
      error is in the notes.
    `find_prelude` is seqify.find_prelude by default and can be replaced (a stub in the test, no game).
    """
    if outcome != "cleared":
        return outcome, {}
    dep, why = attempt_dependent(kit)
    if not dep:
        return outcome, {}
    notes: dict = {"attempt_dependent": why, "cleared_attempt": cleared_attempt(result_text)}
    try:
        planp, plan_notes = cleared_plan(kit, result_text)
        notes.update(plan_notes)
        if planp is None:
            return "unverified", notes
        if find_prelude is None:
            import seqify  # py/seqify.py
            find_prelude = seqify.find_prelude
        k = find_prelude(planp, ["level=9001", f"levelfile={level_file}", "clearance=1"], worker,
                         max_prelude=max_prelude, timeout_s=timeout_s, mod_file=mod_file, extra_cfg=extra_cfg)
    except Exception as e:  # noqa: BLE001 -- any failure of the check is "unverified", never a clear
        notes["seqify_error"] = f"{type(e).__name__}: {e}"
        return "unverified", notes
    notes["seq_prelude"] = k
    return ("cleared" if k is not None else "mid_session_only"), notes
