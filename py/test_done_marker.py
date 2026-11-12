"""run_session's end marker is a line that begins with it, not a substring -- no GD.

What is pinned:
  * `fxcensus[session_end] ...`, written well before the session's last line, is not the end;
  * `session_end: rendering restored ...` is written before the solver's memory is let go, and is
    not the end either;
  * `session_end: <why>` and `suite: done N levels` are.

run_session hands this function complete lines only (an unfinished last line waits for its
newline), so a half-written `session_end: rendering resto` is never tested.

usage: python py/test_done_marker.py   (exit 1 on the first failure)
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gdtas import results as R  # noqa: E402

TEARDOWN = (
    "deadticks: total=107676 dead=17531 (16.28%) episodes=5\n"
    "fxcensus[session_end] BatchNode=138 CircleWave=1 Layer=11\n"
    "audio: soundWhileSolving=0 blocked=7\n"
    "level record changed: none (restored 3)\n"
    "session_end: rendering restored (the solve had the screen off)\n"
    "dpsolve: memory released at session end - private 3578 -> 1140 MB\n"
)


def check(name, got, want):
    if got != want:
        print(f"FAIL {name}: got {got!r}, want {want!r}")
        sys.exit(1)
    print(f"ok   {name}")


check("fxcensus line and teardown are not the end", R.marker_line_seen(R.SESSION_END, TEARDOWN), False)
check("final line is the end", R.marker_line_seen(R.SESSION_END, TEARDOWN + "session_end: level_complete\n"), True)
check("end among other new lines", R.marker_line_seen(R.SESSION_END, "x: 1\nsession_end: gave_up\ny: 2\n"), True)
check("suite marker", R.marker_line_seen("suite: done", "suite: done 22 levels\n"), True)
check("suite marker not seen in a level's end", R.marker_line_seen("suite: done", "session_end: level_complete\n"), False)
check("empty", R.marker_line_seen(R.SESSION_END, ""), False)
print("all ok")
