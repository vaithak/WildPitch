# WildPitch audit: how the human-controlled player is moved toward the ball today

Owner request (verbatim, docs/PLAYTEST_FEEDBACK.md, "after CP0.3c" entry): "the players need to be moved in the exact
direction of ball if the ball is just there, or during long passes, is there any assust from system side to aid player
towards the ball as long it is moving in the general direction of the ball?"

**Baseline:** read-only audit of the working tree at commit 9898d7b plus uncommitted edits by a parallel session (match
clock D-036: `team_dir()` replaced `team == 0 ? 1 : -1` in seats.c/player.c/sim_internal.h, and 6 new lines were
inserted in tuning.h after line 69). seats.c and player.c line numbers are unchanged by those edits. tuning.h cites
below use the **current** (post-edit) numbering. Cites are given as `file:line` with the function name; they point to
local source files (no URLs). Nothing was built or run. All numbers in "Inferences" are derived by hand from the code,
not measured.

## Q1. seats.c: what the assist machinery does, and when the human's legs are assisted vs on the raw stick

### Takeaway
Movement assistance exists in only three situations: (1) the **targeted receiver of a pass the human himself made**
(or a header pass by the human), until a takeover, SWITCH, ball gone or pass end; (2) for at most **30 ticks after an
auto-switch**, and only if a stick was held at the switch and not changed; (3) a **header attempt** after PASS/SHOOT
was pressed. Everywhere else, including every loose ball, every untargeted long ball, and passes from AI team-mates,
the human's raw stick drives the legs with no pull toward the ball.

### Cited Findings
**Pipeline: where assistance is injected**
- Each tick `players_step` calls `seats_update` first (switching and the receiver's goal), then builds two inputs per
  seat player: `pin` (what he does: the raw human stick and buttons) and `mv` (how the legs move). `mv` starts as the
  raw input, and `seats_assist_receive(s, seat, &mv[i])` may overwrite it — src/sim/player.c:665-679 (`players_step`).
- `header_assist_move(s, i, &mv[i])` is applied after that, to every player — src/sim/player.c:683.
- `move()` uses `mv`; buttons/kicks use `pin` — src/sim/player.c:687-688. Gain of a loose ball uses `pin` — player.c:728.
- `seats_assist_receive` has exactly two assisted branches; otherwise it returns with the raw stick untouched —
  src/sim/seats.c:261-275:
  - Branch 1: `pass_target == me && pass_stick[0] != -128` → steer the legs to `recv_goal` at `recv_speed` — seats.c:266-267.
  - Branch 2: `latch_ticks[seat] && ball.owner < 0` → steer to `meet_choose(..., no preference)` at
    `min(4*dist, sprint top speed)` — seats.c:268-270.
  - The sprint bit on the legs comes from the assist, not the human's button — seats.c:274. `team_steer_speed`
    converts a target and speed into a stick vector whose magnitude is speed/top, and sets sprint if speed > walk —
    src/sim/team.c:22-37.

**`meet_choose` (where can he meet the ball)** — src/sim/seats.c:58-90
- Forward-simulates the ball in 50 ms steps for up to 80 steps (4 s), with air drag and gravity in flight and the rolling
  decel on the ground. On landing it sets z=0 and vz=0, so **bounces are ignored** — seats.c:64-76 (comment at 53-54).
- Points with ball z > 1.9 m are skipped — seats.c:77.
- A point is reachable if `d / reach_v + 0.15 s <= t`, where `reach_v = 0.8125 × sprint top speed` (6.5 m/s for an
  8 m/s player) — seats.c:62, 80.
- With no preference the cost is the distance `d` (the closest reachable point). With a stick preference the cost is
  `t/2 + 0.3 × (1 − cos(stick, direction to the point)))` — seats.c:81-85.
- If nothing is reachable, `found = 0` and `pos` is the last predicted point — seats.c:88.

**Pass hand-off: `seats_pass_handoff`** — src/sim/seats.c:92-110
- Called from `contact()` whenever a kick has a receiver `r >= 0` — src/sim/player.c:328. Also called from
  `header_contact` for a header pass, with meet = the mate's current position, t = 1.0 s, plan CLOSEST — src/sim/header.c:132.
- It records `pass_target`, `pass_from`, `pass_meet`, `pass_meet_tick` and `pass_plan`, and resets `recv_away`,
  `recv_gone` and `recv_goal` — seats.c:96-104.
- **If the kicker is not a seat's player (`seat < 0`), or the receiver is already seat-owned, `pass_stick` is set to
  -128 and the function returns** — seats.c:105. With `pass_stick == -128`, `recv_update` is never called
  (seats.c:202) and `seats_assist_receive` branch 1 is skipped (seats.c:266). In that case there is no receiver
  assist.
- Otherwise it saves the passer's stick as `pass_stick` (the "no-opinion" reference) and switches the seat to the
  receiver. The stick is latched only when the receiver is the keeper — seats.c:106-109.
- AI kicks do set a receiver: `do_kick` sets `k.to = to + 1` for passes, throughs and lobs, and 0 for shots —
  src/sim/brain.c:392-409. AI throws do the same — brain.c:526, 550.

**`pass_plan` values** (src/sim/sim.h:160: `WP_PLAN_CLOSEST=0, RUN=1, STRIDE=2`)
- Tap pass: RUN if the receiver is moving away from the ball at more than `T_PASS_RUNNER_MS`, else CLOSEST —
  src/sim/player.c:193-196 (`pass_solve`).
- Through ball: STRIDE (attempts 0-1) or RUN (attempt 2 / short-room fallback) — player.c:239, 249 (`through_solve`).
- **Lofted pass: plan stays CLOSEST, `meet` = the receiver's current position, `t_meet` = the 1.5 s default.**
  `lob_velocity` aims the ball at `pos + vel × 0.8 s` but returns no meeting point — player.c:286-289, 296, 254-266.
- Throw: like a tap pass — player.c:290-294. Punt: no receiver at all — player.c:295.

**`recv_update` (the assisted receiver's goal, speed and takeover)** — src/sim/seats.c:113-155
- "Opinion" means the stick is held and differs from `pass_stick` by more than `T_RECV_STICK_TOL` (48 units, squared
  distance). A released or unchanged stick means no opinion — seats.c:116-120.
- For RUN or STRIDE with no opinion and more than 3 ticks to the plan: goal = `pass_meet`, at just-in-time speed
  `dist / time_left`, clamped to [4.5 (STRIDE) or 0, sprint top] — seats.c:123-127.
- Otherwise (CLOSEST, an opinion, or ≤ 3 ticks left): goal = `meet_choose(pref)`, speed = `min(4 × dist, sprint top)`.
  `recv_gone` counts ticks with no reachable point — seats.c:128-133.
- Head-on brake: if the ball (>1 m/s) is coming at him, within 0.5 s of travel, and the goal is roughly toward it
  (cos > 0.7), speed is capped at `T_RECV_HEADON_MS` — seats.c:135-139.
- Takeover. "Away" means the stick is more than 120° from the goal (`dot < −0.5`). If the goal is within 0.5 m of him
  (a standing receive), "away" means any stick not toward the ball (`dot < 0.3`) — seats.c:146-147. After
  `T_RECV_TAKEOVER_TICKS` of opinion+away, or `T_RECV_GONE_TICKS` of no reachable point, `pass_stick` is set to -128 —
  seats.c:148-154. **This is one-way for the rest of that pass**: nothing sets `pass_stick` back except a new hand-off.

**Pass end** — src/sim/seats.c:182-192
- The pass ends (`pass_target = -1`) when the phase is not PLAY, someone owns the ball, it is deflected (`last_touch`
  not the passer), more than `T_PASS_FLIGHT_MAX` ticks pass, **or it is on the ground slower than 1.0 m/s** —
  seats.c:184-186. On a deflection, `deflect_switch` gives each seat of the passing team the mate with the best
  `meet_choose` time, latching the stick (seats.c:158-177, 190).

**Latch (`latch_stick` / `latch_ticks`)**
- Set only in `do_switch` when `latch != 0` and a stick is held at the switch, to `T_SWITCH_LATCH_TICKS` —
  seats.c:31-36. Also set in `deflect_switch` when control stays on the same man — seats.c:171-175.
- Callers that latch: the keeper hand-back (seats.c:207), defensive auto-switch (seats.c:236), loose-ball auto-switch
  (seats.c:243), `seats_on_gain` (seats.c:256) and a hand-off to the keeper only (seats.c:109). **The manual SWITCH
  button passes `latch = 0`** (seats.c:217), so after a manual switch there is no chase assist.
- It is cleared at once when the stick changes at all (exact inequality on both axes), and otherwise counts down —
  seats.c:198-201.
- While latched with the ball owned by that player, the dribble stick is zeroed — player.c:675-678. While latched with
  the ball loose, the legs chase `meet_choose` — seats.c:268-270.
- Header comment: "After any auto-switch the stick the human was holding is ignored ... until it changes or is released,
  at most T_SWITCH_LATCH_TICKS" — seats.c:14-15. In the code this holds only if a stick was non-zero at the switch.

**Auto-switch** — src/sim/seats.c:193-244
- Never away from the ball carrier, and not while busy with a kick or header — seats.c:209-211.
- If they have the ball or their pass is travelling: switch to the mate nearest a spot 1.5 m goal-side of the ball if
  he is more than `T_SWITCH_DEF_HYST` nearer, with `T_SWITCH_DEF_COOL` — seats.c:223-237.
- Loose ball: **skipped while any pass is in flight** (`pass_target >= 0`), during cooldown, or when someone owns it.
  Otherwise switch to the outfield mate nearest the ball's **current** position if he is more than `T_SWITCH_HYST`
  nearer — seats.c:239-243 (`nearest_to_ball`, seats.c:40-50, uses `fv2_len(ball.pos − p.pos)`). It does not use time
  to ball or a predicted landing point.
- When an AI mate wins the ball, control jumps to him, latched — seats.c:249-257 (`seats_on_gain`, called from
  `gain_apply` player.c:605).

**Headers: `header_assist_move`** — src/sim/header.c:135-146
- Only while a header action is live: PASS/SHOOT was pressed while `header_wanted` held, i.e. the ball comes within
  1.2 s, passes within 2 m, and is at least 1.30 m high at the closest approach — header.c:25-34, player.c:355-360.
  The legs then step under the ball's straight-line (no-drag) path via `team_steer(p, meet, 0.08, 0.8)` — header.c:142-144.
  There is no automatic movement toward a high ball unless the button is pressed.

### Inferences
- The receiver assist covers only passes **the human's own seat player kicked** (or header-passed). A pass from an AI
  team-mate to the human's current player reaches seats.c:105 with `seat < 0` and gets no steering. He keeps only the
  1.3 m stretch reach (player.c:558-565). This is probably the largest gap relative to the owner's request.
- An AI pass to a *different* AI mate blocks the loose-ball auto-switch for the whole flight (seats.c:239), so the
  human stays on his current man with a raw stick until the pass ends or is won (`seats_on_gain`).
- The latch's exact-equality cancel (seats.c:199) means analogue stick jitter of one unit ends the chase assist
  almost at once. On an 8-way keyboard it survives until the key changes, but for no more than 0.5 s.

### Gaps
- Not measured how often, in live 7v7 play, the human's player receives AI-made passes vs his own passes.
- `tools/playtest/human.c` was skimmed (header only); it tests dispossession and kickoff races, not approach assist.

## Q2. player.c move(): stick to velocity, any steering toward a loose ball, gain_check reach and near-misses

### Takeaway
`move()` turns the stick (or the assist's synthetic stick) into a target velocity `dir × top × |stick|`, approached at
`T_ACCEL` 18 m/s². The only "go to the ball" logic in `move()` applies when the player **already owns** the ball. For
an unowned ball there is no steering or cone, and control is won only if the ball's swept path comes within 1.0 m
(0.8 m against an opponent's pass in flight).

### Cited Findings
- Stick → direction: normalised `move_x/127, move_y/127`. Analogue magnitude scales speed (capped at 1) —
  src/sim/player.c:25-29, 433-436.
- Top speed: `player_top_speed(sprint)` = `T_WALK_SPEED` 6.0 or `T_SPRINT_SPEED` 8.0 m/s (× pace stat; sprint fades
  when tired) — src/sim/stats.c:61-73; tuning.h:84-85.
- Velocity update: `vel += clamp(dir × top − vel, (has ? T_ACCEL : T_STOP_DECEL) × dt)` — player.c:437-438. That is
  18 m/s² while steering and 14 m/s² when stopping (tuning.h:83, 86). No stick means braking to a stop; nothing pulls
  toward a loose ball.
- Body facing turns at `T_TURN_DEG_S` 720°/s, 540°/s with the ball, and 420°/s sprinting above 6.5 m/s
  (tuning.h:89-91, player.c:444-448). Facing does not limit velocity direction; only the accel cap does.
- Step-up: **only when `s->ball.owner == i`**. With the stick released and the ball out of foot reach, he steps up to
  it at no more than jogging speed — player.c:420-427. With the stick held and the owned ball beyond `T_TOUCH_REACH`,
  the direction is replaced by the direction to the ball — player.c:428-432. Ownership lasts until the ball is more
  than `T_LOSE_DIST` 2.2 m away — player.c:477, tuning.h:100.
- The file-header comment "Releasing the stick: the player steps up to the ball if it is out of reach" (player.c:8)
  describes only the owned-ball case. The code gates it on `ball.owner == i` (player.c:421).
- `gain_check` (player.c:542-578):
  - Not eligible during an action, with touch cooldown, airborne, or with ball z > 1.45 m (`WP_BAND_BODY_MAX`; keeper
    hands 1.45) — player.c:545-548, sim.h:77.
  - Distance is **swept**: from the player's post-move position to the ball's segment `pos .. pos + vel × dt` —
    player.c:551-554.
  - Reach = `T_CONTROL_REACH` 1.0 m. It is `T_INTERCEPT_REACH` 0.8 m when the other team's pass is in flight, and
    `T_KEEPER_REACH` 1.0 for keeper hands — player.c:556-557; tuning.h:98, 216-217.
  - Only the pass's target may stretch to `T_CONTROL_STRETCH` 1.3 m, and only if the ball's closest approach on its
    current line is beyond 1.0 m — player.c:558-565, tuning.h:161.
  - A planned runner (RUN/STRIDE, assist active, no opinion) declines a ball passing more than 0.5 m away until 12
    ticks before the meet — player.c:567-575.
  - Too hot if `|ball.vel − p.vel| > T_CONTROL_MAX_REL` 16 m/s — player.c:576.
  - The closest eligible player wins — player.c:725-731.
- Other reaches:
  - `KICK_REACH` 1.15 (contact) and `KICK_START_REACH` 1.6 (start a kick on an owned or loose ball) — player.c:15-16, 361, 373.
  - Body radius `T_BODY_R` 0.30 + `T_BALL_R` 0.11 = 0.41 m — tuning.h:92, 9; ball.c:98. A body hit is skipped when the
    player could control the ball (ball.c:104-106). Otherwise it deflects the ball and sets `T_BODY_HIT_COOLDOWN` 12
    ticks (ball.c:113).
- Tick order: `players_step` (moves, then `gain_check`), then `ball_step` — src/sim/sim.c:150-151.
- For comparison, the AI chases with the same `meet_choose`. `AI_CHASE` steers to `meet_choose(no pref)` at
  `min(4 × d + 2, sprint)` — src/sim/brain.c:633-638.

### Inferences
- A ball 1.2 m to the side of the player's path is never gained by a normal player: 1.2 > 1.0. Only the targeted
  pass receiver could stretch for it (≤ 1.3 m, heavier touch). It also never touches the body (0.41 m). The player
  runs past it.
- The AI's loose-ball approach (`AI_CHASE`) is steered every tick to a predicted meeting point. A human's is not,
  unless one of the three Q1 assists is active.

### Gaps
- No empirical measurement of near-miss rates in live play. Nothing was built or run, because the tree was being edited
  concurrently.

## Q3. Concrete failure scenarios

### Takeaway
All of (a), (b), (c) and (e) fail or are unassisted in common cases. (d), the ball passing between ticks, is already
handled by the swept reach.

### Cited Findings
- **(a) Loose ball 2-3 m away, 20-40° off the stick.** With no auto-switch latch active, legs = raw stick
  (seats.c:271-272). `move()` has no unowned-ball steering (player.c:420-438). Gain needs swept distance ≤ 1.0 m
  (player.c:557, 566). If the human is already the nearest man, no auto-switch happens (hysteresis 2 m, seats.c:243),
  so no latch.
- **(b) A ground ball not targeted at him (clearance, untargeted pass, intercepted/deflected pass) crossing his path.**
  An untargeted kick calls no hand-off (player.c:299-311, 328 only if `r >= 0`). Punts (player.c:295), shots and chips
  (brain.c:404) never set a target. An opponent's pass in flight gives the defensive switch (seats.c:223-237), latched
  only if a stick was held, and lowers his reach to 0.8 m (player.c:556-557). A pass deflected off the human's
  opponent ends the pass. Only the *passing* team gets the best-time switch (seats.c:188-190); the other team falls
  back to the nearest-distance loose-ball rule.
- **(c) Lofted or long pass dropping somewhere.**
  - Targeted lob by the human: plan CLOSEST, so `recv_update` re-solves `meet_choose` every tick. That covers in-flight
    drag and points ≤ 1.9 m high, but not bounces (seats.c:71, 77, 123-133).
  - Lob from an AI mate: no assist (seats.c:105).
  - Untargeted lob (`lob_velocity` with `r < 0` aims 18 m along the stick, player.c:259-260), punt or long clearance:
    no `pass_target` is set, so auto-switch to the nearest mate to the ball's current ground position
    (seats.c:240-243). There is a latch chase only if a stick was held at that switch and for ≤ 30 ticks. Otherwise he
    does not run to the landing spot by himself.
- **(d) Ball passing between ticks.** The reach is swept over the coming tick (player.c:549-554), and `gain_check` runs
  after players move and before the ball moves (sim.c:150-151). Ball substeps are capped at 0.08 m
  (`T_SUBSTEP_MAX_MOVE`, tuning.h:25). The player moves ≤ 8/60 = 0.13 m per tick. The test "pass to a runner never
  goes through the body" (tests/test_reception.c:101) relies on this.
- **(e) After the hand-off ends or he takes over.**
  - Takeover (stick > 120° away for 15 ticks, or for a standing receive any non-toward-ball stick for 15 ticks):
    permanent raw stick for that pass (seats.c:146-154).
  - The pass also ends, which drops all receiver assist, when a grounded ball drops below 1.0 m/s (seats.c:186). A
    short-weighted pass that dies before he gets there leaves him on the raw stick.
  - SWITCH clears the receive (seats.c:216).
  - After the latch's 30 ticks (or any stick change), raw stick (seats.c:198-201).

### Inferences
- (a) is pure geometry. For a stationary ball at distance d, running straight along the stick at angle θ, the closest
  approach is d·sin θ. It misses when d·sin θ > 1.0 m, i.e. θ > asin(1/d): **30° at 2 m, 23.6° at 2.5 m, 19.5° at 3 m.**
  The owner's 20-40° band therefore mostly misses at 2.5-3 m. An 8-way keyboard can be up to 22.5° off any bearing, so
  a keyboard player cannot reliably "point at" a ball 3 m away. (Derived, not measured.)
- (b) A crossing ball needs timing as well as direction. With no lead-point steering, a 0.15 s timing error at 8 m/s is
  about 1.2 m, beyond the 1.0 m (or 0.8 m) reach. (Derived.)
- (c) `meet_choose` accepts points with ball z up to 1.9 m (seats.c:77), but feet/body control stops at 1.45 m
  (player.c:548). A meet chosen at 1.45-1.9 m can only be played with a header button press. Without one, the ball
  could bounce off the body (ball.c:104-113 requires z ≤ 1.45 to skip the body hit). Ignoring bounces, with
  `T_BOUNCE_GRIP` taking up to 39% of horizontal speed per bounce (tuning.h:16), makes predicted post-landing points
  too far for lobs. These are inferred risks, not observed.
- (e) Because the takeover is one-way, a human who briefly makes a run (≥ 250 ms) loses the assist for the rest of that
  pass even when he turns back toward the ball.

### Gaps
- No live measurement of how often each scenario happens. The playtest harnesses that could measure it
  (`tools/playtest/recv*.c`, `live2b.c`, `icpt.c`) were not run.

## Q4. Governing constants (src/sim/tuning.h, current numbering) and in-file defines

### Takeaway
The reach constants (1.0 / 0.8 / 1.3 m) and the 30-tick latch are the main constants governing the human's approach.
Movement physics (accel 18 m/s², 6/8 m/s) bounds how fast a late correction can land.

### Cited Findings
- Movement: `T_ACCEL` 18.0 (tuning.h:83), `T_WALK_SPEED` 6.0 (84), `T_SPRINT_SPEED` 8.0 (85), `T_STOP_DECEL` 14.0
  (86), `T_TURN_DEG_S` 720 / `_BALL` 540 / `_SPRINT` 420 (89-91), `T_BODY_R` 0.30 (92).
- Control:
  - `T_CONTROL_REACH` 1.0 (98), `T_CONTROL_MAX_REL` 16.0 (99), `T_LOSE_DIST` 2.2 (100), `T_TOUCH_REACH` 1.0 (107).
  - `T_BODY_HIT_COOLDOWN` 12 (114), `T_INTERCEPT_REACH` 0.8 (216), `T_KEEPER_REACH` 1.0 (217).
  - `T_CONTROL_STRETCH` 1.3 (161), `T_STRETCH_SPILL` 0.3 (162), `T_SPRINT_SPILL` 0.12 (151).
- Receive/takeover:
  - `T_RECV_STICK_TOL` 48 (163), `T_RECV_TAKEOVER_TICKS` 15 (164), `T_RECV_GONE_TICKS` 6 (165),
    `T_RECV_HEADON_MS` 4.0 (166), `T_PASS_RUNNER_MS` 2.0 (150).
  - `T_THROUGH_STRIDE` 0.8 (153), `T_THROUGH_STAND_V` 6.0 (157) and `T_THROUGH_STAND_DELAY` 0.3 (158), used by
    `through_solve`.
- Switching:
  - `T_SWITCH_COOL` 30 (117), `T_SWITCH_LOCK_MANUAL` 90 (118), `T_SWITCH_HYST` 2.0 (119).
  - `T_SWITCH_DEF_HYST` 3.0 (120), `T_SWITCH_DEF_COOL` 45 (121), `T_PASS_FLIGHT_MAX` 240 (122),
    `T_SWITCH_LATCH_TICKS` 30 (167).
- Hard-coded (not in tuning.h):
  - `meet_choose`: 0.8125 × sprint (6.5 m/s), 0.15 s reaction, 50 ms × 80 horizon, z ≤ 1.9 m, preference weight 0.3
    (seats.c:61-85).
  - Assist speed `4 × dist` (seats.c:132, 270). Pass ends below 1.0 m/s (seats.c:186). Runner lets the ball run while
    > 0.5 m and > 12 ticks to the meet (player.c:574).
  - `KICK_REACH` 1.15 / `KICK_START_REACH` 1.6 (player.c:15-16). Header `HEAD_REACH_XY` 0.45 / `_Z` 0.32,
    `WAIT_MAX` 70 (header.c:9-11).
  - Height bands: `WP_BAND_FEET_MAX` 0.55, `WP_BAND_BODY_MAX` 1.45, `WP_BAND_HEAD_MAX` 2.05 (sim.h:76-78).
- These hard-coded values are not all in tuning.h, despite the CLAUDE.md rule "all gameplay tunables in
  src/sim/tuning.h".

### Inferences
- Comment/code mismatches worth fixing when this is touched:
  - player.c:161 says tap pace is "5.5 m/s at 5 m, +0.25 per metre, max 9". The code uses `T_PASS_ARR_MIN` 8.0 /
    `_PER_M` 0.3 / `_MAX` 12.0 (tuning.h:124-126, whose comment notes the change).
  - D-020 still quotes "none beyond 26 m, scored down beyond 18 m". tuning.h has 32 / 24 m (148-149).
  - seats.c:14 ("after any auto-switch") and player.c:8 ("steps up to the ball") read broader than the code; see Q1/Q2.

### Gaps
- Exact current line numbers of tuning.h constants after line 69 may shift again: a parallel session is editing it.

## Q5. Existing tests and harnesses

### Takeaway
Tests thoroughly cover the **targeted-pass receiver assist** and the deflection switch. Nothing tests a human
approaching a loose ball at an angle, an untargeted long ball, an AI-to-human pass, or the latch chase outside
deflections.

### Cited Findings
- tests/test_reception.c (D-020 acceptance):
  - A: solver meets runners ahead (lines 44-81).
  - B/C: assisted runners receive without slowing; never through the body (83-106).
  - S: with stats (108-139).
  - D/E: keys pressed or released during flight — takeover must be a real run, ≤ 1 miss otherwise (141-182).
  - F: deflection switch within 6 ticks, follow-through with ≤ 1 switch and ≤ 6 ticks moving away (184-223).
  - G: untargeted hold power (225-240). H: live short room (242-281).
  - N1-N3 (283-330), incl. N2 "a pass coming straight to a standing receiver rolls in to his foot" (301-314).
  - All scenarios are passes the seat itself made.
- tests/test_passing.c:
  - 4: an auto-switch on a loose ball picks the nearer mate. It checks only the switch, not movement (91-97).
  - 6: a lofted pass by the human is received by the targeted mate (118-130).
  - 7-8: headers (143-190). 9: live taps to moving mates (191+).
- tests/test_ball_control.c: dribble, cut, shot latency and release/sole-trap of an **owned** ball (e.g. 199-205). No
  loose-ball approach test.
- tools/playtest/README.md lists the harnesses:
  - Reception with the stick pressed: `recv.c recv2.c recv3.c trace72.c ft.c ft2.c`.
  - Live: `live2b.c live.c live2.c through.c`. Taps received/intercepted: `icpt.c`.
  - Dispossession: `human.c`. AI pass outcomes: `passfail.c`.
  - Built manually against `build/libwp_sim.a` (README lines 8-11).

### Inferences
- New acceptance tests for the owner's request would need scenarios such as:
  - A stationary or rolling loose ball at 2-3 m and 20-40° off an 8-way key.
  - An AI mate's pass to the human.
  - An untargeted lob or clearance landing 10-20 m away with the stick held toward it.
  - Assist continuity after a short takeover.
  `tests/test_reception.c`'s `scene`/`step2` helpers (lines 17-41) are a natural base.

### Gaps
- The harness sources were not read in full; their exact metrics for this request are unknown.
