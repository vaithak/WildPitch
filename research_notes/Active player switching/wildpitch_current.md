# WildPitch: how active-player switching works today (code audit at HEAD 570b31c)

Scope: a read-only audit of the committed code (`git show HEAD:...`; the working tree matched HEAD for these files when audited). Citations are `path:line` at HEAD 570b31c. Nothing was built or run. Every frequency figure below is **reasoned from the code, not measured**, and is labelled as such.

Shorthand: **seat** = one human controller slot. `s->seat_player[k]` is the player index that seat k controls. 60 ticks = 1 s (`WP_TICK_HZ 60`, src/sim/sim.h:17).

---

## Q1. Where switching is implemented, and every trigger with its code and tunables

### Takeaway
All switching lives in `src/sim/seats.c`. Its main entry, `seats_update()`, runs first in every `players_step` tick. Two more calls come from elsewhere: `seats_pass_handoff()` from the kick-contact code and `seats_on_gain()` from ball-gain, keeper and restart code. There are **eight distinct triggers**. Only two of them (the loose-ball rule and the opponent-has-it rule) check the cooldown, so **there is no global minimum dwell time**. The loose-ball rule comes in two versions picked by the assist setting. Assist **Off** uses a distance rule (2 m hysteresis). Assist **Low/Normal/High**, with Normal the shipped default, uses a time-to-ball rule with YSoccer's 0.5x hysteresis.

### Cited Findings

**Call order (once per tick)**
- `players_step` calls `seats_update(s, in)` first: "switching first: this tick's inputs go to the new player". The team plan, per-player input and then movement follow — src/sim/player.c:767-768.
- `seats_on_gain()` is called from `gain()` when any player gains control, src/sim/player.c:703. It is also called from the keeper catch, src/sim/keeper.c:45, and from the restart taker, src/sim/restart.c:105, :223. The kick hand-off `seats_pass_handoff()` is called at kick contact, src/sim/player.c:396, and for a header pass, src/sim/header.c:127. `gain()` runs during the per-player loop, after `seats_update`. So a cooldown-checked switch in `seats_update` and an on-gain switch can both happen in the same tick.
- `do_switch()` (src/sim/seats.c:24-37) does five things on every switch. It clears the old player's kick charge (`kick_charge = 0; charge_kind = 0xFF`), sets `seat_player`, sets `switch_cool[seat] = cool` and resets the pass-pick stick memory (`pass_pick_reset`). If `latch` is set and the stick is held, it copies the stick into `latch_stick` with `latch_ticks = T_SWITCH_LATCH_TICKS`.

**The file's own summary of the rules** (src/sim/seats.c:1-16) is quoted here:
> Never switch away from the ball carrier … Targeted pass: control hands over to the receiver on the CONTACT tick (a whiff doesn't switch). While that pass is in flight there is no auto-switch … Deflection off anyone: switch at once to the team-mate with the best time to the ball. Loose ball: auto-switch to the outfield team-mate nearest the ball (hysteresis + cooldown). After any auto-switch the stick the human was holding is ignored … until it changes or is released, at most T_SWITCH_LATCH_TICKS … SWITCH button: nearest team-mate to the ball other than the current one; locks auto-switch for a while.

**Tunables** (src/sim/tuning.h):

| Name | Value | Meaning | Line |
|---|---|---|---|
| `T_SWITCH_COOL` | 30 ticks (0.5 s) | cooldown set by auto/hand-off/gain switches | tuning.h:134 |
| `T_SWITCH_LOCK_MANUAL` | 90 ticks (1.5 s) | cooldown after a manual switch | :135 |
| `T_SWITCH_HYST` | 2.0 m | distance rule (assist Off): challenger must be this much nearer the ball | :136 |
| `T_SWITCH_DEF_HYST` | 3.0 m | opponent-has-it rule: challenger this much nearer the goal-side spot | :137 |
| `T_SWITCH_DEF_COOL` | 45 ticks (0.75 s) | cooldown set by the opponent-has-it rule | :138 |
| `T_PASS_FLIGHT_MAX` | 240 ticks | a pass in flight stops blocking after this | :139 |
| `T_SWITCH_TIME_K` | 0.5 | time rule (assist on): challenger time < 0.5 x mine | :89 |
| `T_SWITCH_LATCH_TICKS` | 30 ticks | max time the inherited stick is ignored | :191 |
| `T_RECV_STICK_TOL` | 48 (/127) | stick-change tolerance for the receiver "opinion" and the latch (assist on) | :187 |
| `T_RECV_TAKEOVER_TICKS` | 15 | stick > 120° from the meet for this long gives full manual control of the receiver | :188 |
| `T_RECV_GONE_TICKS` | 6 | no reachable meet for this long ends the receive assist | :189 |
| `T_RECV_LOCK_S` | 0.6 s | no receiver takeover once the ball ETA is below this | :86 |
| `T_CHASE_*` | F = 20/30/40°, E = 40/55/70°, max t 2.0/3.0/3.5 s, share 0.15/0.30/0.50 s, stick min 48, 3 mates, press max 12 m | chase-cone assist (Low/Normal/High) | :80-88 |

**Trigger 1: pass hand-off (own pass).** `seats_pass_handoff()` (src/sim/seats.c:92-118) records `pass_target`, `pass_from`, the meet point, the meet tick and the plan. If the kicker is a seat's player and the receiver is not already seat-owned, it calls `do_switch(s, seat, receiver, T_SWITCH_COOL, in, receiver is keeper)`. The latch is used only for a pass to the keeper ("the stick the passer was holding (toward his own goal) must not carry over", :116-117). The switch happens on the contact tick, does not check `switch_cool`, and resets it to 30. For an AI kicker, or a receiver who is already seat-owned, it only records the pass. Under D-037 it may also arm the receive assist for the receiving seat (sentinel `pass_stick = {-127,-128}`, :105-112).

**Trigger 2: receiver lock while our pass travels.** This is not a switch but a block. In the time rule, `if (s->pass_target >= 0 && s->players[s->pass_target].team == p->team) continue; /* our pass: let it come */` (:259), and `s->pass_target == me` also skips (:258). In the distance rule any pass in flight blocks: `if (s->switch_cool[k] || s->pass_target >= 0 || b->owner >= 0) continue;` (:272). The pass ends on a phase change, on an owner, on a deflection, after `T_PASS_FLIGHT_MAX` ticks, or when the ball is grounded and below 1 m/s (:199-201). During the pass, `recv_update` (:121-170) steers the receiver. Full manual control arrives only after 15 ticks of the stick held more than 120° from the meet, and never once the ETA is under `T_RECV_LOCK_S` = 0.6 s (:160-169). Otherwise it arrives after 6 ticks with no reachable meet, or on a SWITCH press (:234).

**Trigger 3: deflection.** If a pass in flight is touched by anyone other than the passer and is loose (`b->owner < 0 && b->last_touch >= 0 && b->last_touch != s->pass_from`, :199), then `deflect_switch()` (:173-192) gives each seat of the passing team the team-mate with the smallest `meet_choose` time. It ignores the cooldown and sets cool 30 and the latch. If the seat keeps the same man, a held stick is still latched (:186-190).

**Trigger 4: our keeper has played it.** `if (p->role == WP_ROLE_KEEPER && b->owner != me && s->phase == WP_PHASE_PLAY && s->pass_target != me)`, the seat is moved to `nearest_to_ball()` (distance, outfield only) immediately. This happens before the `busy` check and **without checking the cooldown** (:222-226).

**Guards before the remaining triggers** (:227-229). There is no switch if the phase is not PLAY, if the seat owns the ball ("never switch away from the ball carrier"), or if the player is `busy` (a kick wind-up or header attempt not yet done). A charging but not-yet-kicking player is **not** busy.

**Trigger 5: manual SWITCH button** (:230-238). On the rising edge only (`pressed = buttons & WP_BTN_SWITCH && !(prev_buttons & WP_BTN_SWITCH)`), the seat goes to `nearest_to_ball(s, team, me, 0)`, meaning distance only, skipping the current man, keepers and other seats' men. The cooldown is set to `T_SWITCH_LOCK_MANUAL` (90) with no latch. If the seat was a pass receiver, the receive assist is dropped. The manual switch does not itself check the cooldown, so it always works. Bindings are keyboard Q (src/game/keymap.c:16, :110) and gamepad LB (src/game/gamepad.c:33, :191).

**Trigger 6: opponent has it, or their pass is travelling (D-029)** (:239-256):
```c
int theirs = (b->owner >= 0 && s->players[b->owner].team != p->team) ||
             (s->pass_target >= 0 && s->players[s->pass_from].team != p->team);
if (theirs) {
    if (s->switch_cool[k]) continue;
    fx sg = FX_INT(team_dir(s, p->team));
    fv2 spot = fv2_add(b->pos, fv2_make(-fx_mul(sg, FX(1.5)), 0));   /* 1.5 m goal-side along x */
    ... best = outfield team-mate (not another seat's) nearest `spot` by distance ...
    if (best >= 0 && best != me && dme - bd > T_SWITCH_DEF_HYST) do_switch(s, k, best, T_SWITCH_DEF_COOL, si, 1);
    continue;
}
```
This rule is distance-only and the same at every assist level. The goal-side spot is offset only along x, ignoring the ball's y position relative to the goal. The rule ignores velocity, facing and whether the man is goal-side. It always `continue`s, so the loose-ball rules never run while an opponent pass is travelling.

**Trigger 7a: loose ball, assist Low/Normal/High (D-037, the shipped default)** (:257-270):
```c
if (s->switch_cool[k] || b->owner >= 0 || s->pass_target == me) continue;
if (s->pass_target >= 0 && s->players[s->pass_target].team == p->team) continue;   /* our pass: let it come */
Meet mm = meet_choose(s, p, fv2_make(0, 0), 0);
fx tme = mm.found ? mm.t : FX(9.0), tb = 0;
... best = team-mate (not me, not keeper, not seat-owned) with found && smallest mq.t ...
if (best >= 0 && tb < fx_mul(tme, T_SWITCH_TIME_K)) do_switch(s, k, best, T_SWITCH_COOL, si, 1);
```
Assist level comes from the menu, `cfg.assist_level = g.menu.assist` (src/game/main.c:361). The 7v7 and 5v5 game configs default to `WP_ASSIST_NORMAL` (src/sim/sim.c:21, :36).

**Trigger 7b: loose ball, assist Off (the legacy D-017 rule)** (:272-276):
```c
if (s->switch_cool[k] || s->pass_target >= 0 || b->owner >= 0) continue;
int to = nearest_to_ball(s, p->team, me, &dn);
fx dme = fv2_len(fv2_sub(b->pos, p->pos));
if (to >= 0 && dme - dn > T_SWITCH_HYST) do_switch(s, k, to, T_SWITCH_COOL, si, 1);
```

**Trigger 8: an AI team-mate (or our keeper, or a restart taker) wins the ball** (:282-290). `seats_on_gain()` makes the first seat on that team take him: `do_switch(..., T_SWITCH_COOL, ..., latch=1)`. It ignores the cooldown and clears the seat's `pass_target`. This is an owner decision: "control jumps to the AI team-mate - or the keeper - who wins it" (docs/PLAYTEST_FEEDBACK.md:81-82; D-022).

**Kick-off.** `sim_kickoff_setpiece` forces the seat on the kicking team onto the taker (src/sim/sim.c:117-118).

**What `meet_choose` returns** (src/sim/seats.c:58-90). It predicts the ball every 50 ms for 80 steps (4 s horizon), with air drag and rolling deceleration but no bounces. Candidate points are those at ≤ 1.9 m height that the player can reach in time at `0.8125 x sprint top speed` plus a 0.15 s reaction. With no stick preference, `cost = d`, the distance from the player to the candidate point. The returned `m.t` is therefore **the ball's arrival time at the reachable point closest to the player, not his earliest possible arrival**. The function does not take his current velocity or facing into account.

**Latch** (:213-219, :301-304; src/sim/player.c:774-778). After an auto-switch with a held stick, `latch_ticks = 30`. Each tick the latch counts down, and it is cancelled when the stick changes: for assist on, by more than `T_RECV_STICK_TOL` = 48 (jitter-tolerant, D-037); for assist Off, by any change. While the latch is active and the ball is loose, `seats_assist_receive` drives the new man's legs to his `meet_choose` point at arrive-speed. If he owns the ball, the stick is zeroed ("don't dribble with it"). **When the ball is owned by an opponent** (the opponent-has-it rule), neither branch applies (`ball.owner < 0` fails, `owner == i` fails). The latched stick is then passed through as raw input, so the held stick drives the new defender straight away. With assist on, the chase cone may also bend it toward the carrier (`chase_carrier`, :395-418).

**Chase-cone assist interplay (D-037)** (:422-453). This assist never switches. It only bends a held stick (≥ 48/127) by at most F toward the player's meet point, scaled by `share_weight()` (:364-389). That weight is continuous: 1 if he is the quickest of the 3 team-mates nearest the ball, fading to 0 when one of them is `T_CHASE_SHARE_S` quicker. The code comment says this is "so two near-equal men never make the pull flicker". So the pull is made flicker-proof, but the switch decision beside it is a separate rule that uses a different predictor (`meet_choose`, no bounces, versus `meet_chase`, with bounces and the actual sprint state).

**Team plan interplay (D-030)** (src/sim/team_plan.c:88-107). The AI "ball-winner" is the lowest-time player with 15% hysteresis (`pick`, :42-47). A seat's man counts as winner only "if he's fastest AND engaging (close <4 m, or moving at it)". Otherwise the AI winner is chosen **from non-seat players only**, so an AI team-mate chases or presses whenever the human isn't the clear engaged-fastest. When our team was last in possession (`us == 1`), winner = owner or -1, so no AI chaser is assigned for our own loose ball (:87-88).

### Inferences
- Triggers ranked by how often they can fire, reasoning from the code only. Hand-off and on-gain fire on every own-pass contact and every AI-team-mate gain; they are frequent but each is tied to a ball event. The two cooldown-gated rules can fire at most once per 0.5 s (loose ball) or once per 0.75 s (opponent has it) each. Manual switches are unlimited, one per rising edge.
- The two loose-ball rules differ in kind, not just in tuning. Distance (Off) is position-only, uses a 2 m absolute band and is blocked by any pass. Time (on) is a ratio band [0.5, 2] and is blocked only by our pass to someone.

### Gaps
- I did not trace whether `gain()` can fire `seats_on_gain` in the same tick as a `seats_update` switch. Both are possible in one tick by call order, but I didn't find a concrete scenario in code or tests.
- Multi-seat (2 humans on one team) behaviour was not audited. `seats_on_gain` always takes "the first seat on that team" (seats.c:285-286).

---

## Q2. What was previously decided (DECISIONS, PLAYTEST_FEEDBACK, reports)

### Takeaway
Switching was decided in D-017, amended by D-020 (deflection, latch), D-029 (defender auto-switch, by owner request) and D-037 (time-based loose-ball switch with YSoccer's 0.5x hysteresis). The owner has **never complained about flicker** in the feedback log. His only switching request was "controlling defender should automatically switch". The research reports recommended YSoccer/SWOS-style cooldowns and a time-based rule, but no hard ping-pong limit was implemented as a rule.

### Cited Findings
- D-017 (CP0.2a): "Control follows the ball: hand-off to the receiver on the contact tick (a whiff doesn't switch); no auto-switch while the pass is in flight; never switch away from the ball carrier … Loose ball → auto-switch to the nearest outfield team-mate (2 m hysteresis, 0.5 s cooldown). Q = manual switch (nearest to ball other than you; locks auto 1.5 s)." — docs/DECISIONS.md:123-131
- D-019: "Auto-switch never fires while a header attempt (or kick wind-up) is pending." — docs/DECISIONS.md:155
- D-020: "Deflection mid-pass: switch at once to the team-mate with the best time to the ball; after any auto-switch the inherited stick is ignored (he chases the ball) until it changes/releases, ≤ 30 ticks." It also lists as **not built**: "time-based (0.25 s) switch hysteresis (still 2 m distance)." — docs/DECISIONS.md:181-195
- D-029 (owner request): "Defender auto-switch: when they have the ball (or their pass is travelling) control moves to the team-mate nearest a spot just goal-side of the ball if he's > 3 m closer than the current man (0.75 s cooldown)." — docs/DECISIONS.md:314-315
- D-037: "Loose-ball auto-switch by time to the ball (YSoccer's 0.5x hysteresis), no longer blocked by a pass to the other team. Off = the pre-D-037 sim bit for bit." — docs/DECISIONS.md:432-437
- Owner feedback, CP0.3b (verbatim): "also, controlling defender should automatically switch." — docs/PLAYTEST_FEEDBACK.md:104. The owner's decision for CP0.3: "when an AI team-mate wins the ball, control jumps to him (held stick latched); when our keeper has the ball, the human controls the keeper." — docs/PLAYTEST_FEEDBACK.md:81-82. CP0.2a: "no specific passing/switching notes yet." — :57
- reports/Assisted movement toward the ball.md compares the open engines: SWOS uses a "nearest-man switch, 25-tick cooldown"; YSoccer will "switch only if < 0.5× controlled man's time"; GRF has a "300 ms post-switch" bias (table, lines 15-17). It recommends: "The loose-ball auto-switch should choose by `meet_choose` time … switch only when the challenger needs less than 0.5× the controlled man's time. It should also stop skipping the switch whenever any pass is in flight, and skip it only while a pass targets the seat's own man." (line 85). It also quotes EA's "Auto Switching Move Assistance" as the basis for the latch (line 11).
- reports/Through ball and pass reception.md proposed for deflections: "YSoccer-style hysteresis switches again only for a time advantage of at least 0.25 s", plus an acceptance target "≤ 1 auto-switch per 60 ticks without input" (lines 126, 174).
- reports/Team shape and collaborative AI.md, on GRF's AI possession player: "A challenger takes over only if (candidateTime + 10 ms)/(designatedTime + 10 ms) < 0.85" (line 24). This is the basis for the 15% team-plan winner hysteresis, which applies to the AI winner, not the human's switch.
- reports/Pass receiver selection.md has nothing on active-player switching; its "hysteresis" is about the pass-preview ring (lines 57, 78, 99).

### Inferences
- **Doc vs code mismatch.** D-037 says the time switch is "no longer blocked by a pass to the other team". In code an opponent pass in flight sets `theirs` and the opponent-has-it branch `continue`s before the time rule (seats.c:241-256). The time rule itself skips only for our own pass (:259). So the statement is true of line 259, but in practice an opponent pass is handled by the distance-to-goal-side-spot rule, not the time rule.
- The 0.25 s absolute time-advantage hysteresis suggested in the reception report was not adopted. A pure 0.5x ratio was chosen instead (D-037).

### Gaps
- No playtest feedback entry addresses switch frequency or flicker, so whether the owner perceives flicker today is unknown from the docs. The prompt for this research implies the concern exists, but I found no recorded quote.

---

## Q3. Scenarios that cause frequent switching; dwell time, stick blocking, indicator

### Takeaway
On its own, the time rule's 0.5x ratio is a wide band: switching back needs a 4x swing. Most flicker therefore comes from **rules that hand control back and forth** and from **discontinuities**:
1. AI team-plan chasers keep getting nearer than the human's man and pulling control away.
2. The distance-only, x-offset goal-side rule tracks a moving carrier or pass every 0.75 s.
3. The 9.0 s fallback is a cliff: any found team-mate wins at once.
4. Cooldown-free switches (on-gain, keeper hand-back, deflection, hand-off) can chain.

There is no global minimum dwell, stick input never blocks an auto-switch, and the only switch indicator is a static yellow triangle over the controlled player's head.

### Cited Findings
- **Cooldown coverage.** Only the opponent-has-it rule (`if (s->switch_cool[k]) continue;`, seats.c:244) and the two loose-ball rules (:258, :272) read `switch_cool`. Hand-off (:117), keeper hand-back (:225), manual press (:235), deflection (:185) and on-gain (:289) never read it; they only reset it.
- **Stick does not block.** No auto-switch condition reads `si->move_x/move_y`. The stick is used only to latch after the switch (:32-36).
- **Charge loss.** `do_switch` clears the old player's `kick_charge` and `charge_kind` (:26-27). `busy` covers only an active kick or header action (:227-228), so an auto-switch during a held shot or pass charge on a loose ball discards it. test_passing #4 asserts exactly this (tests/test_passing.c:91-97).
- **9.0 fallback.** `fx tme = mm.found ? mm.t : FX(9.0)` (:261). Challengers must be `found` (≤ 4 s horizon, :64), so any found team-mate has `tb ≤ 4.0 < 4.5 = 0.5 x 9.0` and wins.
- **Minimum time.** A player at the ball gets `t ≥ 0.15 s` (reaction, :80), so near the ball a challenger would need t < 0.075 s, which is impossible: the seat's man is never switched away when he is right at the ball.
- **The human's man is excluded from AI "winner" unless fastest and engaging**, and the AI then picks its own non-seat winner (src/sim/team_plan.c:97-106).
- **A released stick never chases** (D-037 owner decision, docs/DECISIONS.md:424-425). `seats_chase_assist` returns when the stick is below 48 (seats.c:430). The latch is armed only if the stick was held at the switch (:32).
- **Ownership.** A dribbled ball keeps its owner within `T_LOSE_DIST` = 2.2 m (src/sim/player.c:575; tuning.h:116). A sprint knock-on is `T_TOUCH_LEAD_SPRINT` = 1.30 m (tuning.h:120), so an opponent's ordinary dribble keeps `owner >= 0` and stays under the opponent-has-it rule throughout.
- **Indicator.** For each seat's player, render.c draws a yellow downward triangle above the head (`{1, 0.95f, 0.3f, 1}`), plus a charge bar when charging (src/game/render.c:254-262). It is hidden only in `--demo` (`hide_markers = demo`, src/game/main.c:374). There is no switch flash, no next-man preview and no off-screen arrow (grep for offscreen/indicator found none). The white ring on the grass is the pass-receiver preview (D-021), not a switch preview (render.c:209-219). The HUD shows the controlled player's stats (src/game/hud.c:5-27).
- **Camera.** The camera target is 60% the seat's player and 40% the ball, eased with `1 - exp(-3·dt)` (src/game/main.c:517-525). Every switch moves the camera target.

### Inferences (scenarios, reasoned from code, not measured)
1. **AI chaser steals control (loose ball, assist on, stick released).** Human man A is 10 m from a still ball: `tme ≈ 10/6.5 + 0.15 ≈ 1.69 s`. A is not "engaging" if he's standing still and more than 4 m away, so team_plan makes AI mate B the winner and B runs at the ball. The switch fires when `t_B < 0.845 s`, i.e. B within about 4.5 m. Now B is human-controlled. With a released stick he isn't latched, so he decelerates; with a held stick he chases for ≤ 30 ticks. If B (human) is now fastest and engaging (< 4 m or moving at it), he stays winner. If he is still more than 4 m out and standing, A becomes the AI winner, chases, and reclaims control when `t_A < 0.5·t_B` (A within about 1.8 m), or on gain via `seats_on_gain`. **Possible A→B→A in roughly 1–2 s with no human input.** With the stick held toward the ball the human stays engaged and this mostly doesn't happen.
2. **Opponent dribbles past my defenders (all assist levels).** The goal-side spot moves with the carrier at up to sprint speed (~8 m/s). For two defenders, `dme - bd` can change at up to about 2x carrier speed when the carrier runs from one toward the other. A ≥ 3 m advantage then recurs every cooldown, so the ceiling is **one switch per 45 ticks ≈ 1.3 switches/s**. The same AI-winner effect applies: the AI presser gets nearest the spot and takes control. The human's man then stops if the stick is released, and the new AI presser takes over 0.75 s later. Each switch also re-latches the held stick, which drives the new man raw (no meet steering) while an opponent owns the ball.
3. **Opponent pass travelling across our players.** `theirs` is true while their pass is in flight (:242). The spot moves at pass speed (6–12+ m/s), so the nearest-to-spot man changes fast. Switching is limited only by the 45-tick cooldown, so a long cross-field pass can produce 2–3 switches in its flight. The rule doesn't predict where the ball will be, so control follows the ball's current position, not the interception point.
4. **Two defenders roughly equidistant from a loose ball.** Time rule: the 0.5x ratio makes a tie stable, since neither is under half the other. This is the strength of D-037. Distance rule (assist Off): the 2 m band plus the 0.5 s cooldown means a ball rolling at v from A toward B swings `dA - dB` at up to 2v. At 6 m/s the 4 m swing from -2 to +2 takes about 0.33 s, so switching is cooldown-bound at **≤ 2 switches/s** while the ball keeps moving between them.
5. **Ball moving across or away (time-rule discontinuities).** `m.t` is the arrival time at the reachable point closest to him, not his earliest intercept, and it ignores his velocity and facing. Examples:
   - A man sprinting away from the ball is scored the same as one facing it.
   - A fast ball out of the current man's 4 s reach jumps `tme` to 9.0, and any team-mate with a found meet wins at once (the cliff).
   - A lofted ball above 1.9 m for most of the horizon, with bounces ignored in `meet_choose`, can flip `found` as it descends.

   These jumps let the ratio cross 0.5 in one tick. The cooldown then caps it at one switch per 0.5 s.
6. **Ball bouncing between players (scramble).** A gain by an AI team-mate switches at once (on-gain, no cooldown). If he immediately loses the ball to an opponent, the opponent-has-it rule needs `switch_cool == 0`. On-gain set 30, so the next switch waits 0.5 s; after that the 45-tick cooldown applies. If it goes loose and back, the loose-ball rule fires again after 0.5 s. A sequence gain → loss → loose → gain can give **a switch every ~0.5 s**, plus cooldown-free on-gain switches in between.
7. **Keeper.** A seat on the keeper (via on-gain after a catch, or a hand-off to the keeper) is moved to the nearest outfield man as soon as the keeper doesn't own the ball and isn't the pass target (:222-226), regardless of cooldown. After a keeper throw or punt, control jumps at once (distance rule, not time) to whoever is nearest the ball at release. That is often the man nearest the keeper, not the target.
8. **Manual switch toggles between two men.** Press 1 goes to B, the nearest other than A. Press 2 goes to the nearest other than B, usually A again if A was second nearest. Repeated Q **ping-pongs between the two nearest** and never reaches a third man. There is no directional (stick or right-stick) switch and no cycling order.
9. **Visual cost.** Each switch moves the camera target 60% of the inter-player distance (eased about 0.33 s) and jumps the yellow marker, so flicker is visually amplified. There is no transition cue telling the player that control moved.

### Gaps
- No measured switch-rate data for realistic play. The only count is STATUS's "750 switches" in the 10-minute scripted determinism match (docs/STATUS.md:185). That script steers seat 0 at the ball and **randomly holds SWITCH** (re-rolled every 30 ticks, tests/scripted_match.h:16-17), on the 5v5 regression format with assist **Off** (`sim_default_config` memsets assist to 0, src/sim/sim.c:56-68; tests/test_determinism.c:76-79). The seat is also cleared for the last quarter (:21). The figure is dominated by manual mashing and the legacy distance rule, so it says nothing about flicker in the shipped Normal mode.
- Real-time frequencies for scenarios 1–6 are upper bounds from the cooldowns and hysteresis. Actual rates need a headless measurement (e.g. count switches per minute with a neutral or held stick in 7v7 Normal).

---

## Q4. Tests covering switching and what they assert

### Takeaway
Tests cover the hand-off, never switching away from the carrier, the manual switch, the distance rule, deflection follow-through, the defender switch and latch jitter. **No test directly asserts the shipped time-based loose-ball rule** (seats.c:257-270) or any anti-flicker property (switches per second, ping-pong) in Normal assist. The time rule is exercised only implicitly through the 7v7 golden hash.

### Cited Findings
- **tests/test_passing.c** (setup uses `sim_default_config`, so assist Off and the distance rule):
  - #1: control moves to the receiver on contact, not before (`seat_before_kick_ok`, `seat_player[0] == MATE`; :56-58).
  - #2: one-two hand-off chain (:63-75).
  - #3: "never switch away from the ball carrier": SWITCH pressed while owning leaves `seat_player == ME` (:77-82). A manual switch on a loose ball goes to MATE, the nearest (:83-86). "auto-switch doesn't immediately undo a manual one" after 30 idle ticks (:87-89).
  - #4: "auto-switch on a loose ball: the nearer team-mate takes over, the old player's charge is cleared" (:91-97).
- **tests/test_reception.c** (deflection): control moves within 6 ticks of the deflection (`switched_t - defl_t <= 6`). After it, `switches <= 1 && away <= 6`, meaning at most one more switch and at most 6 ticks moving away from the loose ball while holding the stale pass stick (:185-222).
- **tests/test_ai_rules.c** #6 (7v7, default assist Normal): with the opponent on the ball and our striker far up the pitch, control moves to a non-keeper defender within 20 ticks, and that defender is within 10 m of the ball (:131-149). Elsewhere: control jumps to an AI mate who wins it (`s.ball.owner == 3 && s.seat_player[0] == 3`, :63) and to the keeper holding the ball after a restart (:259).
- **tests/test_chase_assist.c** #8: "the latch survives analogue jitter but not a real change". A stick of (102,1) against a latch of (100,0) keeps `latch_ticks > 0`, and (0,127) clears it (:145-156). Other tests there cover the cone, not switching.
- **tests/test_determinism.c**: counts seat-0 switches in the scripted 5v5 match and asserts `switches >= 20` ("switching (manual + auto + pass hand-off) is exercised") and `bad_owner == 0`, the D-022 invariant that the owner is loose, a seat's man, or on a team with no seat (:31-41, :109-115). The 7v7 run (assist Normal) only feeds a golden hash (:44-60).
- **tests/test_sim.c:83** places the ball "nearest to me: no auto-switch" as a setup guard; it is not a switching assertion.

### Inferences
- Missing coverage that matters for flicker:
  - (a) the time rule's 0.5x band: equal times shouldn't switch, and under half should;
  - (b) a rate limit, e.g. "≤ 1 auto-switch per 60 ticks without input", which the reception report proposed but only tests in the deflection follow-through;
  - (c) the opponent-dribbling ping-pong under the opponent-has-it rule;
  - (d) the 9.0 cliff;
  - (e) manual-switch toggling;
  - (f) the AI-winner steal with a released stick.

### Gaps
- I did not check test_ai_match.c / test_scoring.c in depth. My grep found no switch assertions there beyond setup.
