# Ball physics in open-source and classic football games (code-level notes)

Method: shallow-cloned and read the actual repositories (Nov 2026 HEADs):
- Google Research Football (GRF) — https://github.com/google-research/football, engine in `third_party/gfootball_engine/src/` (Bastiaan Konings' GameplayFootball)
- rcssserver (RoboCup 2D) — https://github.com/rcsoccersim/rcssserver
- YSoccer (libGDX Java port) — https://github.com/ysoccer/ysoccer, `java/core/src/main/java/com/ygames/ysoccer/`
- swos-port (zlatkok's reverse-engineered C++ reimplementation of Sensible World of Soccer) — https://github.com/zlatkok/swos-port, `src/`
- Kick Off / Kick Off 2: no source available; only secondary descriptions.

Line numbers below refer to those HEADs. "(my calc)" marks derived numbers that are not in the sources.

---

## 1. Google Research Football / GameplayFootball: ball class, drag, friction, bounce, spin, rendering, touches

### Takeaway
GRF integrates the ball at a fixed 10 ms step (100 Hz) in SI units (metres, m/s, ball radius 0.11 m). Each step applies, in order: gravity, quadratic air drag, ground bounce, then ground friction while the ball is within a 2.5 cm "grass layer". Ground friction has a quadratic part (0.04·v²) and a constant part (1.6 m/s²), so the ball stops in finite time. After that come post, crossbar and net collisions, then a spin model that pulls ball velocity and rolling spin toward each other (slip to roll), then a Magnus "swerve" term, then the position update. Passes set the velocity directly: `dir * 36 * (power + 0.3)`, where power is about `1.8 * (dist/60)^1.4`. The ball is drawn as a 3D mesh whose orientation quaternion is integrated from the spin.

### Cited Findings
- **Constants** (`src/onthepitch/ball.cpp` L29–35): `bounce = 0.62f // 1 = full bounce`, `linearBounce = 0.06f // bigger = more brake force`, `drag = 0.015f`, `friction = 0.04f`, `linearFriction = 1.6f // arbitrary scale`, `gravity = -9.81f`, `grassHeight = 0.025f`. The ball radius is hard-coded as `0.11f` throughout. — [GRF ball.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/ball.cpp)
- **Timestep:** `constexpr float timeStep = 0.01f;//0.001f; // seconds` (L158). `Ball::Process()` (L572–583) runs one prediction, takes `momentum`, `rotation_ms` and `positionBuffer = Predict(10)` (the position 10 ms ahead), and appends to `ballPosHistory`. So the authoritative state advances one 10 ms step per Process. — [GRF ball.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/ball.cpp)
- **The integrator doubles as the predictor.** `CalculatePrediction()` simulates 3 s ahead (`ballPredictionSize_ms = 3000`, plus `cachedPredictions = 100` extra steps; `gamedefines.hpp` L64–65) every tick. If the ball was not touched, it shifts the cached array by one instead of recomputing (comment at L169–171). AI uses `Predict(t)`. — [GRF ball.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/ball.cpp)
- **Update order per step** (L178–526):
  1. Gravity: `vz += -9.81 * dt`.
  2. Air drag (quadratic, applied to the whole 3D velocity magnitude): `v' = v - 0.015 * v^2 * dt`, keeping direction.
  3. Grass influence: `ballBottom = z - 0.11; bias = clamp(1 - ballBottom/0.025, 0, 1); bias = pow(bias, 0.7)`. The comment says "at half grass height, there's already a bigger amount of friction than 50%".
  4. Bounce: `if (z < 0.11 && vz < 0) { frictionFactor = NormalizedClamp(-vz - 0.5, 0, 12); vz = -vz * 0.62; vz = max(vz - 0.06, 0); } z = 0.11`. The `linearBounce` subtraction is what kills small bounces: any rebound under 0.06 m/s becomes 0.
  5. Ground friction, applied when `z < 0.11 + 0.025`, horizontal speed only: `newVelo = velo - (0.04*bias) * velo^2 * dt; newVelo = clamp(newVelo - 1.6*bias*dt, 0, 100000)`.
  6. Woodwork (post radius 0.07, absorb `postAbsorbInv = 0.8`), checked only on the first predicted step. Nets are spring-like forces (`netAbsorbInv = 0.95`, `powFactor = 2.6`, `powerFac = 1.8`).
  7. Spin/roll coupling (next bullet).
  8. Magnus swerve.
  9. `nextPos += momentum * dt`; orientation quaternion integrated.
  — [GRF ball.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/ball.cpp)
- **Ground spin model** (L429–502), applied only while in the grass layer:
  - The target roll rate is `xR = vy / 0.11; yR = vx / 0.11`, i.e. the angular velocity of pure rolling. Comment: "x movement causes roll over y axis".
  - Spin moves toward that target at a capped rate of `maxRotationChangePerSecond = 1.0 * pi * grassInfluenceBias` rad/s per second, plus `4*pi` on the bounce-impact step.
  - In the other direction, the ball's linear velocity is blended toward the "spin-implied velocity" (`ballRotationMomentum = rot * radius * 1000`) with `rotBias = 0.01 * grassInfluenceBias` per step, plus `0.5 * frictionFactor` on a hard landing. Comment: "lower == ball is lighter. higher == pitch/ball contact seems more 'rubbery'".
  - Effect: backspin bites on landing, topspin makes the ball run on, and a skidding ball gradually becomes a rolling one.
  — [GRF ball.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/ball.cpp)
- **Magnus / swerve** (L506–522): `rotVec = angles(rotation_ms) * 10; swerveAmount = NormalizedClamp(|v|, 0, 70); swerveAmount = pow(sin(swerveAmount * pi * 0.94), 2.6); swerve = (normalize(v) * swerveAmount * 30) x (-rotVec); v += swerve * dt`. The strength is a bell-shaped function of speed, peaking at about 35 m/s. — [GRF ball.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/ball.cpp)
- **How spin is stored and set:**
  - `rotation_ms` is a per-millisecond rotation quaternion. `SetRotation(x, y, z, bias)` takes rad/s, multiplies by 0.001, clamps each axis to ±0.49π, and slerps the current spin toward the new one by `bias` (L120–133).
  - `Touch(v)` sets momentum and forces `z >= 0.11` (L90–103).
  - `SetPosition` zeroes momentum and spin (L105–112).
  — [GRF ball.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/ball.cpp)
- **Pass velocity from distance** (`src/onthepitch/AIsupport/AIfunctions.cpp` L1000–1017, `AI_GetAutoPass`):
  - Ground pass: `heightOffset = 0.11; powerFactor = 1.8; distanceExp = 1.4`.
  - High pass: `heightOffset = 0.45 - NormalizedClamp(d, 0, 60) * 0.15; powerFactor = 1.15`.
  - Then `direction = normalize(flat_dir + (0, 0, heightOffset)); power = pow(NormalizedClamp(d, 0, 60), 1.4) * powerFactor`.
  - In `humanoid.cpp` L479: `touchVec = ballDirection * 36 * (ballPower + 0.3)`.
  - Manual aim target: `playerPos + inputDirection * clamp(inputPower * 60, 1, 100)` (AIfunctions.cpp L1049).
  — [GRF AIfunctions.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp); [GRF humanoid.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)
- **Pass power cap and imperfection** (`humanoid.cpp` L2228–2290, `GetBestPossibleTouch`):
  - `maxPowerShortPass = 30`, `maxPowerHighPass = 42` (m/s), scaled by the animation's `touch_maxpowerfactor*0.7 + 0.3`.
  - `maxPower += currentBallSpeed * 0.5` ("can use some of current ballmomentum").
  - Power above the cap is converted into lift: `z += clamp(missing, 0, 10) * 0.25`.
  - Difficulty adds random rotation of up to ±0.5π × min(randomRotation, 0.5).
  - A far ball loses power: `*= 1 - distanceFactor * 0.3`, while `z += distanceFactor * 1.5` "play higher ball (== less ground friction)".
  — [GRF humanoid.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)
- **Pass spin** (`humanoid.cpp` L487–507):
  - A curve is added from the body-to-touch angle: `amount = bodyTouchAngle * 0.25` (×0.2 for high passes). The velocity is rotated by `amount * (0.4 + 0.6 * NormalizedClamp(|v|, 0, 70))`, and `zcurve = amount * -340` (sidespin, rad/s).
  - Topspin/backspin: `forwardness = 3.5` (ground) or `-1.3` (high pass, i.e. backspin). `xRot = dir.y * clamp(|v|, 0, 15) * forwardness`, and likewise `yRot` from `dir.x`. The spin is applied with `SetRotation(xRot, yRot, zcurve, 0.9 * (1 - bumpyRideBias))`.
  - Shots use `SetRotation(..., 0.7 * ...)`, traps 0.5, ball control 0.6.
  — [GRF humanoid.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)
- **Bad contact blends in the incoming ball velocity** (`humanoid.cpp` L365–378, L402): `bumpyRideBias = curve(curve(clamp(fullBallDistance/0.4 - 0.001, 0, 1), 1), 0.5)` and `touchVec = touchVec*(1 - bias) + currentBallVec*bias`. The further the ball is from the animation's ideal touch point (touchable distance 0.4 m), the more of the ball's existing momentum survives. — [GRF humanoid.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)
- **Shot power** (`humanoid_utils.cpp` L377+ `GetShotVector`):
  - `adaptedDesiredPower = 45 * (0.7 + sqrt(desiredPower) * 0.3)` m/s, multiplied by factors for direction, run speed and position offset.
  - Capped at `(32 + shotpower_stat * 13) * (0.2 + animMaxPowerFactor * 0.8)`, then boosted by `1 + NormalizedClamp(|playerMove - ballMove|, 0, 10) * 0.2`.
  — [GRF humanoid_utils.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid_utils.cpp)
- **Dribbling is touch-based, not glued.** Possession comes from periodic animation "touch frames" that call `Ball::Touch` with a computed `GetBallControlVector`/`GetTrapVector`. There is also an "EXPERIMENTAL controlled ball collision" when the ball is drifting away (`ballDistanceNow < 0.6 && ballDistanceFuture > 0.65`) (`humanoid.cpp` L307–355). — [GRF humanoid.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)
- **Rendering:** `Ball::Put()` sets the 3D geometry's position and `orientationBuffer` quaternion (L585–589). The orientation is `rotationPredictTimeStepped * nextOrientation`, integrated each step (L528–534). The roll is therefore physical: the same spin state drives both physics and visuals. There is no special shadow code in ball.cpp; the 3D renderer handles shadows. — [GRF ball.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/ball.cpp)

### Inferences
- Rolling ball on full grass, ignoring spin coupling: `dv/dt ≈ -(0.015+0.04)·v² - 1.6`. From 10 m/s this gives roughly 13.5 m and 3.6 s to stop (my calc: `d = ln(1 + k·v0²/c)/(2k)`, `t = atan(v0·√(k/c))/√(k·c)`, with k = 0.055, c = 1.6). The quadratic term dominates at pass speeds: at 25 m/s the deceleration is about 36 m/s² (my calc). That explains why GRF passes launch at a high 20–30 m/s.
- GRF's three ideas can be ported to fixed point:
  - The grass layer with a `pow(bias, 0.7)` ramp gives a smooth transition between air and ground and no chatter at z = r.
  - Subtractive `linearBounce` and `linearFriction` give hard stops with no exponential tail.
  - The spin/velocity blend handles skid-to-roll.
  
  `pow` and `sin` would need table or polynomial replacements for a no-libm sim.
- Spin is stored as a quaternion only because it is a 3D mesh. A 2.5D game can store `(spin_topspin, spin_side)` scalars and get the same coupling.

### Gaps
- The exact contents of `GetBallControlVector` and `GetTrapVector` (dribble touch speed) were not traced; they are long and depend on animations.
- The GRF Python environment runs the engine at 10 steps per environment step. I did not re-verify that wrapper, so "100 Hz physics" rests on the 10 ms `timeStep` and `Predict(10)` in Process.

---

## 2. YSoccer (Ball.java, constants, kicks, aftertouch, dribble) and SWOS (swos-port)

### Takeaway
Both are top-down 2D games with a separate z. Both store ball velocity in polar form (speed plus heading) and use simple per-tick rules.
- **YSoccer** uses sqrt-shaped grass friction, linear air drag, an explicit spin scalar that rotates the heading (curve), bounce with restitution `0.9·grass.bounce` and a hard cutoff, and kick strength built up over 0.25 s while the button is held. Aftertouch adds spin from joystick input.
- **SWOS** uses integer fixed point with a constant per-tick speed subtraction (no exponential at all), constant gravity, a bounce that removes a pitch-dependent fraction of planar speed and vertical speed, and a bounce cutoff. Aftertouch is a ten-tick window that nudges the ball's destination point (spin) and can switch the kick to a high kick or a long pass.

### Cited Findings — YSoccer
- **Units and tick:** `SUBFRAMES = 8`, `VIRTUAL_REFRESH_RATE = 64`, so `SUBFRAMES_PER_SECOND = 512` (`framework/GLGame.java` L23–26). Physics runs at 512 Hz; speeds are pixels per second and `Const.SECOND = 512`. — [YSoccer GLGame.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/framework/GLGame.java)
- **Constants** (`match/Const.java` L9–17): `BALL_R = 4`, `GRAVITY = 332.8f / SECOND` (per subframe, i.e. 332.8 px/s²), `AIR_FRICTION = 0.28f`, `SPIN_FACTOR = 12.0f`, `SPIN_DAMPENING = 7.0f`, `BOUNCE = 0.9f`, `PASSING_SPEED_FACTOR = 0.3f`. — [YSoccer Const.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/match/Const.java)
- **Per-pitch grass `Grass(friction, bounce)`** (`match/Pitch.java` L35–43): frozen (4, 0.70), muddy (12, 0.55), wet (6, 0.60), soft (10, 0.60), normal (8, 0.65), dry (6, 0.65), hard (6, 0.70), snowed (4, 0.70), white (10, 0.60). — [YSoccer Pitch.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/match/Pitch.java)
- **Update order** (`match/Ball.java` `updatePhysics`, L145–223):
  1. Spin: `a += SPIN_FACTOR/SECOND * s; s *= 1 - SPIN_DAMPENING/SECOND` (spin turns the heading angle `a` in degrees and decays exponentially).
  2. Position: `x += v/SECOND*cos(a); y += v/SECOND*sin(a)`.
  3. If `z < 1`, grass friction: `v -= grass.friction/SECOND * sqrt(|v|)`. Otherwise air: `v *= 1 - AIR_FRICTION/SECOND`.
  4. Wind (only when airborne).
  5. Stop: `if (v <= 0) { v = 0; s = 0; }`.
  6. `z += vz/SECOND`; if `z > 0`, `vz -= GRAVITY; vz *= 1 - AIR_FRICTION/SECOND`.
  7. Bounce: `if (z < 0 && vz < 0) { z = 0; if (vz > -40) vz = 0; else { v *= (1 + vz/1400f); vz *= -BOUNCE * grass.bounce; } }`.
  
  Each bounce costs horizontal speed in proportion to impact speed (an impact at −140 px/s loses 10%), and bounces slower than 40 px/s are killed.
  — [YSoccer Ball.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/match/Ball.java)
- **Prediction:** `updatePrediction()` (L225–263) runs the same `updatePhysics` forward for 2 s (`BALL_PREDICTION = 2*SECOND/SUBFRAMES`), three times, with heading −45°, 0 and +45° (left, centre, right variants). — [YSoccer Ball.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/match/Ball.java)
- **Kick and pass build-up with aftertouch** (`match/PlayerStateKick.java` L44–181):
  - On button release before 0.15 s → PASSING with `v = 240`, plus `PASSING_SPEED_FACTOR * distance` to the auto-selected mate.
  - KICKING starts at `v = 250`. SHOOTING starts at `v = 270` and adds `(650 + 50*shooting)/SECOND` per subframe while held (the comment gives a final range of 270–367).
  - In passing and kicking, holding the button for up to 0.25 s adds `(960 + 2*shooting)/SECOND` per subframe.
  - Vertical speed: `vz = f * (120 + v*(1 - 0.05*shooting)*timer/SECOND)`. `f` depends on stick direction relative to the kick: within 67.5° → low (0 for passes, 0.4 kick, 0.35 shot); more than 112.5° (pulling back) → high (1.33/1.2/1.2); otherwise medium.
  - Aftertouch spin: a sideways stick (22.5° to 157.5° off) adds `s += 130/SECOND * sign(angle_diff)` per subframe.
  — [YSoccer PlayerStateKick.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/match/PlayerStateKick.java)
- **Dribble is not glued.** The ball is pushed (`match/PlayerStateStandRun.java` L22–57): if the owner is within 9 px and the ball is inside a ±(70 + 3·control)° slice and more than 3.5 px ahead, `ball.v = max(ball.v, (1 + 0.06 - 0.004*control) * player.v)` and the heading is corrected toward the player's facing (halving the angle difference if it exceeds 22.5°). If the ball is behind, `ball.v = max(ball.v, 1.2 * player.v)`. A dribbling player runs at `0.9 * speed`. — [YSoccer PlayerStateStandRun.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/match/PlayerStateStandRun.java)
- **Gaining possession** (`match/Player.java` L303–361):
  - An ellipse in front of the player (foci 6.5 px at ±75°, sum < 14) plus `|playerVel - 0.5*ballVel| < 320 + 10*control` → the player takes the ball and `ball.v = player.v`.
  - Otherwise the ball deflects (`collisionPlayer`).
  - Vertical speed is damped: `ball.vz /= (2 + control)`.
  — [YSoccer Player.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/match/Player.java)
- **Rendering:**
  - Rolling animation: `f = (f + 4 + v/2500) % 4` per subframe (L142), so the frame index advances in proportion to speed, i.e. distance travelled, across 4 sprite frames. `data[subframe].fmx = (int)floor(f)` (L640).
  - The sprite is drawn at `(x - BALL_R, y - z - 2 - BALL_R)` (`BallSprite.java`): height is a pure screen-y offset.
  - Shadow (`SceneRenderer.drawBallShadow`): `x + oX + 0.65*z`, `y - 3 + 0.46*z` (four mirrored shadows at night). The shadow slides diagonally with height, and there is special handling to draw it over the goal when `z > CROSSBAR_H`.
  — [YSoccer Ball.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/match/Ball.java); [BallSprite.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/match/BallSprite.java); [SceneRenderer.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/match/SceneRenderer.java)
- **Wall response** (`inFieldKeep`, L279–312): `v *= 0.5`, the heading is reflected, and `s = -s` (spin reverses). — [YSoccer Ball.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/match/Ball.java)

### Cited Findings — SWOS (swos-port)
- **Number formats:**
  - Position and z are `FixedPoint` Q16.16 (`_fp` literal = `value * 65536`, `util/FixedPoint.h` L177–185).
  - Speed is Q7.9 pixels per tick (`_speed` literal = `value * 512`, `sprites/Sprite.h` L145–148).
  - Target frame rate is `kTargetFpsPC = 70`, `kTargetFpsAmiga = 50` (`video/timer.h`).
  — [swos-port FixedPoint.h](https://github.com/zlatkok/swos-port/blob/master/src/util/FixedPoint.h); [Sprite.h](https://github.com/zlatkok/swos-port/blob/master/src/sprites/Sprite.h); [timer.h](https://github.com/zlatkok/swos-port/blob/master/src/video/timer.h)
- **Update order** (`game/ball/ball.cpp` `updateBall`, L145–163): `updateBallAnimation`, `updateBallSpeedAndXYCoordinates`, `updateBallZCoordinate`, `bounceOffInvisibleWalls`, `handleGoalFrameCollisions`, `handlePostAndCrossbarCollisions`, `checkUpdatedBallOutOfPlay`, `updateBallShadow`, `calculateNextBallPosition`, `updateBallQuadrants`. — [swos-port ball.cpp](https://github.com/zlatkok/swos-port/blob/master/src/game/ball/ball.cpp)
- **The ball moves toward a destination point.** `calculateDeltaXAndY(speed, x, y, destX, destY)` gives per-tick dx/dy. Kicks set a far-away `destX/destY` plus a speed (L767–795).
- **Constant (linear) friction, no exponential decay:**
  - `speedReduction = m_ballAirSpeedReduction (4)` while airborne.
  - On the ground (`z.whole() == 0`): `kControlledBallSpeedReduction = 13`, plus the pitch adjustment when no player has the ball.
  - Then `speed = max(0, speed - speedReduction)` in Q7.9 units per tick (L15–16, L785–794).
  — [swos-port ball.cpp](https://github.com/zlatkok/swos-port/blob/master/src/game/ball/ball.cpp)
- **Pitch tables** (`game/game.cpp` `initPitchBallFactors`, L848–862). Pitch order is frozen, muddy, wet, soft, normal, dry, hard (`game/pitch/pitch.cpp` L23–33).
  - Speed-reduction adjust: PC `{-3, 4, 1, 0, 0, -1, -1}`; Amiga `{-2, 2, 3, 0, 0, -1, -1}`.
  - `kBallSpeedBounceFactors = {24, 80, 80, 72, 64, 40, 32}` (/256).
  - `kballZAxisDampenFactors = {88, 112, 104, 104, 96, 88, 80}` (/256).
  — [swos-port game.cpp](https://github.com/zlatkok/swos-port/blob/master/src/game/game.cpp); [pitch.cpp](https://github.com/zlatkok/swos-port/blob/master/src/game/pitch/pitch.cpp)
- **Gravity, bounce and stop rule** (`updateBallZCoordinate`, L797–848):
  - Gravity: `deltaZ.raw = (deltaZ.raw - kBallAirFriction) | 1` with `kBallAirFriction = 3291` raw Q16.16 (about 0.0502 px/tick²). The comment explains the `| 1`: it "prevents a tiny nonzero trajectory from becoming zero prematurely through quantization".
  - On ground contact (z < 0): `speed -= speed * bounceFactor >> 8` (normal pitch: −25% planar speed per bounce); `z = 0`; `deltaZ = -deltaZ; deltaZ -= (deltaZ >> 8) * zDampen; deltaZ |= 1`. Normal pitch: restitution 1 − 96/256 = 0.625.
  - `if (deltaZ > 0.625_fp) playBallBounceSample(); else deltaZ = 0;`: a hard cutoff ends bouncing.
  — [swos-port ball.cpp](https://github.com/zlatkok/swos-port/blob/master/src/game/ball/ball.cpp)
- **Kick and pass speeds** (`game/player.cpp`):
  - Normal kick: `kBallKickingSpeed = 4.3125_speed`, `kBallKickingDeltaZ = 1.25_fp`, plus a shooting- or finishing-skill table of −0.75 to +1.1875 (L304–360).
  - Passes (L586–680): the speed is chosen from squared distance thresholds `{2500, 10000, 22500, 40000, 62500, 90000, 122500}` px² (i.e. 50, 100 … 350 px) mapping to `{3.0, 3.25, 3.5, 3.666, 3.832, 4.0, 4.166, 4.332}_speed`, plus a skill bonus of 0 to 0.75. A free pass with no receiver uses 3.5.
  - The destination is extended by doubling dx/dy until it leaves the pitch, so the ball carries on past the receiver.
  — [swos-port player.cpp](https://github.com/zlatkok/swos-port/blob/master/src/game/player.cpp)
- **Aftertouch** (`ball.cpp` L33–48, L345–523):
  - `kSpinDuration = 10` ticks with `kSpinMultiplierFactors = {5,4,3,2,2,2,2,1,1,1}`. Each tick, a per-direction left/right factor (kick ±32/±23, pass ±16/±11) times the multiplier is added to the ball's destination point. Spin bends the path by moving the target, not through a force.
  - The spin side latches on the first input.
  - On tick 4 (`kKickSetupFrame`), the stick decides between a normal kick (`deltaZ = 1.375`, `speed = 5.0`) and, if pulled back, a high kick (`deltaZ = 2.0`, `speed = 5.25`). Vertical kicks keep 3/4 speed and diagonals 7/8 (`adjustKickSpeedForDirection`).
  - For passes, left, right or back input selects a long pass (`speed += speed >> 3`).
  — [swos-port ball.cpp](https://github.com/zlatkok/swos-port/blob/master/src/game/ball/ball.cpp)
- **Dribble is semi-glued** (`player.cpp` L163–300).
  - `calculateIfPlayerWinsBall` sets `ball.speed = player.speed + kControlledBallSpeedIncrease[ballControl]` on every other pair of ticks (`currentTick & 2`). The increase runs from 0.254 down to 0.0625 _speed, so worse control lets the ball run further ahead. The destination is set in the player's direction.
  - `positionBallInFrontOfPlayer` snaps the ball to a jiggle offset in front of the player with speed 0, and halves `deltaZ`.
  — [swos-port player.cpp](https://github.com/zlatkok/swos-port/blob/master/src/game/player.cpp)
- **Rendering:**
  - `ballMovingFrames = {kBallSprite4, 3, 2, 1}` (4 frames). In `updateBallAnimation`, `cycleFramesTimer -= speed/1.0_speed + 1` each tick, and the frame advances when the timer goes below 0 (reset to `frameDelay`, which defaults to 5). The source comment: "Faster travel subtracts more from the frame timer, making the four ball images rotate proportionally faster" (L730–765).
  - Shadow: `shadow.x = x + z/2 + 1; shadow.y = y + z/4 + 1 - 10` "to fake the original game's oblique projection" (L1073–1082).
  — [swos-port ball.cpp](https://github.com/zlatkok/swos-port/blob/master/src/game/ball/ball.cpp)
- **Landing prediction for AI** (`calculateNextBallPosition`, L1084–1131): a coarse loop stepping 1, 2, 4 or 8 ticks at a time (by height) until predicted z < 0. — [swos-port ball.cpp](https://github.com/zlatkok/swos-port/blob/master/src/game/ball/ball.cpp)

### Inferences
- SWOS proves that an all-integer, fixed-point, constant-deceleration ball model with a bounce cutoff is enough for a classic-feeling game. The `|1` low-bit trick and the bounce threshold are practical ways to avoid fixed-point limit cycles. Pass speed is a lookup table on distance, not a formula.
- On a normal SWOS pitch, a 4.0 px/tick pass rolls `v²/(2a)` = 16/(2·13/512) ≈ 315 px in about 158 ticks, about 2.25 s at 70 Hz (my calc, assuming one logic tick per frame; not verified).
- YSoccer's `dv = -k·sqrt(v)·dt` friction sits between constant and linear-drag friction. It also stops in finite time: `v(t) = (sqrt(v0) - k·t/2)²` (my calc).

### Gaps
- I did not verify whether SWOS logic runs once per 70 Hz frame or at a lower rate.
- SWOS pixel-to-metre scale is inconsistent: pitch length is 640 px (y 129–769) but the goal is 64 px wide, about 8.7 px/m against about 6.1 px/m for the length (my calc from `pitchConstants.h`).
- YSoccer's pixel-to-metre scale was not checked.

---

## 3. Kick Off / Kick Off 2 (no glued ball, aftertouch)

### Takeaway
No source code is available. Secondary sources confirm the defining behaviour: the ball is not glued and is kicked ahead of the player while running; aftertouch bends or dips the ball. No formulas are documented.

### Cited Findings
- Wikipedia: "the ball did not stick to the player's feet but instead was realistically kicked ahead from the players", and "The ball, as in real game, travels ahead of the player." — [Wikipedia: Kick Off (series)](https://en.wikipedia.org/wiki/Kick_Off_(series))
- Reviews and fan sources describe joystick aftertouch that curves or dips the ball (used to bend free kicks around walls). One source says aftertouch arrived with the Extra Time expansion. — search-result summaries of [everygamegoing Kick Off 2 review](https://www.everygamegoing.com/larticle/Kick-Off-2-000/29620) and [forceforgood Kick Off 2](https://forceforgood.co.uk/sport/kick-off-2/). I saw only search summaries, not the full pages; treat as low confidence.

### Inferences
- The YSoccer "push the ball to `max(ball.v, 1.06·player.v)` when it is in front" rule and SWOS's `player.speed + small increment` rule are concrete open implementations of the Kick Off-style "ball runs ahead" dribble.

### Gaps
- No disassembly or source for Kick Off or Kick Off 2 ball physics was found.

---

## 4. Other open source: rcssserver (RoboCup 2D) and others

### Takeaway
rcssserver uses the simplest possible model: pure exponential decay `v *= 0.94` per 100 ms cycle, plus multiplicative noise. A kick is an acceleration clamped to 2.7 m/cycle, and speed is clamped to 3 m/cycle. There is no z and no stop threshold. I did not research the other projects (RoboCup 3D, Slime soccer, Godot/Unity projects, OpenGolf) in code.

### Cited Findings
- `src/serverparam.cpp`: `SIMULATOR_STEP_INTERVAL_MSEC = 100` (L123), `BALL_SIZE = 0.085`, `BALL_DECAY = 0.94`, `BALL_RAND = 0.05`, `BALL_WEIGHT = 0.2`, `BALL_SPEED_MAX = 3.0 // [12.0.0] 2.7 -> 3.0`, `BALL_ACCEL_MAX = 2.7` (L151–157), `KICKPOWERRATE = 0.027` (L199), `MAXPOWER = 100.0` (L200). — [rcssserver serverparam.cpp](https://github.com/rcsoccersim/rcssserver/blob/master/src/serverparam.cpp)
- **Update order** (`src/object.cpp` `MPObject::_inc`, L476–625):
  1. Clamp accel to `maxAccel` and add it to vel.
  2. Clamp vel to `maxSpeed`.
  3. `vel += noise()`, where noise is a random vector of magnitude `U(0, rand*|v|)` (L447–454).
  4. `vel += wind()`.
  5. Resolve post collisions ("elastic, so the magnitude does not change, but the heading does").
  6. `pos = new_pos; vel *= decay; accel = 0`.
  — [rcssserver object.cpp](https://github.com/rcsoccersim/rcssserver/blob/master/src/object.cpp)
- **Kick** (`src/player.cpp` ~L1351–1362): `eff_power = power * kickPowerRate * (1 - 0.25*dir_diff/π - 0.25*dist_ball/kickableMargin)`, i.e. the kick weakens when the ball is off to the side of the body or far from the foot. Then `accel = fromPolar(eff_power, dir + bodyAngle)`. — [rcssserver player.cpp](https://github.com/rcsoccersim/rcssserver/blob/master/src/player.cpp)

### Inferences
- At 60 Hz, the rcss decay is equivalent to 0.94^(1/6) ≈ 0.98974 per tick (my calc).
- Pure geometric decay travels `v0/(1 - 0.94) = 16.7·v0` in total. A maximum kick (2.7 m/cycle) travels about 45 m, and the ball never truly stops (my calc). This is the "infinite exponential tail" that a game must cut off with a threshold.
- The `(1 - 0.25·angle - 0.25·dist)` kick-efficiency factor is a cheap, learnable-AI-friendly way to make contact quality matter.

### Gaps
- Not covered in code: RoboCup 3D (uses the ODE rigid-body engine, so no hand-written ball model), Slime Soccer, Godot/Unity projects, xsoccer, OpenGolf ground roll. These were skipped to stay within the tool budget.

---

## 5. Cross-cutting: exponential vs constant friction; slide vs roll; bounce with spin; stop threshold; rendering roll, shadow and height

### Takeaway
Every game engine studied adds some non-exponential component so the ball actually stops:
- GRF: quadratic plus constant.
- YSoccer: sqrt.
- SWOS: pure constant.

Only the research simulator (rcss) is purely exponential. Only GRF models skid-to-roll and spin on bounce. The 2D games fake rolling with 4 sprite frames advanced in proportion to speed, and fake height with a screen-y offset plus a shadow that slides diagonally with z.

### Cited Findings
- **Friction models:**
  - GRF ground: `v -= 0.04·bias·v²·dt` then `v = max(0, v - 1.6·bias·dt)`; air: `v -= 0.015·v²·dt`. — [GRF ball.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/ball.cpp)
  - YSoccer ground: `v -= f/512·sqrt|v|`; air: `v *= 1 - 0.28/512`. — [YSoccer Ball.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/match/Ball.java)
  - SWOS: `speed = max(0, speed - 13(+pitch))` on the ground, `- 4` in the air. — [swos-port ball.cpp](https://github.com/zlatkok/swos-port/blob/master/src/game/ball/ball.cpp)
  - rcss: `vel *= 0.94`. — [rcssserver object.cpp](https://github.com/rcsoccersim/rcssserver/blob/master/src/object.cpp)
- **Stop thresholds and anti-jitter:**
  - GRF: the clamp-to-0 friction, and `vz = max(vz·0.62 - 0.06, 0)` on a bounce.
  - YSoccer: `if (v <= 0) {v = 0; s = 0;}` and `if (vz > -40) vz = 0`.
  - SWOS: `max(0, …)`, the bounce cutoff `deltaZ <= 0.625 → 0`, and the `|1` low bit.
  - rcss: none.
  — sources as above.
- **Slide vs roll and bounce with spin:** only GRF has them. Spin is driven toward v/r at a capped rate, velocity is blended toward ω·r, and both effects are strongly boosted on the bounce step (`frictionFactor = NormalizedClamp(-vz - 0.5, 0, 12)`). YSoccer's spin is only a heading-curve scalar, and on a bounce it applies `v *= 1 + vz/1400`. SWOS removes `speed·bounceFactor/256` on each bounce. — sources as above.
- **Rolling render:**
  - YSoccer: 4 frames, `f += v/2500` per 1/512 s subframe.
  - SWOS: 4 frames, timer decremented by `speed + 1` px/tick.
  - GRF: 3D mesh orientation integrated from physical spin.
  — sources as above.
- **Height and shadow:**
  - YSoccer: ball drawn at `y - z - 2`; shadow at `(x - 1 + 0.65z, y - 3 + 0.46z)`.
  - SWOS: shadow at `(x + z/2 + 1, y + z/4 - 9)`.
  — sources as above.

### Inferences
- For a deterministic Q16.16 sim at 60 Hz, the evidence supports this ground model per tick: `v -= c_lin` (constant, gives a hard stop) plus `v -= k·v²` or a small linear-drag term (gives fast-ball fade). Clamp at 0, and snap below an epsilon.
- Use a grass layer of about r..r+2.5 cm, as in GRF, to blend between air and ground rules.
- For bounces: `vz' = max(e·|vz| - c, 0)` with e ≈ 0.6 (GRF 0.62, SWOS 0.625 on a normal pitch, YSoccer 0.9 × 0.65 = 0.585). On each bounce also reduce horizontal speed by a fraction (SWOS 25%, YSoccer |vz|/1400) or with GRF-style spin coupling.
- Remarkably, all three engines converge on a restitution of about 0.6 for a normal pitch (my observation).
- Rolling visuals should be driven by accumulated distance, using a fixed-point accumulator modulo the frame count, as both 2D games do in effect. Shadow offset is linear in z.

### Gaps
- None of the engines document the real-world calibration of their constants. They are hand-tuned (GRF: "arbitrary scale").
