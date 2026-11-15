# WildPitch — guide for Claude sessions

A native C17 + SDL3 football game where players *learn* from drills you design, in a procedurally
generated world. Long-term project built in small, playtested checkpoints with the owner.

## Start here (every new session)
1. Read `docs/STATUS.md` — current checkpoint, what works, **exact next step**, known issues.
2. Skim `docs/ROADMAP.md` (where we're going) and `docs/DECISIONS.md` (what's settled — don't re-litigate).
3. Read `docs/PLAYTEST_FEEDBACK.md` for the owner's latest feedback.
4. Deeper context when needed: `docs/ARCHITECTURE.md` (code map + rules), `docs/DESIGN.md` (vision snapshot;
   online original: https://claude.ai/artifact/DeVni416rCHCCboZqG8B4J).

## Build / run / test
```sh
tools/fetch_deps.sh                       # once: fetches pinned SDL3 into third_party/SDL (git-ignored)
cmake -S . -B build -G Ninja && cmake --build build
./build/wildpitch                         # play (F3 = debug overlay, Esc = pause menu: Controls rebinds keys)
./build/wildpitch --screenshot out.bmp    # visual check: renders --frames N (default 60), saves BMP, exits
./build/wildpitch --demo                  # 8 scripted runners (avatar motion check); --gallery = pose sheet
                                          # --style 0|1|2 (Modern/Arcade/Retro), --zoom 0|1|2; P/Z keys in game
./build/wildpitch --autoplay              # scripted striker dribbles + shoots (stderr logs kick/goal ticks)
./build/wildpitch --check-kick-pose       # numeric avatar/sim contact check (also runs in ctest)
ctest --test-dir build --output-on-failure   # all headless tests
tools/check_determinism.sh                # gcc+clang × -O0/-O2 must give identical golden hash
./build/bench_sim                         # sim speed (ticks/s); note it in STATUS when it changes a lot
```
Golden hash changes are only OK when sim behaviour changed on purpose:
`WP_UPDATE_GOLDEN=1 ./build/test_determinism`, and say so in the commit message.

## Hard rules
- **`src/sim/` is deterministic:** no SDL, no float/double at runtime, no libm, no malloc, no pointers in
  `SimState`, RNG only from `SimState.rng`. Use `fx` (Q16.16) from `sim/fixed.h`. `FX()` only on literals.
- **Never put two side-effecting calls (RNG draws, etc.) in one expression or argument list** — evaluation
  order is unspecified and gcc/clang differ (this bit us in CP0.0). One RNG draw per statement.
- **Adding a field to SimState/WpPlayer/WpBall?** Hash it in `sim_hash()` and update
  `WP_SIMSTATE_SIZE_EXPECTED` in `sim.c` (a `_Static_assert` enforces this).
- **Presentation never writes to the sim.** Rendering/audio read `SimState`; input enters only via `WpInput`.
- **AI = candidate actions × utility scores** (DECISIONS D-008) — this is where learned brains plug in later.
- **Runs cool:** the frame loop must never busy-wait; menus redraw only on change. Measure, don't guess.
- **Dedication (D-018):** "In the memory of my brother Shubham Thakkar ❤️" — the owner's exact words. Keep the
  launch card, README line and (later) title/credits line; never edit the wording, never remove it.
- No new third-party dependencies without asking the owner. No licensed names/likenesses; football only.
- Code is C17, warnings-clean (`-Wall -Wextra -Wconversion -Wshadow`).
- **Keep it modular without costing efficiency** (owner request): one responsibility per file (see
  ARCHITECTURE.md code map), private cross-module interfaces in `*_internal.h`, all gameplay tunables in
  `src/sim/tuning.h`, no per-tick allocation, data in flat arrays. Prefer a new small module over growing a big one.

## Working agreement with the owner
- Incremental checkpoints (CP). Each CP ends with: tests passing, determinism check passing, a screenshot
  sanity check, `docs/STATUS.md` updated with a **playtest guide**, and a local git commit. Then stop and ask
  the owner to playtest. Log their feedback in `docs/PLAYTEST_FEEDBACK.md` and re-plan the next CP.
- Update `docs/STATUS.md` after every meaningful unit of work (not just at the end) — sessions can die.
- Record any decision with lasting impact in `docs/DECISIONS.md`.
- Commit locally at checkpoints / meaningful units (message ends with the Co-Authored-By line); never push unless asked.
- Subagents (Opus) are welcome for independent work; give them this file + the relevant docs.
- Ask the owner when a choice is genuinely theirs (feel, scope, naming, art); otherwise pick a sane default and note it.
