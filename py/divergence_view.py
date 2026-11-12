"""Look at where the model and GD part, in a browser, with no GD running.

    python py/divergence_view.py --objrects objrects.txt --gd dump.csv \\
        --model model.trace.csv --t0 6074 --t1 6165 --out view.html

    # two models against the same GD run, the moving geometry the run recorded, and the plan's presses
    python py/divergence_view.py --objrects objrects.txt --gd dump.csv \\
        --model off=fw8/model.trace.csv --model on=fw9/model.trace.csv \\
        --groups dp_groups.txt --plan dp_plan.txt --t0 6074 --t1 6165 --out view.html

It writes ONE html file (no server, no library, nothing fetched): the level's geometry as the physics
sees it (objrects.txt's boxes, circles and slope lines, the moving objects where the run's recording
puts them), GD's player from a dump.csv and each model's from a trace.csv, a scrubber, and a strip
that plots the difference per tick against the tolerance so the first tick where they part is the
first thing on the page. Open it in any browser; arrow keys step a tick.

Why it exists: the question "where does the model part from GD, and what is standing there" had
only been answered by printing columns. The geometry and both worldlines are already on disk, so a
picture drawn from them needs no game, and a person can check a divergence without one.

What it will not do silently (the habits of gdtas/compare.py):
  * every difference is named MODEL - GD, in the page and on stdout;
  * ticks are compared at the SAME tick only; a tick that one side lacks is a gap on the plot and a
    line in the summary, never an intersection taken quietly;
  * every input is listed in the page with its size and sha256, so a picture can be tied to the files
    that made it;
  * a model trace that ends while GD is still alive says so (its last tick is the model's death, or
    the end of the window it was asked for).
"""

from __future__ import annotations

import argparse
import functools
import hashlib
import json
import sys
from bisect import bisect_right
from html import escape
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HERE = Path(__file__).resolve().parent
TEMPLATE = HERE / "gdtas" / "divview_template.html"

MODES = ["cube", "ship", "ball", "ufo", "wave", "robot", "spider", "swing"]
LONG_RUN_TICKS = 1500      # a window longer than this is a run to watch, not a divergence to look at
# GameObject::m_objectType values the viewer does not draw: decoration, and the triggers/modifiers
HIDDEN_TYPES = {7, 20}


# ------------------------------------------------------------------ small readers

def _f(s: str, default: float | None = None) -> float | None:
    try:
        return float(s)
    except (TypeError, ValueError):
        return default


def _r(v: float | None, nd: int = 3):
    """JSON-sized number: None stays None, an integer-valued float is written as an int."""
    if v is None:
        return None
    v = round(v, nd)
    return int(v) if v == int(v) else v


def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def source(role: str, label: str, path: Path) -> dict:
    return {"role": role, "label": label, "path": str(path), "bytes": path.stat().st_size,
            "sha256": sha256_of(path)}


def _header(line: str) -> dict[str, int]:
    return {name: i for i, name in enumerate(line.rstrip("\r\n").lstrip("﻿").split(","))}


def mode_index(s: str) -> int:
    """GD's dump says 'cube'; the model's trace says 0."""
    s = s.strip()
    if s.isdigit():
        return int(s)
    return MODES.index(s) if s in MODES else -1


# ------------------------------------------------------------------ the two kinds of worldline

def read_gd(path: Path, t0: int, t1: int, attempt: int | None) -> tuple[int, dict[int, dict], int | None]:
    """GD's dump.csv holds EVERY attempt; one is shown. With attempt=None the one with the most live
    rows inside the window wins (the later attempt on a tie). A row with dead != 0 ends the series:
    the corpse keeps being ticked and its y runs off to -1e6.
    Returns (attempt, {tick: row}, death tick or None)."""
    with open(path, encoding="utf-8-sig", errors="replace") as f:
        col = _header(f.readline())
        need = ("attempt", "tick", "x", "y", "yvel")
        for k in need:
            if k not in col:
                raise SystemExit(f"{path}: no '{k}' column; this is not a GD dump.csv")
        ia, it, idead = col["attempt"], col["tick"], col.get("dead")
        counts: dict[int, int] = {}
        rows_by_attempt: dict[int, list[list[str]]] = {}
        for line in f:
            p = line.rstrip("\r\n").split(",")
            if len(p) <= max(ia, it):
                continue
            try:
                t = int(float(p[it]))
                a = int(float(p[ia]))
            except ValueError:
                continue
            if attempt is not None and a != attempt:
                continue
            if t < t0 or t > t1:
                continue
            rows_by_attempt.setdefault(a, []).append(p)
            if idead is None or p[idead] == "0":
                counts[a] = counts.get(a, 0) + 1
    if not rows_by_attempt:
        raise SystemExit(f"{path}: no rows in ticks {t0}..{t1}"
                         + (f" for attempt {attempt}" if attempt is not None else ""))
    pick = attempt if attempt is not None else max(counts or rows_by_attempt,
                                                    key=lambda a: (counts.get(a, 0), a))
    get = lambda p, k, d="": p[col[k]] if k in col and col[k] < len(p) else d
    out: dict[int, dict] = {}
    death = None
    for p in rows_by_attempt[pick]:
        t = int(float(p[it]))
        dead = get(p, "dead", "0") not in ("0", "")
        if death is not None:
            break
        out[t] = {
            "x": _f(p[col["x"]]), "y": _f(p[col["y"]]), "vy": _f(p[col["yvel"]]),
            "rot": _f(get(p, "rot"), 0.0), "mode": mode_index(get(p, "mode", "cube")),
            "up": int(_f(get(p, "upsideDown"), 0) or 0), "og": int(_f(get(p, "onGround"), 0) or 0),
            "mini": 1 if (_f(get(p, "vsize"), 1.0) or 1.0) < 0.9 else 0,
            "snap": int(_f(get(p, "snapuid"), -1) or -1),
            "dual": int(_f(get(p, "dual"), 0) or 0), "y2": _f(get(p, "p2y")),
            "speed": _f(get(p, "speed")),
            # gameplay rotation (a Rotate Gameplay trigger): 0 = running +x, 1 = -y, 2 = -x, 3 = +y
            "frame": int(_f(get(p, "gframe"), 0) or 0),
        }
        if dead:
            death = t
    return pick, out, death


def read_model(path: Path, t0: int, t1: int) -> dict[int, dict]:
    """The dp trace.csv (leveldp --out X writes X.trace.csv)."""
    out: dict[int, dict] = {}
    with open(path, encoding="utf-8-sig", errors="replace") as f:
        col = _header(f.readline())
        for k in ("tick", "x", "y", "vy"):
            if k not in col:
                raise SystemExit(f"{path}: no '{k}' column; this is not a leveldp trace.csv")
        get = lambda p, k, d="": p[col[k]] if k in col and col[k] < len(p) else d
        for line in f:
            p = line.rstrip("\r\n").split(",")
            try:
                t = int(float(p[col["tick"]]))
            except (ValueError, IndexError):
                continue
            if t < t0 or t > t1:
                continue
            out[t] = {
                "x": _f(p[col["x"]]), "y": _f(p[col["y"]]), "vy": _f(p[col["vy"]]),
                "rot": _f(get(p, "rot"), 0.0), "mode": mode_index(get(p, "mode", "0")),
                "up": int(_f(get(p, "flip"), 0) or 0), "og": int(_f(get(p, "grounded"), 0) or 0),
                "mini": int(_f(get(p, "mini"), 0) or 0),
                "held": int(_f(get(p, "held"), 0) or 0),
                "snap": int(_f(get(p, "clampuid"), -1) or -1) if get(p, "clampuid", "-").lstrip("-").isdigit() else -1,
                "dual": int(_f(get(p, "dual"), 0) or 0), "y2": _f(get(p, "y2")),
                "frame": int(_f(get(p, "frame"), 0) or 0),
            }
    return out


def infer_reversed_frames(rows: dict[int, dict]) -> int:
    """The model runs a half turn of the gameplay (GD's gframe 2) as a REVERSED run in frame 0 -- State::rev -- and its
    trace has no column for it, so the trace says frame 0 where GD says 2. A row in frame 0 whose x runs backwards (to the
    next row, else from the previous one) is read as frame 2, so the two are comparable. The other frames are left as
    they are: 1 and 3 have been seen to agree with GD's, and a reversed run inside them has not been seen. Returns how
    many rows were read this way."""
    n = 0
    for t, r in rows.items():
        if r.get("frame", 0) != 0:
            continue
        nxt, prv = rows.get(t + 1), rows.get(t - 1)
        d = (nxt["x"] - r["x"]) if nxt is not None else ((r["x"] - prv["x"]) if prv is not None else 0.0)
        if d < -0.01:
            r["frame"] = 2
            n += 1
    return n


def read_plan(path: Path) -> list[list[int]]:
    """input=T,V lines; V is 1 for down. A press at T acts on the row of tick T+1."""
    ev = []
    for line in path.read_text(encoding="utf-8-sig", errors="replace").splitlines():
        if line.startswith("input="):
            a, _, b = line[6:].partition(",")
            try:
                ev.append([int(a), 1 if int(b) else 0])
            except ValueError:
                pass
    return ev


# ------------------------------------------------------------------ where they part

def divergence(gd: dict[int, dict], model: dict[int, dict], t0: int, t1: int, tol: float) -> dict:
    """Compared at the SAME tick only. Differences are MODEL - GD."""
    first = None
    reason = ""
    max_dx = max_dy = max_dv = 0.0
    n = 0
    mt = sorted(model)
    for t in range(t0, t1 + 1):
        g, m = gd.get(t), model.get(t)
        if g is None or m is None:
            continue
        n += 1
        # x is part of the state: a teleport, a missed push or a wrong clamp can move it alone while y and vy stay equal
        # (a 43 px jump with every other column the same is a part, and the first thing the view must stop on)
        dx = m["x"] - g["x"] if m.get("x") is not None and g.get("x") is not None else 0.0
        dy, dv = m["y"] - g["y"], m["vy"] - g["vy"]
        max_dx, max_dy, max_dv = max(max_dx, abs(dx)), max(max_dy, abs(dy)), max(max_dv, abs(dv))
        why = ""
        if abs(dy) > tol:
            why = "y"
        elif abs(dv) > tol:
            why = "vy"
        elif m["mode"] != g["mode"]:
            why = "mode"
        elif m.get("frame", 0) != g.get("frame", 0):
            why = "frame"        # the rotation of the gameplay is part of the state: parting there is parting
        elif abs(dx) > tol:
            why = "x"            # last, so a tick that fails on a cause (a rotation the model lacks moves x too) names the cause
        if why and first is None:
            first, reason = t, why
    gd_last = max(gd) if gd else None
    # ticks GD lacks inside the span the model covers (up to GD's last): a gap is reported, never intersected away
    gaps = [t for t in range(mt[0], min(mt[-1], gd_last or mt[-1]) + 1) if t not in gd] if mt else []
    return {
        "first": first, "reason": reason, "compared": n,
        "max_abs_dx": _r(max_dx, 6), "max_abs_dy": _r(max_dy, 6), "max_abs_dvy": _r(max_dv, 6),
        "model_first": mt[0] if mt else None, "model_last": mt[-1] if mt else None,
        # GD still alive after the model's last row: the model died there, or the trace stops short
        "model_ends_early": bool(mt and gd_last is not None and mt[-1] < gd_last),
        "gap_count": len(gaps), "gap_ticks": gaps[:50],
    }


# ------------------------------------------------------------------ geometry

def _parse_groups(path: Path, t1: int):
    """One recording: ({uid: its rows up to t1}, {uid: initial on}, cover). `cover` is the last tick the recording speaks
    for: its last row, or the `end,<tick>` trailer the recorder writes when that is later (the level can go quiet before
    the run ends, and those ticks are covered too)."""
    rows: dict[int, list[tuple]] = {}
    init: dict[int, bool] = {}
    last = end = -1
    with open(path, encoding="utf-8-sig", errors="replace") as f:
        for line in f:
            p = line.rstrip("\r\n").split(",")
            if p[0] == "init":
                if len(p) >= 3:
                    init[int(p[1])] = p[2] != "0"
                continue
            if p[0] == "end":
                if len(p) >= 2 and p[1].lstrip("-").isdigit():
                    end = max(end, int(p[1]))
                continue
            if not p[0].lstrip("-").isdigit() or len(p) < 8:
                continue
            t = int(p[0])
            last = max(last, t)
            if t <= t1:
                rows.setdefault(int(p[1]), []).append(
                    (t, int(p[1]), float(p[2]), float(p[3]), float(p[4]), float(p[5]), int(float(p[6])), float(p[7])))
    return rows, init, max(last, end)


def read_groups(paths, t0: int, t1: int, bbox: tuple[float, float, float, float]):
    """dp_groups.txt files: `init,uid,on` lines, then `tick,uid,cx,cy,w,h,on,rot` rows, one per change (and an
    `end,<tick>` trailer); an object holds its last row until the next.

    Several files are laid one over the other the way the model lays them (overlayGroupTimeline, dp/src/dp/groups.hpp): a
    later file wins for every tick it covers, for each object it has rows for; the earlier file supplies the rest. The
    model also re-times the earlier file where the two meet (bootAlignShift); that is NOT done here, so past the last file's
    coverage an earlier file's moving objects are only approximately placed -- and that is said.

    Returns ({uid: rows}, {uid whose initial state is off}, notes). Rows kept: the last one BEFORE t0 (what the window
    starts from) and every one inside it. An object is kept when any of its poses in that span reaches the box."""
    x0, y0, x1, y1 = bbox
    layers = [_parse_groups(Path(p), t1) for p in paths]
    rows, init, cover = layers[0]
    rows = {u: list(v) for u, v in rows.items()}
    init = dict(init)
    covers = [cover]
    for over_rows, over_init, over_cover in layers[1:]:
        for uid, over in over_rows.items():
            rows[uid] = over + [r for r in rows.get(uid, []) if r[0] > over_cover]
        init.update(over_init)
        covers.append(over_cover)
    notes: list[str] = []
    if t1 > max(covers) + 2:                    # (a recording stops on the tick the run does: one or two ticks over is not news)
        notes.append(f"the recording of the moving objects ends at tick {max(covers)}: past it they hold their last "
                     f"recorded pose, so the ticks {max(covers) + 1}..{t1} show them frozen")
    elif len(layers) > 1 and t1 > covers[-1]:
        notes.append(f"the last recording covers up to tick {covers[-1]}; past it the earlier file places the moving "
                     f"objects without the model's re-timing, so they are approximate there")
    last_before: dict[int, tuple] = {}
    win: dict[int, list[tuple]] = {}
    for uid, v in rows.items():
        for r in sorted(v, key=lambda r: r[0]):
            if r[0] < t0:
                last_before[uid] = r
            elif r[0] <= t1:
                win.setdefault(uid, []).append(r)
    out: dict[int, list[list]] = {}
    for uid in set(last_before) | set(win):
        rs = ([last_before[uid]] if uid in last_before else []) + win.get(uid, [])
        hit = any(r[2] + r[4] / 2 >= x0 and r[2] - r[4] / 2 <= x1 and r[3] + r[5] / 2 >= y0
                  and r[3] - r[5] / 2 <= y1 for r in rs)
        if hit:
            out[uid] = [[r[0], _r(r[2]), _r(r[3]), _r(r[4]), _r(r[5]), r[6], _r(r[7])] for r in rs]
    return out, {u for u, on in init.items() if not on and u in out}, notes


def mover_alignment(gd_rows: dict[int, dict], movers: dict[int, list], t1: int, max_shift: int = 40) -> list[dict]:
    """Does the recording of the moving objects belong to the GD run it is shown with? GD's dump names the object a cube
    stands on (snapuid), and the player's height then IS that object's top (or underside) plus the cube's half. For each
    recorded mover with at least six such ticks, the median |residual| at the tick itself and the time shift d (the recording
    read at tick+d) that makes it smallest. A recording of another run of the same objects shows as a consistent d with a
    residual near zero there: the motion is the same, started at another tick."""
    contacts: dict[int, list[tuple]] = {}
    for t, r in gd_rows.items():
        u = r.get("snap", -1)
        if u in movers and r.get("og") == 1 and r.get("mode") == 0 and not r.get("frame") and r.get("y") is not None:
            contacts.setdefault(u, []).append((t, r["y"], bool(r.get("mini")), bool(r.get("up"))))
    out = []
    for uid, cs in contacts.items():
        if len(cs) < 6:
            continue
        rows = movers[uid]
        ticks = [x[0] for x in rows]

        def resid(d: int):
            res = []
            for t, y, mini, up in cs:
                k = bisect_right(ticks, t + d) - 1
                if k < 0 or t + d > t1:
                    continue
                cy, h = rows[k][2], rows[k][4]
                half = 9.0 if mini else 15.0
                res.append(abs(y - ((cy - h / 2 - half) if up else (cy + h / 2 + half))))
            return sorted(res) if len(res) * 5 >= len(cs) * 4 else None

        r0 = resid(0)
        best = None
        for d in range(-max_shift, max_shift + 1):
            r = resid(d)
            if r is not None and (best is None or r[len(r) // 2] < best[1]):
                best = (d, r[len(r) // 2])
        if r0 is not None and best is not None:
            out.append({"uid": uid, "n": len(cs), "median_at_0": round(r0[len(r0) // 2], 4),
                        "best_shift": best[0], "median_at_best": round(best[1], 4),
                        "at_limit": abs(best[0]) == max_shift})       # the true shift may be larger than the search reaches
    return out


def alignment_notes(al: list[dict]) -> list[str]:
    """One line per consistent shift: the recorded movers that fit GD only when read d ticks away."""
    shifted: dict[int, list[dict]] = {}
    for a in al:
        if a["best_shift"] != 0 and a["median_at_0"] > 0.25 and a["median_at_best"] < 0.05:
            shifted.setdefault(a["best_shift"], []).append(a)
    notes = []
    for d, items in sorted(shifted.items(), key=lambda kv: -len(kv[1])):
        uids = ", ".join(str(a["uid"]) for a in items[:6]) + (" ..." if len(items) > 6 else "")
        worst0 = max(a["median_at_0"] for a in items)
        reach = "at least " if any(a.get("at_limit") for a in items) else ""
        notes.append(f"{len(items)} recorded moving object(s) the player stands on (uid {uids}) fit GD to "
                     f"{max(a['median_at_best'] for a in items):.4f} px when the recording is read {reach}{d:+d} ticks away and miss "
                     f"by up to {worst0:.2f} px at the tick itself: it looks like another run's motion of the same objects; "
                     f"use the recording made with this dump")
    return notes


def read_obb(path: Path) -> dict[int, tuple[float, float]]:
    """obb.txt: GD's own oriented box (getOrientedBox) for every object turned by something other than a quarter turn,
    as four corners (x0,y0 .. x3,y3). What the page needs is the box's own size, the two sides of the first corner:
    uid -> (side 0-1, side 1-2). Turning that box clockwise by the object's rot about its centre gives the corners back."""
    out: dict[int, tuple[float, float]] = {}
    with open(path, encoding="utf-8-sig", errors="replace") as f:
        col = _header(f.readline())
        for k in ("uid", "x0", "y0", "x1", "y1", "x2", "y2", "oob"):
            if k not in col:
                raise SystemExit(f"{path}: no '{k}' column; this is not an obb.txt")
        for line in f:
            p = line.rstrip("\r\n").split(",")
            if len(p) <= max(col.values()) or p[col["oob"]] != "1":
                continue
            c = [float(p[col[k]]) for k in ("x0", "y0", "x1", "y1", "x2", "y2")]
            out[int(p[col["uid"]])] = (round(((c[2] - c[0]) ** 2 + (c[3] - c[1]) ** 2) ** 0.5, 3),
                                       round(((c[4] - c[2]) ** 2 + (c[5] - c[3]) ** 2) ** 0.5, 3))
    return out


def read_objrects(path: Path, bbox: tuple[float, float, float, float], keep: set[int],
                  show_triggers: bool) -> tuple[list[list], dict[int, list], int]:
    """The boxes the physics reads (objrects.txt). Returns (objects inside the box, rows of the
    `keep` uids wherever they stand, how many of the drawn objects belong to a group).
    Row: [id, type, cx, cy, w, h, uid, radius, rot, sy0, sy1, sdir]."""
    x0, y0, x1, y1 = bbox
    objs: list[list] = []
    kept: dict[int, list] = {}
    grouped = 0          # of the objects drawn, how many belong to a group (so a trigger can move them)
    with open(path, encoding="utf-8-sig", errors="replace") as f:
        col = _header(f.readline())
        for k in ("id", "type", "cx", "cy", "w", "h", "uid"):
            if k not in col:
                raise SystemExit(f"{path}: no '{k}' column; this is not an objrects.txt")
        g = lambda p, k, d=0.0: _f(p[col[k]], d) if k in col and col[k] < len(p) else d
        for line in f:
            p = line.rstrip("\r\n").split(",")
            if len(p) <= col["uid"]:
                continue
            ty = int(g(p, "type", -1))
            uid = int(g(p, "uid", -1))
            if ty in HIDDEN_TYPES and not (show_triggers and ty == 20):
                if uid not in keep:
                    continue
            cx, cy, w, h = g(p, "cx"), g(p, "cy"), g(p, "w", 30.0), g(p, "h", 30.0)
            inside = cx + w / 2 >= x0 and cx - w / 2 <= x1 and cy + h / 2 >= y0 and cy - h / 2 <= y1
            if not inside and uid not in keep:
                continue
            row = [int(g(p, "id")), ty, _r(cx), _r(cy), _r(w), _r(h), uid, _r(g(p, "radius")),
                   _r(g(p, "rot")), _r(g(p, "sy0")), _r(g(p, "sy1")), int(g(p, "sdir"))]
            if uid in keep:
                kept[uid] = row
            if inside and uid not in keep:
                objs.append(row)
                if int(g(p, "groups")) > 0:
                    grouped += 1
    return objs, kept, grouped


# ------------------------------------------------------------------ assembling the page

def series_payload(label: str, kind: str, rows: dict[int, dict], t0: int, t1: int,
                   death: int | None = None) -> dict:
    keys = ("x", "y", "vy", "mode", "up", "og", "mini", "snap", "dual", "y2", "held", "frame")
    cols: dict[str, list] = {k: [] for k in keys}
    for t in range(t0, t1 + 1):
        r = rows.get(t)
        for k in keys:
            v = None if r is None else r.get(k)
            cols[k].append(_r(v, 4) if isinstance(v, float) else v)
    return {"label": label, "kind": kind, "death": death, **cols}


def build(args) -> dict:
    t0, t1 = args.t0, args.t1
    sources = []
    series = []
    notes: list[str] = []
    gd_rows: dict[int, dict] = {}
    death = None
    attempt = None
    if args.gd:
        attempt, gd_rows, death = read_gd(Path(args.gd), t0, t1, args.attempt)
        series.append(series_payload("GD", "gd", gd_rows, t0, t1, death))
        sources.append(source("gd", f"GD dump.csv, attempt {attempt}", Path(args.gd)))
    models: list[tuple[str, dict[int, dict]]] = []
    for spec in args.model or []:
        label, eq, p = spec.partition("=")
        if not eq:
            label, p = Path(spec).parent.name or Path(spec).stem, spec
        rows = read_model(Path(p), t0, t1)
        if not rows:
            raise SystemExit(f"{p}: no rows in ticks {t0}..{t1}")
        reversed_rows = infer_reversed_frames(rows)
        if reversed_rows:
            notes.append(f"{label}: {reversed_rows} row(s) in a reversed run are read as gameplay frame 2 "
                         f"(the model's trace has no column for State::rev)")
        models.append((label, rows))
        series.append(series_payload(label, "model", rows, t0, t1))
        sources.append(source("model", label, Path(p)))
    if not series:
        raise SystemExit("nothing to show: give --gd and/or --model")

    xs = [r["x"] for s in ([gd_rows] + [m[1] for m in models]) for r in s.values() if r["x"] is not None]
    ys = [v for s in ([gd_rows] + [m[1] for m in models]) for r in s.values() for v in (r["y"], r.get("y2"))
          if v is not None and -1e5 < v < 1e5]
    bbox = (min(xs) - args.margin_x, min(ys) - args.margin_y, max(xs) + args.margin_x, max(ys) + args.margin_y)

    movers: dict[int, list] = {}
    init_off: set[int] = set()
    gfiles = [args.groups] if isinstance(args.groups, str) else list(args.groups or [])
    if gfiles:
        movers, init_off, gnotes = read_groups(gfiles, t0, t1, bbox)
        notes.extend(gnotes)
        for k, g in enumerate(gfiles):
            sources.append(source("groups", f"recorded moving geometry" + (f" (layer {k + 1} of {len(gfiles)})" if len(gfiles) > 1 else ""), Path(g)))
    # the objects the players stand on are drawn even when they sit outside the box for a moment
    snaps = {r["snap"] for s in ([gd_rows] + [m[1] for m in models]) for r in s.values() if r.get("snap", -1) >= 0}
    objects, mover_base, grouped = read_objrects(Path(args.objrects), bbox, set(movers), args.show_triggers)
    sources.append(source("objrects", "level geometry", Path(args.objrects)))
    if not gfiles and grouped:
        notes.append(f"{grouped} object(s) drawn here belong to a group, so a trigger can move them, and no --groups recording "
                     f"was given: they are drawn where the level loads them, whatever the run did to them")
    if getattr(args, "obb", None):
        # a turned object's own box size (GD's oriented box): columns 12 and 13 of its row
        obb = read_obb(Path(args.obb))
        sources.append(source("obb", "oriented boxes of turned objects", Path(args.obb)))
        for row in list(objects) + list(mover_base.values()):
            row += list(obb[row[6]]) if row[6] in obb else [None, None]
    if gd_rows and movers:
        notes.extend(alignment_notes(mover_alignment(gd_rows, movers, t1)))
    placed = []
    for uid, rows in movers.items():
        if uid in mover_base:
            placed.append({"base": mover_base[uid], "rows": rows, "off0": uid in init_off})

    summary = {}
    for label, rows in models:
        if gd_rows:
            summary[label] = divergence(gd_rows, rows, t0, t1, args.tol)
    plan = read_plan(Path(args.plan)) if args.plan else []
    if args.plan:
        sources.append(source("plan", "input plan", Path(args.plan)))
    # A short window opens on the first tick a model parts at, slowly and close; a long one (a whole level)
    # opens at its first tick, at real time and further out, to be watched through.
    long_run = (t1 - t0 + 1) > LONG_RUN_TICKS
    firsts = [s["first"] for s in summary.values() if s["first"] is not None]
    start = args.start if args.start is not None else (t0 if long_run or not firsts else max(t0, min(firsts) - 6))
    return {
        "meta": {"title": args.title or "divergence view", "t0": t0, "t1": t1, "tol": args.tol,
                 "attempt": attempt, "direction": "model - GD", "bbox": [_r(v) for v in bbox],
                 "snap_uids": sorted(snaps), "modes": MODES, "notes": notes, "start": start,
                 "speed": args.speed if args.speed else (1.0 if long_run else 0.1),
                 "scale": 1.6 if long_run else 2.4},
        "sources": sources, "objects": objects, "movers": placed, "series": series, "plan": plan,
        "summary": summary,
    }


def render(payload: dict) -> str:
    html = TEMPLATE.read_text(encoding="utf-8")
    data = json.dumps(payload, separators=(",", ":")).replace("</", "<\\/")
    return html.replace("__TITLE__", escape(payload["meta"]["title"])).replace("__DATA_JSON__", data)


def print_summary(payload: dict) -> None:
    m = payload["meta"]
    print(f"ticks {m['t0']}..{m['t1']}   tolerance {m['tol']}   differences are {m['direction']}"
          f"   GD attempt {m['attempt']}")
    print(f"objects {len(payload['objects'])}   movers {len(payload['movers'])}")
    for n in m.get("notes", []):
        print(f"  note: {n}")
    for label, s in payload["summary"].items():
        if s["first"] is None:
            line = f"  {label}: agrees with GD on all {s['compared']} compared ticks"
        else:
            line = f"  {label}: first part at tick {s['first']} ({s['reason']})"
        line += f"   max |dx| {s['max_abs_dx']}  max |dy| {s['max_abs_dy']}  max |dvy| {s['max_abs_dvy']}"
        if s["model_ends_early"]:
            line += f"   MODEL ENDS at tick {s['model_last']} while GD continues"
        if s["gap_count"]:
            line += f"   GD lacks {s['gap_count']} tick(s) inside the model's span"
        print(line)


class _Quiet(SimpleHTTPRequestHandler):
    def log_message(self, *args):          # a page load per tick of the scrubber is not news
        pass


def make_server(directory: Path, port: int) -> ThreadingHTTPServer:
    """The folder over http on the loopback address only (nothing else on the machine can reach it)."""
    handler = functools.partial(_Quiet, directory=str(directory))
    return ThreadingHTTPServer(("127.0.0.1", port), handler)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    ap.add_argument("--objrects", required=True, help="objrects.txt (the level's boxes)")
    ap.add_argument("--gd", help="GD's dump.csv (every attempt; one is shown)")
    ap.add_argument("--attempt", type=int, default=None,
                    help="which attempt of the dump (default: the one with the most live rows in the window)")
    ap.add_argument("--model", action="append", metavar="[LABEL=]TRACE.csv",
                    help="a leveldp trace.csv; repeat for several (flag on / flag off, say)")
    ap.add_argument("--groups", action="append",
                    help="dp_groups.txt, the recorded moving geometry (without it the objects stay where objrects.txt "
                         "puts them). Repeat to lay recordings one over the other as the model does: a later file "
                         "wins for the ticks it covers. Use the recording of the SAME run as the GD dump")
    ap.add_argument("--obb", help="obb.txt, GD's oriented boxes: with it a hazard turned by other than a quarter turn is "
                                  "drawn as its box turned (without it, as the bound objrects.txt records)")
    ap.add_argument("--plan", help="the plan (input=T,V lines), drawn as a strip of presses")
    ap.add_argument("--t0", type=int, required=True)
    ap.add_argument("--t1", type=int, required=True)
    ap.add_argument("--tol", type=float, default=0.05, help="the difference counted as parted (default 0.05)")
    ap.add_argument("--margin-x", type=float, default=500.0)
    ap.add_argument("--margin-y", type=float, default=300.0)
    ap.add_argument("--show-triggers", action="store_true", help="also draw triggers (type 20) as dots")
    ap.add_argument("--start", type=int, default=None,
                    help="the tick the page opens on (default: 6 ticks before the first part in a short window, "
                         f"the first tick in one longer than {LONG_RUN_TICKS})")
    ap.add_argument("--speed", type=float, default=None,
                    help="playback speed, 1 = real time at 240 ticks a second (default 0.1, or 1 in a long window)")
    ap.add_argument("--title", default="")
    ap.add_argument("--out", required=True)
    ap.add_argument("--serve", action="store_true",
                    help="then serve the page's folder on http://127.0.0.1:PORT until Ctrl+C. A viewer that shows "
                         "local files as static snapshots (the Claude desktop app's built-in browser does) runs "
                         "the page's script only when it is opened over http")
    ap.add_argument("--port", type=int, default=8765, help="with --serve (0 = any free port)")
    a = ap.parse_args(argv)
    if a.t1 < a.t0:
        ap.error("--t1 is before --t0")
    payload = build(a)
    out = Path(a.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(render(payload), encoding="utf-8")
    print_summary(payload)
    print(f"wrote {out} ({out.stat().st_size / 1e6:.2f} MB)")
    if a.serve:
        srv = make_server(out.parent, a.port)
        print(f"serving {out.parent} at http://127.0.0.1:{srv.server_address[1]}/{out.name}   (Ctrl+C stops)", flush=True)
        try:
            srv.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            srv.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
