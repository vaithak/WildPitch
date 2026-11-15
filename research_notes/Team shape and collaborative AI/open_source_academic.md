# Team shape and collaborative AI — open-source / academic football AI (code-level notes)

Scope: how existing open-source/academic football AIs keep formation and coordinate (positioning, role
assignment, pressing, marking, support), boiled down to formulas and snippets usable in a deterministic
Q16.16 7v7 sim. Primary sources were read directly from source (GRF engine, helios-base + librcsc, Buckland
Simple Soccer) where possible. Paths below are relative to each repo root.

Source URLs used:
- GRF engine: https://github.com/google-research/football/tree/master/third_party/gfootball_engine/src/onthepitch
  (files: `teamAIcontroller.cpp`, `AIsupport/AIfunctions.cpp`, `match.cpp`, `team.cpp`,
  `player/controller/elizacontroller.cpp`, `player/controller/playercontroller.cpp`,
  `player/controller/strategies/offtheball/default_{def,mid,off}.cpp`)
- helios-base: https://github.com/helios-base/helios-base (`src/player/strategy.cpp`, `src/player/bhv_basic_move.cpp`,
  `src/formations-dt/normal-formation.conf`)
- librcsc: https://github.com/helios-base/librcsc (`rcsc/formation/formation_dt.cpp`, `rcsc/player/world_model.cpp`)
- Buckland Simple Soccer (mirror): https://github.com/wangchen/Programming-Game-AI-by-Example-src/tree/master/Buckland_Chapter4-SimpleSoccer

---

## Q1. Google Research Football (GRF) built-in AI: how each player's target is computed; how many chase

### Takeaway
GRF computes every off-ball player's target as (formation slot, stretched into a ball-following "block"
whose depth/width blend between offense and defense by a continuous `possessionBias`) → blended with a
force-field support spot when in possession → pulled toward a goal-side "defend position" against a
man-marked opponent when not → clamped by a smooth offside-trap line → speed scaled by distance-based
"laziness". Exactly **one** player (the "designated possession player", closest-to-ball with 15% hysteresis)
goes for the ball; at most **2** players (closest to the opponent ball-carrier, *including* man-markers) hunt
the carrier, and only within a role-dependent `huntDistanceThreshold`; plus one temporary "team pressure"
player for 500 ms when the (human) team triggers pressure.

### Cited Findings

**Pipeline / dispatch** — [elizacontroller.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/controller/elizacontroller.cpp)
- Off-ball strategy is picked by the *static* formation role: LB/CB/RB → `defenseStrategy`, DM/LM/CM/RM/AM →
  `midfieldStrategy`, CF → `offenseStrategy`; only applies if `match->GetDesignatedPossessionPlayer() != player`
  and not GK. The designated player instead runs `GetOnTheBallCommands` (when
  `GetTimeNeededToGetToBall_ms() < 1000`).

**Who goes to the ball (anti-swarm core)** — [team.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/team.cpp), [match.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/match.cpp)
- Per team: `designatedTeamPossessionPlayer = AI_GetClosestPlayer(this, match->GetBall()->Predict(0).Get2D(), false);`
- Match-wide designated player switches team/player only with hysteresis on time-to-ball:
  ```cpp
  float timeRating = (float)(candidateTime + 10) / (float)(designatedTime + 10);
  if (timeRating < 0.85f) designatedPossessionPlayer = candidate;
  ```
  i.e. a challenger must be ≥15% faster (+10 ms bias) to take over.

**Formation-slot target: `TeamAIController::GetAdaptedFormationPosition`** — [teamAIcontroller.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/teamAIcontroller.cpp)
- Formation entries are normalized coords in [-1,1] per slot (`player->GetFormationEntry().position`) plus a role.
- Defaults: `depth = 0.45f; width = 0.95f;` tactics: `position_offense_depth_factor 0.9`, `position_defense_depth_factor 0.75`,
  `position_offense_width_factor 0.9`, `position_defense_width_factor 0.8`.
- Ball reference is a time-averaged ball with urgency: `urgencyBias = 1 - NormalizedClamp(|focal - me|, 2, 30)`;
  `ballX = Ball.GetAveragePosition(3500*(1-0.7*urgencyBias)).x`, `ballY = Ball.GetAveragePosition(4000*(1-0.5*urgencyBias)).y`
  (players far from action follow a heavily smoothed ball — a cheap damping trick).
- Possession blend (continuous, not binary):
  ```cpp
  possessionAmountBias = NormalizedClamp(fadingTeamPossessionAmount - 0.5f, 0.3f, 0.7f);
  ballBias = NormalizedClamp((ballX/pitchHalfW) * -side, -0.7f, 0.7f); // 0 own half,1 opp half
  ballBiasBias = (1 - |possessionAmountBias*2-1|) * 0.6f;   // trust ball position when possession unclear
  possessionBias = possessionAmountBias*(1-ballBiasBias) + ballBias*ballBiasBias;
  possessionBias = clamp(possessionBias + (offensivenessBias-0.5f)*0.3f, 0, 1);
  adaptedDepth = depth * (offDepth*possessionBias + defDepth*(1-possessionBias));
  adaptedWidth = width * (offWidth*possessionBias + defWidth*(1-possessionBias));
  ```
- Block centre: `centerX = clamp(ballX*(1-sideFocusStrength) + sideX*sideFocusStrength + offsetX)`, `centerY = ballY`;
  then `centerX *= (1-adaptedDepth)*0.95 + 0.05` and `centerY *= (1-adaptedWidth)*0.9 + 0.1` (keeps the block inside the pitch).
  Bounds: `backX = centerX - adaptedDepth*pitchHalfW*(-side)`, `frontX = centerX + ...`, `lowY/highY = centerY ∓ adaptedWidth*pitchHalfH`.
- Back bound is clamped to the offside-trap line: `if (backXBound*side > offsideTrapX*side) backXBound = offsideTrapX;`
- Mind-set per role (`AI_GetMindSet`, [AIfunctions.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)):
  GK 0, CB 0, LB/RB 0.25, DM 0.25, LM/CM/RM 0.5, AM 0.75, CF 1.0. Used everywhere as a defend↔attack weight.

**Slot → world mapping: `AI_GetAdaptedFormationPosition`** — [AIfunctions.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- Midfield stretch (only for slots near x=0): `stretchBias = curve(clamp(1-|x*1.2|,0,1)) * midfieldFocusStrength; x = x*(1-stretchBias) + (midfieldFocus*2-1)*stretchBias`.
- Linear map into the box: `pos.x = backX + (x*0.5+0.5)*(frontX-backX)`, `pos.y = lowY + (y*-side*0.5+0.5)*(highY-lowY)`.
- yFocus (shift toward ball's flank): `bias = (1 - |yFocus-pos.y|/(highY-lowY)) * (0.2 + 0.8*|yFocus|/pitchHalfH) * yFocusStrength`, with `yFocusStrength = 0.5*possessionBias + 0.2*(1-possessionBias)`.
- microFocus (players near the ball collapse toward it, nearly binary): `dist = |(microFocus - purePos)*(1,0.8)| / 50`; if `dist<1`: `b = curve(1-dist, 0.3)` + small peak at dist≈0.15, `b *= microFocusStrength`; `pos = lerp(pos, microFocusPos, b)` where microFocusPos keeps 40% of the player's own y.
  In defense, microFocus point is `defensiveFocusPos` = ball x shifted 2 m toward own goal and clamped behind `backXBound`.

**Dynamic roles (position swapping) — Hungarian assignment** — `TeamAIController::CalculateDynamicRoles` (teamAIcontroller.cpp)
- Outfield players only (GK removed). Cost = `round(10 * |(pos + vel*0.5) - adaptedFormationPos(slot)|)` (integer!).
- It solves a *bottleneck-flavoured* assignment: sort all costs, then for thresholds `distances[i]` (i stepping by 5)
  set every cost ≥ threshold to 50000 and run `hungarian_solve` (libhungarian, Stachniss 2004); first solution with
  total < 50000 wins → minimizes the max distance first, then sum. Result: `SetDynamicFormationEntry(slot)`.
- Strategies blend static vs dynamic slot by distance to the action:
  `actionDistance = NormalizedClamp(dist(me, designatedPossessionPlayer), 15, 20)`;
  `staticPositionBias = curve(k*actionDistance)` with k = 1.0 (def), 0.9 (mid), 0.8 (off); `desired = static*b + dynamic*(1-b)`.
  → players near the ball (<15 m) use their swapped slot; far players keep their own (stability).

**Support in possession: `GetSupportPosition_ForceField`** (elizacontroller.cpp)
- `attackBias = NormalizedClamp(fadingTeamPossessionAmount-0.5, lo, hi)` (def 0.2–0.9, mid 0.1–0.7, off 0.1–0.6); `desired = lerp(desired, supportPos, attackBias)`.
- Force field of `ForceSpot{origin, Attract/Repel, decay, power}`: base-position attractor weight 0.7 (power scaled `0.3+0.7*NormalizedClamp(dist,0,20)`); opponent repel 0.3 × role multiplier (CB/LB/RB 2.2, DM 2.0, CM/LM/RM 1.6, AM 1.2, CF 1.0); teammate repel 0.4; ball repel 1.0; "flock to possession player" 0.45; attacking-run attractor 2.0 toward opponent goal line.
- One `forwardSupportPlayer` (closest to a point 1.5 m ahead of the ball-carrier, re-chosen every 1500 ms) gets its anchor pushed forward by `(0.3+0.7*mindset)*12 m`; others use a "lane" offset `22 m * mindset^1.5`, attenuated by distance to lane y = `-sign(carrier.y)*8`.
- Offside line used for support: `AI_GetOffsideLine` = x of the *second-deepest* opponent (explicit "one-but-deepest" loop).

**Defensive component / marking** — `PlayerController::AddDefensiveComponent`, `TeamAIController::CalculateManMarking` ([playercontroller.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/controller/playercontroller.cpp))
- Bias of defensive pull: `pow(clamp(C - mindset - fadingTeamPossessionAmount, 0, 1), 0.7)` with C = 1.9 (defenders), 1.5 (mids), 1.3 (forwards).
- Defend point: from the marked opponent's predicted pos (`pos + vel*0.5`), a "shooting point" on the opp→goal line at `shootThreshold` from goal (24 m for the ball carrier, 8 m for others), clipped to the offside-trap line; the marker wants to be ≥ `bufferDistance = 4 m` closer to that point than the opponent; if already late, shift `0.7*slack` toward own goal.
- Man-marking assignment is greedy over the top `numMarkedOpponents = 3` most dangerous opponents (sorted by danger in `Process()`); for each, the non-GK teammate with best `CalculateMarkingQuality` is assigned and removed. Quality = line perpendicular to the player→goal direction (half-length clamp(|toGoal|,4,14)); penalties for opponent distance from the line (`sqrt(NormalizedClamp(d,0,60))*0.5`) and along the line (`sqrt(|u*2-1|)*0.5`); ×0.6 if the opponent is already goal-side ("he's passed us"); final `0.8*q + 0.2*(1 - dist/pitchW)`.

**Defensive line / offside trap** — `TeamAIController::Process`, `ApplyOffsideTrap`
- `offsideTrapX` ("deepestDanger") = the most goal-ward of: `pitchHalfW - (30 + 20*offensivenessBias)`; adapted current ball x
  (ball x + fall-back `offsetX = 20 + 10*(1-offensiveness)` that fades as the ball nears 6 m from goal); ball predicted 700 ms ahead;
  opponent carrier `pos + vel*0.1 + 4 m`; own offside line minus 4 m slack ("despite teammates slacking behind line this much, just hold the line").
- Smooth trap: positions within ±2 m of the trap line are compressed: `x' = front + 2*clamp((x-front)/4, 0, 1)` (front = trap−2) — avoids hard clamps that cause jitter.

**Pressing / how many chase** — elizacontroller.cpp `RequestCommand`
- `huntDistanceThreshold = (10 + (1-mindSet)*10) * (0.5*fatigueInv + 0.5*(1 - normVel)) * (0.3 + 0.7*aiDifficulty)`.
- If team doesn't have best possession, player isn't man-marking, and distance to opp carrier `< huntDistanceThreshold`: only if the player is among `huntingPlayersNum = 2` closest to the carrier (`// remember, this includes players with man marking id`) does it go to `GetDefendPosition(opp)` (perpendicular-bisector intercept point on the opp→goal line) with `forceMagnet = true`.
- `ApplyTeamPressure()`: 500 ms window; `teamPressurePlayer = AI_GetClosestPlayer(team, oppPos + oppVel*0.24 + 1 m goal-side, ..., excluding goalie)` and that player's man-marking target is switched to the carrier.
- Laziness (`GetLazyVelocity`): `lazinessByRole = mindSet + teamPossession*(1 - 2*mindSet)`; `lazinessByPosition = NormalizedClamp(actionDist, 20*(..), 65*(..))`; `v *= 1 - lazinessByPosition*(0.5+0.5*lazinessByRole)` with dribble-speed floor and a short-term "breath" cap. Far-from-ball players jog → reads as shape-holding rather than sprinting swarm.

### Inferences
- GRF's anti-swarm is structural: (1) exactly one designated ball-winner with 15% hysteresis, (2) a hard cap of 2 hunters, (3) everyone else targets a formation slot. The 7v7 equivalent is ~1 presser + 1 cover.
- Everything is continuous lerps of a handful of scalars (`possessionBias`, `mindset`, `actionDistance`) → maps cleanly to Q16.16 with `NormalizedClamp` = `clamp((v-a)/(b-a),0,1)` (precompute 1/(b-a) as fx). `pow(x,0.7)`/`curve()` can be replaced with small LUTs or piecewise-linear.
- `CalculateDynamicRoles` already uses integer costs (`round(dist*10)`), so the Hungarian step is directly portable to integer sim; for 6 outfield players, brute-force over 720 permutations (minimize max then sum) is a deterministic, malloc-free alternative.
- The time-averaged ball (`GetAveragePosition(ms)`) is an easy-to-implement low-pass filter (EMA in fx) that kills formation jitter.

### Gaps
- Exact implementation of `GetAveragePosition`, `curve()`, the danger ranking that sorts `tacticalOpponentInfo`, and `SelectAttackingRunPlayer` scoring were not extracted (functions exist in the same files).
- `GetBestPossiblePassTarget` / pass-target scoring in `_GetPassingOdds` was not read in detail.
- GRF's built-in AI is hand-tuned with floats and is known to be non-deterministic across platforms; numbers are tuning starting points, not validated values.

---

## Q2. RoboCup 2D (HELIOS / agent2d): Delaunay formation, Strategy::getPosition, interception, defensive line

### Takeaway
agent2d gets formation from a lookup: (ball position) → 11 home positions, via Delaunay triangulation over
hand-edited training samples (fedit) and barycentric-style linear interpolation within the triangle containing
the ball. Exactly one player intercepts, decided by comparing *predicted interception steps* (self vs. fastest
teammate vs. fastest opponent); all others go to their home position, clamped behind the offside line.
helios-base (open release) contains **no marking**; marking/mark-lines are in the closed HELIOS team.

### Cited Findings
- Paper: Akiyama & Noda, "Multi-agent Positioning Mechanism in the Dynamic Environment", RoboCup 2007, LNCS 5001 (Springer 2008): Delaunay triangulation maps a focal point (ball) to each player's desired position; evaluated in RoboCup 2D vs. a Normalized Gaussian Network — [Springer](https://rd.springer.com/chapter/10.1007/978-3-540-68847-1_38); a JACIII 2007 paper found Delaunay + linear interpolation gave the best performance of the compared models — [Fuji Press](https://www.fujipress.jp/jaciii/jc/jacii001100080998/); a 2008 TJSAI paper extends it to multi-dimensional input — [J-STAGE](https://www.jstage.jst.go.jp/article/tjsai/23/4/23_4_255/_article/-char/en).
- Formation file format ([normal-formation.conf](https://github.com/helios-base/helios-base/blob/master/src/formations-dt/normal-formation.conf)): JSON-like, `"method" : "DelaunayTriangulation"`, a `role` list (`number`, `name` e.g. CenterBack/SideBack, `type` G/DF/MF/FW, `side` L/C/R, `pair` = mirror partner), then `data` samples `{ "index", "ball": {x,y}, "1".."11": {x,y} }`. normal-formation.conf has **115** samples. Symmetric pairs are stored explicitly (sample 0 ball (54.5,−36) and sample 1 ball (54.5,36) mirror y and swap paired roles).
- Interpolation ([formation_dt.cpp](https://github.com/helios-base/librcsc/blob/master/rcsc/formation/formation_dt.cpp) `FormationDT::interpolate`): find triangle containing ball (`findTriangleContains`); if none, use nearest vertex's sample. Otherwise: intersect line (v0→ball) with segment v1–v2 at point I;
  `r12 = r1 + (r2 - r1) * (d(v1,I)/(d(v1,I)+d(v2,I)))`; then `result_012 = r0 + (r12 - r0) * (dist_0b / (dist_0b + dist_ib))` where `dist_0b = d(v0,ball)`, `dist_ib = d(I,ball)`. Mathematically equals barycentric interpolation. Degenerate case → average of three.
- `Strategy::updatePosition` ([strategy.cpp](https://github.com/helios-base/helios-base/blob/master/src/player/strategy.cpp)):
  focus point is **predicted** ball at the first interception: `ball_step = min(1000, teammateStep, opponentStep, selfStep)`;
  `ball_pos = wm.ball().inertiaPoint(ball_step)`; `f->getPositions(ball_pos, M_positions)`.
  Offside clamp: `max_x = wm.offsideLineX()`; if a teammate reaches the ball within 50 steps, `max_x = max(max_x, trap_pos.x)`; `max_x -= 1.0`; every home pos `x > max_x` → `x = max_x`.
- Situation switching (`Strategy::updateSituation`) selects among formation files (offense/defense/normal) by intercept steps:
  `our_min = min(self_min, mate_min)`; `opp_min <= our_min - 2` → Defense_Situation; `our_min <= opp_min - 2` → Offense_Situation; else Normal. (2-step dead zone = hysteresis.)
- Roles are a static `M_role_number[unum]` table with `Strategy::exchangeRole(unum0, unum1)` swapping two entries — no automatic dynamic assignment in the base code.
- Ball chase decision ([bhv_basic_move.cpp](https://github.com/helios-base/helios-base/blob/master/src/player/bhv_basic_move.cpp)):
  ```cpp
  if ( ! wm.kickableTeammate()
       && ( self_min <= 3
            || ( self_min <= mate_min && self_min < opp_min + 3 ) ) )
  { Body_Intercept().execute( agent ); ... }
  const Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );
  double dist_thr = wm.ball().distFromSelf() * 0.1; if ( dist_thr < 1.0 ) dist_thr = 1.0;
  Body_GoToPoint( target_point, dist_thr, dash_power )
  ```
  Each agent computes the same intercept table locally, so normally only the fastest teammate intercepts (plus anyone ≤3 steps away). The arrival tolerance grows with ball distance (10%, min 1 m) → far players stop fidgeting.
- Tackle is tried first (`Bhv_BasicTackle(0.8, 80.0)`, prob threshold 0.8, 80° dir).
- Own defensive line (`WorldModel::updateOurDefenseLine`, [world_model.cpp](https://github.com/helios-base/librcsc/blob/master/rcsc/player/world_model.cpp)): x of the **second-deepest** own player (first = usually GK). Offside line likewise from second-last opponent / ball.
- HELIOS team description papers (TDPs) mirrored by WrightEagle: [HELIOS 2008 TDP](https://wrighteagle2d.github.io/robocup/2008/2D_TDP_helios.pdf), [HELIOS 2009 TDP](https://wrighteagle2d.github.io/robocup/2009/helios-rc09-tdp.pdf) (not read in detail this session).

### Inferences
- For a 7v7 fixed-point sim the Delaunay approach can be simplified to a **regular grid of ball positions** (e.g. 5×3 or 7×5) with per-cell slot positions, bilinear interpolation in fx — same "ball → shape" learning surface, trivially deterministic, and it is literally a table a player could "learn" from drills. Triangulation of hand-placed samples is only needed if samples are irregular.
- Using the *predicted interception point* (not current ball) as the formation focus is a cheap win against lag.
- "Fastest-to-ball by predicted steps, with an N-step slack" (`self_min <= mate_min && self_min < opp_min+3`) is the canonical anti-swarm rule; it needs a per-tick intercept-time table (one entry per player) — flat array, no allocation.

### Gaps
- Marking ("mark lines"), pressing, and CYRUS/Gliders/WrightEagle internals are not in helios-base; I did not retrieve their code this session. WrightEagle's base release and CYRUS papers would need separate reading.
- `Body_Intercept` internals (intercept table simulation) not extracted.

---

## Q3. RoboCup SPL / humanoid: dynamic role assignment, hysteresis

### Takeaway
SPL teams (B-Human and predecessors) assign the ball-playing role by estimated time-to-reach-ball with
asymmetric thresholds (hysteresis), and position other robots by maximizing a product of [0,1] rating terms
inside each role's Voronoi region; hysteresis also comes from rating the current/last target better.

### Cited Findings
- B-Human positioning ("position role" docs): each robot has a tactical position with a Voronoi region; non-ball roles choose a pose by numerical gradient ascent on a rating that is the **product** of [0,1] terms; rating is zero outside the Voronoi region (exponential falloff near the border); `baseRating` = Gaussian on distance to the tactic's base pose; `fieldBorderRating`; `communicationRating` = Gaussian around last communicated target (hysteresis); `ballDistance` = 1 − Gaussian around ball (keep away from ball). Robots may stand still if a far target is not much better (rating replaces distance thresholds). Defender rates distance to the ball–goal line, spacing from nearest teammate, and per-opponent ideal marking positions weighted by scoring quality; midfielder prefers far from nearest teammate and a preferred ball distance — [B-Human docs: position role](https://docs.b-human.de/master/behavior/position-role/).
- B-Human 2017 paper: one striker always approaches the ball; a supporter waits in an offensive position for rebounds — [Röfer et al. 2018, "B-Human 2017 – Team Tactics and Robot Skills"](https://www.informatik.uni-bremen.de/kogrob/papers/RC-Roefer-etal-18.pdf) (via search summary).
- GermanTeam 2008 XABSL example `decide_current_role`: player becomes supporter when its time-to-reach-ball exceeds the teammates' minimum by 1000 ms, and becomes striker immediately when it is >2500 ms better; striker state uses a 1500 ms state timer — [XABSL gt2008 decide_current_role](https://www.sim.informatik.tu-darmstadt.de/xabsl/examples/gt2008/option.decide_current_role.html), [estimate_time_to_reach_ball](https://www.sim.informatik.tu-darmstadt.de/xabsl/examples/gt2008/option.estimate_time_to_reach_ball.html) (thresholds from search summary; page not fetched).
- McMillen & Veloso 2006: hysteresis by switching roles infrequently or only when one robot is significantly better suited — [McMillen 2006 DARS](https://git.mcmillen.dev/mcmillen/www-home/raw/branch/main/papers/McMillen-2006-DARS-final.pdf) (via search summary).

### Inferences
- Pattern to copy: `chaser = argmin time_to_ball`, but the current chaser keeps the role unless a challenger is better by a margin (GRF: 15% ratio; GT2008: ~1 s absolute; HELIOS: 2-step dead zones). Pick one margin as a tunable in `tuning.h`.
- B-Human's "product of ratings, zero outside region" maps well onto D-008's candidate × utility: candidates = a few sample points around the slot; utility = product of fx terms; region gate = formation cell.

### Gaps
- B-Human's actual role-assignment source (e.g. its Strategy/Tactic modules in BHumanCodeRelease) was not retrieved; docs page didn't describe assignment. Exact current thresholds unknown.
- UT Austin Villa 3D "SCRAM"/dynamic role assignment (MacAlpine, Price, Stone) — known to me but not verified this session.

---

## Q4. Classic game AI: Buckland "Simple Soccer"

### Takeaway
Simple Soccer (5-a-side) uses: home regions (pitch split into a grid, per-team-state region tables), a team FSM
(Attacking/Defending) that swaps the region table, "only the closest team member chases" (plus no receiver
assigned), and a grid of support spots scored by pass-safety, shot possibility, and distance from the
controlling player; one "supporting attacker" (closest attacker to the best spot) runs there.

### Cited Findings (all from [Buckland_Chapter4-SimpleSoccer mirror](https://github.com/wangchen/Programming-Game-AI-by-Example-src/tree/master/Buckland_Chapter4-SimpleSoccer))
- `TeamStates.cpp`: `ChangePlayerHomeRegions(team, NewRegions[TeamSize])`; Attacking: `BlueRegions = {1,12,14,6,4}`, `RedRegions = {16,3,5,9,13}`; Defending: `BlueRegions = {1,6,8,3,5}`, `RedRegions = {16,9,11,12,14}` (pitch region indices; GK fixed).
- `SoccerTeam::CalculateClosestPlayerToBall`: per tick, squared distance of each player to ball; store min → `m_pPlayerClosestToBall`.
- `FieldPlayerStates.cpp` `ReturnToHomeRegion::Execute`: chase only `if (player->isClosestTeamMemberToBall() && (player->Team()->Receiver() == NULL) && !player->Pitch()->GoalKeeperHasBall())`; when inside home region (`Region::halfsize`), switch to `Wait` and target own current position.
- `SupportSpotCalculator`: grid `NumSweetSpotsX 13`, `NumSweetSpotsY 6` over 90%×80% of the pitch, only in the attacking half (`x < numX/2 - 1`). Score per spot = 1.0 + `Spot_CanPassScore 2.0` if `isPassSafeFromAllOpponents` + `Spot_CanScoreFromPositionScore 1.0` if `CanShoot` + `Spot_DistFromControllingPlayerScore 2.0 * (Optimal - |Optimal - dist|)/Optimal` with `OptimalDistance = 200.0` (pitch units). Recomputed at `SupportSpotUpdateFreq 1` (Hz, via a Regulator). Unused weights: `Spot_ClosenessToSupportingPlayerScore 0.0`, `Spot_AheadOfAttackerScore 0.0`.
- `SoccerTeam::DetermineBestSupportingAttacker`: among players with `Role()==attacker` excluding the controller, pick min squared distance to the best support spot.
- Params: `WithinRangeOfHome 15.0`, `WithinRangeOfSweetSpot 15.0`, `BallWithinReceivingRange 10.0`, `ChancePlayerAttemptsPotShot 0.005`.

### Inferences
- The support-spot grid + score is already candidate × utility (D-008). In 7v7, a coarse grid (e.g. 8×5 in the attacking 2/3) re-scored every N ticks (Buckland: 1 Hz) is cheap; time-slice to keep per-tick cost flat.
- Buckland's tie-breaking by "closest" without hysteresis can flicker between two equidistant players — add the margin from Q3.

### Gaps
- Game AI Pro / AI Game Programming Wisdom chapters on team coordination (role slots, coordination tokens, influence maps) were not retrieved this session.

---

## Q5. Preventing "ball-chasing swarm" in team-sport AI

### Takeaway
Every system surveyed solves swarming with the same three mechanisms: (a) a single designated ball-winner
chosen by (predicted) time-to-ball with hysteresis, (b) a small hard cap on additional pressers/markers
(GRF: 2 hunters, 3 marked opponents), and (c) everyone else targets a ball-conditioned formation slot, with
speed/arrival tolerance relaxed by distance from the action.

### Cited Findings
- GRF: one designated possession player with `timeRating < 0.85f` switch; `huntingPlayersNum = 2`; `numMarkedOpponents = 3`; laziness by distance — [elizacontroller.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/controller/elizacontroller.cpp), [match.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/match.cpp), [teamAIcontroller.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/teamAIcontroller.cpp).
- HELIOS: intercept only if fastest (`self_min <= mate_min && self_min < opp_min + 3`) or very close (`self_min <= 3`); else go home; `dist_thr = max(1, 0.1*ballDist)` — [bhv_basic_move.cpp](https://github.com/helios-base/helios-base/blob/master/src/player/bhv_basic_move.cpp).
- Buckland: only `isClosestTeamMemberToBall()` and no receiver assigned — [FieldPlayerStates.cpp](https://github.com/wangchen/Programming-Game-AI-by-Example-src/blob/master/Buckland_Chapter4-SimpleSoccer/FieldPlayerStates.cpp).
- B-Human: non-ball roles have a `ballDistance` rating (1 − Gaussian) that actively repels them from the ball — [B-Human docs](https://docs.b-human.de/master/behavior/position-role/).

### Inferences (implementation sketch for WildPitch, deterministic fx)
1. Per tick (sim): `ticks_to_ball[i]` for all players (flat array, simple constant-speed intercept estimate).
2. Chaser per team = argmin with hysteresis: keep current unless `t_new*FX(1.0) < t_cur*FX(0.85)` (GRF), and `chaser` stored in SimState (hash it).
3. Out of possession: presser = chaser; cover = 2nd-closest to carrier goal-side (cap = 2 total, GRF); markers = greedy over top-K dangerous opponents (K≈2 in 7v7) with GRF marking-quality or simpler "goal-side of man, distance + angle" score.
4. Everyone else: slot target = formation table lookup at (filtered/predicted ball), depth/width scaled by possessionBias, back line clamped to `offsideTrapX` with the GRF ±2 m soft compression, swaps via Hungarian/brute-force on integer costs only for players within ~15–20 m of the ball.
5. In possession: one forward-support player + support-spot scoring (Buckland) blended with slot by `attackBias`.
6. All "learned brain" hooks are then just the utility weights (mindset, lerp factors, rating weights).

### Gaps
- No primary-source quantitative study comparing swarm-prevention methods was found; the claims above are from code inspection, not evaluation.
