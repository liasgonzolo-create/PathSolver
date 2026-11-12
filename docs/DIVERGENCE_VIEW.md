# Divergence view

A picture of where the model parts from GD, made from files already on disk — **no game
running**. `py/divergence_view.py` writes one self-contained HTML page (no server, no library,
nothing fetched): open it in any browser.

```bash
python py/divergence_view.py --objrects objrects.txt --gd dump.csv \
    --model model.trace.csv --t0 6074 --t1 6165 --out view.html
```

## Opening it

Double-click the file, or open it in any browser. **A viewer that shows local files as static
snapshots will show the page but not run it** — some built-in browsers of editors and apps do this
for a file on disk (nothing moves, and the keys and buttons do nothing). Add `--serve` to serve the
page's folder over `http://127.0.0.1:8765/` (this machine only) and open that address instead; the
script then runs there. `--port 0` takes any free port, Ctrl+C stops it.

```bash
python py/divergence_view.py ... --out view.html --serve
```

## Watching a whole level

Give it the whole run instead of a window: `--t0 1 --t1 <the last tick>`, a GD `dump.csv` (or the
`data/gdref/lv<N>.csv` reference) and the model's trace of the same plan from tick 0. A window longer
than 1,500 ticks is treated as a run to watch: it opens on its first tick, at real time (1x is 240
ticks a second), with the camera further out; a shorter one opens slowly on the first tick a model
parts at. `--start` and `--speed` override both, and the page's speed menu goes from 0.02x to 8x.
The side panel shows x against the furthest x any worldline reaches, and the time in seconds.

```bash
python py/divergence_view.py --objrects data/objrects_lv14.txt --gd data/gdref/lv14.csv \
    --model "model=lv14_whole.trace.csv" --plan data/solution_lv14_dp.txt \
    --t0 1 --t1 20802 --out lv14.html --serve
```

The model's whole-run trace is `leveldp <objrects> --replay <plan> --out lv14_whole` with the level's
side files (`--triggers` / `--objgroups` / `--obb` / `--groups`, whichever the level has);
`py/fidelity_diff.py`'s `model_replay(..., whole_run=True)` assembles exactly that and leaves the
argv beside the trace. A trace that ends before GD's does is marked on the page (`MODEL ENDS`).

A whole level is a few MB of page (lv14 with 3,655 objects and 20,802 ticks of two worldlines: 2 MB)
and plays at well over 100 frames a second on a laptop: the timeline is drawn once and the cursor
over it, and only the stretch of path near the playhead is stroked.

## What is on the page

* **The level as the physics sees it**, from `objrects.txt`: solids, hazards (boxes, or circles for
  saws), portals, pads, rings, slopes as the surface line GD sampled with the solid side filled.
  Decorations and triggers are left out (`--show-triggers` draws the triggers as dots).
* **The moving geometry the run recorded** (`--groups dp_groups.txt`): each object holds its last
  recorded row until the next one, exactly as the model reads that file, and is drawn where it was
  at the tick on screen. An object the recording switches off is an outline. Without `--groups` the
  objects stay where `objrects.txt` puts them (the page says how many drawn objects belong to a
  group and so could have been moved). See *Moving objects* below for what to give it.
* **GD's player** from a `dump.csv` and **each model's** from a `trace.csv` (`--model`, repeated:
  flag on and flag off against the same GD run, say). The thin box is the hitbox the model uses, GD
  the largest, the models inside it, so boxes that coincide are all still visible. On it a plain
  silhouette says the mode — cube a square with a face, ship a pointed hull with a dome, ball a
  circle with a spoke that turns as it rolls, ufo a saucer, wave an arrowhead, robot a body with a
  visor and two feet, spider a body with four legs, swing a round body with a wing bar — flipped
  when gravity is, and the ship and the wave tilt the way they move. They are not the game's art,
  only enough to tell the modes apart. A dashed box is the object that side says it stands on (GD's
  `snapuid`, the model's `clampuid`); a line joins the two players and goes red when they are
  further apart than the tolerance.
* **A timeline**: `y(t)` of every worldline, then `|dy|` (solid) and `|dvy|` (dashed) of each
  model against GD on a log scale with the tolerance drawn as a line, then the button (from
  `--plan`, or the model's `held` column). A dashed vertical line marks where each model first
  parts; GD's death and a model trace's end are marked too. Click or drag to scrub.
* **The numbers at the tick** (x, y, vy, mode, on-ground, held, the snapped object) for every
  worldline and, for each model, its difference from GD with the cells beyond the tolerance in red.
* **The inputs**: every file with its size and sha256, so a picture can be tied to what made it.

Keys: `space` play/pause, `←` `→` one tick (`shift` ten), `Home` `End`, `D` the first tick where a
model parts, `F` follow GD, `[` `]` speed, wheel zoom, drag pan, double-click to reset the view.
Hovering an object says what it is (id, type, uid, size, whether the recording moves it).

## What it will not do silently

The habits of `gdtas/compare.py`, because a picture is the easiest place to lose them:

* Every difference is **model − GD**, said in the header, in the table and on stdout.
* Worldlines are compared at the **same tick** only. A tick one side lacks is a gap in the plot
  and a count in the summary (`GD lacks N tick(s) inside the model's span`), never an intersection
  taken quietly. A model that starts late (an anchored section) is not called early-ending; one
  whose trace stops while GD is still alive says so (`MODEL ENDS at tick N`): that tick is the
  model's death, or the end of the window it was asked for.
* A `dump.csv` holds every attempt. One is shown: `--attempt N`, or by default the one with the
  most live rows in the window. The page says which. A row with `dead` set ends GD's series — the
  corpse keeps being ticked and its y runs away.
* "Parted" means |dy| or |dvy| beyond `--tol` (default 0.05), or the modes differ, or the gameplay
  rotation differs (`frame`: the rotation is part of the state, and it can turn a tick before y
  moves), or |dx| is beyond the tolerance; checked in that order, so the reason named is the first
  that failed. `x` is last on purpose: a rotation the model lacks moves x as well, and the
  rotation is the cause. It is in the rule at all because x can leap alone (a teleport, a clamp, a
  push the model missed) with every other column equal, and that has to stop the `D` key. The
  tolerance can be changed on the page.

## Rotation

Two different things turn in a level like an official one with a Rotate Gameplay trigger.

**The gameplay.** The world does not turn; the player's axes do. GD's dump says which way with
`gframe` (the model's trace: `frame`): the player runs +x in frame 0, −y in 1, −x in 2, +y in 3
(measured on a rotating level's reference run), and `vy` is its speed along the other axis (+y, +x,
+y, +x). The page turns each silhouette, the `vy` line and the on-ground bar with the frame, shows
`rotN` beside the mode, and marks the rotated stretches with a coloured band over the button strip
of the timeline. The positions need no turning: `x` and `y` are world coordinates in both files.

The model runs a half turn (frame 2) as a *reversed* run in frame 0 (`State::rev`), and its trace has
no column for that, so a trace says frame 0 where GD says 2. A frame-0 row whose `x` runs backwards
is therefore read as frame 2 (and the page says how many rows it read that way). The frames 1 and 3
are taken as the trace gives them. This inference is checked against GD's own dump: the first tick
of a half turn does not move, the reversed steps start on the next, and the one after the last
frame-2 tick still moves back, which is the layout the rule reads.

**Objects.** A hazard (or a block) turned by something other than a quarter turn is drawn as its own
box, turned clockwise by its `rot` about its centre, not as the axis-aligned bound `objrects.txt`
records. The box's own size comes from GD's oriented box when `--obb obb.txt` is given (exact: the
corners come back to within the file's rounding), else from the bound undone
(`a = (W|cos| − H|sin|) / cos 2θ`; checked to 0.05 on every turned hazard of an official level that
is not near 45°). Near 45° that cannot be undone and the object stays its bound. Recorded movers
(`--groups`) are turned the same way at the angle each row says. Saws and rings are round and need
no turning.

## Moving objects

The page shows an object where a recording says it was; it cannot know where GD really had it. Three things follow.

* **Give it the recording made with the dump.** A trigger-driven motion starts when the player trips it, so two runs of one
  level move the same objects through the same path at different ticks. On an official level whose recordings came from other
  runs than the reference dump, every moving block the player stood on matched GD to 0.0002 px, but only when the recording was
  read a consistent number of ticks away (−19 for one group of blocks, +3 or +4 for another, −22 for a third; 0 for one that
  happened to line up) -- and missed by 1–3.5 px at the tick itself. On a custom level where the recording and the dump came
  from the same run, the one block the player stood on matched at the tick itself (median 0.000 px, 93% within 0.05, worst 0.29).
  The page runs this check itself: for each recorded mover a cube stands on, the player's height must be the mover's top (or
  underside) plus the cube's half. It says so when a recording fits GD only at another tick, and gives the shift.
* **Several recordings are laid as the model lays them**: repeat `--groups`; a later file wins, for each object it has rows
  for, up to the last tick it speaks for (its last row, or its `end,<tick>` trailer), and the earlier file supplies the rest.
  The model also re-times the earlier file where they meet; that is not done here, so past the last file's coverage the
  earlier file's objects are approximate, and the page says so. Past every file's coverage the objects hold their last row,
  and the page says they are frozen.
* **Only recorded movers move.** Without a recording every object stands at its load position.

Per tick the rule is the one the model uses: the row at the tick or the last one before it.

## Reading a model trace beside GD's dump

The two files are keyed by the same tick. The model's `trace.csv` is what `leveldp --out X` writes
(`X.trace.csv`); `py/quick_regress.py --tmp DIR` leaves one per section. GD's side is a worker's
`dump.csv` or the reference in `data/gdref/lv<N>.csv`. Nothing keeps a worker's dump for you: copy it
somewhere before the next run on that worker overwrites it.

## Limits

* The picture is of the **boxes**, not the artwork. The camera and zoom are not drawn. A solid turned
  by other than a quarter turn, a portal or a pad is drawn as its bound; the partner of a dual player
  is placed for an unrotated run only (no official level has one in a rotated frame).
* A level with a great many objects makes a bigger page; the generator keeps only what lies within
  `--margin-x` / `--margin-y` (500 / 300) of the worldlines it was given, plus the recorded movers
  that come into that box during the window.
* The objects a plan would use are the objects in `objrects.txt`; nothing here runs the model. To
  see a different plan, replay it first and give the trace.

Tests: `python py/test_divergence_view.py` (hand-built files; also parses the page's script when
`node` is installed).
