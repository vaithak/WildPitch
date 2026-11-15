# WildPitch — Roadmap

**How this roadmap works**
- Work is split into **checkpoints (CP)**. Each CP ends with something the owner can *play*, a
  **playtest guide** ("try this, judge that"), passing automated tests, and a local git commit.
- After each CP the owner playtests and gives feedback → logged in `docs/PLAYTEST_FEEDBACK.md` →
  the **next** CP is re-planned. Only the next 1–2 CPs are detailed; later ones are deliberately coarse.
- Phases and gates come from the owner's design doc (`docs/DESIGN.md`). Gates are not skipped.
- Detail levels: 🔵 detailed (committed scope) · 🟡 sketched (likely to change) · ⚪ coarse (direction only).

Status legend: ✅ done · 🚧 in progress · ⏭ next · ⬜ later

---

## Phase 0 — FEEL  (gate: a stranger enjoys a 5-minute 5v5 kickabout; laptop stays cool)

### CP0.0 — Foundations ✅ 🔵
Not playable; invisible plumbing every later CP depends on.
- CMake + vendored static SDL3 (3.4.8), `tools/fetch_deps.sh`.
- `src/sim/` deterministic core: Q16.16 fixed-point maths, integer sqrt, trig tables, seeded RNG,
  `SimState` (plain struct, no pointers), `SimInput` per tick, `sim_step()`. No SDL/floats/libm.
- Test harness + golden-hash determinism test (gcc & clang, -O0 & -O2); headless benchmark
  (`bench_sim`: ticks/second).

### CP0.0b — Human avatars ✅ 🔵  (from owner feedback on CP0.0)
- Procedural skeleton footballers (D-009): run cycle, kick, jump-header, keeper stance; seeded skin/hair/height/build.
- Presets to choose by eye: Modern / Arcade / Retro pixel (P key); zoom presets (Z key).
- Sim: player jump state (`z`, `vz`, `action`, `action_timer`) for headers (D-010).
- `--demo` (8 scripted runners), `--gallery` (pose sheet), `--style`, `--zoom`, `--frames` flags.

### CP0.1 — "Ball on grass" ✅ 🔵
**Playable:** one player you control on a tilted 2.5D pitch with goals, and a ball that feels right.
- Window, 60 Hz fixed-step loop with interpolated rendering, vsync + sleep (no spinning).
- Tilted tabletop camera that follows play; pitch markings, goals with nets, shadows.
- Ball physics in 3D (x, y, height): gravity, bounce with energy loss, rolling friction, air drag,
  spin/curl hook (basic).
- Kicks as **parametric actions** (D-012): foot, contact band, swing direction, power, spin + tick timings shared
  by sim and avatar pose.
- Controlled player: accelerate/turn with momentum, **dribble** (ball kept at feet with small touches),
  **sprint**, **shoot** (hold to charge power; release), **chip** (modifier).
- Goal detection (ball fully over line, under bar) → score + reset to kickoff.
- Ball contact in **height bands** (D-010) from day one: feet/body/head; a ball overhead can't be trapped.
- Turn rate in the sim (no instant facing snaps). *(Dribble-touch flick done in CP0.1b.)*
- Debug overlay (F3): fps, sim ms/tick, ball height/speed.
- Cap presentation at 60 fps even on 120/144 Hz monitors (thermal budget); camera easing time-based.
  *(Done via vsync interval; measured 61.9 fps on a 60 Hz display; 120/144 Hz untested — owner please report.)*
- **Playtest:** does dribbling feel responsive? Do shots/chips feel weighty? Is the camera/tilt
  readable? Does the laptop stay quiet?

### CP0.1b — Dribble feel fix ✅ 🔵  (owner feedback on CP0.1)
- Rhythmic predictive touches (D-014), no speed flicker, flick animation, steady-state dribble tests.

### CP0.1c — Rolling friction ✅ 🔵  (owner feedback on CP0.1b)
- Speed-dependent ground friction (D-015): 12 m/s pass rolls 18 m instead of 55 m; roll-distance tests.

### CP0.1d — Ball looks physical ✅ 🔵  (owner: "ball physics still looks a bit weird" → research report)
- Impulse grass bounce, no-magnet possession (step-up + sole trap, capped gather, contact quality), rolling
  panels/blur/streak/shadow (D-016). Optional later: R4 topspin/skid-to-roll, R5 drag crisis (see report).

### CP0.2 — "Pass and move" ⏭ 🔵
**Playable:** your full 5-a-side team (4 + keeper) vs. a passive opposition (cones that move a bit).
- Pass: tap = short ground pass to the best teammate in the stick direction; hold = through ball
  into space (power by hold time). Lofted pass.
- Switch player (auto + manual), receiving/first touch, ball control errors at speed.
- Teammates hold simple formation shape and make basic runs (placeholder logic, replaced in CP0.3).
- **Headers playable:** lofted pass/cross → contextual jump-header (pass/shoot while the ball is in the air).
- HUD: score, clock, controlled-player marker, pass power bar.
- **Playtest:** do passes go where you expect? Is switching intuitive?

### CP0.3 — "Instinct AI" ⬜ 🟡
**Playable:** first real 5v5 match vs AI.
- **Instinct layer** as *candidate actions × utility scores* over an observation vector
  (this is the exact interface the learned brain plugs into in Phase 1 — not a state machine).
- Actions: hold shape, support, run into space, press, mark, intercept, tackle/shoulder challenge,
  pass/shoot/dribble decisions for AI on the ball.
- Goalkeepers: positioning, dives, catches/parries, distribution (dive/catch avatar poses).
- Aerial duels: AI jumps for headers; timing beats height.
- Simple stats per player (pace, control, passing, shooting, tackling, stamina) feeding utility.
- **Playtest:** do AI teammates feel helpful? Is the opponent beatable but not dumb?

### CP0.4 — "A real match" ⬜ 🟡
- Rules: out of play (kick-ins/throw-ins, corners with headed chances, goal kicks), basic fouls (late tackles) → free kicks,
  kickoff after goals, halves (~2×4–6 min configurable), half-time, full-time screen.
- Gamepad support (hotplug), remappable keys, pause menu.
- Stamina + sprint cost; the **shout** button (v1: "press!" / "drop back").
- Replay of last 10 s (free thanks to determinism: inputs log).

### CP0.5 — "Juice and cool" (Gate 1 candidate) ⬜ ⚪
- Audio (kicks, crowd murmur, whistle) — *needs `libasound2-dev`/`libpulse-dev` installed*.
- Visual juice: player animation frames, ball trail, net ripple, light post-process.
- Battery mode (30 fps), power/thermal reading, menus that redraw only on change; measure CPU%,
  power draw, temps → record in `docs/PERF.md`.
- Title screen + quick match setup. **Gate 1 playtest with others.**

---

### Format support (whenever 7v7/11v11 arrive; likely early Phase 1) ⚪
Formation tables per format, pitch markings scaled from pitch config, camera framing from pitch size,
pitch/goal presets per format (5v5 40×25, 7v7 ~55×35, 11v11 105×68).

## Phase 1 — LEARN ⚪ (gate: after a week of a pressing drill, testers see the squad press differently, untold)
Rough order (will be re-planned after Gate 1):
- 7v7 default format.
- Per-player brain: small fixed-point net adding to instinct utility scores; learned core + role adapters.
- Headless drill simulator (many × real time); evolution-strategies trainer on a low-priority thread.
- Drill editor v1 (rondos, pressing triggers, finishing), training-day calendar with fatigue cost.
- Tactic board v1 + team playbook familiarity. "Reset to instinct" per skill.
- Visualise what changed (before/after drill replays).

## Phase 2 — WORLD ⚪ (gate: 20th-hour testers still find new pitches/rules/techniques)
Seeded world + towns, pitch generator (slope, surface, size, wind), weather, player genome, career loop
(recruit/train/plan/play/review), saves, Almanac v1, emergent-technique detection. Early-access candidate.

## Phase 3 — TOGETHER ⚪ (gate: 100 LAN matches, zero desyncs)
Local multi-seat (pitch/touchline seats), LAN lockstep + discovery, squad files, ghost squads, managers' duel.

## Phase 4 — LAUNCH ⚪
Online + ranked (training-log verification), macOS build, .deb/Flatpak/AppImage, SDL_GPU renderer if needed.
