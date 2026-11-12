"""The look-only trigger table lives twice: the mod's levelwarn::kLookOnly (src/mod/level_warnings.hpp, the play-menu warning) and dp's actLookOnlyId
(dp/src/dp/triggers.hpp, --actlookchain). They must name the same ids; this reads both sources (GD-free, instant) and fails when they differ.

    python tools/check_look_only_tables.py
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MOD = ROOT / "src" / "mod" / "level_warnings.hpp"
DP = ROOT / "dp" / "src" / "dp" / "triggers.hpp"


def ids(path, pattern):
    text = path.read_text(encoding="utf-8")
    m = re.search(pattern, text, re.S)
    if not m:
        raise SystemExit(f"{path}: table not found")
    body = re.sub(r"//[^\n]*", "", m.group(1))
    return [int(x) for x in re.findall(r"\d+", body)]


mod = ids(MOD, r"kLookOnly\[\]\s*=\s*\{(.*?)\};")
dp = ids(DP, r"kLook\[\]\s*=\s*\{(.*?)\};")
if mod != sorted(mod) or dp != sorted(dp):
    print("a table is not sorted (binary_search needs it sorted)")
    sys.exit(1)
if mod != dp:
    print("look-only tables differ")
    print("  only in the mod (levelwarn::kLookOnly):", sorted(set(mod) - set(dp)))
    print("  only in dp (actLookOnlyId):", sorted(set(dp) - set(mod)))
    sys.exit(1)
print(f"look-only tables agree: {len(mod)} ids")
