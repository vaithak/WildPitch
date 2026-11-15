# WildPitch ball physics audit (simulation + rendering), 2026-11-15

Sources are the code (file:line, current working tree) and measurements from throwaway programs linked against
`build/libwp_sim.a`: `m1.c` (rolls, bounces, flights), `m2.c` (dribble, stick release, gather, first touch, chest),
`m3.c` (gather sweep over 20 timing offsets), `m4.c` (landing brake, bodies, net, posts). They live in
`/tmp/claude-1000/-home-vaithak-WildPitch/6ee2a09e-6816-4cfe-887a-b9e6ea79e320/scratchpad/` (this folder may be cleared).
Screenshots: `--autoplay --zoom 2 --frames 150/200` in the same folder (`ap150.png`, `ballcrop.png`).
Pitch is 40 x 25 m, goal 4 x 2 m, depth 1.2 m (sim.c:14-18). Default view is **wide**: `start_zoom = 0` (main.c:232),
50 m across 1280 px, so about 25.6 px/m at the look-at point (main.c:5).

## Ranked list: what looks wrong, by visual impact at default (wide) zoom

### Takeaway
The biggest "weird" effects are not in the rolling-friction formula (now plausible). They come from (1) the ball being
drawn as a disc with a **fixed** dark spot that never rotates, so every motion reads as a sliding puck, and (2) code that
**overwrites ball velocity** instead of applying physical impulses: the wind-up gather (ball pulled to the foot at up
to 19 m/s, sometimes straight through the player), the no-stick "sole stop" (5.7 m/s killed in one tick 1.3 m from
the player, then the ball rolls back to him), and the possession pull that drags a received ball through the
receiver. Then come the flat 15% horizontal-speed loss on every bounce, even 2 cm hops, and the step from air
drag to full rolling friction on landing.

### Cited Findings (ranked: how often it happens in normal play × how big it is on screen)
1. **No visible rotation; fixed "eyeball" spot.** The ball is three soft discs: an outline, a white disc and a dark disc at a
   constant offset (+0.2r, +0.15r) (render.c:219-223). Nothing in the sim or renderer tracks a roll angle. The sim has no
   rotation state except the sidespin scalar `spin` (sim.h:52). At wide zoom the drawn radius is 0.11 × 1.5 × 25.6 ≈ 4.2 px,
   so the spot is ≈1.6 px radius and offset ≈0.8 px. It is always in the same place, which reads as a sliding puck
   and makes slow rolls look like a stopping slide. Screenshot `ballcrop.png` (close zoom) shows the centred dark dot
   ("eyeball") on a flying ball. — render.c:219-223; sim.h:52
2. **Wind-up gather moves the ball by velocity override, up to 19 m/s and through the player's body.** During a
   kick wind-up the owned ball's velocity is set every tick to `p->vel + (spot - ball) * 60/remaining`, with `z=vz=0`
   (player.c:185-192). Measured over 20 timing offsets while sprinting (m3): forward pass gather speed 0.0–1.3 m/s, so the
   ball **brakes from about 7 m/s to near 0 in one tick (|dv| up to 7.4 m/s)** and waits for the foot. A 90° pass pulls
   the ball at 12.1–14.0 m/s. A 135° pass pulls it at 15.7–17.9 m/s. A back pass pulls it at 16.9–19.2 m/s over up to 1.08 m in 3 ticks. In the
   back-pass log (m2) the ball crosses the player's centre line (gap x 0.46 → 0.06 → −0.35 m at y=0), so it passes
   *through* him, and then the "kick" **slows** it from 18.1 to 9.0 m/s. — player.c:185-192, :97; m2/m3 output
3. **The no-stick "sole stop" brakes the ball remotely and then reels it back.** With no stick input the owner sets
   `ball.vel = p.vel + (spot - ball)*3` every 4 ticks (player.c:195-201). This branch runs *before* the foot-reach check
   (player.c:203), so there is no reach limit. Sprint release (m2): the ball runs to 1.32 m ahead and is then cut from 6.40 to 0.69 m/s in **one tick**
   while the player is still 1.3 m away. It then reverses and rolls back toward him at up to 1.36 m/s in 4-tick
   steps (+0.73, +0.35 m/s jumps). Jog release: stepwise drops of −2.06, −0.82, −0.84, −0.86 m/s every 4 ticks, then the
   ball turns around (−0.5 m/s). — player.c:195-203; m2 output
4. **Receiving a pass: the ball is pulled through the receiver.** First touch at 0.68 m from the player's centre
   cuts about 7.0 → 1.34 m/s in one tick (player.c:237-238; spill = rel/16 × 0.45). That is fine as a first touch. But the
   owner is exempt from body collision (ball.c:91), and with no stick the pull in item 3 aims at the spot in front of the
   facing direction. A ball arriving from behind or ahead is pulled straight through the body: measured minimum
   ball-to-player-centre distance **0.01 m** (body radius 0.30 + ball 0.11), with speed stepping 1.23 → 2.87 → 2.31 → 1.86…
   every 4 ticks (m4). — ball.c:91; player.c:195-201, 237-238; m4 output
5. **Every grass bounce removes 15% of horizontal speed, whatever the impact.** `vel *= T_BOUNCE_KEEP (0.85)` for
   any landing with vz < −0.6 (ball.c:39-43; tuning.h:11). Full-power shot (30 m/s, vz 4.2): 22.9 → 19.4 → 17.6 → 14.9 →
   14.3 → 12.1 m/s across three bounces; the third hop's apex was only 5.9 cm (m1). Shot-min (14/1.2): a 6.9 cm hop costs
   13.43 → 11.39 m/s in one tick, equivalent to −122 m/s² (m4). On screen the ball speed **steps down with no visible
   bounce**: at wide zoom a 6 cm hop is about 1.5 px.
6. **Landing changes deceleration abruptly (air ≈ 1.6–2.4 m/s², ground ≈ 6.4 m/s²).** Airborne balls get only quadratic
   drag (ball.c:22). Grounded balls get `0.9 + 0.35v + 0.013v²` (ball.c:30; sim_internal.h:23). Measured per tick (m4,
   shot 14/1.2): −2.4 m/s² while flying, −1.65 m/s² during 1–2 cm micro-hops, then **−6.4 m/s² the tick it settles** at
   11.2 m/s. Low skimming balls "glide" and then brake hard on contact.
7. **Bounce threshold discontinuity.** Landing with vz just below −0.6 m/s: `vz=0`, horizontal speed kept 100%
   (ball.c:44). Just above: 15% lost. Measured: the shot-min ball's last landing (vz ≈ −0.5) went 11.22 → 11.17 m/s, while the
   bounce before it lost 2.0 m/s (m1). — ball.c:39-45; tuning.h:12
8. **Dribble ball sits at a nearly fixed offset.** Steady state (m2): jog gap 0.53–0.56 m, sprint gap 0.85–0.91 m. Ball
   speed oscillates 4.89–5.66 m/s (jog, touch impulse +0.78 m/s every 16 ticks) and 6.40–7.68 m/s (sprint, +1.27 m/s every 20
   ticks) around the player's 5.28 / 7.04 m/s. The touches are real impulses, not teleports, but the gap varies by only 3–6 cm
   (about 1–1.5 px at wide). Without rotation (item 1) the ball looks glued in front of the feet, not pushed.
   — player.c:203-227; tuning.h:52-57
9. **Chest/thigh control then a quick kick: the ball teleports to the ground.** The gather sets `b->z = 0` (player.c:191).
   Measured (m2): a chest-controlled ball at z = 0.82 m goes to z = 0.00 in one tick when a shot is pressed (≈21 px pop at wide).
   The same gather also pulled it 1 m through the player at 11.6 m/s (from behind to in front). First touch above
   foot height only sets `vz=0` (player.c:239), so the ball otherwise drops under gravity from chest height while owned.
10. **Pass power is fixed at 12 m/s plus 50% of the run velocity (aim-independent).** Pass power `T_PASS_SPEED` (tuning.h:67;
    player.c:52) plus `0.5 × p.vel` (player.c:14, 97). Measured kicked speeds from a sprint (m3): forward 14.8, 45° 14.3,
    90° 12.5, 135° 10.2, back 9.0 m/s. Arrival speeds of a 12 m/s ground pass (m1): 9.1 m/s at 5 m, 6.2 m/s at 10 m, 3.2 m/s
    at 15 m; it stops at 18.05 m after 4.48 s. Short passes arrive hot; passes beyond ~18 m never arrive.
11. **Chips have no backspin and roll on like long balls.** Full chip (15 m/s, vz 8.5): apex 3.28 m, first landing at
    21.1 m at 11.3 m/s, five bounces, still 4.9 m/s at 33.9 m and out of play at 4.5 s (m1). The minimum chip (8/5) stops at
    16.5 m. — tuning.h:73-76; ball.c:37-46
12. **Body collisions are cylinders in xy only.** Head-on restitution 0.30 (tuning.h:44): 10.6 → −3.15 m/s (m4). A
    glancing hit keeps all tangential speed (10.5 → 8.0 m/s, deflected 57°). A ball falling onto a statue's head (z 1.75 m)
    is pushed **sideways 0.36 m** to the cylinder edge with vz unchanged (−3.74 m/s), instead of bouncing up (m4;
    ball.c:92-105).
13. **Net drag depends on the substep count.** `k = 1 − 5·(WP_DT/4)` is applied *per substep* (ball.c:141). The substep count n
    grows with speed (ball.c:171-174), so a fast ball gets n× more net drag per tick. Effect is modest: a 30 m/s shot
    settles 0.82 s after entering, a 10 m/s shot after 0.40 s (m4). Looks fine in practice.
14. **Posts:** 24.3 → −14.4 m/s (restitution 0.6, tuning.h:23); no vz change and no spin. Plausible. — m4
15. **Rendering details (smaller):**
    - Ball position/height are linearly interpolated prev→cur with alpha (render.c:149-151; main.c:326). This is correct.
      The camera target uses **uninterpolated** `g.cur` positions (main.c:332-337), so the camera target moves in 60 Hz
      steps. The exponential ease hides most of it.
    - Height is projected at full `ppm·s` while ground depth is foreshortened by `tilt` 0.62 (render.c:15). Heights read
      about 1.6× taller than the same distance in depth. A full chip apex 3.28 m ≈ 84 px at wide.
    - The ball centre is drawn at z + 0.11 m with an exaggerated radius 0.165 m (render.c:219-220), so the disc extends 0.055 m
      below its ground point. The ground shadow (rx ≈ 4.1 px, ry ≈ 2.5 px at wide; render.c:166-169) is mostly hidden
      behind a rolling ball. The shadow sits directly under the ball, while player shadows are offset (+0.12, −0.08)
      (render.c:160), so the light directions don't match.
    - Strobing: a 30 m/s shot moves 0.5 m = 12.8 px per 60 Hz frame at wide (≈1.5 ball diameters); a 12 m/s pass moves
      5.1 px. There is no trail or motion blur.
16. **Fixed-point precision: measured, not a problem.** For rolls at 3–22 m/s in six directions, the last non-zero
    speed before the stop is ≤0.013 m/s (no visible snap to zero) and the heading drift is ≤0.013° (m1). The stop rule
    `hs <= dec → 0` (ball.c:31) fires at ≈0.015 m/s per tick, so the ball eases to rest under the 0.9 m/s² constant term.

### Inferences
- Items 1–4 are probably most of the owner's "still looks weird". Items 2–4 happen on every pass, every stick release
  and every reception, and item 1 affects every frame the ball moves.
- Items 2–4 share one cause. Possession is done with **position-servo velocity overrides**: `vel = target_vel + gain·error`,
  re-applied every tick or every 4 ticks with no reach or collision limit. That is a magnet. A physical version would apply
  impulses only when a foot can reach the ball (≤ T_TOUCH_REACH), cap Δv per touch, and let the player move to the ball
  rather than the ball to the player (or keep the body collision active for the owner outside a small foot zone).
- Rolling rotation can be drawn without touching the sim: integrate a presentation-only roll angle from the interpolated
  displacement (angle += distance / R about the axis perpendicular to motion) and draw 2–3 panel spots that move across the
  disc. At 12 m/s the ball turns about 17 rev/s, which is 0.29 rev per 60 Hz frame. At 30 m/s it is 0.72 rev/frame, past the
  wagon-wheel limit, so cap the visual spin rate or blur the panels above about 15 m/s.
- From general physics (not verified with sources in this audit): a football is a hollow shell (I ≈ ⅔mR²). A ball kicked
  without spin first **slides** and slows quickly until v ≈ 0.6·v0, then rolls with low resistance. The `0.35·v` linear term
  is a stand-in for that slide phase but also applies to slow dribble rolls. A two-phase slide→roll model (needing a roll
  speed or spin state) would explain "fast bleed, then creep" more faithfully.
- From general physics (not verified with sources): bounce friction is limited by μ·(1+e)·|vz|. A shallow, fast bounce
  should therefore lose little horizontal speed (≈0.3–1.2 m/s for the hops measured, vs 2–3.4 m/s now). A steep bounce
  should lose more, up to the rolling condition. A fixed 0.85 does the opposite of real grass on skimming shots (item 5).
  Making the horizontal loss ∝ |vz| (capped) would also remove the discontinuity at the 0.6 m/s cutoff (item 7).
- Restitution 0.55 (2 m drop → 0.56 m first rebound, measured in m1) is probably at the low (dead-pitch) end of real grass
  values. Not sourced here.
- Without topspin/backspin state, the game has no dipping drives, no chips that check up, and no skidding topspin on landing.
  Item 11 (chips rolling 34 m) is a direct result.

### Gaps
- No external sources were consulted (scope was code + measurement). Real-world reference values (grass rolling
  resistance, slide μ, rebound height, pass speeds) are not cited.
- Not measured: keeper body interactions (T_KEEPER_BODY_R 0.48), headers, the AI (none yet), and Arcade/Retro styles
  (pixel_factor > 1 shrinks the ball to even fewer pixels).
- Only one screenshot pair was viewed (close zoom). Wide-zoom pixel sizes above are computed from render.c/main.c
  constants, not measured from a capture.

## Is there any slide-to-roll phase or spin beyond a single curl scalar? Is rotation visible?

### Takeaway
No. The ball state is pos, z, vel, vz and one sidespin scalar (sim.h:49-55). There is no slide/roll distinction, no
top/backspin and no roll angle. Rotation is invisible (fixed spot, render.c:223).

### Cited Findings
- WpBall = pos, z, vel, vz, spin ("sidespin… curls the ball"), owner, last_touch — sim.h:49-55.
- Magnus acts only in the air: `a = T_CURL·spin·(−vy, vx)` (ball.c:23-24; T_CURL 0.045, tuning.h:18). Spin decays
  0.5/s in the air and 4.0/s on the ground (ball.c:28, 33; tuning.h:19-20). It has no effect on the ground or on bounces.
- Curl measured: 25 m/s, spin 3 → lateral 0.59 m at 15 m and 1.62 m by 25 m. Spin 2.15 at the first landing, still 1.54
  when rolling (m1), but it no longer bends anything on the ground.
- Spin only comes from striking across the run: `spin = across·18`, clamped to 60° → 3.0 (player.c:78-84). Passes have
  spin 0 (player.c:50). Touches and first touch zero it (player.c:199, 223, 240).

### Inferences
- A rolling ball shows no rotation, while the dribble ball holds a fixed offset (ranked item 8), so on-ball play reads as
  a carried puck. This is presentation-fixable (see the ranked list Inferences).

### Gaps
- None beyond those listed in the ranked section.

## Bounce, restitution, micro-bounces, and the roll transition

### Takeaway
Bounce maths is stable: no jitter, at most about 5 hops, the last ones 1–6 cm. But the horizontal loss is a flat 15% per
bounce with a cliff at the 0.6 m/s cutoff, and the deceleration triples on landing (see the ranked list, items 5–7).

### Cited Findings
- `if z<=0 && airborne: if vz < −0.6 → vz = −0.55·vz, vel *= 0.85; else vz = 0` (ball.c:37-46; tuning.h:10-12).
- Full chip: bounces at 21.1, 28.7, 31.9, 33.3, 33.9 m with apexes before each of 3.28, 0.85, 0.23, 0.065, 0.016 m. The
  bounce event fires only when vz < −1.5 (ball.c:40) (m1).
- 2 m drop → 0.56 m first rebound (e = 0.55) (m1).
- A rolling ball at z=0, vz=0 takes the ground branch (`airborne = z>0 || vz>0`, ball.c:18), so the transition is clean. No
  re-launch was observed.

### Inferences
- Micro-hops below 2 cm (< 0.5 px at wide) are invisible, so they are harmless as jitter. They still trigger the 15% loss and
  the low air drag (items 5–6).

### Gaps
- Bounce on a slope or on the goal-frame base is not modelled (flat pitch only).

## Drag in air vs ground, curl decay, net drag, bodies, posts

### Takeaway
The air drag constant is sensible. The ground model is plausible in total distance. The jump between air and ground
decel, the substep-dependent net drag and the xy-only body cylinders are the oddities.

### Cited Findings
- Air: `kd = 0.013·|v|·dt` applied to all three components (ball.c:21-27). Full shot 30 m/s, vz 4.2 → lands at 21.2 m doing 22.9 m/s (m1).
- Ground roll profile at 12 m/s (m1): v = 10.4 / 9.0 / 6.8 / 3.7 / 1.8 / 0.47 m/s at t = 0.25 / 0.5 / 1 / 2 / 3 / 4 s; stops at 18.05 m, 4.48 s.
  Other speeds: 3 m/s → 2.77 m / 2.17 s; 6 m/s → 7.61 m / 3.28 s.
- Net, bodies, posts: ranked items 12–14 (ball.c:91-108, 141-147; m4).

### Inferences
- The "pass doesn't stop" complaint is fixed numerically. The remaining roll weirdness is more likely the missing
  rotation plus the possession overrides.

### Gaps
- Real-world grass deceleration curves were not sourced.

## Pass speed, dribble touches, kick velocity inheritance, gather

### Takeaway
Pass speed is fixed (12 m/s + 0.5·player velocity, 9–15 m/s actual) regardless of distance. Dribble touches are real
impulses but leave a near-constant gap. The wind-up gather is the single most magnet-like behaviour (ranked item 2).

### Cited Findings
- See ranked items 2, 8 and 10 (player.c:14, 52, 97, 185-192, 203-227; m2/m3).
- The touch sizing adds friction back as `decel(|need|)·T/2` (player.c:222). The ball still loses 0.05–0.07 m/s per
  tick between touches (m2), consistent with the shared `ball_roll_decel` (sim_internal.h:23).

### Inferences
- Pass speed scaled to the target distance (or charge) would fix "hot short passes / dying long passes". It is a gameplay
  choice for the owner.

### Gaps
- No measurement with an AI receiver (AI control arrives in CP0.3; `player_can_control` is seated players only, sim_internal.h:26-29).

## Rendering: interpolation, shadow, height, motion

### Takeaway
Interpolation is correct. The fixed spot (no rotation), the tiny ball (≈8 px diameter at wide), strobing at shot speeds
and a shadow mostly hidden under a rolling ball are the visual issues. See ranked items 1 and 15.

### Cited Findings
- Ranked items 1 and 15 (render.c:149-170, 219-223; main.c:5, 232, 326, 332-337).

### Inferences
- A short speed-scaled trail or stretched ellipse along the velocity would fix the strobing at 20–30 m/s. Rotating panel spots
  would fix the puck look. Both are presentation-only, so they cost no determinism.

### Gaps
- No frame-by-frame capture of a moving ball at wide zoom was analysed.

## Fixed-point precision at low speed

### Takeaway
Measured, not a problem (ranked item 16).

### Cited Findings
- Last non-zero speed ≤ 0.013 m/s and heading drift ≤ 0.013° over 30 roll cases (m1). `WP_DT` = 1092/65536 s (0.025% slow,
  sim.h:18), which is negligible.

### Inferences
- `fx_mul` floors toward −∞ (fixed.h:26), so negative components round slightly differently. Measured as a ≤0.013° heading
  difference, it is invisible.

### Gaps
- None.
