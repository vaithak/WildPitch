# Decision log

Newest last. Each entry: ID, date, decision, why, who decided. These override `docs/DESIGN.md` where they differ.
Never delete entries; supersede them with a new entry.

**D-001 · 2026-11-14 · Language: C17** (owner). Plain C17, no engine. Matches design doc recommendation.

**D-002 · 2026-11-14 · Team size: 5v5 first, then 7v7 default, 11v11 later for cups** (owner).
The sim supports N players per side from the start (compile-time max 11).

**D-003 · 2026-11-14 · Art: tilted 2.5D "tabletop"** (owner). Mild perspective tilt; ball height shown by
lift above a ground shadow.

**D-004 · 2026-11-14 · Git: local commits at each checkpoint / meaningful work unit; never push unless asked** (owner).

**D-005 · 2026-11-14 · SDL3 3.4.8 vendored as source, built static, NOT committed** (Claude).
`third_party/SDL` is git-ignored (55 MB); `tools/fetch_deps.sh` fetches the pinned tag. Revisit (commit a
trimmed copy or tarball) before packaging so offline clones build.

**D-006 · 2026-11-14 · Renderer starts on SDL_Renderer (SDL_RenderGeometry), not SDL_GPU** (Claude).
Why: far less code for a 2D-first tabletop view; SDL_Renderer itself uses GPU backends. All drawing goes
through `src/game/render.*` so it can be swapped for SDL_GPU later if profiling demands. Owner informed.

**D-007 · 2026-11-14 · Simulation numeric model** (Claude).
Q16.16 fixed point (`fx` = int32), 64-bit intermediates; units: 1.0 = 1 metre, time step 1/60 s.
Sim compiled with `-fwrapv`; no floats, no libm, no pointers in `SimState`; RNG state lives in `SimState`.
The renderer may use floats (presentation only, never fed back into the sim).

**D-008 · 2026-11-14 · AI architecture = candidate actions × utility scores** (Claude, from design doc).
The instinct layer scores candidate actions from an observation vector; Phase 1's learned brain adds to the
same scores. No hard-coded state machines for decisions.

## Open (owner to decide later)
- Name: keep WILDPITCH? (working title in use)
- Business model: one-time purchase vs open-source core.
- Audio: requires `sudo apt install libasound2-dev libpulse-dev libudev-dev` then re-configure (needed by CP0.5).
