# -*- coding: utf-8 -*-
"""The keys every worker launch carries, and the one a launch for a person adds. (no GD; milliseconds)

    python py/test_worker_keys.py

The replay panel is on by default in the mod, for a person watching a solve. A worker is watched by nobody, so every launch
that goes through gdtas.worker (run_session for the cold runs and collects, Worker.open for the MCP's gd_* tools) turns it
off, in one place; observe.launch_visible, the launch for a person, asks for it. What is pinned here is that one place.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from gdtas import worker  # noqa: E402


class WorkerKeys(unittest.TestCase):
    def test_a_run_session_launch_turns_the_panel_off_and_holds_the_window(self):
        out = worker.with_worker_keys(["enabled=1", "level=14"])
        self.assertEqual(out, ["enabled=1", "level=14", "windowguard=1", "replaypanel=0"])

    def test_the_callers_own_values_win(self):
        out = worker.with_worker_keys(["replaypanel=1", "windowguard=0"])
        self.assertEqual(out, ["replaypanel=1", "windowguard=0"])

    def test_one_key_set_leaves_the_other_added(self):
        self.assertEqual(worker.with_worker_keys(["replaypanel=1"]), ["replaypanel=1", "windowguard=1"])
        self.assertEqual(worker.with_worker_keys(["windowguard=0"]), ["windowguard=0", "replaypanel=0"])

    def test_the_input_is_not_changed(self):
        cfg = ["enabled=1"]
        worker.with_worker_keys(cfg)
        self.assertEqual(cfg, ["enabled=1"])

    def test_a_serving_worker_has_it_off_too(self):
        # Worker.open writes BASE_CFG plus the caller's extras: the MCP's gd_* tools and the serve workers.
        self.assertEqual(worker.BASE_CFG["replaypanel"], "0")
        self.assertEqual(dict(worker.BASE_CFG, level="1", **{"replaypanel": "1"})["replaypanel"], "1")


class VisibleLaunch(unittest.TestCase):
    def test_a_launch_for_a_person_asks_for_the_panel_unless_told_otherwise(self):
        import inspect

        from gdtas import observe
        src = inspect.getsource(observe.launch_visible)
        self.assertIn('startswith("replaypanel=")', src)
        self.assertIn('"replaypanel=1"', src)


if __name__ == "__main__":
    unittest.main(verbosity=2)
