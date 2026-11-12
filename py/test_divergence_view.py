# -*- coding: utf-8 -*-
"""py/divergence_view.py on hand-built files. (no GD, no solve; seconds)

    python py/test_divergence_view.py
    python py/test_divergence_view.py -v

The viewer is a picture, so what is pinned here is the part a picture cannot be trusted for: which
GD attempt is shown, where the two worldlines are said to part, that a model trace which stops
early says so, that a tick one side lacks is never intersected away, that a recorded mover holds its
last row, and that the page the generator writes is one the browser can run (its script parses and
its data block is valid JSON).
"""

from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import divergence_view as dv  # noqa: E402

GD_HEAD = "frame,attempt,tick,x,y,yvel,rot,mode,upsideDown,onGround,dead,vsize,snapuid,dual,p2y,speed"
MODEL_HEAD = "tick,x,y,vy,mode,grounded,dual,y2,flip,mini,held,clampuid,rot"
OBJ_HEAD = "id,type,cx,cy,w,h,groups,uid,radius,rot,sy0,sy1,shz,sdir"


def gd_row(att, t, y, vy, dead=0, og=0, snap=-1, mode="cube", vsize=1):
    return f"{t},{att},{t},{1000 + 1.6 * t:.3f},{y},{vy},0,{mode},0,{og},{dead},{vsize},{snap},0,0,1"


def model_row(t, y, vy, mode=0, og=0, held=0, clamp=-1, mini=0, dx=0.0):
    return f"{t},{1000 + 1.6 * t + dx:.3f},{y},{vy},{mode},{og},0,0,0,{mini},{held},{clamp},0"


class Fixture(unittest.TestCase):
    def setUp(self):
        self.d = Path(tempfile.mkdtemp(prefix="divview_test_"))
        self.addCleanup(shutil.rmtree, self.d, True)

    def write(self, name, lines):
        p = self.d / name
        p.write_text("\n".join(lines) + "\n", encoding="utf-8")
        return p

    def objrects(self):
        return self.write("objrects.txt", [
            OBJ_HEAD,
            "1,0,1100,100,30,30,0,10,0,0,0,0,0,0",       # a solid inside the corridor
            "8,2,1120,140,6,12,0,11,0,0,0,0,0,0",        # a hazard inside
            "9,7,1100,120,30,30,0,12,0,0,0,0,0,0",       # decoration: never drawn
            "10,20,1100,120,30,30,0,13,0,0,0,0,0,0",     # trigger: not drawn by default
            "1,0,90000,100,30,30,0,14,0,0,0,0,0,0",      # far away
            "1,0,1300,100,30,30,2,15,0,0,0,0,0,0",       # a mover that is recorded below
            "1,0,9000,100,30,30,2,16,0,0,0,0,0,0",       # a mover that never comes near
        ])

    def gd(self):
        rows = [GD_HEAD]
        # attempt 1: dies early; attempt 2: the full run; attempt 3: only a few rows in the window
        for t in range(1, 40):
            rows.append(gd_row(1, t, 100 + t, 0.1 * t, dead=1 if t >= 30 else 0))
        for t in range(1, 81):
            rows.append(gd_row(2, t, 100 + 0.5 * t, 0.5, snap=15 if t > 50 else -1))
        for t in range(10, 14):
            rows.append(gd_row(3, t, 100, 0))
        return self.write("dump.csv", rows)

    def model(self, name, parts_at=None, ends_at=80, shift=0.0, dx=0.0):
        rows = [MODEL_HEAD]
        for t in range(10, ends_at + 1):
            past = parts_at is not None and t >= parts_at
            y = 100 + 0.5 * t + (shift if past else 0.0)
            rows.append(model_row(t, y, 0.5, dx=dx if past else 0.0))
        return self.write(name, rows)


class Generation(Fixture):
    def build(self, **kw):
        a = dv.argparse.Namespace(objrects=str(self.objrects()), gd=str(self.gd()), attempt=None, model=[],
                                  groups=None, plan=None, t0=10, t1=80, tol=0.05, margin_x=300.0,
                                  margin_y=300.0, show_triggers=False, title="t", out="x", start=None, speed=None)
        for k, v in kw.items():
            setattr(a, k, v)
        return dv.build(a)

    def test_the_attempt_with_the_most_live_rows_in_the_window_is_shown(self):
        p = self.build(model=[str(self.model("a.csv"))])
        self.assertEqual(p["meta"]["attempt"], 2)       # attempt 1 dies at 30, attempt 3 has four rows

    def test_an_explicit_attempt_wins(self):
        p = self.build(model=[str(self.model("a.csv"))], attempt=3)
        self.assertEqual(p["meta"]["attempt"], 3)
        gd = next(s for s in p["series"] if s["kind"] == "gd")
        self.assertEqual([i for i, v in enumerate(gd["y"]) if v is not None], [0, 1, 2, 3])

    def test_a_dead_row_ends_the_series_and_gives_the_death_tick(self):
        p = self.build(model=[str(self.model("a.csv"))], attempt=1)
        gd = next(s for s in p["series"] if s["kind"] == "gd")
        self.assertEqual(gd["death"], 30)
        self.assertIsNone(gd["y"][31 - 10])              # the corpse is not drawn

    def test_agreement_means_no_first_part(self):
        p = self.build(model=["same=" + str(self.model("a.csv"))])
        s = p["summary"]["same"]
        self.assertIsNone(s["first"])
        self.assertEqual(s["compared"], 71)

    def test_the_first_tick_that_parts_and_the_reason(self):
        p = self.build(model=["off=" + str(self.model("a.csv", parts_at=44, shift=0.2))])
        s = p["summary"]["off"]
        self.assertEqual((s["first"], s["reason"]), (44, "y"))
        self.assertAlmostEqual(s["max_abs_dy"], 0.2, places=5)

    def test_a_jump_in_x_alone_is_a_part(self):
        # Every other column is the same as GD's; only x leaps 43 px at tick 44. It used to read as agreement.
        p = self.build(model=["jump=" + str(self.model("a.csv", parts_at=44, dx=43.0))])
        s = p["summary"]["jump"]
        self.assertEqual((s["first"], s["reason"]), (44, "x"))
        self.assertAlmostEqual(s["max_abs_dx"], 43.0, places=3)
        self.assertEqual(s["max_abs_dy"], 0)
        self.assertEqual(s["max_abs_dvy"], 0)
        self.assertEqual(self.build(model=["jump=" + str(self.model("a.csv", parts_at=44, dx=43.0))])["meta"]["start"], 38)

    def test_an_x_difference_inside_the_tolerance_is_not_a_part(self):
        p = self.build(model=["near=" + str(self.model("a.csv", parts_at=44, dx=0.04))])
        self.assertIsNone(p["summary"]["near"]["first"])

    def test_x_is_named_last_when_a_cause_parts_on_the_same_tick(self):
        # The reasons are listed in a fixed order (y, vy, mode, frame, then x), so a tick that fails on a cause keeps
        # naming it: only the ticks that used to read as agreement are new.
        p = self.build(model=["both=" + str(self.model("a.csv", parts_at=44, shift=0.2, dx=43.0))])
        self.assertEqual((p["summary"]["both"]["first"], p["summary"]["both"]["reason"]), (44, "y"))

    def test_a_difference_inside_the_tolerance_is_not_a_part(self):
        p = self.build(model=["x=" + str(self.model("a.csv", parts_at=44, shift=0.04))])
        self.assertIsNone(p["summary"]["x"]["first"])

    def test_a_model_that_stops_while_gd_runs_says_so(self):
        p = self.build(model=["short=" + str(self.model("a.csv", ends_at=60))])
        s = p["summary"]["short"]
        self.assertTrue(s["model_ends_early"])
        self.assertEqual(s["model_last"], 60)

    def test_a_short_window_opens_slowly_on_the_first_part(self):
        m = self.build(model=["off=" + str(self.model("a.csv", parts_at=44, shift=0.2))])["meta"]
        self.assertEqual((m["start"], m["speed"], m["scale"]), (38, 0.1, 2.4))

    def test_a_short_window_with_no_part_opens_on_its_first_tick(self):
        self.assertEqual(self.build(model=["same=" + str(self.model("a.csv"))])["meta"]["start"], 10)

    def test_a_long_window_opens_at_the_start_in_real_time_and_further_out(self):
        n = dv.LONG_RUN_TICKS + 50
        gd = self.write("long_gd.csv", [GD_HEAD] + [gd_row(2, t, 100 + 0.01 * t, 0.01) for t in range(1, n + 1)])
        mod = self.write("long_m.csv", [MODEL_HEAD] + [model_row(t, 100 + 0.01 * t + (0.3 if t >= 700 else 0), 0.01)
                                                       for t in range(1, n + 1)])
        m = self.build(gd=str(gd), model=["m=" + str(mod)], t0=1, t1=n)["meta"]
        self.assertEqual((m["start"], m["speed"], m["scale"]), (1, 1.0, 1.6))      # not 694: it is a run to watch

    def test_the_opening_tick_and_speed_can_be_given(self):
        m = self.build(model=[str(self.model("a.csv"))], start=33, speed=0.5)["meta"]
        self.assertEqual((m["start"], m["speed"]), (33, 0.5))

    def test_the_gap_count_is_exact_and_the_list_is_capped(self):
        keep = [t for t in range(1, 81) if not (20 <= t <= 79)]
        gd = self.write("holes.csv", [GD_HEAD] + [gd_row(2, t, 100 + 0.5 * t, 0.5) for t in keep])
        p = self.build(gd=str(gd), model=["m=" + str(self.model("a.csv"))])
        s = p["summary"]["m"]
        self.assertEqual(s["gap_count"], 60)           # ticks 20..79 lie inside the model's span 10..80
        self.assertEqual(len(s["gap_ticks"]), 50)      # the list is capped, the count is not

    def test_a_model_that_starts_late_is_not_called_early_ending(self):
        p = self.build(model=["anchored=" + str(self.model("a.csv"))])         # its rows start at 10
        self.assertEqual(p["summary"]["anchored"]["model_first"], 10)
        self.assertFalse(p["summary"]["anchored"]["model_ends_early"])

    def test_a_tick_gd_lacks_inside_the_models_span_is_listed_not_hidden(self):
        gd = self.write("gap.csv", [GD_HEAD] + [gd_row(2, t, 100 + 0.5 * t, 0.5) for t in range(1, 81) if t != 40])
        p = self.build(gd=str(gd), model=["m=" + str(self.model("a.csv"))])
        self.assertEqual(p["summary"]["m"]["gap_ticks"], [40])
        self.assertEqual(p["summary"]["m"]["compared"], 70)

    def test_decorations_and_triggers_and_far_objects_are_left_out(self):
        p = self.build(model=[str(self.model("a.csv"))])
        uids = sorted(o[6] for o in p["objects"])
        self.assertEqual(uids, [10, 11, 15])           # 12 decoration, 13 trigger, 14 far, 16 far mover

    def test_triggers_can_be_asked_for(self):
        p = self.build(model=[str(self.model("a.csv"))], show_triggers=True)
        self.assertIn(13, [o[6] for o in p["objects"]])

    def test_the_sources_are_listed_with_a_hash(self):
        p = self.build(model=[str(self.model("a.csv"))])
        roles = sorted(s["role"] for s in p["sources"])
        self.assertEqual(roles, ["gd", "model", "objrects"])
        self.assertTrue(all(re.fullmatch(r"[0-9a-f]{64}", s["sha256"]) for s in p["sources"]))

    def test_the_direction_is_named(self):
        p = self.build(model=[str(self.model("a.csv"))])
        self.assertEqual(p["meta"]["direction"], "model - GD")


class Rotation(Fixture):
    """The gameplay rotation (GD's gframe): 0 runs +x, 1 -y, 2 -x, 3 +y. The model has no frame 2 -- it runs a half turn as
    a reversed run in frame 0 -- and its trace has no column for that.

    The half turn is laid out the way GD's dump of an official level shows one: the turn tick itself does not move
    (x(A) - x(A-1) = 0), the reversed steps are ticks A+1 .. B+1, and the frame ends at B (the tick after still moves back)."""

    A, B = 30, 50

    def xs(self, half_turn):
        x, out = 5000.0, {}
        for t in range(1, 81):
            if half_turn and self.A + 1 <= t <= self.B + 1:
                x -= 1.3
            elif not (half_turn and t == self.A):
                x += 1.6
            out[t] = x
        return out

    def gd(self, frame_of, half_turn=False):
        xs = self.xs(half_turn)
        rows = [GD_HEAD + ",gframe"]
        for t in range(1, 81):
            rows.append(f"{t},1,{t},{xs[t]:.3f},100,0,0,cube,0,1,0,1,-1,0,0,1,{frame_of.get(t, 0)}")
        return self.write("rot_gd.csv", rows)

    def model(self, frame_of, half_turn=False):      # a half turn is frame 0 in the model's trace, whatever GD calls it
        xs = self.xs(half_turn)
        rows = [MODEL_HEAD + ",frame"]
        for t in range(1, 81):
            f = frame_of.get(t, 0)
            rows.append(f"{t},{xs[t]:.3f},100,0,0,1,0,0,0,0,0,-1,0,{0 if f == 2 else f}")
        return self.write("rot_model.csv", rows)

    def build(self, gd, model):
        a = dv.argparse.Namespace(objrects=str(self.objrects()), gd=str(gd), attempt=None, model=["m=" + str(model)],
                                  groups=None, plan=None, t0=1, t1=80, tol=0.05, margin_x=300.0, margin_y=300.0,
                                  show_triggers=False, title="t", out="x", start=None, speed=None)
        return dv.build(a)

    def test_a_reversed_run_in_frame_zero_is_read_as_the_half_turn(self):
        half = {t: 2 for t in range(self.A, self.B + 1)}
        p = self.build(self.gd(half, True), self.model(half, True))
        self.assertIsNone(p["summary"]["m"]["first"])            # GD's frame 2 against the model's reversed frame 0: one state
        m = next(s for s in p["series"] if s["label"] == "m")
        self.assertEqual([m["frame"][t - 1] for t in (self.A - 1, self.A, 40, self.B, self.B + 1)], [0, 2, 2, 2, 0])
        self.assertTrue(any("frame 2" in n for n in p["meta"]["notes"]))

    def test_the_other_frames_are_left_as_the_model_says(self):
        three = {t: 3 for t in range(self.A, self.B + 1)}
        p = self.build(self.gd(three), self.model(three))
        m = next(s for s in p["series"] if s["label"] == "m")
        self.assertEqual([m["frame"][t - 1] for t in (self.A - 1, self.A, self.B, self.B + 1)], [0, 3, 3, 0])
        self.assertIsNone(p["summary"]["m"]["first"])
        self.assertEqual(p["meta"]["notes"], [])

    def test_a_rotation_the_model_does_not_make_is_a_part_even_before_y_moves(self):
        gd = self.gd({t: 3 for t in range(50, 70)})
        s = self.build(gd, self.model({}))["summary"]["m"]       # the model never turns: frame 0 throughout
        self.assertEqual((s["first"], s["reason"]), (50, "frame"))
        self.assertEqual(s["max_abs_dy"], 0)                     # y and vy agree the whole way: only the frame says so

    def test_a_half_turn_the_model_does_not_make_shows_in_x_and_in_the_frame(self):
        half = {t: 2 for t in range(self.A, self.B + 1)}
        s = self.build(self.gd(half, True), self.model({}, False))["summary"]["m"]     # the model keeps running +x
        self.assertEqual(s["reason"], "frame")                   # x parts too (max |dx| below), but the rotation is named: it is the cause
        self.assertGreater(s["max_abs_dx"], 1.0)

    def test_the_gd_frame_is_read_from_gframe(self):
        one = {t: 1 for t in range(10, 20)}
        p = self.build(self.gd(one), self.model(one))
        g = next(s for s in p["series"] if s["kind"] == "gd")
        self.assertEqual([g["frame"][t - 1] for t in (9, 10, 19, 20)], [0, 1, 1, 0])


class TurnedObjects(Fixture):
    def test_obb_gives_a_turned_objects_own_box_size(self):
        obb = self.write("obb.txt", [
            "uid,id,type,cx,cy,rot,x0,y0,x1,y1,x2,y2,x3,y3,oob",
            "11,8,2,1120,140,-29,1120,137.2,1122.3,138.5,1120,142.7,1117.7,141.4,1",   # sides 2.64 and 4.79
            "10,1,0,1100,100,0,1085,85,1115,85,1115,115,1085,115,0",                    # not turned: no row kept
        ])
        sizes = dv.read_obb(obb)
        self.assertEqual(sorted(sizes), [11])
        self.assertAlmostEqual(sizes[11][0], 2.64, places=2)
        self.assertAlmostEqual(sizes[11][1], 4.79, places=2)

    def test_the_rows_carry_the_size_only_when_obb_is_given(self):
        a = dv.argparse.Namespace(objrects=str(self.objrects()), gd=str(self.gd()), attempt=None,
                                  model=["m=" + str(self.model("a.csv"))], groups=None, plan=None, t0=10, t1=80, tol=0.05,
                                  margin_x=300.0, margin_y=300.0, show_triggers=False, title="t", out="x", start=None,
                                  speed=None)
        plain = dv.build(a)
        self.assertTrue(all(len(o) == 12 for o in plain["objects"]))
        a.obb = str(self.write("obb2.txt", ["uid,id,type,cx,cy,rot,x0,y0,x1,y1,x2,y2,x3,y3,oob",
                                            "11,8,2,1120,140,-29,1120,137.2,1122.3,138.5,1120,142.7,1117.7,141.4,1"]))
        rows = {o[6]: o for o in dv.build(a)["objects"]}
        self.assertEqual(len(rows[11]), 14)
        self.assertAlmostEqual(rows[11][12], 2.64, places=2)
        self.assertEqual(rows[10][12:], [None, None])             # an object without an obb row: no size, so no turned box


class Movers(Fixture):
    def groups(self):
        return self.write("groups.txt", [
            "tick,uid,cx,cy,w,h,on,rot", "init,15,1", "init,16,0",
            "5,15,1300,100,30,30,1,0",          # before the window: what the window starts from
            "30,15,1300,150,30,30,1,0",
            "60,15,1300,150,30,30,0,0",         # switched off
            "30,16,9000,100,30,30,1,0",         # far away: never kept
        ])

    BOX = (900, -200, 1700, 400)

    def test_a_recorded_mover_keeps_the_row_before_the_window_and_the_rows_inside(self):
        rows, off, notes = dv.read_groups([self.groups()], 10, 80, self.BOX)
        self.assertEqual(sorted(rows), [15])
        self.assertEqual([r[0] for r in rows[15]], [5, 30, 60])
        self.assertEqual(off, set())

    def test_an_object_that_starts_off_is_reported(self):
        g = self.write("g2.txt", ["init,15,0", "40,15,1300,100,30,30,1,0"])
        rows, off, notes = dv.read_groups([g], 10, 80, self.BOX)
        self.assertEqual(off, {15})

    def test_a_later_recording_wins_for_the_ticks_it_covers_and_the_earlier_supplies_the_rest(self):
        early = self.write("early.txt", ["init,15,1", "1,15,1300,100,30,30,1,0", "20,15,1300,110,30,30,1,0",
                                         "50,15,1300,120,30,30,1,0", "70,15,1300,130,30,30,1,0", "end,70"])
        late = self.write("late.txt", ["init,15,1", "1,15,1300,100,30,30,1,0", "20,15,1300,160,30,30,1,0", "end,40"])
        rows, off, notes = dv.read_groups([early, late], 1, 60, self.BOX)
        # the late file speaks for ticks up to 40 (its trailer, past its last row at 20); the early file only past that
        self.assertEqual([(r[0], r[2]) for r in rows[15]], [(1, 100), (20, 160), (50, 120)])
        self.assertTrue(any("without the model's re-timing" in n for n in notes))   # t1 = 60 is past the late file's cover (40)
        # ...and a window past every file's cover says the movers are frozen instead
        rows, off, notes = dv.read_groups([early, late], 1, 80, self.BOX)
        self.assertEqual([(r[0], r[2]) for r in rows[15]], [(1, 100), (20, 160), (50, 120), (70, 130)])
        self.assertTrue(any("frozen" in n for n in notes))

    def test_the_overlay_notes_nothing_while_the_last_recording_covers_the_window(self):
        early = self.write("e.txt", ["1,15,1300,100,30,30,1,0", "end,100"])
        late = self.write("l.txt", ["1,15,1300,100,30,30,1,0", "20,15,1300,160,30,30,1,0", "end,90"])
        rows, off, notes = dv.read_groups([early, late], 1, 80, self.BOX)
        self.assertEqual(notes, [])

    def test_a_window_past_every_recording_is_said_to_show_frozen_movers(self):
        g = self.write("g3.txt", ["1,15,1300,100,30,30,1,0", "30,15,1300,150,30,30,1,0", "end,50"])
        rows, off, notes = dv.read_groups([g], 1, 80, self.BOX)
        self.assertEqual(len(notes), 1)
        self.assertIn("ends at tick 50", notes[0])
        self.assertIn("51..80", notes[0])

    def test_an_object_only_the_earlier_recording_knows_keeps_all_its_rows(self):
        early = self.write("e2.txt", ["1,15,1300,100,30,30,1,0", "30,15,1300,150,30,30,1,0", "end,100"])
        late = self.write("l2.txt", ["1,16,1400,100,30,30,1,0", "end,100"])
        rows, off, notes = dv.read_groups([early, late], 1, 80, self.BOX)
        self.assertEqual([r[0] for r in rows[15]], [1, 30])          # not in the late file: not overridden
        self.assertIn(16, rows)

    # A platform that rises one pixel a tick, and a cube standing on it for 20 ticks; GD's y is the platform's top plus the
    # cube's half, read from the recording at tick + shift. A recording of the same motion started elsewhere is a shift.
    def _contacts(self, shift):
        rec = {t: [t, 1300, 100 + t, 30, 30, 1, 0] for t in range(1, 121)}                     # cy 101 .. 220
        gd = {t: {"snap": 15, "og": 1, "mode": 0, "frame": 0, "mini": 0, "up": 0,
                  "y": rec[t + shift][2] + 15 + 15} for t in range(40, 60)}
        return gd, {15: [rec[t] for t in sorted(rec)]}

    def test_a_recording_started_at_another_tick_is_found_and_the_shift_named(self):
        gd, movers = self._contacts(shift=-7)
        al = dv.mover_alignment(gd, movers, t1=120)
        self.assertEqual([(a["uid"], a["best_shift"]) for a in al], [(15, -7)])
        self.assertGreater(al[0]["median_at_0"], 6.9)
        self.assertLess(al[0]["median_at_best"], 1e-6)
        note = dv.alignment_notes(al)
        self.assertEqual(len(note), 1)
        self.assertIn("-7 ticks", note[0])
        self.assertIn("uid 15", note[0])

    def test_a_recording_of_the_same_run_says_nothing(self):
        gd, movers = self._contacts(shift=0)
        al = dv.mover_alignment(gd, movers, t1=120)
        self.assertEqual(al[0]["best_shift"], 0)
        self.assertEqual(dv.alignment_notes(al), [])

    def test_too_few_contacts_prove_nothing(self):
        gd, movers = self._contacts(shift=-7)
        gd = {t: r for t, r in gd.items() if t < 45}                                 # five ticks
        self.assertEqual(dv.mover_alignment(gd, movers, t1=120), [])

    def _args(self, groups):
        return dv.argparse.Namespace(objrects=str(self.objrects()), gd=str(self.gd()), attempt=None,
                                     model=["m=" + str(self.model("a.csv"))], groups=groups, plan=None, t0=10, t1=80,
                                     tol=0.05, margin_x=300.0, margin_y=300.0, show_triggers=False, title="t", out="x",
                                     start=None, speed=None)

    def test_grouped_objects_without_a_recording_are_said_to_sit_where_the_level_loads_them(self):
        notes = dv.build(self._args(None))["meta"]["notes"]
        self.assertTrue(any("belong to a group" in n and "no --groups recording" in n for n in notes))
        with_rec = dv.build(self._args([str(self.groups())]))["meta"]["notes"]
        self.assertFalse(any("belong to a group" in n for n in with_rec))

    def test_the_page_carries_the_mover_with_its_base_row(self):
        a = dv.argparse.Namespace(objrects=str(self.objrects()), gd=str(self.gd()),
                                  attempt=None, model=["m=" + str(self.model("a.csv"))], groups=[str(self.groups())],
                                  plan=None, t0=10, t1=80, tol=0.05, margin_x=300.0, margin_y=300.0,
                                  show_triggers=False, title="t", out="x", start=None, speed=None)
        p = dv.build(a)
        self.assertEqual(len(p["movers"]), 1)
        self.assertEqual(p["movers"][0]["base"][6], 15)
        self.assertNotIn(15, [o[6] for o in p["objects"]])      # drawn from its rows, not twice


class Page(Fixture):
    def make(self):
        a = dv.argparse.Namespace(objrects=str(self.objrects()), gd=str(self.gd()), attempt=None,
                                  model=["m=" + str(self.model("a.csv", parts_at=44, shift=0.3))], groups=None,
                                  plan=str(self.write("plan.txt", ["input=20,1", "input=24,0"])), t0=10, t1=80,
                                  tol=0.05, margin_x=300.0, margin_y=300.0, show_triggers=False,
                                  title="a <b> & c", out="x", start=None, speed=None)
        return dv.render(dv.build(a))

    def test_the_data_block_is_valid_json_and_the_title_is_escaped(self):
        html = self.make()
        m = re.search(r'<script id="data" type="application/json">(.*?)</script>', html, re.S)
        data = json.loads(m.group(1).replace("<\\/", "</"))
        self.assertEqual(data["plan"], [[20, 1], [24, 0]])
        self.assertIn("a &lt;b&gt; &amp; c", html)
        self.assertNotIn("__DATA_JSON__", html)
        self.assertNotIn("__TITLE__", html)

    def test_the_folder_can_be_served_on_the_loopback_address_only(self):
        import threading
        import urllib.request
        (self.d / "view.html").write_text(self.make(), encoding="utf-8")
        srv = dv.make_server(self.d, 0)
        self.addCleanup(srv.server_close)
        threading.Thread(target=srv.serve_forever, daemon=True).start()
        self.addCleanup(srv.shutdown)
        self.assertEqual(srv.server_address[0], "127.0.0.1")
        with urllib.request.urlopen(f"http://127.0.0.1:{srv.server_address[1]}/view.html", timeout=5) as r:
            body = r.read().decode("utf-8")
            self.assertEqual(r.status, 200)
        self.assertIn('id="data"', body)

    def test_every_mode_has_its_own_silhouette(self):
        html = self.make()
        self.assertEqual(len(dv.MODES), 8)
        for case in range(1, len(dv.MODES)):          # 0 is the default branch (cube, and anything unknown)
            self.assertIn(f"case {case}:", html, msg=f"no silhouette for mode {dv.MODES[case]}")

    # Three turned hazards of an official level with the corners GD reported for them (its obb file): a spike turned 29
    # degrees, one turned 69, and one at 45 where the bound cannot be undone.
    TURNED = [
        {"uid": 7696, "cx": 12999.3, "cy": 728.096, "w": 4.59961, "h": 5.45862, "rot": -29.0,
         "obb": [[12999.3, 725.367], [13001.6, 726.627], [12999.3, 730.825], [12997.0, 729.565]]},
        {"uid": 7712, "cx": 13122.3, "cy": 916.861, "w": 5.41211, "h": 4.14758, "rot": -69.0,
         "obb": [[13124.1, 914.787], [13125.0, 917.215], [13120.5, 918.935], [13119.6, 916.507]]},
        {"uid": 17297, "cx": 21574.5, "cy": 2234.5, "w": 5.23438, "h": 5.23242, "rot": -45.0,
         "obb": [[21575.3, 2231.88], [21577.1, 2233.72], [21573.7, 2237.12], [21571.9, 2235.28]]},
    ]

    @unittest.skipUnless(shutil.which("node"), "node is not installed")
    def test_a_turned_hazard_is_drawn_as_the_box_gd_reports(self):
        html = self.make()
        fn = re.search(r"function rotRect\(.*?\n\}\n", html, re.S).group(0)
        nom = re.search(r"const isTurned = [^\n]*\nfunction nominalOf\(o\) \{.*?\n\}\n", html, re.S).group(0)
        js = nom + fn + """
const cases = %s;
const near = (q, obb) => { let e = 0; for (const p of q) { let d = 1e9; for (const o of obb) d = Math.min(d, Math.hypot(p[0]-o[0], p[1]-o[1])); e = Math.max(e, d); } return e; };
const out = cases.map(c => {
  const side = (a, b) => Math.hypot(a[0]-b[0], a[1]-b[1]);
  const withObb = {r: [0, 2, c.cx, c.cy, c.w, c.h, c.uid, 0, c.rot, 0, 0, 0, side(c.obb[0], c.obb[1]), side(c.obb[1], c.obb[2])]};
  const without = {r: [0, 2, c.cx, c.cy, c.w, c.h, c.uid, 0, c.rot, 0, 0, 0]};
  const n1 = nominalOf(withObb), n0 = nominalOf(without);
  return {uid: c.uid, exact: near(rotRect(c.cx, c.cy, n1[0], n1[1], c.rot), c.obb), undone: n0 ? near(rotRect(c.cx, c.cy, n0[0], n0[1], c.rot), c.obb) : null};
});
console.log(JSON.stringify(out));
""" % json.dumps(self.TURNED)
        f = self.d / "turned.js"
        f.write_text(js, encoding="utf-8")
        r = subprocess.run(["node", str(f)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        by = {o["uid"]: o for o in json.loads(r.stdout)}
        for uid in (7696, 7712, 17297):
            self.assertLess(by[uid]["exact"], 0.06, msg=f"uid {uid}: the box from the obb size is not the obb")
        self.assertLess(by[7696]["undone"], 0.06)       # the bound undone gives the same box away from 45 degrees...
        self.assertLess(by[7712]["undone"], 0.06)
        self.assertIsNone(by[17297]["undone"])           # ...and at 45 it declines (the object stays its bound)

    @unittest.skipUnless(shutil.which("node"), "node is not installed")
    def test_the_page_and_the_script_name_the_same_first_part(self):
        # The page runs its own copy of the rule (firstPart); the generator's summary and the page's badge must not
        # disagree. Models that part in y, in x alone, in both at once, and not at all.
        a = dv.argparse.Namespace(
            objrects=str(self.objrects()), gd=str(self.gd()), attempt=None,
            model=["y=" + str(self.model("y.csv", parts_at=44, shift=0.3)),
                   "x=" + str(self.model("x.csv", parts_at=44, dx=43.0)),
                   "both=" + str(self.model("b.csv", parts_at=44, shift=0.3, dx=43.0)),
                   "same=" + str(self.model("s.csv"))],
            groups=None, plan=None, t0=10, t1=80, tol=0.05, margin_x=300.0, margin_y=300.0, show_triggers=False,
            title="t", out="x", start=None, speed=None)
        payload = dv.build(a)
        html = dv.render(payload)
        fn = re.search(r"function firstPart\(.*?\n\}\n", html, re.S).group(0)
        series = payload["series"]
        gd = next(s for s in series if s["kind"] == "gd")
        models = [s for s in series if s["kind"] == "model"]
        js = ("const T0 = %d, N = %d; const GD = %s; const MODELS = %s;\n" % (
                  payload["meta"]["t0"], payload["meta"]["t1"] - payload["meta"]["t0"] + 1,
                  json.dumps(gd), json.dumps(models))
              + fn + "\nconsole.log(JSON.stringify(MODELS.map(m => firstPart(m, %s))));\n" % payload["meta"]["tol"])
        f = self.d / "firstpart.js"
        f.write_text(js, encoding="utf-8")
        r = subprocess.run(["node", str(f)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        page = json.loads(r.stdout)
        for m, got in zip(models, page):
            s = payload["summary"][m["label"]]
            want = None if s["first"] is None else {"t": s["first"], "why": s["reason"]}
            self.assertEqual(got, want, msg=f"{m['label']}: the page and the generator disagree")
        self.assertEqual([p and p["why"] for p in page], ["y", "x", "y", None])

    @unittest.skipUnless(shutil.which("node"), "node is not installed")
    def test_the_script_parses(self):
        html = self.make()
        scripts = re.findall(r"<script>(.*?)</script>", html, re.S)
        self.assertEqual(len(scripts), 1)
        js = self.d / "view.js"
        js.write_text("(function(){" + scripts[0] + "\n})", encoding="utf-8")
        r = subprocess.run(["node", "--check", str(js)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
