"""Synthetic-level bench for the dp CLI (Path Solver). No game needed.

    cmake -S . -B build-dp -DGDSOLVER_BUILD_MOD=OFF -DCMAKE_CXX_COMPILER=clang++ && cmake --build build-dp
    python tools/synth_bench.py build-dp/dp/leveldp

Writes small objrects dumps (one gamemode each: wave corridors, a skippable ship portal, ship
slalom, cube over spikes, UFO, ball, robot, spider, swing), solves each with and without the
human-style preferences, and prints the number of button changes and of gaps under 6 ticks.
It measures the MODEL's plans only: nothing here says what the game does with them.
"""
import csv, os, subprocess, sys, tempfile

HDR = "id,type,cx,cy,w,h,groups,uid,radius,rot\n"
BASE = "--cap 2000 --shipyq 0.25 --shipvq 1 --threads 8 --capladder 125 --inputgrid 2"
RULES = ("--glitchportal 15 --glitchmode 75 --skyguard 300 --glitchwave 5 --glitchembed 14 --wavepulse 12 "
         "--wavecentre 8 --waveslide --minpulse 2 --wavetoggle 20")
PREF = "--clickcost 20 --clickcap"


class Level:
    def __init__(self):
        self.rows, self.uid = [], 1

    def add(self, oid, otype, cx, cy, w, h):
        self.rows.append(f"{oid},{otype},{cx},{cy},{w},{h},,{self.uid},0,0")
        self.uid += 1
        return self

    def row(self, x0, x1, y):
        for x in range(x0, x1, 30):
            self.add(1, 0, x, y, 30, 30)
        return self

    def save(self, path, end):
        self.add(1, 0, end, 75, 30, 30)
        with open(path, "w") as f:
            f.write(HDR + "\n".join(self.rows) + "\n")


def levels():
    out = {}
    a = Level(); a.row(315, 3600, 435).row(315, 3600, 75); a.add(660, 26, 300, 150, 30, 120); out["wave_open"] = (a, 3900)
    b = Level(); b.row(315, 3600, 225); b.add(660, 26, 300, 150, 30, 120); out["wave_tight"] = (b, 3900)
    c = Level(); c.add(13, 5, 600, 215, 34, 86); c.row(315, 3000, 435); out["portal_skip"] = (c, 3300)
    f = Level(); f.add(13, 5, 300, 150, 34, 120); f.row(315, 3300, 435)
    for i, x in enumerate(range(700, 3000, 260)):
        for y in (range(105, 241, 30) if i % 2 == 0 else range(225, 420, 30)):
            f.add(1, 0, x, y, 30, 30)
    out["ship_slalom"] = (f, 3600)
    g = Level()
    for x in range(500, 3000, 220):
        g.add(8, 2, x, 96, 6, 12)
    out["cube_spikes"] = (g, 3300)
    for name, (pid, pt) in {"ufo": (111, 19), "ball": (47, 16), "robot": (745, 27), "spider": (1331, 33),
                            "swing": (1933, 41)}.items():
        l = Level(); l.add(pid, pt, 300, 150, 34, 120); l.row(315, 3300, 315)
        for i, x in enumerate(range(700, 3000, 280)):
            for y in (range(105, 170, 30) if i % 2 == 0 else range(225, 300, 30)):
                l.add(1, 0, x, y, 30, 30)
            l.add(8, 2, x + 90, 96, 6, 12)
        out[name] = (l, 3600)
    return out


def run(dp, path, args):
    plan = path + ".plan"
    for f in (plan, plan + ".trace.csv"):
        if os.path.exists(f):
            os.remove(f)
    p = subprocess.run([dp, path, "--out", plan] + args.split(), capture_output=True, text=True, timeout=600)
    ev = [tuple(map(int, l[6:].split(","))) for l in open(plan)] if os.path.exists(plan) else []
    gaps = [ev[i][0] - ev[i - 1][0] for i in range(1, len(ev))]
    modes = []
    if os.path.exists(plan + ".trace.csv"):
        for r in csv.DictReader(open(plan + ".trace.csv")):
            if not modes or modes[-1] != r["mode"]:
                modes.append(r["mode"])
    return ("SOLVED" in p.stdout, len(ev), sum(g < 6 for g in gaps), "".join(modes))


if __name__ == "__main__":
    dp = sys.argv[1]
    d = tempfile.mkdtemp(prefix="synth_")
    for name, (lv, end) in levels().items():
        path = os.path.join(d, name + ".csv")
        lv.save(path, end)
        b = run(dp, path, BASE + " " + RULES)
        n = run(dp, path, BASE + " " + RULES + " " + PREF)
        print(f"{name:12s} rules only: solved={b[0]} changes={b[1]:4d} short={b[2]:4d} modes={b[3]} | "
              f"+preference: solved={n[0]} changes={n[1]:4d} short={n[2]:4d} modes={n[3]}", flush=True)
