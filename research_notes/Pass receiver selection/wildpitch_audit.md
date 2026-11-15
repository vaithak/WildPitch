# WildPitch pass receiver selection (`pass_pick`): code audit + empirical sweep

Sources are local: code at file:line (repo `/home/vaithak/WildPitch`, current working tree, `build/libwp_sim.a`
rebuilt 2026-11-15), and harness outputs (H1-H4 below). The harness programs live in the session scratchpad
(`/tmp/claude-1000/-home-vaithak-WildPitch/6ee2a09e-6816-4cfe-887a-b9e6ea79e320/scratchpad/selection/`: `sel.c`,
`ex.c`, outputs `opp1_s60.txt`, `opp1_s300.txt`, `opp0_s300.txt`, `ex.txt`). That folder can be wiped, so the method
is written out here so it can be rebuilt.

**Harness method (sel.c).** `sim_init(sim_default_config())` with the real 5v5 (40 x 25 m pitch), phase PLAY,
and seat 0 on carrier c ∈ {3 midfielder, 4 striker, 1 defender}. The carrier is teleported to a start point
(x ∈ {-14,-8,-2,4,10}, y ∈ {-8,0,8}; team 0 attacks +x), with the ball 0.4 m ahead. He stands still for SETTLE
ticks (60 or 300) so that team.c can move the team-mates. He then dribbles 150 ticks along one of 10 8-way
paths: straight R/U/D, zig-zags, or a cut every 50 ticks. Team-mates and the opposition are moved by the real
`team_input` (team.c). With "opp off", team 1 is re-parked off the pitch every tick.

Every 10 ticks of owned dribble, for each of the 8 keyboard directions (R = +x = attack, U = +y, and so on):
- record `pass_pick()`, plus a mirror of its candidate maths for every mate;
- compute the "lane" = the smallest distance from any opponent to the segment from the ball to the receiver's
  predicted point;
- simulate the real tap on a copy of the state: hold the key with J for 3 ticks, release, keep holding the key,
  and run until someone owns the ball, the ball goes out, or 300 ticks pass.

Across the sweep that is 5,328 samples, 42,624 key presses and 61,680 owned ticks per configuration. Per-tick
picks for all 8 keys feed the flicker stats.

Variant rules were scored offline on the same snapshots with a double-precision mirror of `pass_pick`. The mirror
agrees with the fixed-point original on 99.74% of presses (H1).

**Constructed scenes (ex.c)** use `setup_empty` (statues): the cliff, the exchange rate, the keeper cone, and
prediction versus current position.

- H1 = `sel 1 0 60` (opposition on, settle 60)
- H2 = `sel 1 0 300` (opposition on, settle 300)
- H3 = `sel 0 0 300` (opposition off)
- H4 = `ex` (constructed scenes)

Settle 60 versus 300 changed most headline numbers by at most about 2 points. The exceptions:
- R-key pick rate at x = 4/10 (see below);
- R-key pick rate for carriers 1 and 3: 70% → 78%;
- "stick at current bearing → him": 82.2% → 87.2%.

H1 still contains team-mates who are settling into position, so where H1 and H2 differ, **H2 is the cleaner
baseline**. Opposition on versus off changes only the lane and interception numbers, because team.c places
team-mates relative to the ball, not relative to opponents.

**Caveats for every number below:**
- The opposition is passive ("cones that move a bit", team.c:4-5,73-75), so interception rates are a **lower
  bound**.
- Team-mate geometry is the CP0.2 placeholder (team.c), so the percentages describe today's off-ball shape, not
  a final AI.
- Samples are heavily autocorrelated: 10-tick spacing inside 150-tick dribbles.

---

## 1. What is the current algorithm, exactly?

### Takeaway
`pass_pick` picks the outfield team-mate who scores lowest on `10·(angle off the stick, °) + 8·(distance, m)`,
plus 16 per metre beyond 18 m. Each mate is judged at a crude predicted position. The hard filters are a 55° cone
and 2-26 m. The keeper is considered only when no outfield mate fits, and then only within 20°. The direction is
the stick at the **J-release tick**, or the body `facing` if the stick is released. The receiver is locked then and
not re-picked at contact. The game shows no receiver preview.

### Cited Findings
- Two passes over the team: outfield mates first, the keeper only if no outfield mate qualified
  (`for (pass < 2 && best < 0)`) — src/sim/player.c:69-72.
- Coarse pre-filter: skip if the current distance d0 > T_PASS_MAX_DIST + 4 m (hard-coded `FX(4.0)`) —
  player.c:74.
- Prediction: each mate is judged at `pos + vel · d0/12`, a flight time assuming 12 m/s (hard-coded `FX(12.0)`).
  The point is clamped to 1 m inside the pitch — player.c:75-77. The real pass is then solved separately with the
  exact roll tables and paced to arrive at 5.5-9 m/s (pass_solve, player.c:189-205).
- Distance filter: predicted distance < 2 m (hard-coded) or > T_PASS_MAX_DIST (26 m) → not a candidate —
  player.c:80; tuning.h:81.
- Angle filter: |angle(prediction) − stick| ≤ T_PASS_CONE_DEG = 55°. For the keeper it is a hard-coded 20° —
  player.c:81-84; tuning.h:79.
- Score: `off_deg·10 + dist_m·8`, plus `(dist − 18 m)·16` beyond T_PASS_PENALTY_M; lowest wins — player.c:86-88.
  - The exchange rate is **1° ≈ 1.25 m** below 18 m and 1° ≈ 0.42 m beyond it (24 pts/m).
  - The code comment at player.c:85 says "1 degree off ~ 0.8 m further away", which is inverted: 1 m ≈ 0.8°.
- There is no term for lane openness, opponents, forward progress, receiver facing or marking. Opponents are never
  read: the loop runs only over `p->team`'s players (player.c:70).
- Direction source: `dir = has_aim ? atan2(stick) : p->facing` — player.c:115. With no stick the pick follows the
  body facing, which lags behind cuts because of the turn rate (player.c `move()`).
- Timing: buttons fire on **release** (player.c:331). A tap is ≤ 12 ticks (T_PASS_TAP_TICKS, tuning.h:84); a
  longer hold is a through ball. `make_kick` calls `pass_pick` with the stick at that tick (player.c:121 tap,
  player.c:132 through/lob) and stores the receiver in `k->to` (player.c:125,136). Only the aim and weight are
  recomputed at contact (D-017: "Aimed and weighted at the CONTACT tick"), not the receiver.
- Through ball and lofted pass use the same `pass_pick`, and headers use it too (header.c:95). Nobody in the cone:
  - **tap** → an untargeted 12 m/s ground ball (T_PASS_SPEED, tuning.h:116) along the stick (DECISIONS D-017);
  - **through-ball hold** → 12-20 m/s (tuning.h:91-92).
- Keyboard: W/S/A/D or arrows give move_x/move_y ∈ {−127, 0, 127} (src/game/main.c:46-48), so 8 directions 45°
  apart. The analogue stick is normalised (`stick_dir`, player.c:24-28). There are no dead-zone or snapping rules
  in the sim.
- No receiver preview: `grep pass_target|kick.to src/game/*.c` finds nothing, and no source file other than
  player.c and header.c calls `pass_pick`.
- DECISIONS D-020 records "none beyond 26 m, scored down beyond 18 m" and "Known: a team-mate beyond 26 m isn't
  picked (untargeted 12 m/s ball instead)". It also lists "pass-lane scoring (passes still hit cones standing in
  lanes)" as not built yet (CP0.3) — docs/DECISIONS.md D-020.

### Inferences
- Selection and the pass solve use different flight models. The sign of the selection-prediction error depends
  on distance and was not measured. Long taps average faster than 12 m/s (H4: an 18.4 m tap was owned 1.32 s after
  J went down, wind-up included, so about 14-15 m/s on average). Short taps arriving at 5.5 m/s are probably slower
  than 12 m/s.
- An 8-way keyboard has at most 22.5° between a mate's bearing and the nearest key, which costs up to 225 points:
  the same as **28 m** of distance. With keyboard input the angle term therefore dominates and distance only breaks
  near-ties (see §2a).
- Because the pick is locked at release, the receiver the human "saw" can differ from the one the ball goes to
  only through what happens in the ≤ 200 ms tap window. The 4.5% flicker figure in §2e quantifies that.

### Gaps
- No data on how the human *perceives* the intended receiver without a preview UI. This audit uses geometric
  proxies.

---

## 2. Empirical sweep: where is the pick "wrong"?

### Takeaway
**Coverage, not mis-ranking, is the biggest gap.** About **37% of keyboard presses pick nobody**, and **~86% of
those untargeted balls go out of play**. About **22% of picks have an opponent within 1 m of the lane**, and **29%
of those are intercepted**, against **0.4%** for other picks. Mis-ranking (a nearer, similarly aligned mate passed
over) is rare at about 2.5% of picks. Forward/backward confusion is rare (0.5%). Flicker within a 200 ms tap is
modest (4.5%).

### Cited Findings
**Overall (H1, opposition on, settle 60):**
- Picks: 62.9% of key presses; the keeper 1.0%; nobody 37.1%.
- Forward-component keys (R/UR/DR) pick nobody 58.8% of the time (H1), 56.5% with settle 300 (H2).
- Analogue: 62.3% of all stick angles (2° steps) produce a pick (H1).

**Pick rate by carrier and key (H2):**

| Carrier | R (forward) | UR / DR | U / D | UL / L / DL (backward) |
|---|---|---|---|---|
| Striker (4) | 0% | 20% / 15% | 59% / 58% | 93% / 100% / 95% |
| Midfielder (3) | 78% | 53% / 47% | 57% / 58% | 93% / 100% / 95% |
| Defender (1) | 78% | 53% / 47% | 59% / 44% | 93% / 85% / 46% |

The defender's L (85%) and DL (46%) show the back-pass coverage hole on his side: the outfield mates are ahead or
across, and the keeper cone is only 20°.

Cause: team.c puts the striker 7 m ahead of the ball and on the *opposite* side (y = ∓3.5) (team.c:81). It puts
the midfielder 2.5 m *behind* and 6 m to the side (team.c:82). The striker carrier therefore has **no forward
option by construction**.

**R-key pick rate by start x (H2):** 61% / 63% / 64% / 54% / 18% for x = -14 / -8 / -2 / 4 / 10. With settle 60
(H1) the last two were 41% / 4%: near the opponent goal the striker target is clamped at x = 18.5 (team.c:89),
which leaves the carrier with a striker more than 55° off.

**(a) Nearer mate ignored for a slightly better-aligned one.** Counted once per pick, keeper excluded: an in-cone
outfield mate ≥ 5 m nearer whose angle is at most 15° worse was passed over in **2.6% of picks** (H1; 2.3% H2;
2.2% H3).
- Typical in-game case (H1 verbose, carrier = midfielder 3): key UL picked defender 2 at 13.2 m, 33° off, over
  defender 1 at 3.4 m, 42° off. Scores: 330+106 = 436 against 420+27 = 447.
- Constructed (H4): far mate at 20 m on 5° (score 242) against a near mate at 6 m on bearing B. The near mate
  wins only while B ≤ 15°; at B = 20° the 20 m mate wins. A 4 m mate at 30° loses to the 20 m mate at 5°.
- Runner-up within 50 points (≈ 5° or ≈ 6 m) of the winner: 15.6% of picks that had a rival (H1); 13.6% (H2).
  These are the near-ties a human cannot predict.
- Only 10,878 of about 26.8k picks (≈ 41%) had an outfield runner-up in the cone at all (H1). Most key presses have
  a single candidate.

**(b) Passes into a defender.**

| Lane (nearest opponent to segment) | Picks (H1) |
|---|---|
| < 0.5 m | 2,874 |
| 0.5-1 m | 3,097 |
| 1-2 m | 5,258 |
| 2-4 m | 6,306 |
| ≥ 4 m | 9,255 |

- 22.3% of picks have lane < 1 m.
- Simulated outcomes of targeted taps (H1): received 24,507 · other mate 425 · **opponent 1,730 (6.5%)** · out 20.
- With lane < 1 m: received 4,065 · opponent 1,654, so **28.9% are intercepted**. With lane ≥ 1 m:
  76 / 20,952 ≈ **0.36%**.
- With opposition off (H3): 0 opponent touches, 26,847 received.
- Of the blocked picks, only **578 / 5,971 (~10%)** had an alternative in the cone with lane > 2.5 m (H1).

**(c) Backward picks on forward keys.** A forward-component key picked a mate > 1 m behind the passer in 0.6% of
forward-key presses (H1: 99 cases; H2: 85).

**(d) No pick though a mate is near.**
- 8.3% of all key presses picked nobody while an outfield mate was 2-26 m away and within 70° (H1).
- Outcome of those untargeted balls (H2): out 2,474 · loose 399 · opponent 240 · own mate 30, plus 236 that became
  targeted because geometry changed during the 3-tick tap.
- In general, untargeted taps (H1): **out 13,092 / 15,225 kicked = 86%**, loose 1,207, opponent 836, team-mate 90.
  This is structurally expected rather than a fluke: a 12 m/s ball rolls about 18 m (D-015), the half-width is
  12.5 m, and two-thirds of the start points were at y = ±8. The exact figure depends on that geometry.
- No-key gaps in the circle: every bearing is ≤ 22.5° from a key, so the 55° cone always overlaps two keys. Gaps
  come from filters and from ranking, not from cone width.
- 8-way reachability of each outfield mate 2-26 m away (H2, n = 15,984):
  - picked by the **nearest** key: 92.9%;
  - by **some** key: 96.8% (so 3.2% of mates are shadowed for the keyboard);
  - by some analogue angle: 98.5%.
- Analogue, stick pointed exactly at the mate's *predicted* bearing → him: 97.2%. At his **current** (visible)
  bearing → him: only **87.2%** (H2; 82.2% H1).
- Constructed (H4): a mate at (0,4) relative to the passer at (-10,0), running +y at 6 m/s. A stick at his current
  bearing (21.8°) picks him, but key R picks the other mate (2, at 32° off), because mate 4's predicted point has
  moved to about 43°.

**(e) Flicker / stability.**
- Pick for the same key differs from the pick 12 ticks (200 ms, a tap) earlier: **4.6%** (H1), 4.5% (H2).
- Per-tick change rate: 0.52%.
- No-stick case: the facing-based pick differs from the held-key pick on 2.7% of owned ticks, and on **8.9-9.4%**
  in the first 15 ticks after a dribble cut (n = 8,700, H1/H2), because facing lags the stick.

**(f) The 26 m cliff.** It is not reached in the team.c geometry: 0 outfield mates were at 26-30 m (H1-H3), and the
"cone mate beyond 26 m" no-pick case was 0.2-0.3%, the keeper only. Constructed (H4), passer at x = -12, striker
straight ahead:
- picked at 25.9 m from the passer;
- untargeted at 26.3 m;
- the untargeted 12 m/s ball was owned by that mate only after **5.6 s**, against 1.9 s when targeted at 25.9 m.
  The mechanism is inferred, not traced: the ball stops short (about 18 m of roll), then the loose-ball
  auto-switch lets the human's player chase it.

**(g) Keeper fallback.**
- Keeper picked on 0.9-1.0% of presses, **only with key L** (backward), 393-408 times. Outcomes (H2): received 368,
  other mate 11, out 5, loose 6, opponent 3.
- Constructed (H4), passer at (-8, y), keeper at (-19.2, 0), no outfield mates:
  - keeper reachable from L for bearing offsets 0-19.7°;
  - from DL for 28-35°;
  - **unreachable from any key at 24.1°**.
- The analytic dead band is bearings 20-25° from both neighbouring keys: 10° of every 45°, i.e. **22% of keeper
  bearings cannot be picked from a keyboard**.

### Inferences
- From the player's point of view, the dominant "wrong" outcome is **pressing toward a team-mate and having the ball
  roll out of play**, not a wrong mate. It happens mainly when the mate is 55-70° off the key, or behind/beside
  for forward keys. Lowering the 37% untargeted rate (and making untargeted balls less punishing) matters more than
  re-ranking.
- The lane problem is real but **mostly not fixable by ranking**. Only about 10% of blocked picks had an open
  alternative in the cone, because with 4 mates there is usually only one candidate per key. A lane term mainly
  helps by changing *what happens* when the lone candidate is marked: a weaker or more lofted ball, an untargeted
  pass into space, or simply letting it fail visibly.
- 8-way input is not short of cone width: 92.9% of mates are picked by the nearest key. Its real weaknesses are:
  - the angle-heavy score: 1 key step of 45° = 450 points = 56 m;
  - judging at the predicted point rather than the visible one, which costs about 10-15% of "point straight at
    him" analogue attempts;
  - the 20° keeper cone, which is narrower than half the 45° key spacing.

### Gaps
- No human-intent ground truth: "intended" is proxied by geometry (nearest key, nearer mate, current bearing).
- Interception counts use passive opponents only. Active pressing in CP0.3 will raise them.
- Sideways keys (U/D) landing on a backward mate were not measured; flag (c) covered forward-component keys only.
- The lane is measured to the *selection* prediction point, not the solved meeting point (pass_solve is static in
  player.c and not exposed).
- Through-ball and lofted-pass selection were not swept separately. They share `pass_pick`, so the coverage and
  ranking findings apply, but the lane and outcome numbers do not.

---

## 3. Improvement ideas, with offline evidence

### Takeaway
On the same 42.6k presses:
- widening the cone to 67.5° raises the pick rate from 62.9% to **70.0%** and changes 8.2% of picks;
- a lane penalty alone barely matters (1% of picks change; blocked share 22.3% → 21.3%), because rival candidates
  are rare;
- an angle dead-band of ±22.5° (ignore angle error within one key step) slightly *hurts* nearest-key reachability
  (92.2% → 89.7%).

The bigger levers are coverage, what an untargeted ball does, stability or a preview, and judging at the visible
position.

### Cited Findings
Offline variants (H1, opposition on; mirror ≡ `pass_pick` on 99.74% of presses):

| Variant | Pick rate | Lane < 1 m (of picks) | Picks changed | Nearest-key reach |
|---|---|---|---|---|
| Baseline | 62.9% | 22.3% | — | 92.2% |
| Cone 67.5° | 70.0% | 22.0% | 8.2% | 92.2% |
| Lane penalty (100 pts/m below 1.5 m) | 62.9% | 21.3% | 1.0% | 91.6% |
| Dead-band 22.5° | 62.9% | 22.0% | 2.0% | 89.7% |
| Cone 67.5° + lane | 70.0% | 21.1% | 9.0% | 91.6% |
| All three | 70.0% | 20.0% | 10.9% | 88.3% |

H2 (settle 300) gives the same picture: 63.6 → 70.6% pick rate, and 92.9 → 89.2% reach for all three.

### Inferences (concrete ideas, ranked by evidence)
1. **Wider cone for digital input only (≈ 65-70°, analogue stays 55°)**, or a secondary pass that accepts
   55-70° when nobody is inside 55°. This turns about 7 points of presses from untargeted (86% of which go out) into
   targeted ones. As measured, the plain 67.5° cone also swaps about 0.8% of presses that already had a pick:
   8.2% changed against 7.1 points of new picks and 0.3% mirror noise. A two-stage rule (55° first, then 55-70° only
   if nobody is in 55°) would not swap existing picks, but that variant was not measured. Detect digital input by
   |move_x|, |move_y| ∈ {0, 127}, or pass a flag in WpSeatInput.
2. **Make the untargeted fallback safer.** Today it is a 12 m/s ball that leaves a 40 x 25 pitch 86% of the time.
   Options:
   - weight it to stop short of the touchline or goal line;
   - fall back to the nearest mate within ~90° when the stick has a component toward him;
   - lower T_PASS_SPEED for untargeted taps.
3. **Judge receivers at the visible position, or blend toward it** for the angle term. Keep the prediction for
   distance and the solve. "Point at him → get him" drops from 97% to 87% with the current 12 m/s-flight
   prediction. The selection prediction also uses a different flight model from pass_solve, and the size and sign
   of that error by distance were not measured.
4. **Stability:** hysteresis or a sticky receiver. 4.5% of taps would change receiver within the 200 ms before
   release, and 8.9-9.4% of no-stick picks disagree with the held key just after a cut. Options:
   - keep the last displayed or last-picked receiver unless a rival beats it by a margin (e.g. 50 points ≈ 5°);
   - use the stick direction from press time, or the most recent non-zero stick, instead of `facing` when the
     stick is released (stick history);
   - show a **receiver preview** while J is held. The game has none today. It is the standard way to make
     selection legible, and the hold-to-through-ball mechanic already gives a hold window.
5. **Keeper cone ≥ 22.5° (e.g. 25°)** so every keeper bearing is reachable from a key. Today 22% of bearings are
   dead for a keyboard.
6. **Lane openness:** include it, but expect small ranking gains (≈ 1-2% of picks change, ≈ 1-2 points fewer
   blocked picks) while team-mates are 4 and the opposition is passive. Its larger value is in pace or loft choice
   for the lone candidate (29% of < 1 m lanes are intercepted, against 0.4% otherwise). It is also an input the
   CP0.3 utility AI (D-008) needs anyway.
7. **Re-balance the score for digital input.**
   - Fix the inverted comment.
   - Consider capping the angle term at one key step: the "dead-band" variant cuts reach, so the better route is
     a smaller angle weight (e.g. 5 points/°) plus a small forward-progress bonus.
   - These are untested here; only the variants above were measured.
8. **Move the hard-coded constants into tuning.h** (CLAUDE.md rule "all gameplay tunables in tuning.h"): the 12 m/s
   prediction flight, the 2 m minimum, the +4 m pre-filter and the 20° keeper cone.
9. **Forward-progress preference:** low priority on the evidence. Backward picks on forward keys are only 0.5%.
   The forward problem is the *absence* of a forward candidate (striker 0% on R), which is a team.c/AI positioning
   issue, not selection.

### Gaps
- The variants were judged by geometric proxies, not by simulated outcomes. Re-running sim_tap with each variant
  plugged into the sim would need a code change, which this audit was not allowed to make.
- The angle-weight and forward-bonus variants were not measured.
- Behaviour with active defenders and with CP0.3 off-ball runs is unknown.
