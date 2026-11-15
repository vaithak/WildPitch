# Open-source football game code: pass power, through balls and pass reception

Method: I shallow-cloned each repo and read the source directly (no secondary write-ups). Snapshot commits:
- Google Research Football (GRF) `google-research/football` @ `3d9e754720a95621bba6475c4d3b0d56fe919014`. Engine: `third_party/gfootball_engine/src/` (derived from GameplayFootball).
- YSoccer `ysoccer/ysoccer` @ `478eaef3738e8eef2a7758406663aff237572048` (Java/libGDX, `java/core/src/main/java/com/ygames/ysoccer/`).
- SWOS port `zlatkok/swos-port` @ `5acc902cfcb791903ad5077f495986e6e7a32cb6` (C++ rewritten from the disassembly, `src/game/`).
- HELIOS-base `helios-base/helios-base` @ `0ca18c29a8206c297454b40f4258b09edb98892d`. rcssserver `rcsoccersim/rcssserver` @ `ce870013f2c6b31b9e93774abb2822c2f346c287`.

Short link bases used below:
- GRF = https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/
- YS = https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/
- SW = https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/
- HB = https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/

Units and tick rates:
- **GRF:** metres and m/s. Physics runs at 10 ms steps (`constexpr float timeStep = 0.01f;`, ball.cpp:158; button gauge `gauge_ms += 10`). The RL env sees 1 frame every 10 physics steps (`'physics_steps_per_frame': 10`, gfootball/env/config.py:94).
- **YSoccer:** pixels and px/s. `SUBFRAMES_PER_SECOND = VIRTUAL_REFRESH_RATE(64) * SUBFRAMES(8) = 512`. Half pitch length `GOAL_LINE = 640` px.
- **SWOS:** pixels. Speed is Q7.9 fixed point in px/tick: `"_speed" = value * 512`, "1.0 represents one pixel per tick" (src/sprites/Sprite.h:144-147).
- **rcssserver:** 100 ms cycles. Speeds are in m/cycle.

---

## Q1. GRF: how human short, through and high passes pick a target and a power

### Takeaway
GRF has no separate "through-pass target predictor". All three pass buttons go through one function, `AI_GetPass`. It builds a "manual target" from stick direction and button hold time, then scores every teammate's *predicted* position (current position + velocity × a lead time of at most 0.7 s) by distance to that manual target plus angle to the stick. It then blends auto and manual target by per-pass-type bias constants. The "through" (LongPass button) variant adds only a forward offset of 20% of the pass distance. Power comes from a distance curve: `power = 1.8·(d/60)^1.4`, giving ball speed `|v| = 36·(power+0.3)` m/s, which is then capped.

### Cited Findings
- **Button mapping.** The RL action `long_pass` presses `e_ButtonFunction_LongPass` ([game_env.cpp:207-208](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/game_env.cpp)). In the human controller that button reads the *through pass* settings: `GetReal("gameplay_throughpass_autodirection", _default_ThroughPass_AutoDirection)` ([humancontroller.cpp:~192](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/humancontroller.cpp)). So "long pass" in GRF = the through-ball button.
- **Hold-time gauge.** `gauge_ms += 10; gauge_ms = clamp(gauge_ms, 10, 1000);` (humancontroller.cpp:548-549). On release: `int baseTime_ms = 60; float gaugeFactor = (gauge_ms - baseTime_ms) * (1.0f / float(1000 - baseTime_ms));` clamped to 0..1 (humancontroller.cpp:138-140).
  - Input power per pass type: short pass `inputPower = clamp(pow(gaugeFactor, 0.7f), 0.01f, 1.0f)` (l.173), through pass `pow(gaugeFactor, 0.65f)` (l.190), high pass `pow(gaugeFactor, 0.55f)` (l.207).
  - The pass fires early if the button is still held after `gauge_ms > 500` ("allow anim to kick in before queue is complete"). The pass is re-refined at the moment of contact, so extra hold still counts. — [humancontroller.cpp:126-135](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/humancontroller.cpp)
- **Auto-assist defaults.** These are the weights for auto target vs. stick target (gamedefines.hpp:53-58):
  - Short pass: `_default_ShortPass_AutoDirection = 0.4f`, `_default_ShortPass_AutoPower = 0.7f`
  - Through pass: `_default_ThroughPass_AutoDirection = 0.2f`, `_default_ThroughPass_AutoPower = 0.7f`
  - High pass: `_default_HighPass_AutoDirection = 0.2f`, `_default_HighPass_AutoPower = 0.5f`
  - Source: [gamedefines.hpp](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/gamedefines.hpp)
- **Manual target from stick.** `Vector3 manualTarget = playerPos + inputDirection * clamp(inputPower * 60.0f, 1.0f, 100.0f);` A full gauge therefore means "60 m in the stick direction". `playerPos` is `position + movement.Normalized*0.2` (front-of-foot), or the anim touch position if a touch anim is running. — [AIfunctions.cpp:1049](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- **Receiver lead and scoring.** For every teammate (AIfunctions.cpp:1088-1112):
  - Lead time: `passDuration = 0.3 + dist*0.05; passDuration = pow(clamp(passDuration,0,1),0.7)*0.7`
  - Predicted position: `targetPos = pos + movement * passDuration`, so the lead is capped at 0.7 s.
  - Score (lowest wins): `distanceRating = pow(NormalizedClamp(|targetPos - manualTarget|, 0, 70), 0.8)*0.8` plus `angleRating = |angle(targetPos - playerPos, inputDirection)| / π`.
  - Source: [AIfunctions.cpp:1088-1112](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- **Auto-vs-manual blend** (AIfunctions.cpp:1127-1155):
  - `distanceFactor = 1 - pow(clamp(|autoRel - manualRel|/50, 0, 1), 1.5)`
  - `proximityBonus = pow(1 - NormalizedClamp(|playerPos - autoTarget|, 0, 12), 0.5)`
  - Direction bias: `dirBias = pow(dirBias*distanceFactor, 1 - 0.9*proximityBonus)`
  - Power bias: `powBias = clamp(powBias*distanceFactor*(1+proximityBonus), 0, 1)`
  - Close receivers (<12 m) and stick aims near a mate get pulled strongly toward auto.
  - AI-controlled passers (`ExternalControllerActive`) use full auto (bias 1.0).
  - Source: [AIfunctions.cpp:1127-1155](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- **Through-ball offset.** This is the only through-specific geometry. For `e_FunctionType_LongPass`: `offset = Vector3(-team->GetDynamicSide() * targetDistance * 0.2f, 0, 0)` is added to both auto and manual target. The target moves *toward the opponent goal along the pitch x-axis* by 20% of pass distance (e.g. a 30 m through ball lands 6 m further upfield). The final vector is `normalize(blend of normalized dirs) * (|auto|*powBias + |manual|*(1-powBias))`. — [AIfunctions.cpp:1158-1170](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- **Power curve, `AI_GetAutoPass`** (AIfunctions.cpp:1001-1017):
  - Defaults: `heightOffset = 0.11f; powerFactor = 1.8f; distanceExp = 1.4f`.
  - High pass: `heightOffset = 0.45f - NormalizedClamp(len,0,60)*0.15f; powerFactor = 1.15f`.
  - `resultingPower = pow(NormalizedClamp(len, 0, 60), distanceExp) * powerFactor`.
  - Direction = `normalize(dir2D + (0,0,heightOffset))`, so short and through passes get a small loft of 0.11.
  - Source: [AIfunctions.cpp:1001-1017](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- **Power to ball velocity at contact.** `Vector3 touchVec = ballDirection * 36 * (ballPower + 0.3f);` in m/s. — [humanoid.cpp:479](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)
- **Re-aim at contact.** At the touch frame `AI_GetPass` is called again with the current stick and target positions (humanoid.cpp:447-471):
  - If the new direction is within `maxDeviationAngle = 0.15f * pi` (27°), the new direction, power and target replace the old ones.
  - If it is within 2× that, the old direction rotates by at most 27° and power is blended.
  - Otherwise the original is kept.
  - Source: [humanoid.cpp:447-471](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)
- **Speed cap, `GetBestPossibleTouch`** (humanoid.cpp:2228-2253):
  - Base caps: `maxPowerShortPass = 30.0f` (also used for LongPass/through) and `maxPowerHighPass = 42.0f`.
  - `maxPower = base * (0.3 + 0.7*anim.touch_maxpowerfactor) * (1 - clamp(|posOffset|*2.5, 0, 0.25)) + 0.5*|incoming ball velocity|`.
  - Excess speed is converted to lift: `z += clamp(missing,0,10)*0.25`.
  - A random rotation grows with difficulty: `distanceFactor*0.15 + heightFactor*0.15 + ballMovementFactor*0.3 + difficultyFactor*0.5`, with difficulty reduced by the `technical_shortpass` / `technical_highpass` stats.
  - Source: [humanoid.cpp:2228-2275](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)
- **Cosmetic curve.** A small curl is added from the body-vs-pass angle: `amount = bodyTouchAngle*0.25` (×0.2 for high pass), and `zcurve = amount * -340`. — [humanoid.cpp:487-495](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)
- **Ball physics.** `constexpr float drag = 0.015f; constexpr float friction = 0.04f; bounce = 0.62f; grassHeight = 0.025f`. Drag is quadratic: `v - drag*v²*dt`. — [ball.cpp:29-35,191](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/ball.cpp)

### Inferences
- Worked numbers (my arithmetic from the formulas above, before the 30 m/s cap):
  | Pass distance | Ball speed |
  |---|---|
  | 5 m | ≈ 12.8 m/s |
  | 10 m | ≈ 16.1 m/s |
  | 20 m | ≈ 24.7 m/s |
  | 30 m | ≈ 35.4 m/s → capped to ≤ 30 m/s (+ lift) |
  The `+0.3` term means even a tap pass leaves at ≈ 10.8 m/s. The pass is "always firm", with grass friction and drag slowing it.
- In GRF a "through ball" is an ordinary pass with a forward lead: the receiver's velocity × ≤0.7 s, plus a fixed 20% upfield offset. It has low auto-direction (0.2), so the stick decides the line.
- **Deterministic port.** Every constant here is a plain multiply, clamp or pow. `pow(x,1.4)` and `pow(x,0.7)` can be replaced by small Q16.16 lookup tables over 0..1.

### Gaps
- I did not trace how the receiver's run is *planned* for a through ball. GRF has no explicit "run in behind" request. Off-ball AI runs come from formation code that I didn't read in depth.

---

## Q2. GRF: who becomes the receiver, how they move to the ball, first touch, and stick input during reception

### Takeaway
At the moment of contact the passer's team immediately selects the pass target for the human (`team->SelectPlayer(targetPlayer)`). The receiver is then driven by a "magnet" that blends the human stick with an auto to-ball movement:
- **Move to the ball:** a 3-second ball-prediction search finds the best reachable intercept time. Inputs are quantised to 16 directions in ball-relative space, and candidates are scored by movement-match vs. time.
- **First touch:** the touch sets ball velocity so it arrives at a planned spot ahead of the player's *desired* (stick) movement after 0.25–0.85 s. A difficulty term (ball speed relative to the body, distance, pressure, interception) mixes in random height and keeps some of the incoming velocity. This mix is `bumpyRideBias` / `ballMovementFactor`.

### Cited Findings
- **Control switch on pass.** `if (targetPlayer) { team->SelectPlayer(targetPlayer); }` runs at the pass touch frame. — [humanoid.cpp:474-476](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)
- **Manual switch button** (team.cpp:415-452):
  - Picks the designated possession player, else the best possession player, else `AI_GetBestSwitchTargetPlayer`.
  - That function positions an action point between the ball owner and `ball.Predict(100ms)`, shifted 4 m toward own goal (defence) or 8 m forward (offence) by a possession-weighted bias.
  - The goalkeeper is excluded.
  - Source: [team.cpp:415-452](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/team.cpp), [AIfunctions.cpp:929-999](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- **Ball prediction window.** `ballPredictionSize_ms = 3000`; `defaultTouchOffset_ms = 80` ("how far into an animation the ball is usually touched"); `distanceToVelocityMultiplier = 2.6f`. Velocities: dribble 3.5, walk 5.0, sprint 8.0 m/s. — [gamedefines.hpp:28-73](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/gamedefines.hpp)
- **`AI_GetToBallMovement`** — the receiver's run to intercept (AIfunctions.cpp:521-789):
  - Iterates `time_ms` from `clamp(timeNeededToGetToBall, 40, 2990)` to 3000 in 10 ms steps, with adaptive skipping.
  - Skips predictions with `z >= 1.0` ("unattainable") unless near the touchline.
  - For each candidate, calls `AI_GetTimeNeededForDistance_ms(...)`. That simulates the player decelerating the current velocity over `changeTime_ms = 700` while a reach radius grows from `0.28 m` (usual, "leg extension length") or `0.9 m` (optimistic) at `0.94*maxVelocity`.
  - A candidate is feasible if `timeNeeded <= time_ms`.
  - Rating: `movementRating = 0.4*(1 - |desiredMovement - targetMovement|/8) + 0.6*(1 - |angle|/(π/2))`, then `+ timeRating (1 - t/5000) + perpendicularRating`.
  - With `haste>0` (applied when possession is contested) only time counts: `movementWeight = 0; timeWeight = 1`.
  - The search stops early ("forced") if `dot(toTarget, ballDir) > 0` ("too shallow angle, just go to ball already") or `justInTimeFactor < 0.35`.
  - Output speed: `bestVelocityFloat = clamp((timeNeeded/time) * maxVelocity, idle, sprint)` (time-based, so the player arrives *just in time* instead of sprinting).
  - Source: [AIfunctions.cpp:380-789](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- **Stick quantisation in ball space.** "quantize input 8- or 16-way in relation to 'ball direction space'". The stick is rotated into the ball-travel frame, rounded to 16 directions, then rotated back. — [AIfunctions.cpp:~560-580](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)
- **Human-vs-auto blend for the receiver** (`PlayerController::_MovementCommand`, playercontroller.cpp:~600-660):
  - The designated or best-possession player gets `autoBias = 1` (full magnet).
  - A passed-to human who isn't yet designated gets `magnetBias = max(curve(sameDirFactor, 0.8), pow(GetLastSwitchBias(), 0.5))`. The source comment: "needed for when we get passed a ball while we are not the designated player. we want to at least try to get there."
  - It is multiplied by `1 - inputDirIsOwnHalfFactor` (pushing toward your own half weakens the magnet).
  - `sameDirFactor = 0.5 + 0.5*(1 - |angle(auto, manual)|/π)`.
  - Holding the stick *with* the auto direction keeps the magnet strong. Holding it away weakens it. The magnet weight also fades with time since the switch.
  - Source: [playercontroller.cpp:600-660](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/playercontroller.cpp)
- **Reception command choice** (playercontroller.cpp:342-456):
  - `_TrapCommand` (anim `e_FunctionType_Trap`) is used when the ball's ground speed is ≥2 m/s and the player doesn't have possession.
  - `_BallControlCommand` is used for possession or slow balls.
  - Both copy the human stick as `desiredDirection` (quantised) and stick speed as `desiredVelocityFloat`.
  - With idle stick and `idleTurnToOpponentGoal`, the direction becomes "face the opponent goal".
  - `e_PlayerCommandModifier_KnockOn` multiplies the touch vector by 1.35 (humanoid.cpp:343-345, 393-395).
  - Source: [playercontroller.cpp:342-456](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/controller/playercontroller.cpp)
- **First-touch vector, `GetBallControlVector`** (humanoid_utils.cpp:220-336):
  - `physicsBias = 0.7` with possession, `0.9` without (a reception), `1.0` if the anim exits idle. This is the share of "where the anim body goes" vs. "where the stick wants to go".
  - `desiredMovement = stickDir * (desiredVelocity*0.7 + controllerVelocity*0.3)`.
  - Planned ball spot = `nextStartPos + movement*delay + FFO`, where `delay = pow(v/8,2)*0.60 + 0.25` s ("how many seconds later do we want to be able to hit it again?").
  - `timeToGo = remainingAnimFrames*10ms + delay + 0.08`.
  - `power = pow(|toPlannedBall| / (timeToGo*(0.38+0.02*dribbleStat)*1.1), 0.7)`, with lift `height = clamp(0.1 + 1.5*pow(power/10,1.6), 0, 1.5)`.
  - Source: [humanoid_utils.cpp:220-336](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid_utils.cpp)
- **Trap difficulty, `GetTrapVector` + `GetDifficultyFactors`** (humanoid_utils.cpp:154-218, 338-350):
  - Penalty for ball speed relative to the body: `pow(NormalizedClamp(|playerVel - ballVel|, 10, 50), 1.5) * 5`.
  - "balls farther away from body are harder": `pow(NormalizedClamp(dist, 0.7, 1.3), 2)*2`.
  - Interception penalty when the opponent touched last.
  - All are scaled by `(1 - ballcontrol*0.5) * boostrandom(0.5, 1.0)` and clamped 0..1.
  - Result: `ballControl.z += ballVel.z*heightFactor + heightFactor*4` and `return ballControl*(1-ballMovementFactor) + ballVel*ballMovementFactor`.
  - Source: [humanoid_utils.cpp:154-218, 338-350](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid_utils.cpp)
- **`bumpyRideBias`** (humanoid.cpp:362-378, 400-402):
  - `touchableDistance = 0.4f`.
  - `bumpyRideBias = curve(curve(clamp(fullBallDistance/touchableDistance - 0.001, 0, 1), 1.0), 0.5)`.
  - Then `touchVec = touchVec*(1 - bumpyRideBias) + currentBallVec*bumpyRideBias`.
  - The further the real ball is from the anim's ideal touch point, the more the ball keeps its incoming velocity. A touch only happens if `fullBallDistance < 0.4` and the height difference is < 1.0 m.
  - Source: [humanoid.cpp:362-402](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)
- **Possession test, `AI_HasPossession`.** Ball z ≤ 0.5. Ball within 1.0 m of `pos + vel*0.05 + dir*0.1`. Ball velocity within 6 m/s of player velocity. — [AIfunctions.cpp:843-864](https://github.com/google-research/football/blob/3d9e754720a95621bba6475c4d3b0d56fe919014/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)

### Inferences
- Holding the stick during reception does three separate things in GRF:
  - It steers the run to intercept (16-way, ball-relative).
  - It sets the first-touch direction and how far ahead the ball is pushed: 0.25 s ahead at idle, up to 0.85 s ahead at sprint.
  - It weakens the auto-magnet when pointed away from the ball path.
  
  Releasing the stick gives a "stop the ball / face goal" touch.
- **WildPitch mapping.** "touch quality" = f(|ball vel − player vel|, offset from ideal contact, pressure, skill). Quality lerps the outgoing ball velocity between "planned" and "keep incoming". This can be done with one deterministic RNG draw for the `boostrandom(0.5,1)` term.

### Gaps
- I did not read the animation-selection code (`animcollection.cpp`) that picks which trap anim fits the incoming ball height and speed.

---

## Q3. YSoccer: pass power vs. hold time, target selection, receiver and control switching

### Takeaway
YSoccer has one fire button:
- Tap (released before 0.15 s) = **pass**.
- Hold = kick or shot.

**Pass speed and aim.** A pass leaves at 240 px/s + 0.3 × distance-to-mate. When the stick still points along the kick direction (or isn't touched), the kick angle is auto-corrected by up to ±22.5° toward a point 5 px ahead of the best mate. Stick direction relative to the kick sets loft (low/medium/high) and spin.

**Mate choice.** The best mate is the one with the smallest "frames-to-reach-ball-prediction" within a 27.5° cone.

**Reception.** It is a capture test: inside a small ellipse in front of the feet, with relative speed under 320 + 10·control px/s. The ball then simply takes the player's velocity.

**Control switching** deliberately does *not* jump to the passing mate while the ball is in flight.

### Cited Findings
- **Units.** `SECOND = SUBFRAMES_PER_SECOND`; `VIRTUAL_REFRESH_RATE = 64; SUBFRAMES = 8` (512 subframes/s). `PASSING_SPEED_FACTOR = 0.3f; AIR_FRICTION = 0.28f; GRAVITY = 332.8f/SECOND; BALL_PREDICTION = 2*SECOND/SUBFRAMES` (2 s of predicted ball positions). — [Const.java:8-23](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/Const.java), [GLGame.java:23-27](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/framework/GLGame.java)
- **Kick state (PlayerStateKick.java:44-73).**
  - At `timer > 0.15*SECOND`: if fire is still held → `KICKING` (`ball.v = 250`) or `SHOOTING` (in shot area and sees goal). Otherwise → `PASSING`, with `ball.v = 240f`.
  - Auto-aim applies only `if (angle_diff == 0)`, i.e. stick unchanged from the kick angle or no stick: `player.searchPassingMate(); ball.a += player.passingMateAngleCorrection; ball.v += PASSING_SPEED_FACTOR * player.passingMate.distanceFrom(player);`
  - Source: [PlayerStateKick.java:44-73](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/PlayerStateKick.java)
- **Loft and spin during the 0.25 s window** (PlayerStateKick.java:76-99):
  - `ball.vz = f * (120 + ball.v*(1 - 0.05*shooting)*timer/SECOND)`.
  - `f = 0` (ground pass) when the stick is within 67.5° of the kick direction, `1.33` when > 112.5° (pulled back), else `1.0`.
  - Sideways stick (22.5°..157.5°) adds spin `ball.s += 130/SECOND * sign` per subframe.
  - Holding fire in PASSING adds `(960 + 2*shooting)/SECOND` px/s per subframe.
  - Source: [PlayerStateKick.java:76-99](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/PlayerStateKick.java)
- **`searchPassingMate`** (Player.java:1095-1141):
  - `maxCorrectionAngle = 22.5f; maxSearchAngle = maxCorrectionAngle + 5f`.
  - Skips mates that can't reach the ball (`frameDistance == BALL_PREDICTION`) or aren't in `STAND_RUN/REACH_TARGET/IDLE`.
  - Among the rest, takes the minimum `frameDistance` whose aim point `(ply.x + 5*cos(ply.a), ply.y + 5*sin(ply.a))` (5 px ahead along the mate's facing) is within 27.5° of the kicker's angle. The correction is clamped to ±22.5°.
  - Source: [Player.java:1095-1141](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/Player.java)
- **Reach estimate, `updateFrameDistance`.** For each player, `frameDistance` = first prediction frame `f` with `dist(player, ball.prediction[f]) < speed * f / 64`. Also computed for left- and right-spin predictions (`frameDistanceL/R`). Player `speed = 130 + 4*skills.speed` (px/s). — [Player.java:279-301, 678](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/Player.java)
- **Reception, `getPossession`** (Player.java:303-361):
  - Capture ellipse: foci 6.5 px from the player at ±75° from facing, sum < 14 px, and `ball.z < PLAYER_H (22)`.
  - Check: `|playerVec - 0.5*ballVec| < 320 + 10*skills.control`.
  - On success: `scene.setBallOwner(this); ball.v = v; ball.a = a;` (the ball takes the player's velocity: a "perfect" first touch).
  - On failure: `ball.collisionPlayer(this)` (rebound).
  - Either way: `ball.vz = ball.vz / (2 + skills.control)`.
  - Source: [Player.java:303-361](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/Player.java)
- **Dribble carry after reception** (PlayerStateStandRun.java:23-56):
  - Within 9 px and inside a ±(70+3·control)° slice: `ball.v = max(ball.v, (1 + 0.06 - 0.004*control) * player.v)`.
  - Angle correction: half the offset if > 22.5°.
  - The player's own speed is reduced 10% while owning the ball.
  - Source: [PlayerStateStandRun.java:23-56](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/PlayerStateStandRun.java)
- **Ball friction.**
  - Ground: `v -= grass.friction/SECOND * sqrt(|v|)` per subframe.
  - Air: `v *= 1 - AIR_FRICTION/SECOND`.
  - Grass presets include `new Grass(4f, 0.70f) // frozen`, `(12f, 0.55f) // muddy`, `(6f, 0.60f) // wet`.
  - Source: [Ball.java:155-159](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/Ball.java), [Pitch.java:35-37](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/Pitch.java)
- **Control switching** (Team.java:520-640):
  - When the team owns the ball, control goes to `ball.owner`.
  - When the ball is loose and the team touched it last ("attacking"), switch to `near1` only if `near1betterThan(controlled)`.
  - That function returns false while the controlled player is still kicking. It also returns false if `near1 == controlled.passingMate` ("passing mate, let him get the ball"). Otherwise it switches when `near1.frameDistance < 0.5 * controlled.frameDistance`.
  - Source: [Team.java:520-640](https://github.com/ysoccer/ysoccer/blob/478eaef3738e8eef2a7758406663aff237572048/java/core/src/main/java/com/ygames/ysoccer/match/Team.java)

### Inferences
- From the literal code, the human stays on the passer while the pass travels to its chosen mate. The mate runs under AI until it captures the ball, and the "ball owned" branch then hands control over. I didn't run it to confirm the on-screen behaviour.
- YSoccer's pass-power model: base speed + linear term in distance (0.3 px/s per px). Holding the button is not a power gauge for passes; holding turns it into a kick or shot. That is simpler than GRF.
- Pitch scale: 1280 px over the full length (2×640) suggests roughly 12 px/m on a ~105 m pitch. So a 240 px/s base ≈ 20 m/s. This is my estimate; the code doesn't state a px/m scale.

### Gaps
- YSoccer has no dedicated through-ball action in `PlayerStateKick`. The only "lead" is the 5 px aim point ahead of the mate's facing.
- I didn't read the AI states (`AiStateSeeking`, `AiStateAttacking`) that move the non-controlled receiver toward `ball.prediction[frameDistance]`.

---

## Q4. SWOS (zlatkok/swos-port): pass target, distance→speed table, receiver behaviour, aftertouch

### Takeaway
SWOS passes pick the closest teammate whose bearing from the ball is within ±22.5° of the passer's 8-way facing.
- **Speed:** a 7-step distance table (3.0 → 4.33 px/tick) plus a passing-skill bonus (0 → 0.75 px/tick).
- **Trajectory:** the ball isn't aimed to stop at the receiver. Its destination is the receiver vector doubled repeatedly until it leaves the pitch, so the ball rolls through.
- **Receiver:** chases projected ball coordinates. With aftertouch, a human receiver is given a lead point offset from their own position.
- **Control transfer:** happens when the ball is ≤4 px high and within a speed-dependent radius.
- **Aftertouch:** a 10-tick window where stick input adds curl to the destination and can upgrade the pass to a faster "long" pass (+12.5% speed).

### Cited Findings
- **`doPass`** (player.cpp:584-668):
  - Distance thresholds (squared px): `kPassDistanceThresholds[] = { 2'500, 10'000, 22'500, 40'000, 62'500, 90'000, 122'500 }`, i.e. 50, 100, 150, 200, 250, 300, 350 px.
  - Speeds: `kPassSpeeds[] = { 3.0, 3.25, 3.5, 3.666, 3.832, 4.0, 4.166, 4.332 }` px/tick.
  - Skill bonus: `kPassSpeedIncreaseBySkill[] = { 0, 0.09375, 0.1875, 0.28125, 0.375, 0.5, 0.625, 0.75 }`.
  - `ball.speed = kPassSpeeds[speedIndex] + kPassSpeedIncreaseBySkill[playerInfo.passing]`. With no receiver: `kFreePassSpeed = 3.5_speed` in the facing direction.
  - Source: [player.cpp:584-668](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/player.cpp)
- **Destination extension.** "there's a receiver, extend the pass trajectory until it hits the pitch boundaries": `while(true){ nextX = ball.x+dx; ... if (out of 0..672 × 0..880) break; dx <<= 1; dy <<= 1; }`. — [player.cpp:~632-645](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/player.cpp)
- **CPU mis-passes.** `kAiFailedPassChance[] = { 6, 4, 3, 2, 1, 0, 0, 0 }` is compared with a tick-derived 0..15 value `(currentGameTick & 0x1e) >> 1`. A botched pass halves one component of the vector. — [player.cpp:590-630](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/player.cpp)
- **Receiver selection, `getClosestNonControlledPlayerInDirection`.** `int8_t directionDiff = player->fullDirection - direction*32; if (directionDiff >= -16 && directionDiff <= 16)` (±16 of 256 = ±22.5°). It picks the minimum `ballDistance`. `player.fullDirection` is set each tick from `calculateDeltaXAndY(256, ballX, ballY, player.x, player.y).direction`, the bearing from the ball to the player. — [player.cpp:740-767](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/player.cpp), [playerUpdateCore.cpp:89-91](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/updatePlayers/playerUpdateCore.cpp)
- **Receiver update, `updatePassReceiver`.** In order: `updateHumanPassAssistTarget` → `tryStartingPassReceiverTackle` (a human receiver can fire to tackle/volley near the ball) → `passReceiverCanControlBall` → `setPlayerBallChaseDestination` (go to `team.ballX/ballY`). — [updateAllPlayers.cpp:404-447](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/updatePlayers/updateAllPlayers.cpp)
- **Control radius, `passReceiverCanControlBall`.** `if (!team.ballLessEqual4) return false; kFastPassSpeed = 3.0_speed; return ball.speed >= kFastPassSpeed && team.plCloseToBall || team.plVeryCloseToBall;` Bands: `ballDistance <= 32` very close, `<= 72` close (squared px, ≈5.7 px and 8.5 px). — [playerUpdateCore.cpp:486-495, 107-118](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/updatePlayers/playerUpdateCore.cpp)
- **Taking control, `passReceiverTakesControl`.** `controlledPlayer = passToPlayerPtr; playerSwitchTimer = kPlayerSwitchCooldown (25)`, then `updateBallWithControllingPlayer(player)` (ball glued to the player). — [playerUpdateCore.cpp:498-535](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/updatePlayers/playerUpdateCore.cpp), [updatePlayers.cpp:31-32](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/updatePlayers/updatePlayers.cpp)
- **Human lead point, `updateHumanPassAssistTarget`.** Active only for human teams and when aftertouch is set (`longPass || longSpinPass || leftSpin || rightSpin`) and ball speed ≥ 1.0 px/tick.
  - If the ball heading vs. the player's bearing from the ball differs by more than 90° (64..192 of 256), the pass is cancelled (`passingToPlayer = false`).
  - Within ±4/256 (~5.6°), the player holds position.
  - Otherwise the lead target = own position + `kPassAssistOffsets[(ballDir ± lead + 4) >> 3]` (32-sector table of ±1000-unit vectors), with `leadAngle = longPass ? 32 : 64` (45° or 90° off the ball heading toward the side the ball is on).
  - Source: [playerUpdateCore.cpp:566-607](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/updatePlayers/playerUpdateCore.cpp), [updatePlayers.cpp:86-95](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/updatePlayers/updatePlayers.cpp)
- **Pass aftertouch, `applyPassAfterTouch`** (ball.cpp:33-48, 473-520):
  - The spin window is `kSpinDuration = 10` ticks with `kSpinMultiplierFactors = {5,4,3,2,2,2,2,1,1,1}`. Pass spin adds `kPassingSpinFactors` (±16/±11 per axis) × multiplier to the ball's destination each tick. Spin is latched left/right from stick relative to facing.
  - Once per pass, a long pass is chosen if no direction is held, or if left/right relative to facing is held: `longPass = true; increasePassSpeed()` (`speed + speed>>3` = +12.5%).
  - Holding backwards gives `longSpinPass` (+12.5%).
  - Kicks have a similar window: at tick 4 (`kKickSetupFrame`) they become a normal kick (`5.0_speed`, deltaZ 1.375) or a high kick (`5.25_speed`, deltaZ 2.0). `adjustKickSpeedForDirection` scales vertical kicks to 3/4 and diagonals to 7/8 ("Vertical kicks retain 3/4 speed, diagonals 7/8").
  - Source: [ball.cpp:33-48, 473-520](https://github.com/zlatkok/swos-port/blob/5acc902cfcb791903ad5077f495986e6e7a32cb6/src/game/ball/ball.cpp)

### Inferences
- The SWOS model of "passes go to a direction, not a point", where the ball rolls past the receiver, is the opposite of GRF's point targeting. The receiver must actively intercept, which makes passes feel "through" by default.
- The table gives only a 1.44× speed range (3.0→4.33) over 50→350 px.
- All SWOS logic is integer or fixed-point and tick-deterministic (randomness comes from `currentGameTick` bits). It is a direct model for WildPitch's Q16.16 sim.

### Gaps
- `team.ballX/ballY` for the ordinary (non-aftertouch) receiver chase is written in still-assembly code (`mov [esi+TeamGeneralInfo.ballX], ax`, swos/swos.asm:101080 and others). I did not decode how far ahead it projects.
- The SWOS game tick rate isn't stated in the code I read. The only "70" I found is "The cursor is updated 70 times per second" in docs/SWOS/menus.txt:456, which is about menus.
- The ball's ground-friction constants in ball.cpp weren't extracted.

---

## Q5. RoboCup (HELIOS-base + rcssserver): through-pass generation, first-speed computation, receiver interception

### Takeaway
HELIOS-base `StrictCheckPassGenerator` builds through passes by sampling receive points ahead of the receiver: 15 angles from −40° to +40° × move distances 6–30 m in 2 m steps. For each, it:
- predicts the receiver's reach step;
- loops the ball travel time `step` upward from that reach step;
- computes the exact first kick speed so a geometrically decaying ball (×0.94 per 100 ms cycle) covers the distance in `step` cycles;
- rejects candidates by first-speed and receive-speed bounds and by opponents' reach steps.

On the receiving side, the heard receiver runs `Body_Intercept` if its own intercept step is < 20 and the ball is well seen, else it goes to the announced receive point at max dash.

### Cited Findings
- **Server constants.** `SIMULATOR_STEP_INTERVAL_MSEC = 100`, `BALL_DECAY = 0.94`, `BALL_SPEED_MAX = 3.0` (m/cycle, "[12.0.0] 2.7 -> 3.0"), `PLAYER_SPEED_MAX = 1.05`, `KICKABLE_MARGIN = 0.7`, `BALL_SIZE = 0.085`, `PLAYER_SIZE = 0.3`. — [rcssserver serverparam.cpp:123-203](https://github.com/rcsoccersim/rcssserver/blob/ce870013f2c6b31b9e93774abb2822c2f346c287/src/serverparam.cpp)
- **`createThroughPass` constants** (strict_check_pass_generator.cpp:873-935):
  - Steps: `MIN_RECEIVE_STEP = 6; MAX_RECEIVE_STEP = 35`.
  - Distance: `MIN_THROUGH_PASS_DIST = 5.0; MAX_THROUGH_PASS_DIST = 0.9 * inertia_final_distance(ballSpeedMax, ballDecay)`.
  - `MAX_RECEIVE_BALL_SPEED = ballSpeedMax * pow(ballDecay, MIN_RECEIVE_STEP)`.
  - Angles: `ANGLE_DIVS = 14; MIN_ANGLE = -40; MAX_ANGLE = +40`. Move distance: `MIN_MOVE_DIST = 6.0; MAX_MOVE_DIST = 30.0; MOVE_DIST_STEP = 2.0`.
  - `min_ball_speed = 1.4`.
  - `max_receive_ball_speed = min(MAX_RECEIVE_BALL_SPEED, kickableArea + maxDashPower*dashPowerRate*effortMax*1.5)`. A through ball may arrive no faster than roughly 1.5× the receiver's top dash plus the kickable area, so it stays controllable.
  - Source: [strict_check_pass_generator.cpp:873-935](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/planner/strict_check_pass_generator.cpp)
- **Receive points.** `receive_point = receiver.inertia_pos_ + unit_rvec * move_dist`, where `inertia_pos_` is where the receiver drifts to with no input. Rejection rules:
  - `x < min_receive_x`, where `min_receive_x = min(min(max(10, ballX+10), offsideLineX-10), theirPenaltyAreaLineX-5)`;
  - outside the pitch with a 1.5 m margin;
  - a different teammate is nearest the point (`getNearestReceiverUnum`).
  - Source: [strict_check_pass_generator.cpp:900-1045](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/planner/strict_check_pass_generator.cpp)
- **Run-direction preference** (strict_check_pass_generator.cpp:1063-1105):
  - `start_step = receiver_step`, with no penalty if the angle matches a heard pass request (±20°) or the receiver's current velocity direction (`speed > 0.2` and ±15°).
  - Otherwise `start_step += 1` ("one step penalty"), +1 more near the pitch edge.
  - Then `start_step = max(MIN_RECEIVE_STEP, ballMoveStep(ballSpeedMax, dist), start_step); max_step = max(MAX_RECEIVE_STEP, start_step + 3)`.
  - Source: [strict_check_pass_generator.cpp:1063-1105](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/planner/strict_check_pass_generator.cpp)
- **First speed, `createPassCommon`** (strict_check_pass_generator.cpp:1130-1290):
  - `for step in [min_step, max_step]: first_ball_speed = calc_first_term_geom_series(ball_move_dist, SP.ballDecay(), step)`.
  - `break` if below the minimum first speed (longer steps would be even slower); `continue` if above the maximum.
  - `receive_ball_speed = first_ball_speed * pow(ballDecay, step)` must fall within the receive bounds.
  - Then `kick_count = predict_kick_count(...)` and `o_step = predictOpponentsReachStep(...)`.
  - For type 'T', fail if `o_step <= step`. Exception: the flag is reset if `receive_point.x > 30 && step >= 15 && no goalie && receiver body within 15° of the run direction`.
  - Source: [strict_check_pass_generator.cpp:1130-1290](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/planner/strict_check_pass_generator.cpp)
- **Opponent model, `predictOpponentReachStep`** (strict_check_pass_generator.cpp:1596-1720):
  - Simulates `inertia_n_step_point(ball, vel, cycle, decay)` per cycle and compares it with the opponent's inertia point plus dash distance.
  - Control area = `kickableArea` (or `catchableArea` for a goalie in the box), with `CONTROL_AREA_BUF = 0.15`.
  - For through passes with `first_ball_vel.x > 2.0` beyond the offside line or at x > 30, opponents get *no* bonus distance. Other passes subtract a "bonus distance".
  - Source: [strict_check_pass_generator.cpp:1596-1720](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/planner/strict_check_pass_generator.cpp)
- **Leading and direct passes** use the same machinery with different constants:
  - Leading: `MIN_RECEIVE_STEP = 4; MAX_RECEIVE_STEP = 20; ANGLE_DIVS = 24; DIST_STEP = 1.1`, `MIN_LEADING_PASS_DIST = 3.0`.
  - Direct: `MIN_RECEIVE_STEP = 3; MAX_RECEIVE_STEP = 15`, max dist `0.8*inertia_final_distance`, `min_receive_ball_speed = ptype->realSpeedMax()`.
  - Source: [strict_check_pass_generator.cpp:516-870](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/planner/strict_check_pass_generator.cpp)
- **Receiver behaviour, `SamplePlayer::doHeardPassReceive`.**
  - If a pass message names me this cycle, there's no kickable teammate, the ball pos/vel is fresh (`posCount() <= 1`, `velCount() <= 1`) and `self_min < 20` → `Body_Intercept().execute(this)`.
  - Else `Body_GoToPoint(heard_pos, 0.5, maxDashPower)`, and in both cases `setIntention(new IntentionReceive(heard_pos, maxDashPower, 0.9, 5, time))`.
  - Generic chase rule (`Bhv_BasicMove`): intercept if `self_min <= 3 || (self_min <= mate_min && self_min < opp_min + 3)`.
  - Source: [sample_player.cpp:705-760](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/sample_player.cpp), [bhv_basic_move.cpp:70-90](https://github.com/helios-base/helios-base/blob/0ca18c29a8206c297454b40f4258b09edb98892d/src/player/bhv_basic_move.cpp)

### Inferences
- `calc_first_term_geom_series` lives in librcsc, which I did not clone. The code's own use (`receive_speed = first * decay^step`) is consistent with the standard geometric series `first = dist·(1−decay)/(1−decay^step)`. Treat this closed form as inferred, not quoted.
- This "choose arrival time, solve for kick speed" pattern adapts directly to a fixed-point tick sim. Ball speed decays by a constant factor per tick, so `decay^step` can be a precomputed table. Receivers and opponents are compared in integer ticks, which is fully deterministic.
- For 5-a-side, the HELIOS sampling grid of 15 angles × 12 distances (~180 candidates per receiver) is cheap enough to score per decision tick under the D-008 candidates × utility pattern.

### Gaps
- I did not read librcsc's `InterceptTable` / `Body_Intercept` internals (they're in the separate librcsc repo). The receiver's exact intercept-point selection there isn't documented in these notes.
