# Aerial ball control and lofted passes — WildPitch code audit (2026-11-16)

Scope: read-only audit of /home/vaithak/WildPitch for the owner's CP0.5 request (lofted-pass power meter; aerial
receiving by foot / chest / head with per-stance physics; header passes and chest-then-pass chains). Sources are
repo files cited as `path:line`. NOTE: another session was editing `src/sim/player.c` during this audit (the D-043
slide tackle on the LOB key landed mid-read), so line numbers after ~player.c:390 may drift by a few lines.
"Code does" vs "comment claims" is flagged where they differ.

## 1. Lofted pass (WP_BTN_LOB / WP_KICK_LOB): trigger, solver, and what a power meter needs

### Takeaway
The lob is an instant, edge-triggered kick with NO charge: pressing LOB with the ball reachable fires
`make_kick(..., WP_KICK_LOB, 0, ...)`, and `lob_velocity()` derives height purely from the distance to the picked
receiver (vz = 4 + 0.22·d, clamped 5..9.5 m/s) and lands the ball where the receiver will be 0.8 s from now. No HUD
bar, no hold state; the hold/release machinery (`charge_kind`/`kick_charge`) exists only for PASS/SHOOT/CHIP.

### Cited Findings
- Button: `WP_BTN_LOB = 1 << 6` "lofted pass to the best team-mate toward the stick" — [src/sim/sim.h:31](src/sim/sim.h). Keyboard default A (`WP_KEY_LOB`) — [src/game/keymap.c:14,112](src/game/keymap.c); gamepad West (X) — [src/game/gamepad.c:30](src/game/gamepad.c).
- Trigger is the PRESSED edge, no charge: `if ((pressed & WP_BTN_LOB) && p->action == WP_ACT_NONE && ball_reachable(s, i, KICK_START_REACH)) { make_kick(s, i, WP_KICK_LOB, 0, ...); sim_player_kick(...) }` — [src/sim/player.c:410-413](src/sim/player.c). charge01 is always 0.
- Same key now also = sliding tackle when an opponent has the ball within 3 m (D-043, checked BEFORE the lob): `if ((pressed & WP_BTN_LOB) && ... s->ball.owner != i && slide_wanted(s, i)) slide_start(...)` — [src/sim/player.c:395-396](src/sim/player.c), [src/sim/tackle.c:81-98](src/sim/tackle.c), `T_SLIDE_START 3.0` — [src/sim/tuning.h:201](src/sim/tuning.h).
- Ball in hands: LOB pressed = keeper overarm throw, or on a throw-in the "long throw" (`kind = WP_KICK_LOB`) — [src/sim/player.c:~379-391](src/sim/player.c) (block starting `if (s->ball.held && s->ball.owner == i)`).
- `make_kick` for LOB: `k->power = charge01` with comment "power field = the hold h (0..1): through ball = how deep/soft (D-020); lob: unused"; `k->vz = 0`; windup `T_KICK_WINDUP_CHIP` (6 ticks); receiver = previewed pick, never the keeper — [src/sim/player.c:123-133, 85-91](src/sim/player.c).
- Solver: `lob_velocity()` — target = `q->pos + q->vel * 0.8` (receiver) or 18 m along `dir` (nobody picked); `vz = clamp(T_LOB_VZ_BASE + T_LOB_VZ_PER_M * dist, T_LOB_VZ_MIN, T_LOB_VZ_MAX)`; horizontal speed `ball_lob_speed_for(dist, vz)` — [src/sim/player.c:273-286](src/sim/player.c). Comment says "dropped on the receiver (where he'll be at landing)" but the code uses a FIXED 0.8 s lead, not the solved flight time (comment vs code mismatch).
- `ball_lob_speed_for(d, vz)`: 16-step bisection on `ball_carry_distance(vh, vz)`, a forward model that mirrors `integrate()`'s air branch with substeps and quadratic drag; returns the horizontal speed whose FIRST grass contact is at d — [src/sim/ball.c:247-276](src/sim/ball.c). Agreement test ±0.4 m — [tests/test_passing.c:132-141](tests/test_passing.c).
- Contact applies it: `else if (p->kick.kind == WP_KICK_LOB) b->vel = lob_velocity(s, r, p->kick.dir, &vz);` — [src/sim/player.c:330](src/sim/player.c). The pass handoff then uses the default `meet = s->players[r].pos`, `t_meet = 1.5`, `plan = WP_PLAN_CLOSEST` (no lob-specific meet/landing point is passed) — [src/sim/player.c:307-309, 366](src/sim/player.c).
- Charge machinery for other kicks: loop over PASS/SHOOT/CHIP: press sets `charge_kind`, held increments `kick_charge` up to `T_CHARGE_MAX_TICKS` (36), release fires; PASS released after > `T_PASS_TAP_TICKS` (20) = through ball with h = (charge−20)/(36−20); SHOT/CHIP charge01 = charge/36 — [src/sim/player.c:414-432](src/sim/player.c). Fields: `kick_charge` (uint16), `charge_kind` (uint8, "WP_KICK_PASS (tap/through) / SHOT / CHIP while its button is held, else 0xFF") — [src/sim/sim.h:108-109](src/sim/sim.h).
- Chip (the only existing charged aerial kick) maps charge to power 8→15 m/s and vz 5→8.5 m/s linearly — [src/sim/player.c:138-141](src/sim/player.c), [src/sim/tuning.h](src/sim/tuning.h) `T_CHIP_*`. It is untargeted (no receiver, no landing solve).
- HUD bar: drawn above the controlled player's marker only while `charging` = `charge_kind != 0xFF && kick_charge > (PASS ? T_PASS_TAP_TICKS : 0)`; fill = (charge−20)/16 for PASS, charge/36 otherwise; colours: chip light-blue, pass/through white, shot green→red — [src/game/render.c:240-268](src/game/render.c). The avatar backswing (`pose.charge01`) appears only when the ball is within 2 m — [src/game/render.c:242-245](src/game/render.c), [src/game/avatar.c:265-271](src/game/avatar.c).
- Avatar for a lob: rendered as the CHIP strike with fixed `kick_power01 = 0.5` — [src/game/render.c:232-237](src/game/render.c).
- AI lobs: `do_kick(..., WP_KICK_LOB, mate, mate_pos, 0)` sets `k.power = T_PASS_SPEED` (ignored by `lob_velocity`) — [src/sim/brain.c:401-418, 653](src/sim/brain.c); AI uses LOB for crosses (AI_CROSS), corners to a man in the box, and restarts to a mate > 20 m away — [src/sim/brain.c:371-383, 543-544](src/sim/brain.c).
- Decision history: D-019 specified exactly today's lob ("dropped where he'll be in 0.8 s; vz = 4 + 0.22·d (5–9.5 m/s)") — [docs/DECISIONS.md:147-148](docs/DECISIONS.md). STATUS lists "lofted-pass power meter" as CP0.5 work after the slide tackle — [docs/STATUS.md:7-10](docs/STATUS.md). Owner's verbatim request — [docs/PLAYTEST_FEEDBACK.md (2026-11-16, mid-CP0.4)](docs/PLAYTEST_FEEDBACK.md).

### Inferences
- Adding a hold-for-distance meter is mostly plumbing that already exists: add LOB as a 4th entry to the charge loop (charge_kind = WP_KICK_LOB), pass charge01 into `make_kick` (the `k->power = charge01` slot is already there and "unused" for lobs), and make `lob_velocity` consume it. HUD: one more colour branch in render.c:257-268; the `charging` predicate already covers any non-PASS kind.
- Design conflicts to resolve: (a) release-to-fire adds the hold's latency to every lob (PASS has the same tradeoff, D-019); a "tap = today's auto-weighted lob, hold = longer/shorter" split like PASS/THROUGH would keep the quick lob. (b) The slide tackle shares the key on the PRESSED edge (player.c:395) — fine because slide only fires when an opponent owns the ball, and a lob needs us to own/reach it; but the charge must not start when the press became a slide. (c) The held-ball branch (keeper/throw-in) consumes LOB presses before the charge loop.
- Semantics choice for the meter: (i) meter = distance along the stick (like the chip; untargeted), or (ii) meter scales the distance relative to the picked receiver (land short / on him / beyond him, e.g. into a runner's path, analogous to the through-ball "h" in `through_solve`). `ball_lob_speed_for(d, vz)` already inverts the flight for any d, so either needs no new physics; vz could stay `f(d)` or become a second axis (driven lob vs. high lob).
- Because flight time for long lobs is ~1.3–1.9 s (vz 6–9.5 m/s ⇒ ~2·vz/g ignoring drag) but the lead is a fixed 0.8 s, a running receiver is under-led on long lobs. A meter-driven solve should iterate lead = actual flight time (the tap pass already iterates its lead 3× per D-020).
- Determinism impact: any change to lob behaviour changes the golden hash (test_determinism's scripted seat lobs every 97 ticks — tests/test_passing.c:257 pattern; STATUS:124).

### Gaps
- No playtest data on what lob distances/heights feel right; tuning targets must come from the owner or external research.

## 2. Height bands and what happens when a ball arrives at each height

### Takeaway
There are three bands (feet ≤ 0.55, body ≤ 1.45, head ≤ 2.05 m + jump). Receiving is automatic and uniform: any
loose ball ≤ 1.45 m that passes within reach and < 16 m/s relative (2-D) is "gained" by `gain_apply`, which applies
one spill formula for all heights and, if above the feet band, just zeroes vz so the ball drops from where it is.
There is no foot/thigh/chest distinction in physics, no cushion direction, and no receive animation besides the
generic dribble-touch leg flick. Heads only ever act via a deliberate PASS/SHOOT press (header attempt); 1.45–1.8 m
balls otherwise bounce off the body cylinder.

### Cited Findings
- Bands: `WP_BAND_FEET_MAX 0.55`, `WP_BAND_BODY_MAX 1.45`, `WP_BAND_HEAD_MAX 2.05` ("standing head reach; jumping adds the player's z"), `WP_JUMP_VZ 3.4` (~0.59 m apex) — [src/sim/sim.h:75-79](src/sim/sim.h). D-010: "ALL ball contact is resolved in height bands" — [docs/DECISIONS.md:46-52](docs/DECISIONS.md). Note `WP_BAND_HEAD_MAX` is defined but header reach actually uses header.c's own `HEAD_Z 1.70 ± HEAD_REACH_Z 0.32` (so ~1.38–2.02 m standing) — [src/sim/header.c:7-10, 98-108](src/sim/header.c).
- `gain_check` (loose ball): rejected if player has an action, is airborne (`p->z > 0`), or has `touch_cooldown`; height gate `b->z > (keeper_hands ? T_KEEPER_HANDS_Z : WP_BAND_BODY_MAX)`; reach is swept over the tick; reach = 1.0 m (receiver may stretch to 1.3 m; interceptor 0.8; keeper 1.0); too hot if `|b->vel − p->vel| > T_CONTROL_MAX_REL` (16 m/s; keeper 16 m/s) — [src/sim/player.c:600-636](src/sim/player.c). The relative-speed test is 2-D: the ball's vertical speed is never considered.
- `gain_apply`: `spill = |rel|/16 * 0.45 (+0.12 sprinting) (+ up to 0.3 stretching)`, × (1 − 0.5·k_control); `b->vel = p->vel + rel*spill`; `if (b->z > WP_BAND_FEET_MAX) b->vz = 0; /* chest/thigh: drop it to the feet */`; spin 0; owner = i; touch_cooldown 6 — [src/sim/player.c:639-665](src/sim/player.c). The comment "chest/thigh" is the only mention of body parts in the sim; code treats 0.56 m and 1.45 m identically (vz zeroed, ball falls from that height under gravity).
- While owned but above the feet: `control_owner` loses it if `dist > T_LOSE_DIST (2.2)` or `b->z > WP_BAND_BODY_MAX`; otherwise returns early (`if (b->z > WP_BAND_FEET_MAX || p->action != WP_ACT_NONE) return;`) so there are no dribble touches or sole trap until it drops into the feet band — [src/sim/player.c:535, 548](src/sim/player.c).
- Body cylinder (ball.c): ball deflects off a player (restitution `T_BODY_REST 0.3`, sets `T_BODY_HIT_COOLDOWN 12`) when within `T_BODY_R+T_BALL_R` and between his feet and `p->z + T_BODY_H (1.80)`, UNLESS he could control it (loose, ≤ 1.45 m, rel ≤ 16, no action, no cooldown) or he is in a live header attempt with ball above feet band — [src/sim/ball.c:90-117](src/sim/ball.c). So a 1.45–1.80 m ball not headed bounces off the chest/shoulders as a deflection.
- Test pins current behaviour: overhead ball (z 2.4) is NOT trapped; chest-height slow ball (z 1.1, 5 m/s) IS controlled "dropped to feet" — [tests/test_ball_control.c:138-147](tests/test_ball_control.c); "chest control then an instant shot: the ball falls under gravity, no height teleport" (max per-tick drop < 0.08 m) — [tests/test_ball_control.c:236-249](tests/test_ball_control.c).
- Keeper hands: `T_KEEPER_HANDS_Z 1.45` (comment: "body height until CP0.3b adds hands: higher = parry"), catch rel ≤ 16 m/s, reach 1.0 m; above that only a dive reaches (`T_DIVE_TOP 2.2`, `T_DIVE_CATCH_REL 18`) — [src/sim/tuning.h](src/sim/tuning.h) keeper block; gated in `gain_check` [src/sim/player.c:605-606](src/sim/player.c). A keeper therefore cannot claim a high cross above 1.45 m except by a shot-triggered dive.
- Headers (D-010/D-019): `header_wanted` — ball loose, player grounded, ball above 1.20 m or rising; predicted closest approach within 1.2 s, ≤ 2 m miss, z ≥ 1.30 m; jump if predicted z > HEAD_Z + 0.20 (1.90 m) — [src/sim/header.c:26-35](src/sim/header.c). `header_start` sets STAND/JUMP_HEADER, `kick.foot` = 0 pass / 1 shot — [src/sim/header.c:37-49](src/sim/header.c). `header_step`: jump take-off when time-to-ball ≤ vz/g (≈0.35 s) or after 45 ticks, drifting toward the meet point ≤ 3.5 m/s; give-up when the ball has passed — [src/sim/header.c:52-82](src/sim/header.c). Contact window ±0.32 m vertically, 0.45 m horizontally around a head 0.12 m in front — [src/sim/header.c:84-95](src/sim/header.c). Nearest head wins after everyone moves — [src/sim/player.c:770-780](src/sim/player.c). `header_assist_move` steps the legs under the ball's path (stick only aims) for humans and AI — [src/sim/header.c:137-147](src/sim/header.c), called at [src/sim/player.c:742](src/sim/player.c). Header action end rules — [src/sim/header.c:150-157](src/sim/header.c).
- Avatar: action poses exist only for KICK, KEEPER_DIVE, TACKLE, JUMP/STAND_HEADER (and slide being added) — [src/game/avatar.c:292-367](src/game/avatar.c). A gained ball only triggers the generic "dribble touch" leg flick (`touch01`, leg swinging forward reaches out) — [src/game/avatar.c:259-264](src/game/avatar.c), [src/game/render.c:182, 227](src/game/render.c). No chest, thigh, instep-cushion or volley pose exists. `avatar.c:376` "thigh" is a bone length, unrelated.
- D-012 already envisions kicks with a "contact height band" parameter ("foot L/R, behind-standing-leg flag, contact height band, swing direction, power, spin") — [docs/DECISIONS.md:56-60](docs/DECISIONS.md), but `WpKick` has no such field — [src/sim/sim.h:83-98](src/sim/sim.h).

### Inferences
- "Apt physics per stance" is entirely missing: today the only per-height effect is vz := 0. Natural seams: `gain_apply` (pick a stance from `b->z` and incoming 3-D velocity: instep/sole ≤ 0.55, thigh ~0.55–0.9, chest ~0.9–1.45, head above), each with its own restitution/cushion and output (chest = ball pops up ~0.3–0.6 m and drops in front; thigh = small pop and forward; foot = cushion to the ground). Recording the stance needs a new hashed field (e.g. on `WpBall` or `WpPlayer`) for the avatar to pose it — CLAUDE.md requires `sim_hash()` + `WP_SIMSTATE_SIZE_EXPECTED` updates.
- Steep lobs arrive with large vertical speed (≈ launch vz, 5–9.5 m/s) which is ignored by both the "too hot" check and the spill; a 3-D relative speed (or a separate vertical cushion term) would make high-balls harder to kill, which is what "apt physics" implies.
- The 1.45 m BODY_MAX vs ~1.38 m bottom of the standing head window means a band of ~1.38–1.45 m where both chest control (automatic) and a header (if requested) are possible; 1.45–1.80 m un-headed balls deflect off the body (no controlled chest-high-ball trap for tall balls).

### Gaps
- `WP_BAND_HEAD_MAX` is defined but never read anywhere in src/ or tests/ (grep) - head reach is header.c-local.

## 3. Can a player play first-time off an aerial ball, or chest-then-pass? What happens to presses while the ball is up?

### Takeaway
Header passes and header shots exist (PASS/SHOOT pressed on a high incoming ball). There is no volley and no
first-time kick of a loose ball above 0.55 m. Chest-then-pass works only implicitly: after the automatic chest
control the player OWNS the ball, and `ball_reachable` returns true for the owner at any height, so a PASS/SHOOT
release strikes it from wherever it is (test only covers chest + shot). Inputs are not buffered: a PASS tap released
before the ball is gained is silently dropped; a held PASS that spans the reception becomes a through ball.

### Cited Findings
- Header pass: `header_contact` with `kick.foot == 0` → `to = pass_pick(s, i, dir)`; direction at the mate's CURRENT position (no lead / no meet solve); speed `clamp(0.7·d + 3, 5, T_HEADER_PASS_MAX=12)`, `vz = 1.8` fixed; then `seats_pass_handoff(..., s->players[to].pos, FX(1.0), WP_PLAN_CLOSEST)` — [src/sim/header.c:98-134](src/sim/header.c). Header shot: aim-assisted, `min(11 + 0.25·in_speed, 17)` m/s, vz −0.8 — [src/sim/header.c:107-110](src/sim/header.c), constants [src/sim/tuning.h](src/sim/tuning.h) `T_HEADER_*`.
- Only PASS or SHOOT start a header (`pressed & (WP_BTN_PASS | WP_BTN_SHOOT)`), not LOB or CHIP — [src/sim/player.c:404-408](src/sim/player.c). No headed through ball, headed lob, or flick-on variant; no hold/charge (`header_start` clears the charge) — [src/sim/header.c:47-48](src/sim/header.c).
- Tested: header at goal scores; header pass hands control to the mate; high ball jumps — [tests/test_passing.c:143-169](tests/test_passing.c); missed header frees the player within 20 ticks — [tests/test_passing.c:171-187](tests/test_passing.c). Harness: `--autoplay-header H` (scripted cross at height H, K pressed) — [src/game/main.c:135, 307, 489](src/game/main.c).
- No volley: `ball_reachable` for a non-owner returns 0 if `b->z > WP_BAND_FEET_MAX` — [src/sim/player.c:31-38](src/sim/player.c); kick `contact()` whiffs unless `ball_reachable(s, i, KICK_REACH)` — [src/sim/player.c:292](src/sim/player.c). The gather comment "a dropping ball falls by itself (volley)" applies only to an OWNED ball above the feet during a kick wind-up — [src/sim/player.c:537](src/sim/player.c).
- Chest-then-pass path: owner at any height → `ball_reachable` returns 1 (`if (b->owner == i) return 1;`, [src/sim/player.c:33](src/sim/player.c)) → kick starts; gather skips while z > 0.55 (player.c:537); contact strikes from the current height with the kick's own vz (0 for a ground pass) and the ground-pass solver's velocity — [src/sim/player.c:307-366](src/sim/player.c). `pass_solve`/roll tables assume a rolling ball, so a pass struck from 1 m height first drops and bounces (pace/meet off). Only "chest + instant shot" is tested (no teleport) — [tests/test_ball_control.c:236-249](tests/test_ball_control.c).
- `gain_check` refuses players with any action, so a player who is mid-kick-wind-up or in a header attempt cannot also chest-control the ball — [src/sim/player.c:604](src/sim/player.c).
- Press timing: PASS press while a ball is in the air but `header_wanted` is false (e.g. ball 0.55–1.2 m and falling) sets `charge_kind = PASS`; on release, if the ball isn't reachable yet (not owned and above feet, or > 1.6 m away) the kick is NOT started and the charge is reset — [src/sim/player.c:416-432](src/sim/player.c). If the button is held through the automatic reception and released > 20 ticks after the press, it fires a THROUGH ball instead of a tap pass (player.c:424-426). LOB is edge-only: pressed before reachability = nothing (player.c:410).
- The receive-assist "first-touch direction" (D-039: stick held within 0.6 s of arrival is not a takeover) only affects the legs' steering; `gain_apply` ignores the stick entirely (no directional first touch) — [src/sim/seats.c:158-167](src/sim/seats.c), [src/sim/player.c:639-651](src/sim/player.c).

### Inferences
- The requested chains need: (1) an input buffer for PASS/SHOOT/LOB pressed while the ball is incoming (e.g. "queue the next action at first touch", like a short pre-press window) instead of dropping it; (2) a deliberate aerial first-time option: volley/half-volley for 0.55–~1.0 m (kick allowed at that height with a contact-height-band kick per D-012), chest/thigh cushion followed by an automatic second touch ready for a pass; (3) a header-pass solver that leads the receiver and uses the air model (today: fixed vz 1.8 at the mate's current position, no ball_lob_speed_for).
- A pass struck at chest height by an owner should probably either wait until the ball is in the feet band (gather) or be solved as a volley (3-D launch), otherwise pace/meet predictions (`pass_solve`, which assume ground roll) are wrong.

### Gaps
- Not run in-game; the exact feel of today's implicit chest-then-pass was not measured (headless scripted test would be needed).

## 4. meet_choose / meet_chase / receive assist and AI for lobs, crosses and headers

### Takeaway
Receivers of a lob are steered by `meet_choose` (no-bounce prediction, accepts points up to 1.9 m high, picks the
reachable point CLOSEST to him) because the lob hands off with `WP_PLAN_CLOSEST`; the free-ball chase cone uses
`meet_chase` (bounces modelled, ≤ 1.45 m). AI receivers move to the same meet point and can go for a header if they
are the pass target / first man and `header_wanted` fires; otherwise they gain it automatically at ≤ 1.45 m.

### Cited Findings
- `meet_choose`: 50 ms steps for 4 s, gravity + drag in flight, rolling decel, "bounces ignored" (z clamps to 0, vz 0); skips points with `z > 1.9`; reach at 0.8125 × sprint speed + 0.15 s reaction; no preference → min distance; stick preference → `t/2 + 0.3(1−cos)` — [src/sim/seats.c:53-90](src/sim/seats.c).
- `meet_chase` (D-037 chase cone): same but models grass bounces (same law as ball.c) and only accepts `z ≤ WP_BAND_BODY_MAX` — [src/sim/seats.c:311-350](src/sim/seats.c).
- Receiver update: for `WP_PLAN_CLOSEST` (lobs, header passes, tap passes to non-runners) the goal each tick = `meet_choose(...)`, speed = min(4·dist, top); head-on cap 4 m/s in the ball's last 0.5 s; takeover rules — [src/sim/seats.c:120-170](src/sim/seats.c). D-019 notes the assisted meet point for "ground/lofted passes → the reachable point CLOSEST to the receiver" — [docs/DECISIONS.md:156-157](docs/DECISIONS.md).
- AI receiving: `AI_RECEIVE` candidate at `pass_meet` for planned passes else `meet_choose(...).pos` — [src/sim/brain.c:163-165](src/sim/brain.c); executed as steer-to-goal — [src/sim/brain.c:689-693](src/sim/brain.c).
- AI headers (D-027): on decision beat or when it is the pass target, the team's plan winner or anyone within 3 m with `header_wanted` → `header_start(at_goal)`; at goal within 14 m with a shooting chance, clearance upfield/wide in own third, else nod on 8 m forward (pass_pick finds a mate) — [src/sim/brain.c:607-622](src/sim/brain.c), [docs/DECISIONS.md:286-292](docs/DECISIONS.md). AI aim at head contact = direction to `ai_goal` — [src/sim/player.c:770-779](src/sim/player.c). AI never chooses to chest/control-vs-head; it heads whenever `header_wanted` is true.
- AI crosses: wide in the attacking third, lofted ball to a mate within `T_AI_CROSS_ZONE` (16 m) of goal — [src/sim/brain.c:371-383](src/sim/brain.c); "AI vs AI: ~3.5 crosses/min, ~2 headers/min (7v7)" — [docs/DECISIONS.md:292](docs/DECISIONS.md); test counts crosses — [tests/test_ai_rules.c:282](tests/test_ai_rules.c).

### Inferences
- `meet_choose` accepts 1.45–1.9 m points (head height) that are NOT controllable by `gain_check`; a human receiver who does not press for a header can be steered to a spot where the ball bounces off his body. With the meter, the handoff should pass the solved landing/meet point and time (contact() currently passes the receiver's start position, t = 1.5 s) and perhaps a plan that targets "ball at chest/foot height" vs "head" depending on the chosen reception.
- Because the lob is aimed to LAND at the receiver's predicted spot, it descends steeply through the body band right at him; fine for chest control, but the bounce (ignored by `meet_choose`) is where an under-led runner meets it.

### Gaps
- No metrics exist for lob completion rate, aerial reception success, or header pass accuracy (AI vs AI); would require new headless stats.

## 5. Existing tests/harnesses touching aerial play, and relevant tuning constants

### Takeaway
Aerial coverage is thin: one lob test (peak > 2.5 m, received by the mate), the lob-solver agreement test, header
tests (stand/jump, shot scores, pass hands off, miss frees the player), chest-control and chest+shot tests, keeper
hands tests, and the determinism script that lobs periodically.

### Cited Findings
- Lob over a statue defender: peak > 2.5 m and `got == MATE` — [tests/test_passing.c:118-131](tests/test_passing.c); solver agreement ±0.4 m — [tests/test_passing.c:132-141](tests/test_passing.c); headers 7/8 — [tests/test_passing.c:143-187](tests/test_passing.c); keeper never the target of a lob in the scripted stress — [tests/test_passing.c:257-266](tests/test_passing.c).
- Ball control: overhead not trapped / chest controlled — [tests/test_ball_control.c:138-147](tests/test_ball_control.c); chest + instant shot — [tests/test_ball_control.c:236-249](tests/test_ball_control.c).
- Keeper hands vs chest-height ball outside the box, back-pass — [tests/test_ai_rules.c:160-190](tests/test_ai_rules.c); jump-header body exemption in goal — [tests/test_ball.c:117](tests/test_ball.c).
- Determinism golden includes lobs and headers ("~140 kicks ... 5 headers") — [docs/STATUS.md:124](docs/STATUS.md).
- Tuning constants (values) — all in [src/sim/tuning.h](src/sim/tuning.h):
  - Lob: `T_LOB_VZ_BASE 4.0`, `T_LOB_VZ_PER_M 0.22`, `T_LOB_VZ_MIN 5.0`, `T_LOB_VZ_MAX 9.5`; windup `T_KICK_WINDUP_CHIP 6`.
  - Charge: `T_CHARGE_MAX_TICKS 36` (0.6 s), `T_PASS_TAP_TICKS 20`, `T_KICK_TOTAL_TICKS 26`, `T_KICK_IMMUNE 8`.
  - Chip: `T_CHIP_MIN 8`, `T_CHIP_MAX 15`, `T_CHIP_VZ_MIN 5.0`, `T_CHIP_VZ_MAX 8.5`. Shot vz `T_SHOT_VZ_MIN 1.2` / `MAX 4.2`.
  - Headers: `T_HEADER_SHOT 11`, `T_HEADER_SHOT_MAX 17`, `T_HEADER_SHOT_VZ −0.8`, `T_HEADER_PASS_MAX 12`; header.c locals `HEAD_Z 1.70`, `HEAD_FWD 0.12`, `HEAD_REACH_XY 0.45`, `HEAD_REACH_Z 0.32`, `WAIT_MAX 70` (not in tuning.h — violates the "all tunables in tuning.h" rule), header pass vz 1.8 and speed 0.7·d+3 (min 5) hard-coded in header.c:119-120.
  - Control: `T_CONTROL_REACH 1.0`, `T_CONTROL_MAX_REL 16`, `T_CONTROL_STRETCH 1.3`, `T_STRETCH_SPILL 0.3`, `T_SPRINT_SPILL 0.12`, `T_STAT_CONTROL 0.5`, `T_LOSE_DIST 2.2`, `T_BODY_HIT_COOLDOWN 12`, `T_BODY_H 1.80`, `T_BODY_R 0.30`, `T_BODY_REST 0.30`; spill base factor 0.45 and gain `touch_cooldown 6` hard-coded in player.c:645, 655.
  - Keeper: `T_KEEPER_HANDS_Z 1.45`, `T_KEEPER_REACH 1.0`, `T_KEEPER_CATCH_REL 16`, `T_DIVE_TOP 2.2`, `T_DIVE_CATCH_REL 18`.
  - Ball: `T_GRAVITY 9.81`, `T_AIR_DRAG 0.013`, `T_BOUNCE_REST 0.62`, `T_BOUNCE_LIN 0.06`, `T_BOUNCE_CUT 0.5`, `T_BOUNCE_MU 0.45`, `T_BOUNCE_GRIP 0.39`, `T_BALL_R 0.11`, `T_THROWIN_ARRIVE_Z 0.9` (throw-ins are already arced to arrive at a controllable 0.9 m: [src/sim/player.c:312-321](src/sim/player.c) — a reusable "arrive at height h" solve).
  - Bands: `WP_BAND_FEET_MAX 0.55`, `WP_BAND_BODY_MAX 1.45`, `WP_BAND_HEAD_MAX 2.05`, `WP_JUMP_VZ 3.4` — [src/sim/sim.h:75-79](src/sim/sim.h).

### Inferences
- Needed new tests: lob meter monotonic (more hold = further landing), landing within X m of intent; aerial reception per stance (foot/thigh/chest outcome heights/speeds), steep high ball spill; header pass reaches a moving mate; chest→pass chain and buffered press; AI aerial stats (cross completion, header pass completion).

### Gaps
- Did not inspect `tests/test_reception.c` in depth (grep found no aerial cases there) or `bench_sim` impact.
