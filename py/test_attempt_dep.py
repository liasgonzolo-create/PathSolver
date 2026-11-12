"""attempt_dep.judge_clear without a game: a stub stands in for seqify.find_prelude.
    python py/test_attempt_dep.py
A clear of an attempt-dependent level counts only if the plan clears from a fresh game; a check that fails is
"unverified", never "cleared"; the pinned package and the run's cfg reach the replay."""
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from gdtas import attempt_dep  # noqa: E402


def make_kit(root: Path, persist: bool = False, attempt_read: bool = False, plan: bool = True) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    (root / "objrects.txt").write_text("id,type,cx,cy\n" + ("3641,7,-15,10\n" if persist else "1,0,0,0\n"))
    (root / "triggers.txt").write_text("uid,id,i1mode,i2mode\n" + ("5,3620,5,0\n" if attempt_read else "5,1007,0,0\n"))
    # dp_plan.txt is the plan from before the clearing call's tail; the solution file is the plan that cleared
    (root / "dp_plan.txt").write_text("input=10,1\ninput=12,0\n")
    if plan:
        (root / "solution_lv9_dp.txt").write_text("input=10,1\ninput=12,0\ninput=40,1\ninput=41,0\n")
    return root


class Calls:
    def __init__(self, result):
        self.result, self.args = result, None

    def __call__(self, planp, cfg_level, worker, **kw):
        self.args = (planp, cfg_level, worker, kw)
        if isinstance(self.result, Exception):
            raise self.result
        return self.result


class JudgeClear(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.text = ("dpsolve:   re-anchored at t=30 (backoff 6): 2 kept + 2 new = 4 inputs\n"
                     "complete: attempt=4 step=0 tick=100\n"
                     "dpsolve: solution saved -> C:/w/session/data/solution_lv9_dp.txt\n")

    def judge(self, kit, outcome="cleared", fp=None, **kw):
        return attempt_dep.judge_clear(kit, "L.lvl", 94, outcome, self.text, find_prelude=fp, **kw)

    def test_not_cleared_is_untouched(self):
        for o in ("stuck", "budget", "first_divergence"):
            fp = Calls(0)
            self.assertEqual(self.judge(make_kit(self.tmp / o, persist=True), o, fp), (o, {}))
            self.assertIsNone(fp.args)

    def test_independent_level_is_untouched(self):
        fp = Calls(0)
        self.assertEqual(self.judge(make_kit(self.tmp / "a"), fp=fp), ("cleared", {}))
        self.assertIsNone(fp.args, "no replay for a level without the warning")

    def test_dependent_clears_with_a_prelude(self):
        fp = Calls(2)
        out, notes = self.judge(make_kit(self.tmp / "b", persist=True), fp=fp)
        self.assertEqual(out, "cleared")
        self.assertEqual((notes["seq_prelude"], notes["cleared_attempt"]), (2, 4))

    def test_dependent_without_a_clearing_prelude_is_mid_session_only(self):
        out, notes = self.judge(make_kit(self.tmp / "c", attempt_read=True), fp=Calls(None))
        self.assertEqual(out, "mid_session_only")
        self.assertIsNone(notes["seq_prelude"])
        self.assertEqual(notes["cleared_attempt"], 4)

    def test_a_failing_check_is_unverified_not_cleared(self):
        out, notes = self.judge(make_kit(self.tmp / "d", persist=True), fp=Calls(RuntimeError("worker died")))
        self.assertEqual(out, "unverified")
        self.assertIn("RuntimeError", notes["seqify_error"])

    def test_a_kit_without_the_solution_file_is_unverified(self):
        out, notes = self.judge(make_kit(self.tmp / "e", persist=True, plan=False), fp=Calls(0))
        self.assertEqual(out, "unverified")
        self.assertIn("solution_lv9_dp.txt", notes["seqify_error"])

    def test_a_run_that_filed_no_solution_is_unverified(self):
        self.text = "complete: attempt=4 step=0 tick=100\n"
        out, notes = self.judge(make_kit(self.tmp / "e2", persist=True), fp=Calls(0))
        self.assertEqual(out, "unverified")
        self.assertIn("filed no solution", notes["seqify_error"])

    def test_the_judged_plan_is_the_solution_not_dp_plan(self):
        fp = Calls(0)
        out, notes = self.judge(make_kit(self.tmp / "e3", persist=True), fp=fp)
        self.assertEqual(fp.args[0].name, "solution_lv9_dp.txt")
        self.assertEqual((notes["judged_plan_inputs"], notes["last_splice_inputs"]), (4, 4))

    def test_the_pinned_package_and_cfg_reach_the_replay(self):
        fp = Calls(0)
        mod = Path("pinned.geode")
        self.judge(make_kit(self.tmp / "f", persist=True), fp=fp, mod_file=mod, extra_cfg=["singlereset=1"])
        planp, cfg_level, worker, kw = fp.args
        self.assertEqual(kw["mod_file"], mod)
        self.assertEqual(kw["extra_cfg"], ["singlereset=1"])
        self.assertEqual(worker, 94)
        self.assertIn("levelfile=L.lvl", cfg_level)

    def test_k0_counts_as_cleared(self):
        out, notes = self.judge(make_kit(self.tmp / "g", persist=True), fp=Calls(0))
        self.assertEqual((out, notes["seq_prelude"]), ("cleared", 0))


class Seqify(unittest.TestCase):
    def test_sequence_text_and_refusal(self):
        import seqify
        from gdtas import plan
        t = seqify.sequence_text([(10, 1), (12, 0)], 2, "x")
        self.assertEqual(t.count("attempt="), 3)
        self.assertEqual(t.count("until=1"), 2)
        p = Path(tempfile.mkdtemp()) / "s.txt"
        p.write_text(t)
        self.assertTrue(plan.is_attempt_sequence(p))
        with self.assertRaises(ValueError):
            plan.read_inputs(p)

    def test_find_prelude_passes_package_and_cfg_to_every_replay(self):
        import seqify
        seen = []

        def fake_replay(worker, cfg_level, plan_cfg, timeout_s, workers_root, mod_file=None, extra_cfg=None, log=None):
            seen.append((mod_file, extra_cfg))
            return (len(seen) == 3), "d", []
        orig = seqify.replay
        seqify.replay = fake_replay
        try:
            d = Path(tempfile.mkdtemp())
            (d / "p.txt").write_text("input=5,1\n")
            k = seqify.find_prelude(d / "p.txt", ["level=1"], 94, max_prelude=3, mod_file=Path("m.geode"),
                                    extra_cfg=["a=1"], log=lambda *_: None)
        finally:
            seqify.replay = orig
        self.assertEqual(k, 2)
        self.assertEqual(seen, [(Path("m.geode"), ["a=1"])] * 3)


# A judge's replay that the run's own cfg (dpsolve=1, capture=1, dpsnapshot=1) turned into a solving session,
# as its result.txt read (a custom level, 10-09): the mod's no-input nodeath recording pass ran to the end trigger
# and its `complete:` was read as the plan's clear, though the plan never flew.
SOLVING_JUDGE = """session_start
dpconfig: dpsolve=1 dpsnapshot=1 dpsecauto=0 package=1851478/42a116f2
dpsolve: start level=9001 csv=4931632 bytes horizon=39964
dpsolve: recording what the level does on its own (no inputs, nodeath) before solving
pois: 156 orbs, 0 coins
endtrigger: uid 261 fired at t=23040
clearcheck: x=37110 levelMaxX=37964 goalX=37964 margin=854 tick=23040 solve=0
gt_last: attempt=3 rows=1587051 depth=23040 end=23040
complete: attempt=3 step=0 tick=23040 x=37110 levelMaxX=37964 goalX=37964 pct=97.750504
dpsolve:   bootstrap record (reached the end): 1587051 samples to t=23040 - adopted
session_end: max_attempts""".splitlines()
# Genuine clears print step=0 too (step= is GD's m_currentStep): official lv16 and lv22, a cold run of 10-09.
LV16_CLEAR = "complete: attempt=6 step=0 tick=20928 x=31365 levelMaxX=31315 goalX=31315 pct=100.000000"
LV22_CLEAR = "complete: attempt=34 step=0 tick=21092 x=21853 levelMaxX=24085 goalX=24085 pct=96.134911"


class ClearVerdict(unittest.TestCase):
    def test_a_nodeath_pass_is_not_a_clear(self):
        from gdtas import results
        clear, detail = results.clear_verdict(SOLVING_JUDGE)
        self.assertFalse(clear)
        self.assertIn("NODEATH PASS", detail)

    def test_the_plan_dying_at_tick_1_after_a_nodeath_pass_is_not_a_clear(self):
        from gdtas import results
        lines = SOLVING_JUDGE[:-1] + ["death: attempt=4 tick=1 x=0 wallMs=3 speedX=1", "session_end: max_attempts"]
        clear, detail = results.clear_verdict(lines)
        self.assertFalse(clear)

    def test_a_clear_after_a_nodeath_pass_is_still_a_clear(self):
        from gdtas import results
        lines = SOLVING_JUDGE[:-1] + [LV16_CLEAR]
        self.assertEqual(results.clear_verdict(lines), (True, LV16_CLEAR))

    def test_genuine_clears_with_step_0_are_clears(self):
        from gdtas import results
        for line in (LV16_CLEAR, LV22_CLEAR):
            self.assertEqual(results.clear_verdict(["session_start", line]), (True, line))


class ReplayCfg(unittest.TestCase):
    # the run's cfg a collect passes (10-09), less the level and plan lines
    RUN = ["enabled=1", "quitwhendone=1", "blockinput=1", "cbs=0", "cos=1", "fastdt=0.0166667", "fastloops=1800",
           "skiprender=1", "music=mute", "servemode=0", "dpsolve=1", "capture=1", "dpsnapshot=1", "dpsecauto=0",
           "dparg=--heldall", "attempts=1000000", "singlereset=1", "nodeath=1"]

    def test_solver_keys_are_dropped_and_the_replay_keys_win(self):
        import seqify
        cfg, dropped = seqify.replay_cfg(["level=9001"], ["input=10,1"], self.RUN)
        self.assertEqual(dropped, ["servemode=0", "dpsolve=1", "capture=1", "dpsnapshot=1", "dpsecauto=0",
                                   "dparg=--heldall", "attempts=1000000", "nodeath=1"])
        self.assertIn("singlereset=1", cfg)
        self.assertFalse(any(c.startswith(("dp", "capture", "servemode", "nodeath")) for c in cfg))
        self.assertEqual([c for c in cfg if c.startswith("attempts=")], ["attempts=1"])
        self.assertEqual(cfg[-2:], ["level=9001", "input=10,1"])

    def test_a_replay_that_solves_is_refused(self):
        import seqify

        class R:
            lines = SOLVING_JUDGE
        orig = seqify.run_session
        seqify.run_session = lambda *a, **kw: R()
        try:
            said = []
            clear, detail, _ = seqify.replay(94, ["level=9001"], ["input=10,1"], 60.0, Path("."),
                                             extra_cfg=self.RUN, log=said.append)
        finally:
            seqify.run_session = orig
        self.assertFalse(clear)
        self.assertIn("REFUSED", detail)
        self.assertTrue(said and "dpsolve=1" in said[0])


if __name__ == "__main__":
    unittest.main()
