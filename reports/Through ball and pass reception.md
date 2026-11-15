# Aim passes where runners will be

WildPitch's passing feels wrong for three reasons, and none of them is the ball physics. Passes are aimed at where the receiver *is* rather than where he *will be* when the ball arrives. Any stick movement while the ball travels *cancels* the receive instead of steering it. And the control radius (0.75 m) is narrower than the 0.78–0.82 m by which a passing ball actually misses the foot. The fix is the pattern used by every engine whose code we read, and by coaching itself. First, solve for the meeting point on the runner's predicted path and weight the ball to arrive there at about his running pace; HELIOS and GRF do this in code, and coaching says it as "into his stride without slowing". Second, while the ball travels, read the stick as a *preference among reachable meeting points* and as the first-touch direction, never as a cancel. GRF's receiver "magnet" blends stick and assist; eFootball and FC 26 buffer the stick to choose the touch. Third, widen the clean control reach to about 1.0 m, the figure GRF, RoboCup and WildPitch's own dribble reach all land on, and add a 1.0–1.3 m stretch that costs touch quality. A float prototype of the proposed solver, using WildPitch's exact roll law, puts every through ball from the playtest's open-pitch geometry in front of the runner. Those balls arrive at 5–12 m/s, launch harder than the tap pass to the same man, and close on the receiver at no more than 9.4 m/s. Near goal the picture changes: when the striker is already 12–15 m upfield, as in the live playtest, no in-stride ground ball exists, and he has to pull up to receive it. That is well under the 16 m/s deflection limit. One honest constraint shapes the design: on a 40 m pitch with this drag, a ground ball cannot catch a sprinter far ahead before the byline. The solver therefore needs a "short room" rule, and the through-ball bar can only control *how deep* the ball goes, not raw launch power.

## Nine playtest faults trace to three wrong assumptions in the code

The headless playtest measured the faults precisely, and each traces to a specific line (`/home/vaithak/WildPitch/research_notes/Through ball and pass reception/playtest_findings.md`).

**Through balls aim at a fixed point.** `through_velocity` (`src/sim/player.c:165-176`) aims at the receiver's position *at contact*, plus a lead of 2.5–10 m along a fixed blend of "toward goal" and his run. It weights the ball to arrive at `T_THROUGH_ARRIVE` = 3 m/s. A sprinter covers 11–20 m during the 1.4–2.5 s flight, more than the maximum lead, while the ball averages only 5.5–7.4 m/s. The ball is therefore **2.9–11.2 m behind the runner** at closest approach, at every hold length. In 20 live through balls the receiver's speed at reception was **0.0 m/s** every time. The assisted run in `meet_point` (`src/sim/seats.c:107`) sends him to where the ball slows to 3.5 m/s, even when that point is behind him.

**Tap passes lead by 0.25 s for a 0.9–2 s flight.** `pass_velocity` (`player.c:154-161`) leads by `T_PASS_LEAD_S` = 0.25 s. The assist then picks the reachable point *closest* to the receiver, so a forward runner stops and steps back. He was measured at 0.0–0.6 m/s at reception after standing 0.45–0.67 s.

**Any stick change ends assistance for good.** A stick change of more than 48 units from the hand-off stick (`seats.c:62-65`) permanently ends assisted receive. Every 8-way key change is 90–180 units, and releasing the stick also counts. Nothing then pulls the receiver to the ball. Possession needs `T_CONTROL_REACH` 0.75 m (`tuning.h:51`), and because players move before the control check while the ball moves after it, the ball passed at **0.78–0.82 m and was missed by 3 cm**. That is the owner's complaint, reproduced. A short sideways nudge mid-flight missed **6/18** receptions after a tap. Holding a new direction from the hand-off missed **13/15, 18/18 and 9/9** (toward the ball, sideways, forward). Live, holding forward after a tap gave **0/8** receptions, and releasing the stick after a through ball gave **0/20**.

The remaining findings follow from these three. The minimum through ball (hold 10) launches *softer* than a tap: 9.9 vs 10.7 m/s to a standing mate 10 m away. Through balls clamp to 1.5 m inside the byline and arrive at 3 m/s, so every unreceived one rolled out. Ground passes arrive at a fixed 5.5–6.2 m/s whatever the distance, taking 1.07 s for 8 m and 1.98 s for 20 m, and `pass_pick` has no maximum distance. A deflection off a passive opponent leaves the striker sprinting the wrong way under the stale pass stick, and auto-switch then ping-pongs at ~t+96 and ~t+228. One sprint into a firm pass hit a relative speed of 17.1 m/s, above `T_CONTROL_MAX_REL` 16, and bounced off. One-twos fail because the placeholder midfielder parks behind the ball (`team.c:193`), which is CP0.3 territory. Three things felt right and must be protected: the first touch once controlled goes the stick's way within one tick, control hands over on the contact tick, and taps to a standing mate are clean.

## Every working model solves for a meeting point, then weights the ball to it

**Coaching is unanimous on the goal.** Play the ball "into space ahead of the receiver so they don't have to slow down or change their run". Hit it hard enough that defenders cannot reach the lane but softly enough to control ([Coaching American Soccer](https://coachingamericansoccer.com/?p=2380); [Sport Session Planner](https://www.sportsessionplanner.com/s/AJxJh); [Cupello](https://my.cupello.com/coaching/advice/playing-through-ball-techniques-or-skills)). FIFA's Training Centre adds that balls in behind are usually played on a **diagonal** into the runner's path, and that with the right timing, direction and weight they are "basically undefendable" ([FIFA Training Centre](https://fifatrainingcentre.com/en/game/game-insights/balls-in-behind-the-defence.php)). No coaching source gives a lead in metres. The research notes derive it instead: lead ≈ runner speed × ball travel time, which comes to about 3–7 m in small-sided play. The notes also infer an arrival pace at about the runner's speed, 6–9 m/s for sprinting outfielders. Both are inferences, not measurements.

**Commercial games hide the solver but describe its contract.** FIFA 21's through-ball assistance decides "whether through ball direction and power will be assisted to help play passes into the receiver's path", and the same wording appears for ground passes ([EA Accessibility](https://www.ea.com/able/resources/fifa/fifa-21/xbox-one/controller-settings)). On assisted settings, the power bar mostly selects a nearer or further receiver or point; EA's FIFA Mobile notes say "High Power will now consistently select the further teammate" ([EA Advanced Passing](https://www.ea.com/able/news/gameplay-advanced-passing)). FC 26 says through passes "will also now lead receivers more effectively" ([EA FC 26 Pitch Notes](https://www.ea.com/games/ea-sports-fc/fc-26/news/pitch-notes-fc26-gameplay-deep-dive)). eFootball's "stunning" through ball is "faster than a normal Through Ball and tends to end closer to the receiver's feet" ([Konami](https://www.konami.com/efootball/ar/page/new_controls)), which is the pace-for-lead trade made explicit. Rematch maps a longer hold to more power "to break lines or send it deep" (snippet, [Destructoid](https://www.destructoid.com/how-to-pass-the-ball-in-rematch/)). EA's January 2025 patch fixed passes not hit toward a receiver "moving too quickly" (snippet, [Dexerto](https://dexerto.com/ea-sports-fc/ea-fc-25-jan-15-update-patch-notes-faster-passing-goalkeeper-shooting-buffs-3031894)). That fix was for free-kick passes, but it shows fast runners breaking target prediction even at EA.

**Open-source code shows the mechanics.** The engines differ in sophistication:

| Engine | Target | Speed | Receiver |
|---|---|---|---|
| GRF | Mate's position + velocity × lead time (lead `pow(clamp(0.3+0.05d,0,1),0.7)·0.7` s, capped 0.7 s); through button adds 20% of pass distance upfield ([AIfunctions.cpp:1088-1170](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)) | `36·(1.8·(d/60)^1.4 + 0.3)` m/s, cap 30 m/s ([humanoid.cpp](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)) | Control switches at contact; 3 s ball-prediction intercept search; arrival "just in time" |
| HELIOS (RoboCup) | Samples receive points at ±40° × 6–30 m ahead of the receiver ([strict_check_pass_generator.cpp](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/planner/strict_check_pass_generator.cpp)) | Picks the arrival *step*, then solves the exact first speed for the decaying ball; arrival ≤ ~1.5× receiver dash + kickable area | Intercepts if reachable in < 20 cycles, else goes to the announced point |
| YSoccer | 5 px ahead of the mate within 27.5° | 240 px/s + 0.3 × distance ([PlayerStateKick.java](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/PlayerStateKick.java)) | AI receiver; human stays on passer until capture |
| SWOS | Nearest mate within ±22.5°, then *extends the line to the pitch edge* | 7-step distance table, 3.0→4.33 px/tick, +skill ([player.cpp](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/player.cpp)) | Chases the projected ball; aftertouch gives a lead point |

The HELIOS pattern fits WildPitch best: **choose when and where the runner and ball meet, then solve the launch speed from the ball model**. Its arrival bound, "no faster than about 1.5× the receiver's top dash plus the kickable area", is the only source-backed ceiling on arrival pace that we have. GRF's "+0.3" term, meaning even a tap leaves at about 10.8 m/s, shows how firm game passes are.

**WildPitch already has the exact tool for this.** Rolling deceleration (0.9 + 0.35v + 0.013v²) depends only on current speed. So the distance a ball covers while slowing from v₀ to v is R(v₀) − R(v), and the time taken is T(v₀) − T(v), where R and T are the distance-to-rest and time-to-rest functions. The launch speed for distance d with arrival speed vₐ is R⁻¹(d + R(vₐ)), which is what `ball_roll_speed_for(dist + ball_roll_distance(arrive))` computes today. No flight-time function exists yet, though. The current inverse is an 18-step bisection over a 1200-tick integration loop, one of the two costs (with team logic) that STATUS blames for `bench_sim` falling from 22,700× to 7,100× real time. Two monotone tables built with the same substepped integrator, R(v) and T(v) every 0.125 m/s up to 26 m/s (209 entries each), would give distance, launch speed and flight time in a few lookups. These are the numbers the roll law implies (float prototype at 600 Hz, prediction):

| v (m/s) | 3 | 4.5 | 6 | 8 | 10 | 12 | 16 | 20 | 22 |
|---|---|---|---|---|---|---|---|---|---|
| R(v), m to rest | 2.8 | 5.1 | 7.7 | 11.2 | 14.7 | 18.2 | 24.9 | 31.2 | 34.2 |
| T(v), s to rest | 2.17 | 2.80 | 3.28 | 3.79 | 4.18 | 4.50 | 4.98 | 5.33 | 5.48 |

The R and T values match STATUS's measured 3 m/s → 2.8 m / 2.2 s and 12 m/s → 18 m / 4.5 s. The table also exposes the binding constraint: a ball at 10–15 m/s loses 5.7–9 m/s², so **a ground ball barely outpaces a sprinter at 8 m/s**. A runner 12 m ahead and sprinting cannot be caught in stride before the byline from a central position. Real passers solve this by playing "a straight ball for an angled run, an angled ball for a straight run" (snippet, [Cupello](https://my.cupello.com/coaching/advice/technique-behind-playing-through-ball)), and the solver below does the same by easing the runner or clamping the meet point.

## Receivers in good games are steered, never abandoned

**GRF is the clearest model of stick input during a reception.** At the pass touch frame the passer's team calls `SelectPlayer(targetPlayer)`, exactly as WildPitch does. The receiver is then moved by a blend of the human stick and an automatic to-ball movement. The blend weight is `max(curve(sameDirFactor, 0.8), pow(GetLastSwitchBias(), 0.5))` with `sameDirFactor = 0.5 + 0.5·(1 − |angle(auto, manual)|/π)`. Holding the stick with the auto direction keeps the magnet strong. Holding it away weakens it gradually, and pushing toward your own half weakens it further. The code comment reads "we want to at least try to get there" ([playercontroller.cpp:600-660](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/playercontroller.cpp)). The intercept search rotates the stick into the ball-travel frame and quantises it to 16 directions. It then scores every reachable future ball position by `0.4·speed-match + 0.6·angle-match` with the stick, plus a time term. Among feasible points, the stick therefore *chooses where to meet the ball*. The output speed is `(timeNeeded/time)·maxVelocity`, so the player arrives just in time instead of sprinting and waiting ([AIfunctions.cpp:521-789](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)). The same stick then sets the first touch, which pushes the ball 0.25 s ahead at idle and up to 0.85 s ahead at a sprint (`delay = (v/8)²·0.6 + 0.25`). A released stick gives a "stop it, face goal" touch ([humanoid_utils.cpp:220-336](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid_utils.cpp)).

**Commercial games agree that input before arrival is a touch request, not a takeover.** eFootball: "Use the left analog stick to direct the position you want your player to face after receiving the ball" ([PES Mastery](https://pesmastery.com/efootball-passing-tutorial/)). FC 26: "Earlier input gives more control", and touch difficulty depends on "ball speed, height, pressure, body part used" ([EA FC 26 Pitch Notes](https://www.ea.com/games/ea-sports-fc/fc-26/news/pitch-notes-fc26-gameplay-deep-dive)). FIFA's "Auto Switching Move Assistance" exists "to keep the player you AUTO switch to moving in his current direction" ([EA Accessibility](https://www.ea.com/able/resources/fifa/fifa-21/xbox-one/controller-settings)). That is a published fix for the stale-stick problem of finding 5. SWOS cancels its pass-assist lead point only when the ball's heading differs from the player's bearing by more than 90°, meaning the ball has clearly gone past ([playerUpdateCore.cpp:566-607](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/updatePlayers/playerUpdateCore.cpp)). YSoccer refuses to auto-switch to anyone else while the pass's mate is running for it, and otherwise switches only when another player's frames-to-ball is under half the controlled player's ([Team.java:520-640](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/Team.java)).

**Reach figures converge on about one metre.** GRF's possession test accepts a ball within **1.0 m** of `pos + vel·0.05 + dir·0.1`, with ball and player velocities within 6 m/s of each other. Its trap difficulty rises as the ball goes from 0.7 to 1.3 m from the body ("balls farther away from body are harder"), and its intercept search uses a reach of 0.28 m (usual) to 0.9 m (optimistic) ([AIfunctions.cpp:843-864](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp); [humanoid_utils.cpp](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid_utils.cpp)). RoboCup's kickable area is player size 0.3 + kickable margin 0.7 + ball 0.085 ≈ **1.09 m** ([rcssserver serverparam.cpp](https://github.com/rcsoccersim/rcssserver/blob/ce870013f2c6b31b9e93774abb2822c2f346c287/src/serverparam.cpp)). WildPitch already uses 1.0 m for dribble reach and 1.15 m for kick reach, so the 0.75 m control reach is the outlier. Coaching cues ("get in line with the ball", "attack the ball", "receive on the back foot", "direct the first touch away from pressure"; [Soccer Coach Weekly](https://www.soccercoachweekly.net/coaching-advice/what-is-receiving)) map onto two assist behaviours. A standing or approaching receiver steps into the line and toward the ball. A runner keeps his stride and lets the ball come across him.

WildPitch's no-dice rule (D-013) rules out GRF's random trap error (`boostrandom`). Touch quality has to come from visible causes only: relative speed, stretch distance and sprinting. The existing spill formula already does this.

## Recommendations: solve the meet, steer the receiver, widen the foot

Every constant below is tagged with its basis. **[S]** means source-backed, **[P]** means derived from the float prototype of WildPitch's roll law (predictions to confirm in fixed point), and **[D]** means a design value to tune in playtest.

### The through-ball model: an intercept solve where the bar sets depth

At the contact tick, `through_velocity` predicts the runner's path. Direction u is his velocity direction if he is moving faster than 1.5 m/s, otherwise today's "(2,0) + run" blend. Planned speed s is his current speed, or for a standing runner 6 m/s after a 0.3 s start delay [D]. The pass target point at time t is P(t) = pos + u·(s·t + g), where g = **0.8 m** is a stride ahead of his feet [D]. The coaching "slightly late, into the stride" margin of 0.1–0.3 s is an inference in the notes; 0.8 m at 6–8 m/s equals 0.1 s.

The hold value h ∈ [0,1] picks an arrival pace inside a band tied to the runner. The lower bound is vₐ_lo = max(5, s − 1.5) m/s [D]. The upper bound is vₐ_hi = min(1.5·s, 12) m/s [S: HELIOS's 1.5× dash bound]. Then vₐ = vₐ_hi − h·(vₐ_hi − vₐ_lo). For each t on a 0.05 s grid up to 4 s, take D(t) = |P(t) − ball|, launch v₀ = R⁻¹(D + R(vₐ)) and flight f = T(v₀) − T(vₐ). The answer is the first t where f ≤ t, which is at most 80 table lookups once per pass.

Three constraints apply. First, cap v₀ at **T_THROUGH_MAX = 24 m/s** [S: futsal kicks 24.2 ± 2.2 m/s ([scilit](https://www.scilit.net/publications/6e94471ff950cbe95f5872b63b606675)); GRF caps at 30]. Second, P must lie at least **3 m inside the byline and 1.5 m inside the touchline** [S for the 1.5 m HELIOS pitch margin; D for the 3 m]. Third, launch is never below the tap pass's launch to the same receiver [D].

When no root satisfies the constraints, the short-room rule applies in this order. Raise vₐ toward vₐ_hi (firmer, earlier, closer to his feet, eFootball's "stunning" ball). Then plan the runner up to 25% slower, but never below 5 m/s. Finally fall back to a *clamped lead pass*: the regular tap model aimed at his path point clamped to the margin, with the runner easing to collect it.

Read plainly, **hold changes depth and arrival pace, not launch power.** For a runner moving away, a deeper meeting point needs about the same launch, because the ball simply has longer to catch him. A runner crossing the passer's line gets a slightly *softer* launch for a deeper ball. That is physics, not a bug. The HUD bar should therefore keep its current meaning, "how far ahead" (D-019), and the tester's "bar = power" expectation should be answered by guaranteeing that every through ball is firmer than a tap. This matches FIFA, where power picks the further option, and GRF, where the gauge sets target distance. The reverse mapping (hold = firmer, eFootball-style) is a one-line change and **is the owner's call**.

The float prototype (prediction) runs the playtest's geometry. The passer is at x = −15 for the open-pitch rows, and at x = +2 with the byline at x = 20 for the attacking-third rows. "Gap" is always +0.8 m (ball ahead), against −2.9 to −11.2 m today.

| Runner (relative to passer) | Tap launch now→new | Hold 0: launch / arrive / meet t / closing | Hold 0.5 | Hold 1 |
|---|---|---|---|---|
| 6 m ahead, jog 6 m/s | 14.8 (lead by travel time) | 16.9 / 9.0 / 1.10 s / 3.0 | 16.0 / 7.0 / 1.45 s / 1.0 | 16.1 / 5.0 / 2.05 s / 1.0 |
| 6 m ahead, sprint 8 | 17.2 | 20.6 / 12.0 / 0.90 s / 4.0 | 19.3 / 9.3 / 1.25 s / 1.2 | 19.6 / 6.5 / 1.90 s / 1.5 |
| Level, 8 m wide, sprint 8 | 14.2 | 18.2 / 12.0 / 0.70 s / 9.4 | 16.1 / 9.3 / 0.95 s / 6.5 | 15.2 / 6.5 / 1.50 s / 4.2 |
| 12 m ahead, sprint 8 | 22 (capped) | 22.6 / 9.0 / 1.55 s, runner eased to 6.0 | 22.9 / 7.6 / 1.85 s, eased to 6.7 | 23.3 / 5.2 / 2.55 s, eased to 6.7 |
| Standing, 10 m ahead | 12.5 | 18.9 / 9.0 / 1.25 s | 18.1 / 7.0 / 1.65 s | 18.2 / 5.0 / 2.25 s |
| Attacking third: 6 m ahead, sprint, byline 12 m beyond him | — | 20.6 / 12.0, meet x = 16 | 20.0 / 11.0, meet x = 16.8 (depth clamped) | same as 0.5 |
| Attacking third: 12 m ahead, sprint | — | clamped lead pass (no in-stride solution exists) | same | same |
| Live case: striker at x = 12–15 sprinting, passer at x = 2 | — | clamped lead pass to x = 17: launch 16.8–17.3, arrive 8.0–8.1, flight 1.27–1.29 s; striker must ease to **0.9–3.3 m/s** to meet it | same | same |

Launch speeds and closing speeds are in m/s. Closing speeds peak at 9.4 m/s on the lateral run, well under `T_CONTROL_MAX_REL` 16. The unconstrained meet sits 20–30 m from the passer, which is why the margin rule binds in most attacking-third situations. An *unreceived* ball still rolls on R(vₐ) = 6–18 m and can go out; an overhit through ball should. The acceptance test is therefore that a receiver who follows the assist reaches it, not that untouched balls stop in play.

The last row is the uncomfortable one, and it is the situation the live playtest actually created. A striker with only 2–5 m of room before the 3 m margin reaches it in under 0.6 s, while any ground ball from midfield needs at least about 1.2 s. **In short room the hold does nothing, and the striker has to pull up.** That is what a real striker near the byline does: he checks his run or cuts back. The design goal there is a clean reception inside the pitch with no backward run, not an in-stride one. Weighting the ball softer to die inside the line (arriving at 2.4 m/s) is worse: it doubles the flight to 2.5 s and has him nearly standing. The levers that would restore in-stride balls near goal are a lofted through ball and the AI timing runs later (CP0.3).

The assisted run must deliver the speed the solver assumed. `seats_assist_receive` should steer to the stored P at the just-in-time speed |P − g·u − pos| / (t_meet − elapsed), clamped to 4.5–8 m/s for in-stride plans and to 0–8 m/s for a short-room clamped pass. This is the GRF `bestVelocity` rule. It should steer *through* the point rather than braking into it, because `team_steer`'s 1.5 m slow radius and 0.3 m deadzone are what stop runners dead today. That needs a new `team_steer_speed(p, target, speed, out)` whose stick magnitude is speed/top-speed. Passing a slow radius of 0 to the existing function would divide by zero in `fx_div(dist, slow_radius)`. It should also set the sprint bit on the legs-only input when the plan needs more than `T_WALK_SPEED` 6 m/s, consistent with D-017's "steers the legs only". The full set of new `SimState` state from this report is listed below. All of it is hashed in `sim_hash()`, with `WP_SIMSTATE_SIZE_EXPECTED` updated from 1536. It comprises `pass_meet` (fv2) and `pass_meet_tick` (uint16) for the plan; `recv_away_ticks` and `recv_gone_ticks` (uint8) for the takeover rule; the touch buffer `recv_stick[2]` (int8) plus its age (uint8); and, per seat, `switch_latch_stick[2]` (int8) plus `switch_latch_ticks` (uint8).

With no mate in the cone, an untargeted hold should scale power: v₀ = 12 + 8·h m/s along the stick, plus inherited run velocity [D, following GRF's gauge and Rematch's hold]. That fixes the "hold does nothing" case.

The tap/hold boundary should move from 9 to **12 ticks (200 ms)** [D]. YSoccer uses 0.15 s and GRF a 60 ms base, but the tester's deliberate taps lasted 160–200 ms. Because the minimum through ball becomes an in-stride ball firmer than a tap, a misread tap stops being punishing. A **separate through-ball button**, as FIFA and eFootball use, is the alternative and is the owner's decision.

### The regular pass model: arrival scales with distance and leads by real flight time

The arrival pace should be vₐ(D) = clamp(5.5 + 0.25·(D − 5), 5.5, 9) m/s [D]. It is bounded by the notes' inference that 4–8 m/s is comfortable and above about 10 m/s is "heavy", and anchored to tracking data showing pro passes leave at about 15–16 m/s ([Pfaff et al.](https://arxiv.org/pdf/2210.16474); [Friends of Tracking](https://raw.githubusercontent.com/Friends-of-Tracking-Data-FoTD/LaurieOnTracking/master/Metrica_PitchControl.py)). The launch is v₀ = R⁻¹(D + R(vₐ)), capped at `T_PASS_MAX` 22.

The lead uses real travel time. Aim at q + q̇·f, recompute D and f = T(v₀) − T(vₐ), and repeat three times; this converges in the prototype. Assist the receiver toward the *closest* reachable point (check back, attack the ball) only when his velocity component away from the passer is below 2 m/s [D]. A runner keeps his line and meets the ball ahead.

`pass_pick` should score team-mates at their predicted position at the estimated flight time, as GRF does. It should add a distance penalty beyond 18 m and exclude anyone beyond **26 m** [D]. At 26 m even a 22 m/s launch arrives at only about 6.3 m/s, and futsal's "long" passes average only 14.5 m ([SPORT TK](https://revistas.um.es/sportk/article/view/519451)).

The predicted results [P] cover only the speed and flight time of a pass to a stationary target:

| Distance | Launch | Arrive | Flight now | Flight new |
|---|---|---|---|---|
| 4 m | 7.8 | 5.5 | — | 0.61 s |
| 8 m | 10.8 | 6.25 | 1.07 s | 0.96 s |
| 12 m | 14.2 | 7.25 | 1.43 s | 1.17 s |
| 15 m | 16.8 | 8.0 | — | 1.27 s |
| 20 m | 21.2 | 9.0 | 1.98 s | 1.42 s |

Speeds are in m/s. A tap to a striker 6 m ahead jogging at 6 m/s now meets him 12.7 m away, in stride, after 1.19 s. These remain slower than pitch-control's "15 m/s average", because the roll law is deliberately draggy (D-015). That is acceptable for 5-a-side, and pass speed against interception is a CP0.3 balancing question.

### The reception model: the stick picks the meeting point, never cancels

`seats_update` should replace the 48-unit latch with a per-tick reading of the stick while the seat's player is the pass target. A **neutral stick, or the stick within 48 units of the hand-off stick, is "no opinion"**: the assist runs unmodified. The 48-unit tolerance stays for analogue sticks, but it now only marks "same as before". On an 8-way keyboard it means the same key still held, or no key; releasing must no longer cancel.

Any other stick is a *preference*, applied the GRF way. The assist re-runs `meet_point`'s 80-step prediction each tick and keeps every reachable candidate. It picks the one minimising t_meet/2 s + 0.6·angle(stick, P − pos)/π [S for the 0.6 angle weight from GRF's movement rating; D for the time scale]. Pushing along the ball's direction meets it later and on the move. Pushing toward the ball attacks it earlier. Pushing sideways lines the body up on that side.

A stick held at any point in the last **12 ticks (200 ms)** before control becomes the first-touch direction [D, after "earlier input gives more control"]. The existing dribble-touch code already pushes the ball that way within one tick. A stick released at the moment of control keeps today's behaviour, a stopping touch, which matches GRF's released-stick stop and "released = no opinion". Carrying a stick released earlier in the window into the touch would be an alternative, and is the owner's call.

**Manual control takes over fully** in only four cases. The first is the stick pointing more than 120° from the direction to the chosen meet point for **15 consecutive ticks (250 ms)** [D]; on a keyboard, only the back-pedal and two back-diagonal keys qualify. The second is a SWITCH press. The third is the ball having *gone*: no reachable candidate for 6 ticks, after SWOS's >90° "ball has passed" test. The fourth is the pass ending. The `recv_away_ticks` and `recv_gone_ticks` counters carry the 15-tick and 6-tick rules. Lofted passes use the same takeover path and inherit all of this; their head-height filter in `meet_point` stays.

**Reach becomes 1.0 m clean** (`T_CONTROL_REACH` 0.75 → 1.0) [S: GRF 1.0, rcss 1.09, own dribble reach 1.0]. It is tested against the ball's swept segment over the coming tick, ball.pos to ball.pos + vel·dt, rather than its end position, which closes the move-before-check ordering gap at `player.c:443-451`; at 9 m/s the ball moves 0.15 m per tick. For the pass target only, a **stretch band from 1.0 to 1.3 m** [S for the band, GRF's 0.7–1.3 m difficulty ramp] also controls the ball, with extra spill of +0.3 × (d − 1.0)/0.3 [D]. A stretched ball gets a visibly heavier touch.

The approach is shaped too. When the receiver is closing head-on with less than 0.5 s to go, the assist caps his speed *toward* the ball at 4 m/s [D]. A 9 m/s pass then closes at no more than 13 m/s, which keeps finding 8 under the 16 m/s limit without raising it.

**Deflections** need their own rule. If `last_touch` becomes anyone other than the passer while a pass is in flight, end the pass. Then, ignoring `T_SWITCH_COOL` once, give the seat the team-mate with the lowest time-to-ball from the same prediction; YSoccer-style hysteresis switches again only for a time advantage of at least 0.25 s [D]. On every auto-switch, latch the inherited stick: the new player treats the held stick as neutral and chases the loose ball until the stick changes or is released, for at most 30 ticks [S for the behaviour, FIFA's Auto Switching Move Assistance; D for the duration]. Pass-lane checks, so passes stop going straight into a defender, belong to CP0.3.

### Constants, files and functions

| Constant (tuning.h) | Now | Proposed | Basis |
|---|---|---|---|
| T_CONTROL_REACH | 0.75 | 1.0 m | S (GRF 1.0, rcss 1.09) |
| T_CONTROL_STRETCH (new) | — | 1.3 m, +0.3 spill at the edge | S band (GRF 0.7–1.3) / D spill |
| T_PASS_TAP_TICKS | 9 | 12 | D (YSoccer 0.15 s; tester's taps up to 200 ms) |
| T_PASS_ARRIVE → T_PASS_ARR_MIN/PER_M/MAX | 5.5 fixed | 5.5 + 0.25/m, max 9 | D within inferred 4–10 m/s |
| T_PASS_LEAD_S | 0.25 s | removed (real flight time, 3 iterations) | S (HELIOS solves the actual arrival step; GRF leads up to 0.7 s) |
| T_PASS_MAX_DIST (new) | none | 26 m, penalty from 18 m | D (futsal long 14.5 m mean) |
| T_THROUGH_LEAD_MIN/MAX, T_THROUGH_ARRIVE | 2.5–10 m, 3 m/s | removed; band vₐ ∈ [max(5, s−1.5), min(1.5s, 12)] | S (HELIOS 1.5×) / D |
| T_THROUGH_STRIDE (new) | — | 0.8 m | D (inferred 0.1–0.3 s margin) |
| T_THROUGH_MAX (new) | 22 | 24 m/s | S (futsal kicks 24.2 ± 2.2) |
| T_THROUGH_MARGIN_X/Y (new) | 1.5 / 1.0 | 3.0 / 1.5 m | S (HELIOS 1.5) / D |
| T_RECV_TAKEOVER_TICKS, _DEG (new) | any 48-unit change | 15 ticks, > 120° | D (GRF fades gradually; SWOS cancels > 90°) |
| T_RECV_TOUCH_BUFFER (new) | — | 12 ticks | D |
| T_SWITCH_LATCH_TICKS (new) | — | 30 | S behaviour (FIFA) / D value |

The changes by file:

| File | Function | Change |
|---|---|---|
| `src/sim/ball.c`, `sim_internal.h` | `ball_roll_distance`, `ball_roll_speed_for` | Replace with tables R(v) and T(v), built by the same integrator; add `ball_flight_time` |
| `src/sim/player.c` | `pass_velocity` | vₐ(D) and the flight-time lead |
| `src/sim/player.c` | `through_velocity` | The intercept solve |
| `src/sim/player.c` | `make_kick` | Through-ball `power` field = h; untargeted hold scales power |
| `src/sim/player.c` | `pass_pick` | Predicted positions, maximum distance |
| `src/sim/player.c` | `handle_buttons` | Tap window |
| `src/sim/player.c` | `control` | Swept reach, stretch spill, touch buffer |
| `src/sim/seats.c` | `seats_pass_handoff` | Store the meet point |
| `src/sim/seats.c` | `seats_update` | Takeover rule, deflection switch, stick latch |
| `src/sim/seats.c` | `meet_point`, `seats_assist_receive` | Cost-based choice, just-in-time steer-through, sprint bit |
| `src/sim/team.c` | new `team_steer_speed` | Steer through a point at a given speed (no deadzone or slow radius) |
| `src/sim/sim.h`, `sim.c` | `SimState`, `sim_hash` | New fields; hash and size assert |
| `tests/test_passing.c` | through-ball test (expects > 6 m gained) | Gap, speed and timing assertions |
| `docs/DECISIONS.md` | D-017, D-019 | Amended |

### Acceptance targets mapped to each finding

| # | Finding | Target (headless harness) |
|---|---|---|
| 1 | Stick during flight cancels the receive | Misses ≤ 1 per recv3 cell (now up to 18/18), except a deliberate > 120° hold of ≥ 250 ms; holding forward after a tap ≥ 7/8 received live (now 0/8); releasing after a through ball ≥ 18/20 received (now 0/20) |
| 1 | Missed by 3 cm | No ball passing ≤ 1.0 m from an un-busy target goes uncontrolled (swept check) |
| 2 | Through balls behind the runner | Where in-stride room exists (meet point ≥ 3 m inside the byline): gap at closest approach between 0 and +2.0 m on all geom rows; receiver speed at reception ≥ 4.5 m/s and ≥ 0.6× his pre-pass speed; heading cosine ≥ 0.7; arrival 5–12 m/s |
| 3 | Tap passes stop the runner | Where room exists: running receiver keeps ≥ 60% of his speed and waits ≤ 0.2 s |
| 4 | Bar isn't power | Through launch ≥ tap launch to the same mate in 100% of cases; untargeted hold monotonic 12→20 m/s; taps up to 200 ms stay taps |
| 5 | Deflection: stale stick and ping-pong | Control on the best-time mate within ≤ 6 ticks; no movement away from the loose ball for > 0.3 s; ≤ 1 auto-switch per 60 ticks without input |
| 6 | Through balls roll out | 0 out of play when the assisted receiver follows the plan; meet point ≥ 3 m inside the byline |
| 2, 6 | Short room (live striker at x = 12–15) | Received inside the pitch in ≥ 18/20; no backward run over 1 m; arrival ≤ 9 m/s; receiver speed allowed to drop (prediction 0.9–3.3 m/s) |
| 7 | Fixed, gentle pace | Arrival 5.5–9 m/s rising with distance; flight ≤ 1.0 s at 8 m, ≤ 1.3 s at 15 m; no pick beyond 26 m |
| 8 | Bounce-off at 17 m/s | Closing speed < 16 m/s in 100% of assisted receptions |
| 9 | One-twos impossible | CP0.3 (needs passer follow-up runs) |
| — | Regression guards (felt right) | First touch goes the stick's way within 1 tick; hand-off on the contact tick; tap to a standing mate arrives 5.5–6.5 m/s; live completion ≥ 80% (`test_passing.c:214`); golden updated with a stated reason |

### An ordered plan of five playtestable steps

| Step | Scope | Findings | Owner playtest question |
|---|---|---|---|
| 1 | R/T roll tables and `ball_flight_time`; replace the bisection. Golden changes only by rounding; record `bench_sim` | Enabler | None (invisible); confirm the bench gain |
| 2 | Reception: no-cancel stick reading, takeover rule, swept 1.0 m reach, stretch band, head-on approach cap, touch buffer | 1, 8 | Press keys freely while a pass travels: does the receiver still get it, and does the touch go where you pushed? |
| 3 | Regular passes: vₐ(D), flight-time lead, check-back rule, `pass_pick` distance limit | 3, 7 | Do taps to runners land in stride? Is the pace right at 5, 10 and 20 m? |
| 4 | Through balls: intercept solver, hold = depth, short-room rule and margins, just-in-time steer-through with sprint bit, 12-tick tap window, untargeted hold power. Amend D-019 | 2, 4, 6 | Does the runner keep running onto it? Should hold mean deeper (proposed) or firmer? Separate button? |
| 5 | Deflections: immediate best-time switch, stick latch, time-based hysteresis. Amend D-017 | 5 | After a ricochet, do you control the right man without fighting the stick? |

Each step ends, per CLAUDE.md, with `ctest`, `tools/check_determinism.sh`, a `WP_UPDATE_GOLDEN=1` hash update named in the commit, a screenshot check, STATUS and playtest-guide updates, and a commit. Steps 2 and 3 could share a checkpoint if the owner wants fewer sessions. Step 2 goes first because it is the owner's own complaint and does not depend on the solver.

**Deferred to CP0.3 AI.** Several items belong there rather than in these steps. Passer follow-up runs and one-twos (FIFA's Directed Pass & Go, eFootball's one-two) come first. Then off-ball run timing: "wait patiently until the right moment to start the run", per FIFA's Training Centre. Then pass-lane and interception scoring: FC 26's passing lanes, and HELIOS's opponent reach-step test, which rejects a candidate when an opponent gets there first. Then pass speed rising with lane pressure, as the futsal data show ([Corrêa et al.](https://www.futsalfocus.net/wp-content/uploads/2022/02/Spatiotemporal-coordination-behaviors-in-futsal.pdf)). Last, AI passers sampling diagonal meet points (HELIOS ±40° × 6–30 m) as D-008 candidate actions. The through-ball solver above should be written as a reusable function so the AI can score the same candidates.

## Conclusion

The playtest already pointed at an intercept solve, and the evidence confirms that this is a *modelling* problem, not a tuning one. Constants like a 3 m/s arrival or a 0.25 s lead cannot be tuned into correctness, because they ignore the one quantity every working system computes, the time at which ball and player meet. Once WildPitch computes that time with exact roll tables, the through ball, the tap pass, the assisted run and the receive-time stick all share it. Arrival pace becomes a statement about the runner ("about his speed"), not about the ball. That single change also hands CP0.3 a ready-made candidate generator.

The prototype also surfaced a constraint that should shape expectations. With D-015's deliberately draggy roll on a 40 m pitch, ground through balls to straight runners near goal are often physically impossible in stride. That is why the short-room rule, diagonal meet points and eventually lofted through balls matter more here than in an 11-a-side game. If the owner finds attacking-third through balls too often "clamped", the lever is the roll law's speed-proportional term or a lofted variant, not a bigger lead constant. The two genuinely open feel questions, whether hold means deeper or firmer and whether to add a separate through button, are the owner's to answer at the step-4 playtest.
