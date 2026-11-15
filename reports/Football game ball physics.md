# Unglue the ball before retuning its physics

WildPitch's ball looks weird mainly because of four things the rolling-friction formula doesn't touch. Three are code paths that **overwrite ball velocity like a magnet**: the kick wind-up gather, the no-stick "sole stop", and the possession pull that drags a received ball through the receiver. The fourth is a ball drawn as a disc with a **fixed dark spot that never rotates**, so every roll reads as a sliding puck. The fix order follows from that. First, render-only rotation and motion cues (no golden-hash change). Second, replace the velocity overrides with reach-limited, Δv-capped touches. Third, a stateless, impulse-style bounce that removes the flat 15 % speed loss and the 0.6 m/s cliff. Only after those, and optionally, a topspin state for skid-to-roll and backspin chips, then drag-crisis and Magnus lookup tables. Real-world physics and open-source engines agree on the *shape* of a good model. Rolling deceleration is "constant + speed-proportional" plus quadratic air drag. A struck ball skids and then rolls at about 60 % of launch speed. A grass bounce keeps about 0.55–0.71 of its vertical speed and loses horizontal speed in proportion to the impact, not as a flat fraction. Three independent engines converge on a restitution of about 0.6. The *magnitudes* are a different story on a 40 × 25 m pitch: real turf constants roll a 12 m/s pass about 36 m. That is the "pass doesn't stop" complaint the owner already rejected (D-015). So real physics should set the curve shapes, and the existing "12 m/s pass ≈ 18 m" result should stay fixed while the shapes change.

## Engines that feel right separate roll, bounce and air, and make the ball stop

The best-documented commercial lesson is EA's FIFA 14 fix. Before it, the ball "accelerated and decelerated at a fixed rate regardless of its starting speed": a 30–50 mph ball slowed at the same rate as a 5 mph one. That produced a "floaty", straight flight and a miscalculated Magnus curve. Correcting the drag coefficient made the ball "dip and swerve" with more curve variety ([Scientific American](https://www.scientificamerican.com/article/fifa-physics-how-a-video-game-figured-out-air-resistance/)). Commercial studios otherwise publish feel, not numbers. EA patches ground-pass speed as a balance knob ([FIFA 23 TU2](https://www.ea.com/games/fifa/fifa-23/news/pitch-notes-fifa-23-title-update-2), snippet-level) and trades pass speed against receiver "trap error" ([EA Advanced Passing](https://www.ea.com/able/news/gameplay-advanced-passing)). Rocket League runs a fixed 120 Hz step ([RLGym](https://rlgym.org/Cheatsheets/game_values)). Its reverse-engineered bounce uses a tangential impulse capped by the normal impulse: `J∥ = −m·min(1, Y·|v⊥|/|s|)·μ·s`, with μ = 0.285 and restitution ≈ 0.6 ([smish.dev](https://www.smish.dev/rocket_league/ball_simulation_1)). Psyonix also adds a designer-tuned "hit impulse" because raw rigid-body contact was not predictable enough ([smish.dev part 3](https://samuelpmish.github.io/rocket_league/ball_simulation_3)).

The open-source code is more concrete. **Google Research Football (GRF)** works in SI units at 100 Hz with these constants:

| Term | GRF value |
|---|---|
| Restitution | 0.62 |
| Linear bounce loss | 0.06 m/s |
| Air drag (k·v²) | 0.015 |
| Ground drag (k·v²) | 0.04 |
| Constant ground decel | 1.6 m/s² |
| Grass layer | 2.5 cm, blends air into ground |

GRF also has a spin model that pulls velocity and rolling spin toward each other, so skids become rolls and backspin bites on landing. The ball mesh's orientation is integrated from that physical spin ([GRF ball.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/ball.cpp)). Its dribbling is touch-based. A bad contact blends the incoming ball velocity into the kick, and the blend grows with distance from the ideal touch point ([GRF humanoid.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp)).

**YSoccer** uses sqrt-shaped grass friction. Each bounce costs horizontal speed in proportion to impact (`v *= 1 + vz/1400`). Its dribble pushes the ball to `max(ball.v, 1.06·player.v)` only when the ball sits in a ±70° slice in front of the player ([YSoccer Ball.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/match/Ball.java); [PlayerStateStandRun.java](https://github.com/ysoccer/ysoccer/blob/master/java/core/src/main/java/com/ygames/ysoccer/match/PlayerStateStandRun.java)). **SWOS** shows how far plain integer arithmetic goes. Its Q16.16 port uses constant per-tick deceleration, a pitch-dependent bounce loss (25 % on a normal pitch), a hard bounce cutoff, and an `|1` low-bit trick against quantisation limit cycles ([swos-port ball.cpp](https://github.com/zlatkok/swos-port/blob/master/src/game/ball/ball.cpp)). Both 2D games fake rotation with four sprite frames advanced in proportion to distance travelled, and they slide the shadow diagonally with height. RoboCup's rcssserver is the outlier: pure exponential decay (`v *= 0.94` per 100 ms), which never truly stops ([rcssserver](https://github.com/rcsoccersim/rcssserver/blob/master/src/object.cpp)).

| Engine | Ground decel | Air | Bounce (vertical / horizontal) | Rotation shown |
|---|---|---|---|---|
| GRF | 1.6 + 0.04v² (+0.015v²) | 0.015v² | 0.62, −0.06 m/s / spin-coupled | 3D mesh from physical spin |
| YSoccer | k·√v | linear | 0.585 (normal pitch) / ∝ impact | 4 frames ∝ distance |
| SWOS | constant | constant | 0.625 / −25 % | 4 frames ∝ speed |
| rcss | ×0.94 per cycle | same | none | none |
| WildPitch now | 0.9 + 0.35v + 0.013v² | 0.013v² | 0.55 / flat −15 %, cliff at 0.6 m/s | **fixed spot** |

Two patterns are clear. Every game engine adds a non-exponential term so the ball actually stops, and WildPitch already does this. Every game shows rotation driven by distance or spin, and WildPitch does not.

## Real footballs skid, then roll at 60 % speed, and bounce by impulse

**Rolling.** Turf testing measures deceleration as "a more or less straight line of form **dec = a + b·v**" ([Gabrielsen, ISSS 2004](https://www.isss-sportsurfacescience.org/downloads/documents/A7ZD60AH55_Gabrielsen_Ball_Roll.pdf.pdf)). In the FIFA ramp test a ball reaches the turf at 3.2 m/s and must roll **4–10 m** (Quality) or 4–8 m (Pro) ([FIFA Test Manual I](https://www.sporteimpianti.it/wp-content/uploads/2024/04/FIFA-TTM-I-Layout-v8.pdf)). One certified pitch measured 7.9 m ([FIFA certificate](https://qrubber.cl/wp-content/uploads/2022/07/Pasto-Qgrass-50mm-04mt-20mt-QRubber-cert-FIFA_.pdf)). Fitting that law to the NBI trendline gives a ≈ 0.37 m/s² and b ≈ 0.14 s⁻¹ for an 8 m pitch, with a range of a 0.23–0.70 and b 0.10–0.16 across 5–12 m pitches. That fit is the research team's arithmetic and covers only speeds ≤ 3.2 m/s. Faster rolls are extrapolation. Above about 8–10 m/s, **air drag dominates**: about 2.7 m/s² at 10 m/s, against 0.6–1.0 m/s² from the turf.

My float prototype reproduced the FIFA test: a medium pitch rolls a 3.2 m/s ball 8.3 m. The same constants roll a 12 m/s pass **about 36 m in 10 s**. On WildPitch's 40 m pitch that is unplayable, which is why the higher game constants are a deliberate scale choice rather than an error.

**Skid to roll.** A ball struck with no spin slides. Kinetic friction μ_k·g slows it and spins it up until v = rω ([Cross 2010](https://www.physics.usyd.edu.au/~cross/PUBLICATIONS/48.%20EnhanceBounce.pdf) gives the rolling condition). For a thin shell, α = I/(mr²) = 2/3, so it ends rolling at **v_roll = (v₀ + α·rω₀)/(1+α) = 0.6·v₀** when ω₀ = 0. Topspin shortens the skid and backspin lengthens it. Two inputs to this are not source-backed for footballs. The only friction figure found is tennis-on-grass μ ≈ 0.5–0.6, and only at snippet level ([Cross, Physics Today](https://physicstoday.aip.org/quick-study/tennis-physics-anyone)). α for a real size-5 ball was not found; about 0.62–0.66 is an engineering estimate.

**Bounce.** On FIFA-compliant turf, a 2 m drop must rebound **0.6–1.0 m** ([FIFA Test Manual I](https://www.sporteimpianti.it/wp-content/uploads/2024/04/FIFA-TTM-I-Layout-v8.pdf); certificate measured 0.94 m dry, 0.85 m wet), which gives a normal restitution **e_n ≈ 0.55–0.71**. Tangentially, a ball slides throughout the bounce only at glancing angles (≲ 20°). At steeper angles it grips, with a tangential restitution of about 0.1–0.2 ([Cross, Physics of Bounce](https://www.physics.sydney.edu.au/~cross/BOUNCE.htm)). The game-ready form takes the smaller of two impulses, written in terms of the slip u = v − rω:

```
slide: J_s = μ·(1+e_n)·|v_z|·sign(u)
grip:  J_g = α·(1+e_x)·u/(1+α)
J = J_s if |J_s| < |J_g| else J_g
v_x' = v_x − J,   rω' = rω + J/α
```

The consequence is the opposite of WildPitch's flat 15 %. A fast, shallow, skimming bounce loses little speed. A steep, spinless chip loses up to about 40 % (the grip cap α/(1+α) ≈ 0.39 with e_x = 0). Topspin skips on, and backspin checks. Football-specific oblique-bounce data (μ, e_x) were not found; the lead paper (Rezaei et al. 2011) was not retrievable.

**Air.** The aerodynamic constant is K = ½ρA/m = 0.053 m⁻¹, so the quadratic drag coefficient is k = K·C_D ([Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)). C_D is about 0.5 below the drag crisis and about 0.15–0.20 above it. The crisis sits near 12.5–13 m/s for 32-panel and Brazuca-type balls and 18.7 m/s for the Jabulani ([Kiratidis & Leinweber](https://arxiv.org/pdf/1710.02784)). WildPitch's k = 0.013 (C_D ≈ 0.25) is a fair single-value compromise. Goff & Carré's trajectory fit found C_D = 0.2 for an 18 m/s, 22° launch. Lift follows **C_L ≈ 1.15·Sp^0.83** with Sp = rω/v, plateaus near 0.3, and collapses toward zero in the laminar regime. Beckham's free kick left the foot at about 36 m/s with ω ≈ 63 rad/s (Sp ≈ 0.19) and moved about 3 m sideways ([Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)). Spin fell only 4–8 % by just past the apex (same source). The implied decay time constant of roughly 7–15 s is inference, not a measurement.

**Rotation.** A rolling ball turns at ω = v/r ≈ 9.1·v rad/s, or 1.45 rev/s per m/s. That is about 87° per 60 Hz frame at 10 m/s and about 174° at 20 m/s, where the pattern aliases. The arithmetic is from the research notes. The visual consequence is that a renderer must clamp or blur the drawn spin above about 8–10 rev/s.

| Quantity | Source-backed range | WildPitch now | Confidence |
|---|---|---|---|
| Normal restitution on turf | 0.55–0.71 (FIFA 2 m drop) | 0.55 | High |
| Engine restitutions | GRF 0.62, SWOS 0.625, YSoccer 0.585 | — | High (code) |
| Roll decel, low speed | a 0.23–0.70 m/s², b 0.10–0.16 s⁻¹ | a 0.9, b 0.35 | Medium (fit to trendline) |
| Air k = K·C_D | 0.0106 (C_D 0.2) – 0.0265 (C_D 0.5) | 0.013 | High |
| Drag-crisis speed | 12.5–13 m/s (32-panel/Brazuca) | none | High |
| C_L(Sp) | 1.15·Sp^0.83, ≤ ~0.32 | linear in "spin" | High (turbulent only) |
| Ball–grass μ_k | 0.5–0.6 (tennis proxy) | none | **Low** |
| α = I/mr² | 2/3 (thin shell) | none | Low (theory only) |
| Tangential COR e_x | 0.1–0.2 (generic balls) | none | Low for footballs |
| Spin decay τ | 7–15 s (inferred) | 2 s in air, 0.25 s on ground | Low |

## WildPitch's weirdness lives in four overrides and a painted-on spot

The codebase audit ranks visual problems by how often they occur times how visible they are. The top four are not friction. The ball is three soft discs with a dark spot at a constant offset (`src/game/render.c:219-223`), and nothing in the sim or the renderer tracks a roll angle.

The kick wind-up gather sets `b->vel = p->vel + (spot − ball)·60/remaining` every tick and zeroes `z` (`src/sim/player.c:185-192`). That is measured pulling the ball at **up to 19 m/s through the player's body** on back passes and braking a forward ball from about 7 m/s to 0 in one tick.

With no stick input, the owner runs `vel = p.vel + (spot − ball)·3` every 4 ticks *before* the foot-reach check (`player.c:195-203`). This cuts a sprint-released ball from 6.4 to 0.69 m/s **1.3 m from the player** and then reels it back.

The owner is exempt from body collision (`src/sim/ball.c:91`), so a received ball can be pulled to **0.01 m from the player's centre**.

Behind these come the bounce issues: the flat `T_BOUNCE_KEEP = 0.85` on every landing, even 2 cm hops (`ball.c:39-43`), and the jump from about 2 m/s² air deceleration to 6.4 m/s² on landing.

Each fault maps to a pattern above. The gather and sole stop are the opposite of the YSoccer, SWOS and Kick Off approach of a touch-based, "not glued" dribble. GRF and rcss show the alternative: a contact far from the ideal point *degrades the kick* rather than teleporting the ball. The flat bounce loss contradicts the impulse model, and the fixed spot contradicts every engine studied.

One finding narrows the gather fix. `ball_reachable()` returns 1 for the owner regardless of distance (`player.c:29`), so `contact()` cannot whiff on an owned ball even without the gather. D-013's "a kick by the owner can never whiff" therefore survives any gather change. The gather exists to put the ball where the avatar's foot lands (`--check-kick-pose`) and to keep the outgoing direction clean, not to prevent whiffs.

## Recommendations: equations, constants, files, and expected numbers

All numbers labelled "prototype" come from a float re-implementation at 240 Hz. It reproduces the audit's baseline: a 12 m/s pass stops at 18.1 m in 4.5 s, and the full shot's first bounce goes 22.8 → 19.4 m/s. These are predictions to confirm with `test_ball`, not fixed-point measurements.

### R1 — Draw rotation, speed and height (render-only, no hash change)

Keep a presentation-only ball orientation in the render context: a float 3×3 matrix next to `ctx->anim[]` in `src/game/render.c`. Each frame, compute the interpolated ground displacement Δ = (bx, by) − (bx_prev, by_prev) and rotate the orientation about the world axis (−Δy, Δx, 0)/|Δ| by θ = |Δ|/R. Reset on the existing "no interpolation across resets" path.

Draw the 12 pentagon centres of a truncated icosahedron (the icosahedron vertices). Rotate them by the orientation and keep those facing the camera. Project each onto the disc with the camera tilt, and draw a dark soft disc of radius ≈ 0.3·br scaled by its facing cosine. At wide zoom the ball is only about 8 px across, so playtest whether 12 spots or 2–3 larger ones read better.

Above about 25° of drawn rotation per frame (≈ 3 m/s), fade panel contrast linearly to zero by about 10 m/s, so the ball never wagon-wheels. Above about 15 m/s, draw 2–3 fading ghost discs along the interpolated path over the last half-frame; a 30 m/s shot strobes 12.8 px per frame now. Give the ball shadow the same light offset as player shadows (+0.12, −0.08 m, `render.c:160`) scaled by height, so the shadow separates from a rising ball, as YSoccer and SWOS do with their diagonal z-offset. Optionally feed the camera interpolated positions (`src/game/main.c:332-337`).

Expected outcome: a rolling pass visibly turns, slow rolls read as rolling rather than sliding, and shots show a streak. Test with `--screenshot` at `--zoom 2` across a few `--frames` values and at wide zoom. `ctest` and the golden hash must be unchanged; that unchanged hash is the test that R1 stayed render-only.

### R2 — Replace possession velocity overrides with reach-limited touches (sim, golden update)

**R2a, sole stop (`control()` in `src/sim/player.c:195-201`).** Move the no-stick branch below the `dist > T_TOUCH_REACH` check, so a ball out of reach is never touched. Replace the 4-tick position servo with a **single stopping touch** that reuses D-014's predictor.

The target is X = p_stop + facing·0.4 m, where p_stop = p.pos + p.vel·t_stop/2 and t_stop = |p.vel|/T_STOP_DECEL. Size the ball's velocity so it *rolls to rest* at X: v = D⁻¹(|X − ball|), where D(v) is the stop distance under `ball_roll_decel()`. D⁻¹ should be a `static const fx` table (v every 0.25 m/s up to 25 m/s, so R6 can reuse it), built offline or at init by integrating the same decel and searched by bisection. If the ball is already past X, the touch is a sole trap that matches the player's velocity.

Do not cap this touch's Δv. Hand arithmetic for a sprint release shows why. The player stops in about 1.8 m, and the ball is 0.85–0.91 m ahead doing 6.4–7.7 m/s, so it must drop to about 1.9 m/s: a Δv of 4.5–5.8 m/s. The audit accepts a one-tick cut of that size when the foot can reach the ball.

The touch must also be allowed to cut the dribble cooldown short, as `wrong_way` already does (down to `T_TOUCH_MIN_GAP`). Otherwise the ball rolls out to the 1.32 m the audit measured before the touch can fire.

If the ball is still beyond reach, the recommended default is that the player keeps stepping toward it at up to walking speed for up to about 1 s, then traps it. That keeps the existing stop assertion (`tests/test_ball_control.c:55`: ball speed < 0.3 m/s, distance < 1.0 m, still owned). Letting the ball run free instead is the owner's call, and it would change that test.

**R2b, wind-up gather (`player.c:185-192`).** Stop pulling the ball to a fixed spot; **strike it where it will be**. At wind-up start, predict the ball's position at the contact tick from its current velocity and `ball_roll_decel()`. That predicted point becomes the contact point if it lies within KICK_REACH of the player's predicted position and on the ball's side of the body. If it is not, one bounded nudge of at most about 1.5 m/s per tick corrects the remainder.

Writing to `z` stops entirely. A ball above `WP_BAND_FEET_MAX` falls under gravity, and the wind-up extends by up to about 8 ticks or the kick becomes a volley.

This is a cross-module change. The avatar's kick pose (`src/game/avatar.c`) must take the actual contact point as a parameter, and `--check-kick-pose` must check against that point instead of the fixed `FOOT_SPOT`. A capped pull alone cannot replace the current gather: 4–6 ticks at 1.5 m/s per tick move the ball only 0.25–0.5 m, against the 0.4–0.65 m the audit's forward and 90° gathers need.

D-013's "the owner's kicks never whiff" still holds. `ball_reachable()` returns 1 for the owner (`player.c:29`), so `contact()` succeeds without a gather. The one hole is ownership dropping beyond `T_LOSE_DIST` (2.2 m) mid-wind-up. `KICK_START_REACH` (1.6 m) plus the prediction keeps the ball well inside that.

**Contact quality at `contact()`.** Scale the kick speed by q = 1 − 0.25·θ/π − 0.25·|ball − ideal_spot|/KICK_REACH. Here θ is the angle between (ball − player) and the kick direction, and the distance is measured from the ideal `FOOT_SPOT` point, not from the player centre. A perfect contact therefore gives q = 1 exactly, and the minimum-shot check (`test_ball_control.c:73`, speed > T_SHOT_MIN) still passes for standing shots. This is the rcss formula, re-based ([rcssserver player.cpp](https://github.com/rcsoccersim/rcssserver/blob/master/src/player.cpp)). Also blend in the incoming ball velocity by (1 − q)·0.5, after GRF's `bumpyRideBias`. Bad setup then becomes visible as a weaker, deflected kick instead of a magnet, and the audit's "kick slows the gathered back pass from 18.1 to 9.0 m/s" oddity goes away.

**R2c, first touch (`player.c:237-240`).** Keep the spill formula, which the audit judges fine. With R2a in place, nothing pulls the ball after the touch. For a chest or thigh trap, set vz to a small value (0 to +0.5 m/s) instead of relying on the later `z = 0` teleport. The drop from 0.82 m then takes about 0.4 s, which reads as a chest-down.

**R2d, owner body collision (`collide_bodies()` in `src/sim/ball.c:91`): only if pull-through is still seen after R2a–R2c.** R2a removes the pull that causes it. If a fix is still needed, exempt the owner only inside a foot zone around the *stick or ball-velocity direction*, not facing. D-014's touches follow the stick while the body turns at 540°/s, so a facing-based ±70° zone (YSoccer's slice) would make the owner's body block legitimate cuts and the 180° drag-back. The reversal and cut tests at risk are `test_ball_control.c:42-52` and `:130-131`.

These are **acceptance targets, not prototype predictions**, because R2 was not simulated:

| Target | Now (audit) |
|---|---|
| A gather never moves the ball through the body cylinder | Crosses the centre line |
| Minimum ball-to-player-centre distance ≥ 0.35 m outside the foot zone | 0.01 m |
| No braking while the ball is more than 1.0 m from the player | Braked at 1.3 m |
| No "reel back" faster than 0.3 m/s | Up to 1.36 m/s |
| No single-tick z drop greater than \|vz\|·dt + 1 cm | 0.82 m in one tick |
| All existing `test_ball_control` assertions still pass | — |

### R3 — Stateless impulse bounce (sim, golden update)

In `integrate()` (`src/sim/ball.c:37-46`), replace the `T_BOUNCE_KEEP` branch with the following, applied along the horizontal velocity direction for *every* landing, however small:

```
vin  = −vz                                  (impact speed, > 0)
vz'  = e_n·vin − c;  if vz' < T_BOUNCE_CUT then vz' = 0
loss = min( μ_b·(1+e_n)·vin , g_cap·|v| )   (stateless: assume no incident spin)
vel  = vel·(|v| − loss)/|v|
```

Constants for `src/sim/tuning.h`:

| Constant | Value | Basis |
|---|---|---|
| T_BOUNCE_REST (e_n) | 0.62 | FIFA turf 0.55–0.71; GRF 0.62, SWOS 0.625 |
| T_BOUNCE_LIN (c) | 0.06 m/s | GRF `linearBounce` |
| T_BOUNCE_CUT | 0.5 m/s | Rebound apex 1.3 cm, invisible; ends hops |
| T_BOUNCE_MU (μ_b) | 0.45 | Tennis-grass 0.5–0.6 and RL 0.285 bracket it; **low confidence** |
| T_BOUNCE_GRIP (g_cap) | 0.39 | α/(1+α) with α = 2/3, e_x = 0 |

Delete `T_BOUNCE_KEEP` and `T_BOUNCE_MIN_VZ`. Keep the `WP_EV_BOUNCE` event at −1.5 m/s. All the multiplications are plain `fx_mul` on literals; no new state is needed.

Prototype outcomes:

| Case | Now | Proposed |
|---|---|---|
| 2 m drop rebound | 0.56 m (audit, measured) | ≈0.70–0.74 m (inside FIFA 0.6–1.0; drag ignored in the upper value) |
| Horizontal loss, landing at 0.59 m/s | 0 | 0.43 m/s |
| Horizontal loss, landing at 0.61 m/s | 1.65 m/s | 0.45 m/s |
| Shot-min first hop (vin 1.17 m/s) | 13.4 → 11.4 m/s | 13.4 → 12.6 m/s |
| Full shot first bounce | 22.8 → 19.4 m/s | 22.8 → 20.0 m/s |
| Full chip first bounce | 11.2 → 9.5 m/s | 11.2 → 6.8 m/s (grip cap) |
| Full chip stops at | 39.7 m | **30.5 m** |
| Min chip (8 m/s) stops at | 16.3 m | 11.5 m |

The 0.6 m/s discontinuity disappears, because the loss is continuous in vin.

The trade-off is that low drives now run on further: a shot-min ball stops at 22.8 m instead of 18.6 m. That is the physically correct "skip on" behaviour, and it is the main thing the owner should judge. Because this model assumes no incident spin, *every* steep re-bounce of a chip pays the grip cap. That overstates checking slightly, which R4 corrects.

Audit item 6, the 2.4 → 6.4 m/s² deceleration jump on landing, mostly disappears too, because the cutoff removes the sub-1.3 cm hops that ran under air drag. If a residual jump remains, apply the ground decel with a GRF-style blend while z < 2.5 cm, using a linear ramp in place of GRF's `pow(bias, 0.7)`.

Tests: in `tests/test_ball.c`, add a drop-rebound test (0.6–1.0 m). Add a landing sweep with vz from −0.3 to −1.0 m/s in 0.01 steps at 11 m/s horizontal, asserting the loss is monotonic and no step exceeds 0.05 m/s. Add a chip-stop distance window. Update the STATUS feel table (carry, peak) from the new run.

### R4 — Topspin state: skid-to-roll and spin-aware bounce (sim, golden update; optional)

Add `fv2 roll` to `WpBall` (`src/sim/sim.h:49-55`). It is the contact-point surface velocity r·(ω × ẑ) in m/s, so the rolling condition is simply `roll == vel` and the slip is u = vel − roll, a 2D vector that handles curving paths without a quaternion. Hash it in `sim_hash()` and bump `WP_SIMSTATE_SIZE_EXPECTED` (1520 → 1528 on x86-64, to be confirmed by the `_Static_assert`).

On the ground, while |u| > 0.05 m/s:

```
vel  −= μ_k·g·û·dt
roll += (μ_k·g/α)·û·dt
```

with μ_k = 0.45 (low confidence) and 1/α = 1.5 as an FX literal. Clamp the overshoot to the common rolling velocity v_roll = (vel + α·roll)/(1+α). Once rolling, lock roll = vel and apply `ball_roll_decel()`. In the air, decay roll by τ = 10 s (T_SPIN_DECAY_AIR 0.1/s, replacing 0.5, low confidence). The bounce becomes the full slip-based min(J_s, J_g) impulse from the physics section, with e_x = 0. The prototype showed e_x = 0.15 making a chip *speed up* on its second bounce (4.8 → 5.6 m/s); e_x = 0 limits that to about +0.2 m/s, which is genuine topspin skip.

Kick spin is set per kind as a "spin fraction" f = roll/v at `contact()`:

| Kick | f | Effect |
|---|---|---|
| Pass | 0.5 | Struck slightly above centre |
| Shot | 0 | Driven |
| Chip | −0.6 | Backspin |
| Dribble touch, stopping touch | 1.0 | No skid, so D-015's touch-sizing predictor stays exact |

There is a hidden cost: **every writer of `b->vel` must also set `roll`**. That includes `contact()`, the dribble touch, the stopping touch, the gather, the first touch (set roll = new vel), body and post reflections (reflect roll with vel and scale it by restitution), and the net box (roll = vel). Any writer that is missed produces a phantom skid.

To keep D-015's invariant, re-fit the rolling constants with the skid active. The prototype gives a = 1.0 m/s², b = 0.30 s⁻¹, k_air = 0.013:

| Case | Now | With R4 |
|---|---|---|
| 12 m/s pass (f = 0.5) | 18.1 m / 4.5 s | 18.9 m / 4.5 s; skid ends at 5.0 m, 0.47 s, 9.2 m/s |
| Arrival at 5 / 10 / 15 m | 9.1 / 6.3 / 3.2 m/s | 9.2 / 6.5 / 3.7 m/s |
| 3 m/s roll | 2.8 m | 2.8 m |
| 6 m/s roll | 7.7 m | 7.9 m |
| Full chip, f = −0.6 | 39.7 m | 33.0 m; first bounce 11.2 → 5.6 m/s, second 5.2 → 3.5 m/s |
| Min chip, f = −0.6 | 16.3 m | 13.4 m |

Be honest about the payoff. With distances held fixed, the skid barely changes *where* passes go, and b cannot drop toward the real 0.1 without lengthening rolls. What R4 buys is coherence. Spin, bounce and the drawn rotation now agree: R1 can draw panels from `roll` instead of displacement, so a skidding pass visibly "slides then grips". Backspin and topspin become possible, and the ground for future learned "touch quality" exists.

Tests: skid-end speed within 2 % of (1+αf)/(1+α)·v₀ for f ∈ {0, 0.5}. A rolling ball never re-enters skid without an external write. Repeat R3's tests. Run `bench_sim` and note any change in ticks/s in STATUS.

### R5 — Aerodynamic refinements (sim, golden update; last and optional)

There are three independent switches. Note that changing air drag also changes ground decel, because `ball_roll_decel()` includes it, so a, b need re-fitting each time.

**Drag-crisis lookup table.** Replace the constant T_AIR_DRAG with a piecewise-linear `static const fx` k(v) from the Tango 12 fit ([Kiratidis & Leinweber](https://arxiv.org/pdf/1710.02784)): 0.0285 at ≤ 9 m/s, 0.0250 at 11, 0.0220 at 12, 0.0183 at 13, 0.0147 at 14, 0.0120 at 15, 0.0096 at 17, 0.0092 at 20, 0.0099 at 30, 0.0104 at 35. This gives the real "hang" at 12–15 m/s. In the prototype, though, chip carries moved by only 0–0.6 m, so on this pitch it buys little.

**Magnus from physical spin.** Replace `T_CURL·spin·speed` (`ball.c:23-24`) with a_lat = K·C_L(Sp)·v², where C_L = min(1.15·Sp^0.83, 0.32) comes from a 16-entry Sp table. Fade it to zero between 13 and 9 m/s, because lift collapses in the laminar regime. Map the existing `spin` (≤ 3.0) to Sp ≈ 0.2 at the cap. By hand arithmetic, not prototyped, that should give roughly Beckham-scale bend, 3 m over 27 m at 36 m/s, which is about twice today's 1.62 m at 25 m. The owner should judge whether that much curl suits a 40 m pitch.

**Topspin dip.** Optionally add vertical Magnus from `roll` in the air, for dipping drives.

Do not add a knuckle force unless it draws from `SimState.rng`, one draw per statement.

### R6 — Weighted passes by inverse stopping distance (sim, CP0.2)

For any autonomous 1D deceleration law, the distance from v₀ down to v_arr is D(v₀) − D(v_arr). The launch speed for a pass of length d arriving at v_arr is therefore exactly **v₀ = D⁻¹(d + D(v_arr))**, using the same table as R2a. Replace the fixed `T_PASS_SPEED` 12 m/s (`player.c:52`) with that. Use v_arr ≈ 4–6 m/s for passes to feet (comfortably below `T_CONTROL_MAX_REL` 16), clamp v₀ to 6–20 m/s, and compensate for `INHERIT_VEL` by subtracting the kicker's velocity component along the pass. With R4's skid, give passes their own table built with f = 0.5.

This matches how games treat pass speed: as a designer-owned curve. SWOS uses a distance table ([swos-port player.cpp](https://github.com/zlatkok/swos-port/blob/master/src/game/player.cpp)), GRF uses `power ∝ (d/60)^1.4` ([GRF AIfunctions.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)), and EA patches it. It fixes "short passes arrive hot, passes beyond 18 m never arrive".

### R7 — Small cleanups (mixed)

Three tiny sim changes can ride along with R3. **Net drag** should use the substep's `dt` instead of `WP_DT/4` (`ball.c:141`), so it no longer depends on the substep count. **A ball falling onto a head** (z within 0.2 m of `T_BODY_H`, moving down) should have vz reflected with restitution 0.3 instead of being pushed 0.36 m sideways. **Posts** can reflect `roll` together with vel at no extra cost once R4 exists.

## Ordered plan: ship the visible fixes first

Each step is one playtestable checkpoint-sized unit. It ends with `ctest`, `tools/check_determinism.sh`, a screenshot check, a STATUS update and a commit. The commit message must state any golden update made with `WP_UPDATE_GOLDEN=1 ./build/test_determinism`.

| # | Change | Files / functions | Sim or render | Golden hash | Owner playtest question |
|---|---|---|---|---|---|
| 1 | Rotating panels, contrast fade, speed streak, matched shadow offset | `src/game/render.c` ball draw (~L149-170, 219-223), render ctx | Render-only | Unchanged | Does the ball now roll rather than slide? Spot count at wide zoom? |
| 2 | Stopping touch replaces sole-stop servo; reach-gated | `player.c` `control()`; D⁻¹ table near `ball_roll_decel()` in `sim_internal.h` | Sim | Update | Does releasing the stick stop the ball naturally? Auto-step or let it run? |
| 3 | Gather → strike where the ball will be; no z teleport; contact-quality factor q | `player.c` `control()` gather branch, `contact()`; `src/game/avatar.c` kick pose + `--check-kick-pose` | Sim + render | Update | Do back and side passes still feel crisp and sure? |
| 4 | Chest-down drop; owner body collision only if pull-through persists | `player.c` first touch; maybe `ball.c` `collide_bodies()` | Sim | Update | Do receptions look physical? |
| 5 | Stateless impulse bounce + R7 cleanups | `ball.c` `integrate()`, `lines_and_net()`; `tuning.h` | Sim | Update | Do low drives skip on and chips sit down? Is a ~0.7 m rebound from 2 m too lively? |
| 6 | Distance-weighted passes (fold into CP0.2 aimed passing) | `player.c` `make_kick()` | Sim | Update | Do passes arrive at a receivable pace? |
| 7 | `roll` state, skid-to-roll, spin-aware bounce, kick spin fractions; panels driven by `roll` | `sim.h` WpBall, `sim.c` `sim_hash`, `ball.c`, `player.c`, `render.c` | Sim + render | Update + size assert | Does a struck pass "slide then grip"? Do backspin chips check? |
| 8 | Drag-crisis table, C_L(Sp) Magnus, spin decay τ = 10 s | `ball.c` `integrate()`, `tuning.h` | Sim | Update | Is the curl amount right for a 40 m pitch? |

Steps 2–4 can be a single checkpoint if the owner prefers fewer playtests, since they share one cause (position-servo overrides). Steps 7–8 are optional depth. Stop after 6 if the owner says the ball no longer looks weird.

## Conclusion

The research reframes the owner's complaint. WildPitch's friction numbers are already within reach of what games use, and they are deliberately harsher than real turf for good reason: a real-world constant set rolls passes off a 40 m pitch. What makes the ball look wrong is that it is *told where to be*, by velocity overrides with no reach limit, and that its rotation is never shown. Real physics and engine practice agree on the cure. Possession should be a sequence of bounded impulses that can only happen when a foot reaches the ball. The renderer should show rotation driven by distance. Bounces should cost speed in proportion to impact.

The biggest remaining unknown is football-specific friction data: grass μ_k, e_x and the shell's α are proxies. Expect to tune those three by eye once rotation is visible. The rotating ball itself will make skid, grip and check errors far easier for the owner to spot than they are today.
