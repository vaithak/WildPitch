# Intent-aware movement assistance: techniques, formulas, tuning (aim-assist analogues -> football)

Scope note: public sources almost never publish exact numbers (cone angles, strengths). The shipped-game sources
below give *structure* (zones, inner/outer radii, falloff, opt-outs); concrete numbers are scarce and are flagged as
gaps. Anything marked **[speculation]** / **[inference]** is my own engineering proposal, not from a source.
WildPitch context checked locally: ball rolling decel in `src/sim/tuning.h` is `ROLL_DECEL + ROLL_LIN*v + AIR_DRAG*v^2`
(0.9 m/s^2, 0.35/s, 0.013), i.e. not constant deceleration, which matters for intercept math (see Q2).

## Q1. How do shooters and sports games implement magnetism, friction/slowdown, and auto-pursuit, and how is it blended with raw input?

### Takeaway
Every shipped system breaks assist into separate pieces: **friction/stickiness**, which lowers sensitivity near the target; **magnetism/pull**, which adds a bias toward the target; **centering/snap**; and outcome-side help such as **bullet magnetism** (Witchfire uses enlarged hit zones rather than curving shots). Each piece uses nested zones: an outer radius where it starts and an inner radius where it is at full strength, with falloff by range. All of them are kept subtle enough that player input can still override. In sports games, Madden's Heat-Seeker is the closest analogue to "help me reach the ball". It corrects the *pursuit angle*, only when the defender is already close, and it has an on/off toggle and a window-size setting.

### Cited Findings
- Halo 1 aim assist is described as four mechanisms. **Magnetism**: bullets curve slightly toward the target, by a different amount per projectile. **Snapping**: the camera turns toward an enemy near the reticle. **Acceleration**: the reticle starts slow and ramps up while the stick is held. **Friction**: turn rate drops over an enemy, top speed is reduced, and the crosshair decelerates more gradually after stick release. No numbers are published. — [Aim assist on Halo 1 (fan analysis)](https://goooooouwa.github.io/gaming/2020/02/07/aim-assist-on-halo-1.html)
- Halopedia: "magnetism" slows turning while an enemy is within a radius of the reticle, and it is reduced for distant enemies. The term is easily confused with bullet magnetism. — [Halopedia: Magnetism](https://halopedia.org/Magnetism)
- Destiny community taxonomy: **bullet magnetism** (path correction of rounds), **reticle magnetism** (controller only: a pull toward the target), **reticle friction** (controller only: look sensitivity drops while crossing a target), and **stickiness** (how much the reticle follows a target once you are on it). — [Destiny wiki: Aim Assistance](https://d2.destinygamewiki.com/wiki/Aim_Assistance)
- Insomniac's Resistance 3 GDC 2013 talk (Nick Weihs) covers camera acceleration, dead-zoning, and "aim assist systems such as magnetism, centering, and friction". The public abstract has no formulas. — [GDC Vault: Techniques for Building Aim Assist in Console Shooters](https://gdcvault.com/play/1017942/Techniques-for-Building-Aim-Assist)
- **Witchfire (The Astronauts) is the most concrete shipped-design write-up:**
  - The assist is attached to the *weapon*, so each gun has its own set of circles.
  - Stickiness uses **two radii**. Slowdown *begins* when the larger circle touches the enemy and is at *full* strength when the smaller one touches.
  - Stickiness traces only key bones such as the spine, so flailing limbs do not cause spurious slowdowns.
  - Magnetism also uses two circles. The outer one moves the impact point toward its edge but still misses; anything inside the inner one counts as a hit.
  - A smallest circle steers toward the critical area (the head).
  - Strength *grows with distance up to the weapon's effective range, then drops off quickly*.
  - Stickiness is controller-only, so mouse muscle memory is preserved.
  - Bullets do not curve; hits are resolved against the assist zones.
  - No numeric values are published. — [Witchfire aim assist](https://www.theastronauts.com/2019/02/witchfire-aim-assist)
- Fortnite-style terms from secondary guides: slowdown (friction) near an enemy, plus "rotational aid" for tracking moving targets. — [search summary of Fortnite guide](https://gamertagmythras.com/blog/fortnite/fortnite-aim-assist-settings-guide) (secondary, low authority)
- **Madden NFL 25 "Heat-Seeker" tackling (EA):**
  - It is active only while the Hit Stick, defensive-assist, or wrap-tackle button is held.
  - It "guides your defender on the proper angle to intercept the ball carrier".
  - Its stated aim is to "provide assistance without taking control away from the user" and to eliminate "small errors like oversteers".
  - Defenders with low awareness or agility ratings are more susceptible to fakes, so the assist is rating-gated.
  - It can be turned off online and offline.
  - Sources: EA's Madden 25 defense post, as quoted in the search result (the page now returns 404) — [EA: Madden 25 defense](https://www.ea.com/news/madden-25-defense); also [ESPN Playbook](https://www.espn.com/blog/playbook/tech/post/_/id/4807/madden-25-makes-defensive-adjustments)
- College Football 26 Heat Seeker Assist (community guide, not official): a slight pull toward the ball carrier that is "not like autopilot" and "won't make you tackle someone 10 yards away". It has a **Window Size** setting; the guide recommends 200% and calls the default too small. — [civil.gg CFB 26 Heat Seeker guide](https://civil.gg/tips/cfb-26-heat-seeker-assist-guide)
- EA FC "Auto Switching Move Assistance" (None/Low/Medium/High) keeps a newly auto-selected player running in its current direction for a short time, so the human has time to orient. Auto Switching itself picks "the best suited player… most likely able to take the ball". — [EA gameplay improvements Aug 2022](https://www.ea.com/news/gameplay-improvements-august-2022); [FC settings guide](https://fifauteam.com/fc-27-game-settings/) (secondary)
- **Auto-aim target selection as a utility product (mobile robot shooter):**
  - Score = Π (U_i ^ or × Factor_i) over five terms: aim closeness, distance, HP, cover, and hits already fired. Each U_i is in 0..1.
  - A weight near 0 disables that criterion.
  - A separate **sticky coefficient** keeps the current target during sustained fire, so the aim does not jump to a newly better target.
  - Results: 18% of players opted in, and 80% of those did not revert. — [Game Developer: fair auto-aiming in a robot shooter](https://gamedeveloper.com/design/how-to-create-a-fair-auto-aiming-system-in-a-robot-shooter-)
- **HCI equivalents:**
  - **Sticky icons** (Worden et al. 1997) lower the control-display (CD) gain inside the target, which effectively enlarges it in motor space. They were particularly helpful for older users.
  - **Gravity / force fields** pull the cursor even when it is not over the target. Ahlström's force field lowers CD gain in an area around the target.
  - Caveat: stickiness costs time when the user wants to move *away* past a distractor. — [Bateman/Mandryk/Gutwin UCalgary/USask TR on target assistance](https://www.cs.usask.ca/documents/technical-reports/2009/TR-2009-03.pdf); [Mäkelä et al. 2014 Magnetic Cursor](https://www.medien.ifi.lmu.de/pubdb/publications/pub/maekelae2014perdis/maekelae2014perdis.pdf)

### Inferences
- Football mapping **[inference]**:
  - "Friction" = reduce the stick's *turn-away* authority, or the speed of overrunning, near the ball.
  - "Magnetism" = add a heading bias toward the intercept point.
  - "Bullet magnetism" = widen the *control/touch radius* (outcome-side help, invisible to steering). This is the most robust form; see Q5.
  - "Snapping" = last-meter reach/warp.
- Copy the Witchfire structure:
  - Use **an outer radius/angle where the assist starts and an inner one where it is full**, with linear falloff between them.
  - **Track only the ball's ground position**, not the bouncing height, for steering.
  - Make the strength a **per-player attribute** (Madden ties it to awareness/agility). This fits the "players learn" vision: assist strength or accuracy can be a learned skill.

### Gaps
- No public numeric values for Halo, Destiny, CoD, or Resistance cone angles or strengths. I found no reliable source for "typical" degrees.
- I found no primary source for NHL puck-receiving assist, NBA 2K movement assist, or Rocket League (Rocket League is widely believed to have no ball-chase assist, but this is unverified here).
- A tennis auto-movement patent was found (USPTO 12201901: it computes an automatic movement destination from ball trajectory and speed, and chooses the court-centre side of the ball path, or the court-end side if there is not enough time). The PDF is a scanned image, so the assignee and details are unverified. — [USPTO 12201901](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/12201901). The Virtua Tennis and Top Spin internals were not found.
- "In Pursuit of Fair Aim Assist" was not found under that title.

## Q2. How do you compute an intercept for a moving, decelerating ball (pursuit vs. predicted intercept, aerial balls)?

### Takeaway
Reynolds' pursuit means "seek the predicted position", re-evaluated every tick, with the prediction horizon T chosen heuristically (T = D·c). Football can do better: the ball's future is deterministic and cheap to step, so the right primitive is a **forward-simulated earliest-reachable intercept**. Find the first tick k at which the player's time-to-reach the ball's position at k is ≤ k·dt, then seek or arrive at that point.

### Cited Findings
- Reynolds 1999, pursuit, verbatim:
  - "Effective pursuit requires a prediction of the target's future position… use a simple predictor and… reevaluate it each simulation step."
  - "The position of a character T units of time in the future… can be obtained by scaling its velocity by T and adding that offset to its current position. Steering for pursuit is then simply the result of applying the seek steering behavior to the predicted target location."
  - "Ideally, T would be the time until interception… T could be assumed to be a constant… better pursuit than simple seek (which corresponds to T=0)… A simple estimator of moderate quality is T=Dc where D is the distance between pursuer and quarry, and c is a turning parameter."
  - "care must be taken to reduce T (e.g. to zero) when the pursuer finds itself aligned with, and in front of, its quarry."
  - It also mentions a constant-bearing alternative (keep the target at a constant heading in local space), and notes that "Optimal techniques… exist in… control theory [Isaacs 65]. The versions given here are… nonoptimal."
  - [Reynolds 1999, Steering Behaviors for Autonomous Characters](https://ics-websites.science.uu.nl/docs/vakken/mcrws/papers_new/Reynolds%20-%201999%20-%20Steering%20behaviors%20for%20autonomous%20characters.pdf)
- Reynolds, **offset pursuit**: seek a point offset by radius R from the predicted position. This is useful for approaching the ball from the side the player wants to take it on. — same source.
- Reynolds, **arrival**: same as seek far away. "Outside the stopping radius this desired velocity is clipped to max_speed, inside… ramped down (e.g. linearly) to zero" (ramped_speed = max_speed·distance/slowing_distance). — same source.
- With **constant-deceleration** rolling, the time to cover a distance is a quadratic root, and it is unreachable if the ball stops first. With multiplicative (drag-style) friction the update is per-step scaling. — [StackOverflow 69316990 (kiwix mirror)](https://kaweah.freedombox.rocks/kiwix/stackoverflow.com_en_all_2023-11/questions/69316990/formula-to-calculate-time-needed-for-a-ball-to-reach-destination-where-the-bal); rolling resistance can be modelled as a constant negative acceleration — [gamedev.net](https://gamedev.net/forums/topic/541455-decelerate-golf-ball/541455/)
- Humans are bad at intercepting decelerating or accelerating targets: most early errors occur with decelerating targets, and errors are systematic when acceleration varies. This is a direct argument for the game doing the deceleration prediction *for* the player. — [Port et al. 1997](https://darkwing.uoregon.edu/~prd/pdfs/Port_EBR97.pdf); [Smeets et al. 2016](https://personal.fbw.vu.nl/~jsmeets/2016/Intercept_accelerate-iPER16.pdf)

### Inferences
**Algorithm for WildPitch's fixed-point sim [inference/speculation]:**
- **Stepping:** the decel model is constant + linear + quadratic, so a closed form is messy. Instead, step a *copy* of the ball's ground state with the exact same integrator for up to N ticks.
  - 60 Hz × ~2–3 s = 120–180 steps, a few hundred fx ops each.
  - Run it once per tick for the *controlled* player only, or cache the trajectory per ball-state change, because the ball is identical for all players.
  - The step needs no RNG, so it is deterministic.
- **Reach-time model:** for each step k, compute t_reach(k) = reaction + turn_cost(angle) + dist(p, b_k)/v_max (add an acceleration term if desired).
  - The intercept is the first k with t_reach(k) ≤ k·dt (+ a small tolerance).
  - If no such k exists, target the ball's rest point.
  - Monotonicity is not guaranteed, so do a linear scan, but break early once the ball has stopped.
- **Aerial balls:** steer to the ground-projected point at the first tick where ball height is ≤ a playable height (foot/chest/head band). This mirrors "trace key bones, not the whole body" from Witchfire. Ignore the bounce apex for steering; use the bounce landing points as candidate intercepts.
- **Tie-breaking:** prefer intercepts *ahead of* the ball over chasing from behind (the "constant bearing" idea). Use Reynolds' offset pursuit to arrive on the side facing the player's intended next direction.
- **No overrun:** apply arrival (ramp speed) so the player reaches the intercept at the ball-arrival tick rather than overrunning: desired_speed = min(v_max, dist/(t_ball_arrival)). This is the football analogue of friction.

### Gaps
- No published football-game intercept implementation was found (FIFA/PES patents were not located in this pass).

## Q3. How should the game decide that the player *means* to go to the ball (angle tolerance, distance gating, priority, override)?

### Takeaway
Shipped systems gate assist by:
- **context/button**: Madden only while the tackle/assist button is held;
- **proximity**: CFB "won't help from 10 yards"; Witchfire's outer and inner circles;
- **input device**: stickiness is controller-only;
- **an explicit "stickiness" hysteresis term**, so the target does not flip.

None publish angle values. A sound design is an angular cone between the stick and the direction to the intercept, with outer and inner thresholds, distance gating, and a hard override when the stick points clearly away.

### Cited Findings
- Madden Heat-Seeker activates only while specific defensive buttons are held. That is an explicit "I intend to tackle" signal, and the assist itself only corrects angle and oversteer. — [EA Madden 25 defense (via search excerpt)](https://www.ea.com/news/madden-25-defense)
- CFB 26 Heat Seeker works only when already close, with an adjustable "window size" (a proximity/angle gate). — [civil.gg](https://civil.gg/tips/cfb-26-heat-seeker-assist-guide)
- Witchfire gates by two nested radii and traces key bones only, so transient geometry does not trigger assist. — [The Astronauts](https://www.theastronauts.com/2019/02/witchfire-aim-assist)
- A sticky coefficient keeps the current target selected during sustained action: hysteresis against target switching. — [Game Developer robot shooter](https://gamedeveloper.com/design/how-to-create-a-fair-auto-aiming-system-in-a-robot-shooter-)
- HCI caveat: sticky targets slow the user down when they want to pass *by* a target. Assistance must not trap a user who intends something else. — [USask TR-2009-03](https://www.cs.usask.ca/documents/technical-reports/2009/TR-2009-03.pdf)
- "The game should interpret player intent, not just player input" (forgiveness design rationale). — [Celeste & Forgiveness, Maddy Thorson](https://maddymakesgames.com/articles/celeste_and_forgiveness/index.html) (the rationale appears in multiple secondary devlogs; Thorson's article says the game "wants you to succeed")

### Inferences
**[speculation, starting values for playtest]**
- Let θ = angle between the stick vector and the vector to the intercept point.
  - Full assist when θ ≤ ~20°.
  - Linear falloff to 0 at ~45°.
  - Zero beyond that: an explicit override, so the stick is treated as "pointing elsewhere".
  - Work in fixed point without trig: compare the dot product of unit vectors against cos thresholds (cos20 ≈ FX(0.94), cos45 ≈ FX(0.707)). This avoids atan.
- **Distance gating:** assist only if dist-to-intercept ≤ R_outer (e.g. 8–12 m), with full strength inside R_inner (e.g. 3 m).
- **Feasibility gate:** assist only if the intercept is reachable (from Q2). Never pull toward a ball you cannot get.
- **Hysteresis:** once engaged, widen the release angle (engage at 45°, release at 60°) and require the release condition for ~6–10 ticks (100–170 ms), so stick noise does not toggle it.
  - A deadzone or neutral stick should *not* engage chasing; the player should be free to stand.
  - Optionally, a neutral stick while the ball is coming *to* the player could engage a "receive" assist (FIFA-like). That is an owner decision.
- **Priority:** gate the assist behind higher-priority intents. A dribble or kick in progress or a shield button suppresses it, and so does the stick pointing toward a teammate or space during a pass-and-move (θ large).
  - This fits D-008: assist candidates get utility scores like everything else.
- **Sustain:** require θ inside the cone for ≥ 2–3 ticks before engaging, which filters out stick sweeps passing through the cone. Analogue: Witchfire ignores limbs.

### Gaps
- No published cone angles or engagement durations for any sports game.
- Madden's "window size" units are unknown.

## Q4. How should the raw stick and the assisted direction be blended (confidence lerp, speed adjustment, last-meter snap, motion warping)?

### Takeaway
The proven pattern:
1. Leave the player's input authoritative.
2. Add a bounded correction (angle bias or friction) scaled by a 0..1 confidence.
3. Scale speed to arrive on time instead of overrunning.
4. Hide the last few centimetres with outcome-side forgiveness: an enlarged contact radius, or animation/root-motion warping so the foot visually meets the ball.

### Cited Findings
- Madden: the assist fixes "small errors like oversteers" while not "taking control away from the user". This is a bounded angular correction, not a replacement direction. — [EA (search excerpt)](https://www.ea.com/news/madden-25-defense)
- Developer guidance: magnetism is a subtle bias scaled by distance and target size, and player input must always be able to override it. Too much feels like the game is aiming for you; too little does nothing. — [bugnet.io aim assist over/under correcting](https://bugnet.io/blog/how-to-fix-aim-assist-over-or-under-correcting) (secondary blog, moderate authority)
- Halo friction also changes *deceleration after stick release* (a gentler stop): a velocity-domain effect, not just a heading effect. — [Halo 1 analysis](https://goooooouwa.github.io/gaming/2020/02/07/aim-assist-on-halo-1.html)
- Reynolds arrival: ramp the desired speed linearly to 0 inside a slowing radius. Steering = desired_velocity − velocity, truncated to max force. — [Reynolds 1999](https://ics-websites.science.uu.nl/docs/vakken/mcrws/papers_new/Reynolds%20-%201999%20-%20Steering%20behaviors%20for%20autonomous%20characters.pdf)
- **UE5 Motion Warping:**
  - It adjusts root motion so a character lines up with a named target over a montage **warp window** (notify state start/end).
  - **Skew Warp** makes the character match the target location and rotation at the end of the window.
  - Translation and rotation can be toggled separately. Rotation can "match target" or "Facing", with a rotation time multiplier.
  - Warp points can come from a bone, e.g. the foot. — [UE docs: Motion Warping](https://dev.epicgames.com/documentation/unreal-engine/motion-warping-in-unreal-engine)
- Witchfire resolves hits against enlarged zones rather than bending bullets. This is the outcome-side analogue of "foot reach" assistance. — [The Astronauts](https://www.theastronauts.com/2019/02/witchfire-aim-assist)
- Celeste's positional forgiveness: wall-jump allowed 2 px from the wall (about ¼ tile), super wall-jump about 5 px, plus corner correction that nudges the player around ledges. These are small geometric fudges that players don't notice. — [Celeste & Forgiveness](https://maddymakesgames.com/articles/celeste_and_forgiveness/index.html)

### Inferences
**Blend proposal [speculation]:**
- **Heading:**
  - Let d_raw = stick unit vector and d_ast = unit vector to the intercept.
  - Let w = strength × angle_factor(θ) × dist_factor × skill.
  - d = normalize(d_raw + w·(d_ast − d_raw)), *or better*, rotate d_raw toward d_ast by at most min(θ, w·θ_max_corr) per tick.
  - Rotate-by-max is easier to bound and feels like "correcting oversteer" (Madden). A cap θ_max_corr of ~10–15° per tick is a guess.
  - Fixed point: rotate with a small precomputed sin/cos table, or nlerp with an integer sqrt (already presumably available in `fixed.h`).
- **Speed:** if the intercept is reachable early, scale run speed to v = clamp(dist / t_ball, v_min, v_max) so the player arrives on time rather than overrunning. This is "friction" in football form.
  - Only apply it when the stick magnitude is within ~10% of the intercept direction's demand. Never slow a player who is clearly sprinting past.
- **Last metre:** inside ~1 m, widen the control radius (outcome-side: "bullet magnetism" analogue) and let presentation warp the foot or body pose onto the ball over the last N frames.
  - The sim stays authoritative: the contact rule decides; rendering only interpolates the limb, as in UE motion warping's warp window.
  - This keeps D-rule "presentation never writes to sim".
- **Release:** when assist disengages, blend w back to 0 over ~4–6 ticks rather than dropping it instantly, so the heading does not jump.

### Gaps
- No public source documents FIFA/EA FC's foot-planting or "ball magnet" first-touch warping in detail.
- Typical per-tick correction caps are unpublished.

## Q5. UX evidence: what makes assistance feel good rather than "the game plays for me", and what accessibility/assist-level settings exist?

### Takeaway
The empirical evidence is that *outcome-side* assists (bullet magnetism, area cursor) are effective **and** hard to notice, while *control-side* assists that visibly move the cursor are noticed and disliked. Shipped games pair assists with on/off or level settings, device gating, and skill/rating dependence.

### Cited Findings
- Vicencio-Moreira, Mandryk, Gutwin and Bateman, "The Effectiveness (or Lack Thereof) of Aim-Assist Techniques in FPS Games" (CHI 2014):
  - Five assists were tested across three studies in an Unreal-based game.
  - The assists did well in a target range, but performance dropped once game elements were added.
  - **Bullet magnetism and area cursor held up across situations**; some others were **too noticeable to players**. — [Bateman publication page](https://scottbateman.github.io/publication/2014-01-01-The-effectiveness-or-lack-thereof-of-aim-assist-techniques-in-first-person-shooter-games); [BibTeX](https://bibtex.github.io/CHI-2014-Vicencio-MoreiraMGB.html)
- Follow-up "Does Helping Hurt? Aiming Assistance and Skill Development in a First-Person Shooter Game" (Gutwin, Vicencio-Moreira, Mandryk, CHI PLAY 2016) studies whether assist impedes learning. Its results were not retrieved here. — [DBLP](https://dblp1.uni-trier.de/rec/conf/chiplay/GutwinVM16.html)
- Motor-space techniques (sticky, gravity) improved selection accuracy "without being highly obvious to users" in distant pointing. — [USask TR-2009-03](https://www.cs.usask.ca/documents/technical-reports/2009/TR-2009-03.pdf)
- Reported finding (secondary): in a balanced-assist study, everyone enjoyed the game more, including players without assist. — [USask news "Aim assist in the crosshairs"](https://artscibeta.usask.ca/news/articles/5623/Aim_assist_in_the_crosshairs) (page returned 401 to fetch; claim is from a search snippet only, so unverified)
- Settings in shipped sports games:
  - **Madden** Heat-Seeker can be turned off online and offline. — [EA](https://www.ea.com/news/madden-25-defense)
  - **CFB** exposes window size. — [civil.gg](https://civil.gg/tips/cfb-26-heat-seeker-assist-guide)
  - **EA FC** has Auto Switching Move Assistance None/Low/Medium/High. — [fifauteam settings](https://fifauteam.com/fc-27-game-settings/)
  - Community complaints about FC auto-switching selecting "a random player not even close to the ball" show the cost of opaque intent guesses. — [EA forums](https://forums.ea.com/discussions/fc-25-ultimate-team-en/auto-switching-on-air-balls-and-loose-balls/11443191)
- Witchfire: assists exist to compensate for controller imprecision; mouse aiming is kept free of slowdown to preserve muscle memory. — [The Astronauts](https://www.theastronauts.com/2019/02/witchfire-aim-assist)
- Opt-in rollout: 18% opted in and 80% kept it. Experienced players still mostly prefer manual aim. — [Game Developer robot shooter](https://gamedeveloper.com/design/how-to-create-a-fair-auto-aiming-system-in-a-robot-shooter-)
- Forgiveness ("coyote time", input buffering) is framed as honouring intent. Over-buffering causes "one input treated as multiple" bugs. Fix this with smaller buffers and newest-input priority. — [Celeste & Forgiveness](https://maddymakesgames.com/articles/celeste_and_forgiveness/index.html); [bugnet coyote/buffer explainer](https://bugnet.io/blog/coyote-time-and-input-buffering-explained) (secondary)

### Inferences
- For WildPitch **[inference]**:
  - Prefer **invisible outcome-side help**: a slightly larger first-touch radius, and prediction of where the ball *will* be.
  - Make heading pull subtle and bounded.
  - Expose an **Assist: Off / Low / Medium / High** setting in the pause menu.
  - Make the assist strength per-avatar, in line with the "players learn" theme: a well-drilled player's assist is stronger or more accurate. That also gives a natural place for learned brains (D-008).
  - Keep keyboard and controller parity in mind. Keyboard 8-way input is coarse (45° steps), so it *needs* a wider cone, roughly ≥ 22.5° full-strength, so any 8-way direction near the ball can engage **[speculation]**.
  - A "buffered intent" window helps when the stick briefly leaves the cone (e.g. a 100–150 ms grace before release), analogous to coyote time.
- Test for "plays for me": with the stick released, the player must not walk to the ball, unless an explicit "receive" assist is enabled. With the stick pointed 60°+ away, the trajectory must be identical to assist-off.
  - Both are easy to assert in a deterministic headless test.

### Gaps
- No quantitative "noticeability threshold" (e.g. maximum degrees of correction before players notice) was found for movement assists.
- The full text of the CHI 2014 and CHI PLAY 2016 papers was not retrievable (403), so per-technique parameters are unknown.
