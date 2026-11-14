# WILDPITCH — Design snapshot

> Local snapshot of the owner's design doc **"WILDPITCH — Game & Product Plan"**
> (Claude Doc, rev 13, snapshotted 2026-11-14): https://claude.ai/artifact/DeVni416rCHCCboZqG8B4J
> The online doc is the owner's source of truth for *vision*. This file exists so any session can work
> offline. If the online doc changes, re-snapshot and note the rev here. Decisions that refine or
> override the doc live in `docs/DECISIONS.md` (those win over this file).

## Vision
A football game where you raise a squad of **learning players**, train them over in-game weeks like a
real coach, and take them across a **procedurally generated world** of strange pitches, local football
cultures and rules you have never seen.

**Pitch:** *a football club you grow, not a football game you replay.*

What makes it different:
- **Players learn.** Each player carries a tiny on-device behaviour model. Drills you design change how they
  actually decide, pass and move — not a +2 on a stat bar.
- **The world surprises you.** Hill pitches, frozen lakes, rooftop cages, golden-goal-only villages. Seeds,
  eras and emergent techniques mean year three still holds discoveries.
- **It runs cool.** Native C on Linux, no engine, no emulation layer. Low, steady power draw on a laptop.
- **No licences, no clones.** Stylised top-down 2.5D, invented clubs and towns. The space between FIFA and
  Football Manager, with emergence at the centre.

## Players (personas)
| Persona | Wants | Gets |
|---|---|---|
| Commuter (Debian/Ubuntu laptop) | 20–60 min offline sessions, cool lap, long battery | Offline career, battery mode, instant resume |
| Tinkerer-manager | Systems to master, tactics to design | Drill editor, tactic board, learning players, replay analysis |
| Football fan | Feel of the sport without a licensed replica | Pressing, overloads, set pieces, weather |
| Friends on sofa / LAN | Play together without accounts/servers | Local + LAN co-op/versus, shared squads |

## Hard constraints
- Native Linux, Debian family first; macOS from the same code if cheap.
- No heavy engines/frameworks; small binary, low heat.
- Fully playable offline; internet only for online multiplayer.
- No shooting/weapons — football only.
- Simple mechanics, complex dynamics; discovery that lasts years.
- Players and tactics improve through training, not code.

## Pillars (all features must stand on ≥1, serve the goal; anything adding heat/dependency/licence without doing so is cut)
Goal: **grow a football club that still surprises you years in.**
1. **Learning players** — small model each; drills shape choices; ages, forgets, mentors.
2. **Endless discovery** — seeded world; strange pitches, rules; emergent techniques.
3. **Simple to play** — move, pass, shoot; one shout button; depth from systems.
4. **Cool and native** — C17 + SDL3 only; offline first; low steady power.
Foundation: real football logic — space, pressing, overloads, set pieces, weather.

## Gameplay
Controls learnable in two minutes; depth from 14–22 learning agents + physics + world.
- **Controls:** move, pass (tap short, hold for through ball), shoot/chip, sprint, switch player, one
  **shout** button for team calls. Gamepad or keyboard.
- **Ball:** 2.5D physics with height, spin, curl; bounce/roll vary with surface and weather; wind on exposed pitches.
- **Format:** small-sided default (7v7, ~2×6 min); 11v11 for cup ties and finals. *(See DECISIONS D-002: 5v5 first.)*
- **Fouls, not fights:** shoulder challenges and tackles; timing beats strength.
- **Dynamics:** each player decides with their own trained model; stamina/morale/confidence shift in-match;
  pair chemistry; pitch/weather/folk rules change which tactics work.

**Core loop (weekly):** Recruit → **Train** (design drills, drill days) → Plan (tactic board, set pieces) →
Play (on pitch or touchline) → Review (replays show what they learnt) → next fixture.
**Seasonal:** new season / new town — world, rules and rival meta drift.

## Serendipity engine (7 layers)
1. World seed — towns, climates, rivalries, travel (mountain league: away teams arrive tired)
2. Pitches — slope, surface, size, obstacles, wind (sloped pitch: long balls curl back downhill)
3. Folk rules — local variants (golden goal only; keeper can't leave box; two balls at harvest festival)
4. Player genome — ~40 latent traits → personality/quirks (1-in-10,000 "weather reader")
5. Emergent techniques — learning finds combos of primitives (winger invents a chip-turn; you name it)
6. Living meta — AI clubs train and adapt (everyone presses by season 4 → counter-attack era)
7. Chemistry & stories — pair bonds, rivalries, small events

**Almanac:** personal codex of pitches, rules, techniques (with replay clips), rare traits; seeded, tradeable.
**Fairness:** surprise changes the situation, never the outcome by fiat. No hidden dice; every effect visible in replays and learnable.

## Training as fine-tuning
Player brain = 3 layers: **Instinct base** (hand-written, fixed, always sane) → **Learned core** (formed in
youth, slow) → **Role adapters** (up to 3, LoRA-like, swap per position). Inputs: drills you design, match
experience, veteran mentors. Training days = headless sim, sped up, small capped updates, cost stamina/time.
Only top two layers learn; updates capped per day.

| ML idea | In WILDPITCH | Feel |
|---|---|---|
| Pre-trained base | Youth genome + academy years | Every recruit has a personality |
| Fine-tuning data | Drills + match experience | You shape decisions, not stats |
| Learning rate | Coachability, falls with age | Teens change fast, veterans slowly |
| Adapters | ≤3 role adapters | Full-back learns wing without losing his game |
| Overfitting | Over-drilled patterns predictable | Rival scouts read and block your move |
| Forgetting | Unpractised skills decay | Balanced weekly schedule matters |
| Distillation | Veterans mentor youngsters | Club style passes down generations |
| Evaluation set | Friendlies, drill scores | Test training before a cup tie |
| Compute budget | Training days + fatigue | Every week is a trade-off |

**Tactics training:** team playbook is a shared coordination layer; draw patterns on a tactic board
("press when ball reaches their full-back"), drill them; familiarity rises; unfamiliar = late and loose;
drilled = automatic but readable if never varied.
**Under the hood:** per-player small fixed-point net (~few thousand weights) scoring candidate actions on
top of the utility-based instinct layer. Training = headless drill sims + bounded updates (evolution
strategies first, policy gradients later). One core, low priority, pause on battery optional.

## Multiplayer
Every seat is a **pitch seat** (control one player) or **touchline seat** (manage); humans + trained AI fill the rest.
Modes: Co-op club (2–6 one side), Head to head (1v1..5v5), Mixed, Managers' duel (both squads AI, live shouts),
Ghost squads (async, play a friend's exported squad offline).
**Squad files:** one small file (traits + weights + playbook + training log). **Ranked fairness:** deterministic
training → rebuild from seed + log; servers spot-check. **Touchline:** limited shouts per half, full control at half-time.

## Offline-first
No account to play. Local files in `~/.local/share/wildpitch` (XDG) / `~/Library/Application Support/WildPitch`.
World generated from seed. LAN discovery via UDP broadcast/mDNS. Async via files. Suspend-friendly: fixed-step
sim resumes on exact tick.

## Technical architecture
Plain **C17 + SDL3**, no engine. Deterministic fixed-point simulation is the keystone (lockstep MP, replays,
training verification, ranked). Layers: Game layer (career, tactic board, drill editor, Almanac, immediate-mode UI)
over Simulation (fixed-point 60 Hz, deterministic, 2.5D ball) / Learning (per-player models, drill trainer,
low-priority thread) / World gen / Netcode (UDP lockstep, rollback for players, LAN discovery), over a thin
SDL3 platform layer (vendored, static), targeting Linux (Vulkan/GL3.3, Wayland/X11, .deb) and macOS (Metal).
Stack: C17, SIMD paths later; 2D-first renderer, one atlas, batched draws; fixed-point maths; immediate-mode UI;
versioned binary saves; CMake; headless sim tests; CI replays across x86-64 and ARM64.

**Budgets (≈4-year-old iGPU laptop; release gates):** 60 fps vsync (30 battery); sim 60 Hz fixed; CPU in match
< 1.5 cores avg; menus ≈ 0 (redraw on change); training 1 low-prio thread; memory < 300 MB; install < 50 MB;
cold start < 2 s. Loop sleeps, menus event-driven, arrays contiguous, read `/sys/class/power_supply` and
`/sys/class/thermal` to drop effects before fans spin.

## Platform
Linux reference (Debian 12/13, Ubuntu LTS, Mint, Pop!_OS; x86-64 first), macOS 13+ secondary (Apple Silicon).
Deps vendored + static. Packaging: .deb first, then Flatpak/AppImage; signed .app on macOS.

## Owner's roadmap (phases & gates)
0 **Feel** (ball physics, 1 pitch, 5v5, keyboard + pad) → gate: strangers enjoy a 5-min kickabout, laptop cool.
1 **Learn** (player models, drill editor, training days) → gate: after a week of a pressing drill, testers see the squad press differently untold.
2 **World** (seeded world, career loop, Almanac v1) → gate: 20th-hour testers still find new pitches/rules/techniques.
3 **Together** (local + LAN, co-op/versus, squad files) → gate: 100 LAN matches x86 + Apple Silicon, zero desyncs.
4 **Launch** (online + ranked, macOS, .deb/Flatpak).
Early access after phase 2 suggested.

## Risks (with mitigations)
Learning makes players weird → bounded nudges on sane base, reset-to-instinct. Cross-platform determinism → fixed-point
+ CI replays. Training feels like waiting → happens in in-game days, watch at 4×. Scope creep → ship serendipity layers
1,2,4,5 first. Ranked cheating → deterministic rebuild. Heat → thermal budget as release gate.

## Open questions (status in DECISIONS.md)
Name (WILDPITCH?), team size, art direction, business model, language.
