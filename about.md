# Path Solver (Geode mod, GD 2.2081 / Geode v5)

Solves a classic-mode level into a macro on its own, removes the clicks it
doesn't need, and shows a live progress map while it works.

## Use

1. Open a level, pause.
2. **Solve** (bottom-left of the pause menu). The mod switches to practice
   mode and starts. Watch the map; nothing else to do.
3. When it finishes, the macro is saved and starts playing.
4. Pause again for **Play macro / Stop macro / Re-solve / Stop solver**.

The macro is reloaded automatically next time you open the level. It is
saved as `<Geode save dir>/pathsolver.path-solver/macros/<id>-<name>.gdr.json`.

## What the map shows

- green = blocks, red = hazards, yellow = orbs, cyan = pads, purple = portals
- blue line = planned path with the button up, orange = button held,
  yellow dot = a click
- white line = the try being simulated right now, red x = recent deaths
- bottom bar = whole level: blue is planned, green is verified from the
  start, red ticks above it show where the solver keeps dying

## How it solves

The real game is the simulator, so every game mode, portal, orb and trigger
behaves exactly as it does when you play.

1. **Search** – replay the plan; on death, change it shortly before the death
   (add a press, shorten one, or clear what follows), rewind to a practice
   checkpoint, try again. A change is kept only if the player gets further.
   The look-back distance grows the longer it is stuck.
2. **Verify** – replay the whole plan from tick 0 without rewinding. If it
   dies, go back to step 1 from there.
3. **Remove extra clicks** – for every click, last to first: try deleting it,
   then try merging it with the next one. Keep each change that survives.
4. **Verify** again, save.

Clicks are never shorter than "Shortest click" and never closer together
than "Shortest gap" (mod settings, 25 ms each by default).

## Limits

- Classic levels only (no platformer). One button, player 1; two-player
  levels that need separate P2 input are not handled.
- It finds a path that works and trims it. It does not prove that path is
  the shortest or lowest-click one possible.
- Levels with random triggers may need several repair rounds or fail.
- Trigger-heavy levels use a lot of memory for checkpoints: raise
  "Checkpoint spacing" if the game slows down.
- Runs played by the macro are completed in test mode, so they are not saved
  as real completions.

## Build

Push this folder to a GitHub repo: the included workflow builds the `.geode`
file (Actions tab -> latest run -> Build Output). Or locally with the Geode
CLI: `geode build`.

**This code has not been compiled yet.** Everything that touches a game
member that might be named differently in your bindings version is in
`src/compat.hpp`; a build error will almost certainly point at a line there
or at a hook signature in `src/main.cpp`.
