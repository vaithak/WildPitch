# Open-source football code: how the pass receiver is chosen (human stick input + AI pass targets)

Method: I shallow-cloned each repo and read the source. Everything below quotes the code, not write-ups. This file builds on `research_notes/Through ball and pass reception/open_source_code.md` and does not repeat its pass-power and reception details. It covers only **who gets picked** and **how**.

Snapshot commits and link bases:

| Short name | Repo @ commit | Link base |
|---|---|---|
| GRF | `google-research/football` @ `3d9e754720a95621bba6475c4d3b0d56fe919014` | https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/ |
| YS | `ysoccer/ysoccer` @ `478eaef3738e8eef2a7758406663aff237572048` | https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/ |
| SW | `zlatkok/swos-port` @ `5acc902cfcb791903ad5077f495986e6e7a32cb6` | https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/ |
| HB | `helios-base/helios-base` @ `0ca18c29a8206c297454b40f4258b09edb98892d` | https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/ |
| librcsc | `helios-base/librcsc` @ `65e139327751f2f45d9b5a03b1f59fb5ca700cd2` | https://github.com/helios-base/librcsc/blob/65e139327751f2f45d9b5a03b1f59fb5ca700cd2/rcsc/ |
| FK3 | `anttisalonen/freekick3` @ `baac95a345b498d14664cb567ba382de680b0d2c` | https://github.com/anttisalonen/freekick3/blob/baac95a345b498d14664cb567ba382de680b0d2c/src/ |
| CtC | `Wireframe-Magazine/Code-the-Classics` @ `8a2be24283fee68fe65a320dbdf3150ccbab6ee8` | https://github.com/Wireframe-Magazine/Code-the-Classics/blob/8a2be24283fee68fe65a320dbdf3150ccbab6ee8/soccer-master/soccer.py |

CtC is the "Substitute Soccer" game from the book *Code the Classics Vol. 1*.

Units:
- **GRF:** metres, radians, seconds.
- **YS:** px and degrees. The 2 s ball-prediction array is indexed in frames at 64 frames/s.
- **SW:** px and a 256-step angle (32 steps = 45°). Distances are *squared* px.
- **HB:** metres, degrees, 100 ms cycles.
- **FK3:** metres.
- **CtC:** px and per-frame velocities. The pitch is 1000 px wide (`LEVEL_W = 1000`).

---

## 1. Google Research Football (GRF): human receiver selection in `AI_GetPass`

### Takeaway
GRF scores **every active teammate, keeper included**. Lower score wins; there are no hard filters, no cone and no max distance.

**The score:**
- `0.8·(clamp(|predictedPos − manualTarget|, 0, 70)/70)^0.8` (distance to a stick-and-power aim point), plus
- `|angle(predictedPos − passer, stick)|/π` (angle off the stick).

**The predicted position** is `pos + vel·lead`, with the lead capped at 0.7 s.

**Pass power picks the distance.** The aim point's distance is set by button hold: `inputPower·60 m`, minimum 1 m. A tap favours near mates and a full hold favours mates about 60 m away.

**What is not scored:** open-ness, interception risk and forwardness. The only "forwardness" is the +20% upfield offset added *after* selection for the through ball, and it doesn't change who is picked.

The human path doesn't consider opponents at all. Opponent danger lives only in the AI's separate `_GetPassingOdds` (§2).

### Cited Findings
- **Aim point from stick and gauge.**
  - `Vector playerPos = player->GetPosition() + player->GetMovement().GetNormalized(0) * 0.2f;` If a touch animation is running, the anim touch position is used instead.
  - `Vector3 manualTarget = playerPos + inputDirection * clamp(inputPower * 60.0f, 1.0f, 100.0f);`
  - Source: [AIfunctions.cpp:1043-1049](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- **Candidate set: all active players, no exclusions.**
  - `player->GetTeam()->GetActivePlayers(players);` then `if (players[i] != player /* && players[i]->IsActive()*/)`. The goalkeeper is not excluded.
  - `GetActivePlayers` just pushes `players` in team order where `IsActive()`.
  - Sources: [AIfunctions.cpp:1051-1086](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp), [team.cpp:143-149](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/team.cpp)
- **Predicted receiver position.**
  - `passDuration = 0.3f + dist(mate, playerPos) * 0.05f; passDuration = pow(clamp(passDuration, 0, 1), 0.7f) * 0.7f;` The source comment: "after this time, the player is supposed to have been able to stop".
  - `targetPos = mate->GetPosition() + mate->GetMovement() * passDuration`.
  - So the lead is ≈0.3^0.7·0.7 = 0.30 s for an adjacent mate, rising to 0.7 s for mates ≥14 m away.
  - Source: [AIfunctions.cpp:1088-1096](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- **Score (lowest wins).**
  - `distanceRating = pow(NormalizedClamp(|targetPos - manualTarget|, 0, 70), 0.8f) * 0.8f;` The comment: "pow() this so small differences matter more - from some point on, it just doesn't really matter that much anymore".
  - `angleRating = fabs((targetPos - playerPos).GetNormalized(0).GetAngle2D(inputDirection) / (1.0f * pi)) * 1.0f;`
  - `if (distanceRating + angleRating < bestRating)` with `bestRating` starting at 10000.
  - Source: [AIfunctions.cpp:1080-1112](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- **Angle function.** `GetAngle2D(test)` returns `-atan2(cross, dot)` wrapped into [−π, π], so `angleRating` ∈ [0, 1] (0 = on the stick line, 1 = directly behind). — [base/math/vector3.hpp:281-287](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/base/math/vector3.hpp)
- **Neutral stick.** `if (rawInputDirection.GetLength() < analogStickDeadzone) rawInputDirection = CastPlayer()->GetDirectionVec();` with `analogStickDeadzone = 0.75f`. With no stick, the passer's body facing is the aim direction.
  - Source: [humancontroller.cpp:617-620](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/humancontroller.cpp), [gamedefines.hpp:42](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/gamedefines.hpp)
- **Pass aim uses the raw stick (after deadzone, touchline clamp and switch blend), not the debounced one.**
  - `RequestCommand` calls `_GetHidInput(raw...)` then `_SetInput(raw...)`, which sets `inputDirection` (humancontroller.cpp:51-54, playercontroller.hpp:54).
  - The pass commands built at lines 138-225 read that `inputDirection`.
  - A separate "steady direction" debounce exists. `steadyDirection` updates only if the stick moved < 0.01π or > 0.65π since the last frame, or every 100 ms. It is substituted into `inputDirection` only at line 291, *after* the pass block, for ball-control, trap and movement commands, and is restored at line 387.
  - Comment: "humans can only move an analog stick so fast, and we don't want requeues to happen mid-analogstick-movement".
  - Source: [humancontroller.cpp:46-54, 288-291, 387, 405-420](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/humancontroller.cpp), [playercontroller.hpp:54](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/playercontroller.hpp)
- **Switch blend.** For a recently switched-to player, the stick is blended with the current movement: `switchBias = pow(GetLastSwitchBias(), 0.7)` and `result = current·switchBias·0.5 + manual·(1 − switchBias·0.5)`. — [humancontroller.cpp:631-641](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/humancontroller.cpp)
- **Stick hugs the touchline.** When the player stands outside the pitch half-width or half-length, the stick component pointing further out is zeroed (`rawInputDirection.coords[0] = std::min(0.0f, ...)` etc.). — [humancontroller.cpp:600-616](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/humancontroller.cpp)
- **Gauge sets aim distance.** The gauge maps to `inputPower` per pass type:
  - short: `pow(gaugeFactor, 0.7)`
  - through ("LongPass"): `pow(gaugeFactor, 0.65)`
  - high: `pow(gaugeFactor, 0.55)`
  - `gaugeFactor = (gauge_ms − 60)/940`, clamped 0..1.
  - So hold time chooses *both* the receiver (via `manualTarget` distance) and power.
  - Source: [humancontroller.cpp:138-213](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/humancontroller.cpp)
- **After selection: blend auto vs manual.**
  - `distanceFactor = 1 - pow(clamp(|autoRel - manualRel| / 50, 0, 1), 1.5)` ("only help when at least somewhat close to target").
  - `proximityBonus = pow(1 - NormalizedClamp(|playerPos - autoTarget|, 0, 12), 0.5)` ("extra help with power on close targets (because people can only physicaly press the pass button so short)").
  - Direction bias: `dirBias = pow(dirBias·distanceFactor, 1 − 0.9·proximityBonus)`.
  - Power bias: `powBias = clamp(powBias·distanceFactor·(1+proximityBonus), 0, 1)`.
  - Default biases (from gamedefines.hpp): short 0.4/0.7, through 0.2/0.7, high 0.2/0.5.
  - Through pass offset: `offset = (−dynamicSide·targetDistance·0.2, 0, 0)` added to both targets.
  - Source: [AIfunctions.cpp:1114-1170](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- **Selection happens before the blend.** `targetPlayer = bestTargetPlayer` is set before the direction and power blend, which only mixes `autoTargetRel` and `manualTargetRel`. — [AIfunctions.cpp:1114-1170](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp). `targetPlayer` is used for the control switch, `team->SelectPlayer(targetPlayer)` at the touch frame. — [humanoid.cpp:474-476](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)
- **Re-selection at contact.** `AI_GetPass` is re-run at the touch frame with the then-current stick (humanoid.cpp:447-471). So the receiver can change between button release and foot contact. Large changes are limited by the 27° re-aim window described in the earlier notes.
  - Source: [humanoid.cpp:447-476](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)
- **Forced targets (set pieces).**
  - Kickoff: `forcedTargetPlayer = AI_GetClosestPlayer(team, pos + dir*1.0, false, self)`, with full auto (bias 1.0).
  - A forced target skips scoring entirely and uses the same 0.7 s-capped lead.
  - Source: [humancontroller.cpp:146-161](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/humancontroller.cpp), [AIfunctions.cpp:1065-1078](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)

### Inferences
- **Angle vs distance exchange rate** (my arithmetic from the formulas). 10° off the stick costs 0.056. Distance-to-aim-point costs:

  | Distance from aim point | Cost |
  |---|---|
  | 5 m | 0.097 |
  | 10 m | 0.168 |
  | 20 m | 0.294 |
  | 70 m | 0.8 (cap) |

  So **30° of angle error ≈ 10 m of distance error**. Because the distance curve is concave (^0.8), the first few metres matter most.
- **Ball can miss the selected mate.** With low `dirBias` (0.2 for through or high passes), the ball goes mostly along the stick. So the selected and switched-to mate may not be where the ball goes. This is my reading of the blend code.
- **Tap passes.** With a short tap `manualTarget` sits 1–5 m ahead. Selection then becomes roughly "nearest mate's predicted spot, plus angle". A tap aimed between two mates picks the closer one even if it's somewhat further off-angle.
- **Ties.** The test is a strict `<`, so the first player in team-array order wins an exact float tie. Exact ties are practically impossible with floats. For a Q16.16 port, an explicit index tie-break is needed.
- **Missing pieces.** There is no keeper rule, no "behind me" filter (angle ≤ π only costs up to 1.0, so a backward pass to a very-close mate can win), and no opponent check on human passes. This is a deliberately "assist-only, player is responsible" design.

### Gaps
- I didn't find any per-difficulty or per-setting change to the 0.8/70/π weights. They are literals. Only the auto-direction and auto-power biases are configurable (`gameplay_*_autodirection` / `autopower`).

---

## 2. GRF AI (ElizaController): how the CPU picks a pass target

### Takeaway
The CPU passer works in three steps:
1. It rates each mate's **tactical position**: forward space, local space and closeness to goal, weighted by the passer's role "mindset". The goalkeeper's rating is ×0.7.
2. It keeps only mates that improve on the passer by a role-dependent threshold.
3. For those, it computes **passing odds** for short, through and high passes using a per-opponent interception-time "danger" sum. It picks the max of `tacticalDiff·w + odds·1 − oneTouchIsHard`, subject to minimum thresholds.

There is no stick or angle term; the pass then goes through `AI_GetPass` with a forced target and full auto.

### Cited Findings
- **Weights.**
  - `forwardSpaceWeight = 0.4f; spaceWeight = 0.3f; forwardWeight = 2.0f + mindSet*6.0f;`
  - `tacticalImprovementThreshold = 0.06f*(1 - mindSet)`.
  - `tacticalDiffWeight = 1 + mindSet²·10; passWeight = 1.0`.
  - `passMinimum = 0.2*(1-mindSet) - longPossessionFactor*0.1`.
  - `passThreshold = 0.1 - longPossessionFactor*0.05`, where `longPossessionFactor = pow(NormalizedClamp(possessionDuration_ms, 0, 5000), 2)`. Holding the ball longer lowers the bar to pass.
  - Source: [elizacontroller.cpp:822-860](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/elizacontroller.cpp)
- **MindSet by role** (0 = defensive, 1 = attacking):
  - GK 0, CB 0, LB/RB 0.25, DM 0.25, LM/CM/RM 0.5, AM 0.75, CF 1.0.
  - Source: [AIfunctions.cpp:1196-1216](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- **Tactical situation per player** (`_CalculateTacticalSituation`):
  - `forwardSpaceRating = AI_CalculateFreeSpace(checkPos = pos + forward·sprintVelocity·0.5s, safeDistance 5 m, 0.5 s)`.
  - `spaceRating = AI_CalculateFreeSpace(pos + movement·0.1s, 5 m, 0.1 s)`.
  - `forwardRating = pow(1 - clamp(|oppGoal - pos| / pitchLength, 0, 1), 1.5)` ("more important when close to goal").
  - Source: [player.cpp:502-529](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/player.cpp)
- **Free-space metric** (`AI_CalculateFreeSpace`):
  - Opponent goalkeepers are skipped.
  - Each opponent is advanced `movement·0.2 s`, then moves toward the focus point at sprint speed for `(t − 0.2) s`.
  - `situation += 1 − clamp(dist, 0, safeDistance)/safeDistance`.
  - Returns `1 − NormalizedClamp(sum, 0, 2.5)`.
  - Source: [AIfunctions.cpp:159-191](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- **Candidate loop.** The mate's tactical rating is `(0.4·fwdSpace + 0.3·space + fwdW·forward)/total`, with `if (role == e_PlayerRole_GK) mateTacticalRating *= 0.7f; // don't like playing back to goalie`.
  - The mate is considered only if `mateTacticalRating > selfRating + tacticalImprovementThreshold`.
  - The pass type with the best odds is chosen; on ties the order is short ≥ long ≥ high.
  - `totalRating = (tacticalDiff·tacticalDiffWeight + passOdds·passWeight − oneTouchIsHard)/totalWeight2`.
  - Accept if `totalRating > best && totalRating > passThreshold && passRating > passMinimum`.
  - Source: [elizacontroller.cpp:862-936](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/elizacontroller.cpp)
- **First-time pass penalty.** `oneTouchIsHard = movementDiff − technical_shortpass·movementDiff·0.8`, where `movementDiff = NormalizedClamp(|ballVel − playerVel|, 0, 10)`. — [elizacontroller.cpp:821-823](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/elizacontroller.cpp)
- **Passing odds (interception check), `_GetPassingOdds`.**
  - High pass returns 0 if the mate is < 10 m away.
  - `estimatedTime_sec = 0.7 + dist·0.03`. The target is `mate.pos + mate.vel·clamp(estimatedTime, 0, 0.5)`. For the through ("LongPass") type, add `dist·0.2` upfield.
  - Then, along the line from `origin = passer.pos + vel·0.12`, for each opponent at `pos + vel·0.2`:
    - project to line parameter `u`; only `0 ≤ u ≤ 1.2` counts;
    - for high passes, only `u < 0.2 || u > 0.65` counts (it can only be cut out near the start or end);
    - `oppToIntersect = (perpDist + 1)/sprintVelocity`;
    - `ballToIntersect = (0.7 + |intersect − origin|·u·0.03 + (high && u>0.5 ? 2.5 : 0)) / ballVelocityMultiplier`;
    - `danger += clamp(ballToIntersect − oppToIntersect + 0.5, 0, 1)`.
  - High pass gets `danger += 0.4` ("just don't prefer high passing when low passing is applicable as well").
  - `odds = 1 − NormalizedClamp(danger, 0, 1)`, so one clearly dangerous opponent means odds 0.
  - Source: [elizacontroller.cpp:1094-1157](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/elizacontroller.cpp)
- **Executing the pass.** `_AddPass` sets `forcedTargetPlayer = target`, `inputDirection = 0`, both biases `1.0` and calls `AI_GetPass`. — [elizacontroller.cpp:1041-1056](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/elizacontroller.cpp)
- **Keeper restarts.** When the keeper is the ball retainer, the target is the closest mate to a random point on the line `x = pitchHalfW·dynamicSide` (`Vector3(pitchHalfW * team->GetDynamicSide(), boostrandom(-pitchHalfH, pitchHalfH), 0)`). Elsewhere the opponent goal is `pitchHalfW·(−dynamicSide)`, so this is the team's **own** goal line, i.e. the mate nearest a random spot across the keeper's own end. It's only accepted if the nearest opponent (at `pos + vel·0.1`) is more than 10 m from that mate or the keeper has held the ball > 4 s. — [elizacontroller.cpp:236-262](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/elizacontroller.cpp)

### Inferences
- GRF splits **"who is tactically better"** (space and forwardness) from **"can the ball get there"** (time-race danger). That split maps directly onto WildPitch's candidate × utility pattern (D-008). The same `_GetPassingOdds` could serve as an optional safety tie-breaker for human passes, but GRF itself doesn't do that.
- The danger term is a cheap time race per opponent: perpendicular-distance time vs. ball time, with constant speeds and no physics simulation. It is O(opponents) per candidate and all multiplies and clamps, so it is easy to do in Q16.16.

### Gaps
- `mentalImage` adds a reaction-time delay to opponent positions. I didn't trace its exact lag (`_mentalImageTime`).

---

## 3. YSoccer: `searchPassingMate` (human assist and AI)

### Takeaway
YSoccer filters mates to those inside a **27.5° cone** of the kick angle (measured to a point 5 px ahead of each mate along their facing) that are in a "can receive" state and can reach the ball's predicted path within 2 s. Among those it picks the **smallest `frameDistance`**: the earliest frame at which the mate can reach the ball's 2 s predicted path. That path is computed from the ball's motion *before* the pass speed is set, so this is closer to "who can get to the (nearby) ball soonest" than "who can intercept the pass line soonest".

The kick angle is then corrected toward that mate by at most ±22.5°. The assist applies only if the stick didn't change during the kick window.

The AI uses the *same* function and passes with probability 0.3 every 8 frames when a mate is found. A separate `searchFarPassingMate` handles set pieces and long throws: it takes the mate with the smallest lateral offset from the aim line, between 255 and 510 px away. There is no opponent or interception check in either function.

### Cited Findings
- **`searchPassingMate`** (Player.java):
  - `maxCorrectionAngle = 22.5f; maxSearchAngle = maxCorrectionAngle + 5f;`
  - Filters: `if (ply.frameDistance == BALL_PREDICTION) continue; // cannot reach` and `if (!ply.checkStates(STATE_STAND_RUN, STATE_REACH_TARGET, STATE_IDLE)) continue;`
  - Then `if (ply.frameDistance < minFrameDistance)`, aim at `(ply.x + 5*cos(ply.a), ply.y + 5*sin(ply.a))`, with `plyAngleCorrection = signedAngleDiff(atan2(target - ball), a)` and `if (|corr| < maxSearchAngle)` accept.
  - The correction is clamped to ±22.5.
  - Source: [Player.java:1095-1141](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/Player.java), [PlayerFsm.java:14-49](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/PlayerFsm.java)
- **`frameDistance`** = first index `f` of the 2 s ball prediction with `dist(player, prediction[f]) < speed·f/64`, scanning the whole array. It is recomputed once per frame (`scene.updateFrameDistance()`), and the prediction is refreshed once per frame (`ball.updatePrediction()` every `SUBFRAMES` subframes). — [MatchStateMain.java:88, 266-268](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/MatchStateMain.java); [Player.java:279-301](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/Player.java)
- **When the human assist applies.**
  - On kick start `ball.a = player.kickAngle`.
  - At `timer > 0.15 s` with fire released, the kick becomes a pass, with `angle_diff = (angle − kickAngle)` where `angle` is the stick angle if pressed, else `kickAngle`.
  - `if (angle_diff == 0) { searchPassingMate(); ball.a += passingMateAngleCorrection; ball.v += 0.3·dist; }`
  - Moving the stick off the kick direction during the 0.15 s window disables the assist entirely.
  - Source: [PlayerStateKick.java:22-73](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/PlayerStateKick.java)
- **Stick is 8-way.** `angle = Math.round(EMath.aTan2(y1, x1))` from digital −1/0/1 axes, and the facing is `player.a = player.inputDevice.angle`.
  - Effect: kick directions are multiples of 45°. The ±27.5° search cone slightly overlaps neighbouring octants (45° apart), so a mate between octants can be found from either.
  - Source: [framework/InputDevice.java:54](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/framework/InputDevice.java), [PlayerStateStandRun.java:63](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/PlayerStateStandRun.java)
- **AI use.** `AiStateAttacking` calls `searchPassingMate()` every `MATE_SEARCH_INTERVAL = 8` frames. It goes to the passing state if `passingMate != null && random < PASSING_PROBABILITY (0.3f)`. — [AiStateAttacking.java:24-31, 70-76, 110-116](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/AiStateAttacking.java)
- **`searchFarPassingMate`** (free kicks, corners, long throws):
  - Distance window `TOUCH_LINE/2 < d < TOUCH_LINE`, with `TOUCH_LINE = 510` px.
  - `|angle| < 22.5°`.
  - Pick the minimum `|d·sin(angle)|`, the lateral miss distance (initialised to `510·sin 22.5°`). The angle correction is the full `plyAngle`.
  - Iteration covers `TEAM_SIZE` with no state filter, so the keeper is not explicitly excluded.
  - Source: [Player.java:1019-1051](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/Player.java), [Const.java:75](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/Const.java), [PlayerStateFreeKickSpeed.java:76-84](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/PlayerStateFreeKickSpeed.java)
- **Control switching respects the pass.** `near1betterThan(controlled)` returns false if `near1 == controlled.passingMate`. — [Team.java:627](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/Team.java)

### Inferences
- `searchPassingMate()` runs in the same block that sets `ball.v = 240`, so it reads `frameDistance` values computed from the *pre-pass* ball, which is near the kicker's feet. The AI path calls it mid-dribble with no kick at all. So in practice the metric ≈ "**nearest mate by run time to the ball and its short current path**", restricted to the 27.5° cone. It is not "who intercepts the pass soonest". I inferred this from the code order; I didn't run it.
- Keepers are probably excluded by the state filter, since they use `STATE_KEEPER_*` states (PlayerFsm.java:41-49). I didn't verify that a keeper is never in `STATE_IDLE` or `STATE_STAND_RUN`.
- **Tie-break.** Strict `<` over `team.lineup` order, so the lower lineup index wins. Ties are plausible because `frameDistance` is an integer frame count. It's deterministic if the lineup order is.
- Only the kicker's facing matters. The mate's facing changes only the 5 px aim point (a tiny lead).

### Gaps
- I didn't verify on screen how often the `angle_diff == 0` condition fails for analog pads (`InputDevice` reads digital axes, so analog sticks are presumably quantised elsewhere).

---

## 4. SWOS (swos-port): nearest-to-ball mate within ±22.5° of facing, plus a CPU "nearest player of either team" safety check

### Takeaway
**Human pass.** `getClosestNonControlledPlayerInDirection` keeps teammates that are not controlled, not sent off and in normal state, and whose **bearing from the ball** is within ±16/256 (±22.5°) of the passer's 8-way facing. It picks the **smallest squared distance to the ball**. There is no max distance, no prediction and no opponent check. Ties go to the lowest index.

**CPU pass.** The CPU uses `findClosestPlayerToBallFacing`, which scans **both teams** in the same cone. It passes only if the nearest player in the cone is a teammate. That is a one-line interception-risk test: "the first body the ball meets along that octant must be ours".

### Cited Findings
- **Human selection.**
  - `int8_t fullDirection = direction * 32;`
  - Filter: `if (team.controlledPlayer != player && !player->sentAway && player->state == PlayerState::kNormal)`, then `int8_t directionDiff = player->fullDirection - fullDirection; if (directionDiff >= -16 && directionDiff <= 16)`.
  - Choose `if (player->ballDistance < closestDistance)`.
  - The loop covers `kNumPlayersInLineup = 11` ([swos/swos.h:220](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/swos/swos.h)), so presumably all 11 including the goalkeeper (my assumption). There's no explicit keeper exclusion beyond `state == kNormal`.
  - Source: [player.cpp:743-767](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/player.cpp)
- **Use in `doPass`.** `auto [receiver, receiverDistance] = getClosestNonControlledPlayerInDirection(passingPlayer.direction, team); if (receiver) { team.passToPlayerPtr = receiver; ... }`. With no receiver, the ball goes along the facing at `kFreePassSpeed = 3.5` px/tick. — [player.cpp:588-622](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/player.cpp)
- **Bearing and distance.** `fullDirection` is the bearing from the ball to the player, and `ballDistance` is the squared distance to the ball, both updated each tick (`calculateDeltaXAndY(256, ballX, ballY, player.x, player.y).direction`). — [updatePlayers/playerUpdateCore.cpp:89-91](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/updatePlayers/playerUpdateCore.cpp)
- **Facing is 8-way.** `static_assert(static_cast<int>(Direction::kNumDirections) == 8);` The human pass direction = the current facing (joystick octant). — [direction.h:18](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/direction.h)
- **CPU safety cone, `findClosestPlayerToBallFacing`.**
  - `for (auto currentTeam : { &swos.topTeamData, &swos.bottomTeamData })`, same ±16 cone, minimum `ballDistance`.
  - Code comment: "Keep the first player on ties, matching the top-team-first assembly scan."
  - `findClosestTeammateFacing` returns null if the closest is on the other team.
  - Source: [ai/ai.cpp:615-650](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/ai/ai.cpp)
- **When the CPU passes** (`updateCpuPlayerControls`):
  - If an opponent's controlled player is within `ballDistance < 800` (≈28 px) and the goal is near (`goal.squaredDistance ≤ 180000`), pass to the closest teammate facing, else cooldown.
  - Otherwise pass on a tick-bit gate (`currentGameTick & 0x0c` == 0) when a facing teammate exists.
  - The CPU does not search other directions. It passes only along its current octant.
  - Source: [ai/ai.cpp:385-415](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/ai/ai.cpp)
- **CPU control handoff.** For an AI team, if the pass receiver's `ballDistance` is ≥50 (squared px) more than the current player's, the AI swaps `passToPlayerPtr` and `controlledPlayer`. — [ai/ai.cpp:318-320](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/ai/ai.cpp)
- **CPU mis-pass.** `kAiFailedPassChance[] = {6,4,3,2,1,0,0,0}` against `(tick & 0x1e) >> 1`. This is tick-derived pseudo-randomness, not an RNG stream. — [player.cpp:591, 627-630](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/player.cpp)

### Inferences
- SWOS measures the cone **from the ball**, not the passer's body, and against the *facing octant*. Facing is quantised to 45°, and each cone is ±22.5°, so the eight cones tile the full circle with no gaps or overlap (at exactly ±16 both neighbours accept). Every teammate lies in exactly one octant (or on a shared boundary), so the human always knows which octant "owns" a mate.
- Distance is current position only, with no lead and no velocity, so the nearest mate in the octant wins even if he's running away.
- The CPU "nearest in cone is ours" test is the cheapest possible interception filter. It is integer-only and deterministic: one scan of both 11-player lineups with a fixed tie order.

### Gaps
- I didn't check whether a goalkeeper can be in `PlayerState::kNormal` while holding position. If so, he is a valid pass target in the backward octant.

---

## 5. RoboCup HELIOS-base: AI candidate generation, hard filters and final scoring

### Takeaway
HELIOS doesn't pick "a receiver" directly:
1. It generates hundreds of concrete passes: direct, leading and through, each with a receive point, first speed and step count.
2. Each must pass a strict opponent reach-step race.
3. All surviving actions (passes, dribbles, shots) go into a best-first action-chain search (depth ≤ 4, ≤ 500 evaluations). The leaf state is scored by `ball.x + max(0, 40 − dist(ball, oppGoal))`, plus huge bonuses if a shot is possible.

Safety is a **hard filter** (an opponent who reaches no later than the receiver kills the candidate). Value is **forwardness / goal proximity of the resulting state**.

### Cited Findings
- **Receiver hard filters (`updateReceivers`).**
  - Skip: unknown number; `posCount() > 10` (not seen for >10 cycles); tackling; `pos.x > offsideLineX`; a goalie with `x < ourPenaltyAreaLineX + 15`.
  - Skip if farther than 40 m from the first ball point: `max_dist2 = pow(40.0, 2) // Magic Number`.
  - The remaining candidates are sorted by squared distance to the opponent goal: `ReceiverDistCompare(SP.theirTeamGoalPos())`.
  - Source: [planner/strict_check_pass_generator.cpp:353-428](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/planner/strict_check_pass_generator.cpp)
- **Receiver model.**
  - Position and velocity come from `seenPos`/`seenVel` when fresher.
  - `inertia_pos_ = inertiaFinalPoint(pos, vel)` is where the receiver drifts to with no input.
  - `penalty_distance_ = estimate_virtual_dash_distance(p)` and its `penalty_step_` are confidence penalties for stale observations. Opponents get the same value as a *bonus* distance.
  - Source: [strict_check_pass_generator.cpp:103-129](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/planner/strict_check_pass_generator.cpp)
- **Safety race per candidate.**
  - `o_step = predictOpponentsReachStep(..., step + (kick_count - 1) + 5, &opponent)`.
  - Through ('T'): fail if `o_step <= step`. Exception: reset if `receive_point.x > 30 && step >= 15 && (!opponent || !opponent->goalie()) && o_step >= step` and the receiver's body is within 15° of the run direction.
  - Direct and leading: fail if `o_step <= step + (kick_count - 1)`.
  - On failure `break`, because "ball speed is decreasing in the loop", so slower variants are also unsafe.
  - Source: [strict_check_pass_generator.cpp:1245-1310](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/planner/strict_check_pass_generator.cpp)
- **Course ordering.** `std::sort(M_courses..., CooperativeAction::DistCompare(theirTeamGoalPos()))`. — [strict_check_pass_generator.cpp:226-227](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/planner/strict_check_pass_generator.cpp)
- **First-speed formula** (closes a gap in the earlier notes). `calc_first_term_geom_series(sum, r, len) = sum * (1.0 - r) / (1.0 - std::pow(r, len))`, with the comment `// sum = f * ( 1 - r^n ) / ( 1 - r )`. — [librcsc rcsc/math_util.h:213-220](https://github.com/helios-base/librcsc/blob/65e139327751f2f45d9b5a03b1f59fb5ca700cd2/rcsc/math_util.h)
- **Leaf evaluation (`evaluate_state`).**
  - No holder → `-DBL_MAX/2`. Ball in their goal → `+1e7`. Ball in ours → `-1e7`. Out of pitch → `-DBL_MAX/2`.
  - Otherwise `point = ball.x + max(0, 40 − dist(theirGoal, ball))`.
  - `+1e6` if `can_shoot_from(holder)`, and a further `+5e5` if the holder is self.
  - Source: [sample_field_evaluator.cpp:81-207](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/sample_field_evaluator.cpp)
- **Search limits.** `DEFAULT_MAX_CHAIN_LENGTH = 4; DEFAULT_MAX_EVALUATE_LIMIT = 500;` with a best-first search. — [planner/action_chain_graph.cpp:59-60, 395-525](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/planner/action_chain_graph.cpp)

### Inferences
- HELIOS separates the problem cleanly: **feasibility/safety = hard integer step race**, **preference = state value (forward progress)**. Distance from passer and angle play no role, because there is no human.
- For WildPitch AI, a single-step version (depth 1) maps directly onto D-008: generate candidates per mate (direct + a few lead points), drop those where any opponent's reach-tick ≤ the receiver's reach-tick, then score by forwardness. Integer tick comparisons are trivially deterministic.

### Gaps
- I didn't read `predict_kick_count` or librcsc's `inertiaFinalPoint` internals.

---

## 6. Other open-source football games

### Takeaway
- **Code the Classics "Substitute Soccer" (Python):** the simplest complete human-assist design. Cone `dot > 0.8` (±36.9°) around the 8-way facing, distance limit 300 px, nearest by straight distance. The *goal* is also a candidate, so the same button passes or shoots. The CPU additionally rejects any target with an opponent in a `dot > 0.8` cone closer than the target. The human pass leads the target by iterating `r = playerSpeed × ballSteps(dist)` 8 times.
- **Freekick 3 (C++):** the human gets **no** receiver assist (pure stick × power). The AI scores mates 10–35 m away by forwardness, own-goal-distance damping, a ×0.2 goalkeeper factor and a perpendicular-distance opponent penalty.

### Cited Findings
- **CtC `targetable(target, source)`.**
  - Human and CPU: `return target.team == source.team and d0 > 0 and d0 < 300 and v0 * angle_to_vec(source.dir) > 0.8`. The dot product of unit vectors gives a cone of acos(0.8) = 36.87°.
  - CPU only: `if p.team != target.team and d1 > 0 and d1 < d0 and v0*v1 > 0.8: return False`, i.e. any opponent nearer than the target within a 36.87° cone of the pass line. The source comment: "(If source is player-controlled, that's the player's job)".
  - Source: [soccer.py:187-218](https://github.com/Wireframe-Magazine/Code-the-Classics/blob/8a2be24283fee68fe65a320dbdf3150ccbab6ee8/soccer-master/soccer.py)
- **CtC selection.** `targetable_players = [p for p in game.players + game.goals if p.team == self.owner.team and targetable(p, self.owner)]`, then `target = min(targetable_players, key=dist_key(self.owner.vpos))`. Goals are included, so the nearest of "mate or goal" in the cone is chosen. — [soccer.py:324-332](https://github.com/Wireframe-Magazine/Code-the-Classics/blob/8a2be24283fee68fe65a320dbdf3150ccbab6ee8/soccer-master/soccer.py)
- **CtC 8-way facing.** `vec_to_angle` returns `int(4*atan2(x,-y)/π + 8.5) % 8`, and `angle_to_vec(angle) = (sin, −cos)`. — [soccer.py:105-114](https://github.com/Wireframe-Magazine/Code-the-Classics/blob/8a2be24283fee68fe65a320dbdf3150ccbab6ee8/soccer-master/soccer.py)
- **CtC lead (human only).**
  - `iterations = 8 if team.human() and isinstance(target, Player) else 1`.
  - Loop `t = target.vpos + angle_to_vec(self.owner.dir) * r; vec, length = safe_normalise(t - self.vpos); r = HUMAN_PLAYER_WITHOUT_BALL_SPEED * steps(length)`, with speed 3.3 px/frame.
  - The comment assumes the human keeps holding the direction so the receiver (who becomes the controlled player) runs that way.
  - `steps()` simulates `KICK_STRENGTH = 11.5`, `DRAG = 0.98` per frame.
  - Source: [soccer.py:59-71, 148-173, 351-375](https://github.com/Wireframe-Magazine/Code-the-Classics/blob/8a2be24283fee68fe65a320dbdf3150ccbab6ee8/soccer-master/soccer.py)
- **CtC control switch.** `if isinstance(target, Player): game.teams[self.owner.team].active_control_player = target`. With no target, control goes to the nearest mate to a point 250 px ahead. — [soccer.py:376-392](https://github.com/Wireframe-Magazine/Code-the-Classics/blob/8a2be24283fee68fe65a320dbdf3150ccbab6ee8/soccer-master/soccer.py)
- **CtC CPU pass trigger.** `do_shoot = self.timer <= 0 and target and cost(target.vpos, team) < cost(self.owner.vpos, team)`.
  - `cost = 3500/|pos − ownGoal| + Σ_opp 4000/max(24, |opp − pos|) + (x − centre)²/200 − y·(4·team − 2) + handicap`.
  - Lower is better, so the target must have lower cost: less opponent pressure, further from own goal, more central, more forward.
  - Source: [soccer.py:338-345, 421-441](https://github.com/Wireframe-Magazine/Code-the-Classics/blob/8a2be24283fee68fe65a320dbdf3150ccbab6ee8/soccer-master/soccer.py)
- **Freekick 3 human kick.** `Vector3 kicktgt = mPlayerControlVelocity * kickpower; if (mPlayerKickPowerVelocity > 0.5f) kicktgt.z += kicktgt.length() * 0.3f;`. There is no teammate lookup. — [match/MatchSDLGUI.cpp:958-964](https://github.com/anttisalonen/freekick3/blob/baac95a345b498d14664cb567ba382de680b0d2c/src/match/MatchSDLGUI.cpp)
- **Freekick 3 `AIPassAction`.**
  - Skip mates with `dist < 10` (except at kickoff) or `dist > 35`.
  - Candidate positions: the mate's position and a "break" position `+ runSpeed·1.0 s` upfield.
  - `score = getPassForwardCoefficient = (dir.y_attacking + 1)/2`, then `*= 1 − scaledCoefficient(distToOwnGoal, 20)`, then `if (sp->isGoalkeeper()) thisscore *= 0.2f;`.
  - Opponent penalty: `maxoppdist = clamp(1, 0.2·passLen, 10)`. For each opponent with point-to-line distance `< maxoppdist`, `score −= riskcoeff·(1 − oppdist/maxoppdist)·(1 + 8·scaledCoefficient(distToOwnGoal(target), 50))`.
  - Then `checkKickSuccess`, and keep the max.
  - Kick vector: `pos + vel·1.0 s − from`, with power `max(0.3, 1.2·len/maxShotPower)`.
  - Source: [match/ai/AIActions.cpp:271-343](https://github.com/anttisalonen/freekick3/blob/baac95a345b498d14664cb567ba382de680b0d2c/src/match/ai/AIActions.cpp), [match/ai/AIHelpers.cpp:114-193](https://github.com/anttisalonen/freekick3/blob/baac95a345b498d14664cb567ba382de680b0d2c/src/match/ai/AIHelpers.cpp)
- **Freekick 3 risk coefficient.** `PassRiskCoefficient = mildify(clamp(0.3, Pressure*0.5 + 0.25, 0.7))` with `mildify(v) = pow(v, 0.1)`. — [match/ai/AITacticParameters.cpp:6-15](https://github.com/anttisalonen/freekick3/blob/baac95a345b498d14664cb567ba382de680b0d2c/src/match/ai/AITacticParameters.cpp)
- **Leads I didn't pursue.**
  - The Photon Quantum soccer sample advertises "Pass with direction assistance" with full source, but its source is distributed to Quantum SDK users and I didn't read it — [Photon docs](https://doc.photonengine.com/quantum/current/samples/soccer).

### Inferences
- **Recurring human-assist pattern across SWOS, YSoccer and CtC:** a hard angular cone around the (often 8-way) aim, then "nearest" by some metric:
  - SWOS: squared distance to the ball, ±22.5°.
  - YSoccer: frames-to-reach the ball line, ±27.5°.
  - CtC: Euclidean distance, ±36.9°, < 300 px.

  None of the human paths check opponents. Two code bases say this explicitly: CtC "that's the player's job", and GRF has no opponent term.
- **GRF is the only soft-scoring human system.** It has no cone; angle is a continuous cost that trades against distance to a power-scaled aim point. That's why GRF can pick off-angle mates when the stick is ambiguous.
- **AI systems all add open-ness/interception terms.** The forms differ:
  - SWOS: first body in the cone must be ours.
  - CtC: no opponent nearer within ±36.9°.
  - FK3: perpendicular distance < 20% of pass length, max 10 m.
  - GRF: time-race danger with 1 s saturation.
  - HELIOS: exact reach-step race.

  All also add a forwardness or progress value, and three of them down-weight the keeper: GRF ×0.7, FK3 ×0.2, HELIOS excludes keepers within 15 m of their box line.
- **Determinism / tie-breaks.** Every implementation uses a strict `<` or `>` in array order, so the first index wins. SWOS documents this explicitly ("Keep the first player on ties"). For WildPitch (Q16.16), integer metrics like SWOS squared distance or YSoccer frame counts make ties common, so the tie-break must be explicit (e.g. lower slot index, or secondary key = angle).

### Gaps
- I couldn't find open-source code for the commercial behaviours (FIFA/PES). Those are covered by other notes, if at all.
- I didn't find "Bonk", "OpenSoccer", "Soccer-js" or "xsoccer" repos with human pass targeting in this pass. One brief web search found no other open-source football game with readable stick-based receiver selection. I don't claim they don't exist.
