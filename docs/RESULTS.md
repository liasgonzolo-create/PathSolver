# Results

The tables are cold regression runs of the v0.5.0 build (2026-10-11) —
`python py/cold_regress.py --one-session`, the whole suite inside a single game
session, no plan and no solution file to start from — one with coins
(`--cfg coinroute=1 coins=1`) and one for the end of the level alone. The coin
runs ended every level with every coin, as the game itself counted them. The
repairs are those of the runs the regression baseline was taken from; the times
are those of another one-session run of the same build, made the same day, whose
repairs were the same on every level.

Every cell reads **with every coin**, then in brackets **the end of the level
alone**. A repair cell splits into **divergence + search** (see below).

| # | Level | Repairs | Time | | # | Level | Repairs | Time |
|--:|---|--:|--:|---|--:|---|--:|--:|
| 1 | Stereo Madness | 0 (0) | 7 s (5 s) | | 12 | Theory of Everything | 0 (0) | 7 s (7 s) |
| 2 | Back On Track | 0 (0) | 4 s (4 s) | | 13 | Electroman Adventures | 0 (0) | 6 s (6 s) |
| 3 | Polargeist | 0 (0) | 5 s (5 s) | | 14 | Clubstep | 0 (0) | 7 s (6 s) |
| 4 | Dry Out | 0 (0) | 4 s (4 s) | | 15 | Electrodynamix | 0 (0) | 7 s (7 s) |
| 5 | Base After Base | 0 (0) | 5 s (5 s) | | 16 | Hexagon Force | 3+1 (1+4) | 52 s (56 s) |
| 6 | Can't Let Go | 0 (0) | 5 s (4 s) | | 17 | Blast Processing | 0+1 (0+1) | 10 s (8 s) |
| 7 | Jumper | 0 (0) | 4 s (5 s) | | 18 | Theory of Everything 2 | 1+1 (1+1) | 15 s (13 s) |
| 8 | Time Machine | 0 (0) | 6 s (5 s) | | 19 | Geometrical Dominator | 1+0 (1+2) | 23 s (31 s) |
| 9 | Cycles | 0 (0) | 5 s (5 s) | | 20 | Deadlocked | 0+10 (0+4) | 2 m 7 s (39 s) |
| 10 | xStep | 0 (0) | 6 s (6 s) | | 21 | Fingerdash | 2+4 (1+1) | 56 s (31 s) |
| 11 | Clutterfunk | 0 (0) | 35 s (6 s) | | 22 | Dash | 4+32 (2+16) | 8 m 5 s (5 m 22 s) |

The two numbers in a cell are two separate searches, not a total and a part:
with coins the goal is a different one, so the search takes a different route
from the first round on, and it is not always the longer one.

Against v0.4.0's one-session runs, the 22 levels went from 54 to 35 repairs and
780 to 580 s without coins, and from 70 to 60 repairs and 1,057 to 881 s with
them. The large moves: Deadlocked 21 → 10 and Fingerdash 14 → 6 with coins;
Geometrical Dominator 8 → 3, Fingerdash 8 → 2 and Dash 23 → 18 without them.
Against the run of the trend, Hexagon Force went 1 → 4 with coins and 1 → 5
without, and Dash with coins 27 → 36. The [changelog](../changelog.md) has what
changed.

### The spin-off levels

The official levels of Meltdown, World and SubZero, and The Challenge, run the
same way, one game session per set. World's levels and The Challenge have no
coins, so they have one number. These levels are not in the main game's level
list; the suite reads them from level files (cfg `leveldir=<dir>`, one
`<id>.lvl` per level), which are not distributed here.

| Game | # | Level | Repairs | Time |
|---|--:|---|--:|--:|
| Meltdown | 1001 | The Seven Seas | 0+2 (0+2) | 12 s (13 s) |
| Meltdown | 1002 | Viking Arena | 0+1 (0+1) | 11 s (10 s) |
| Meltdown | 1003 | Airborne Robots | 0+1 (0+1) | 13 s (15 s) |
| World | 2001 | Payload | 0 | 3 s |
| World | 2002 | Beast Mode | 0 | 4 s |
| World | 2003 | Machina | 0 | 3 s |
| World | 2004 | Years | 0 | 6 s |
| World | 2005 | Frontlines | 0 | 6 s |
| World | 2006 | Space Pirates | 0 | 7 s |
| World | 2007 | Striker | 0 | 3 s |
| World | 2008 | Embers | 0+3 | 5 s |
| World | 2009 | Round 1 | 0 | 4 s |
| World | 2010 | Monster Dance Off | 0 | 5 s |
| — | 3001 | The Challenge | 0+1 | 16 s |
| SubZero | 4001 | Press Start | 1+6 (0+2) | 1 m 49 s (30 s) |
| SubZero | 4002 | Nock Em | 5+21 (3+8) | 7 m 18 s (2 m 10 s) |
| SubZero | 4003 | Power Trip | 3+24 (1+23) | 2 m 3 s (1 m 58 s) |

Against v0.4.0, the 17 levels without coins went from 80 to 45 repairs and 408 to
378 s, and the six with coins from 112 to 64 repairs and 792 to 706 s: Nock Em
63 → 26 with coins and 20 → 11 without, The Challenge 8 → 1, Press Start 10 → 2
without coins, and Power Trip 37 → 27 with them.

**Repairs** is how many times the loop had to go back: solve, replay, die,
re-anchor on the game's real state, solve the tail. `0` means the very first
plan the DP produced cleared the level. Each one is split by what the loop's
recorder found when it replayed the dead plan in the model against what the game
did:

* **divergence** — the model and the game had come apart before the death: the
  first difference the recorder found in position or velocity is over 0.01. The
  model is wrong there, and the repair is the game correcting it.
* **search** — the two agreed up to the death. The model, given what the game
  showed, dies there too; the plan was wrong, not the model — typically a route
  that only looked open because the moving geometry past the last replay was not
  yet recorded, or a plan that dies in the model's own walk of it.

On the official levels 29 of the 35 repairs without coins and 49 of the 60 with
them are search; on the spin-off levels 41 of 45, and 55 of 64 with coins. For a
given build the count is normally reproducible: `py/cold_regress.py` prints
each level's count against `data/cold_baseline.json`
(`data/cold_baseline_coins.json` for a coin run), and a change is reported, not
failed. The `[fp]` line each round prints is compared with the baseline run's
separately when a release is accepted. The spin-off levels have baselines of
their own, kept with their level files.

It is not a fidelity score. It counts what the loop had to do, and that depends
on which corridor the search happens to walk as much as on where the model is
wrong — the search is deepest-first, so touching one rule reorders the frontier
and the run takes a different route. Making the model *more* correct can raise
it: closing a route the model only believed in sends the search off to find the
real one.

**Time** comes from the second runs — 8 solver threads on a 16-core desktop — and
it counts everything from the level being built to the solution being written
(the game's own start-up is not in it); the 22 add up to 15 minutes with coins and
10 without. The second runs of the 22 and of the spin-off levels without coins ran
side by side, three games at once; the spin-off coin times are from the run the
baseline was taken from, which shared the machine with one other game. Read the
clock as a guide and not as a contract: it is not what the regression compares,
and a level's time is read from outside the game about once a second.

Almost all of it is the search. Broken down on a run before the section solver
joined the loop, the DP calls were 86 % of the total and 88–93 % on the four
expensive levels; on the ones that finish inside half a minute, most of what is
left is starting the game and loading the level. On a level where section solves
run (Dash with coins, SubZero) a good part of the time is spent searching inside
the game instead.
