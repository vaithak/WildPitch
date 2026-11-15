# Ball physics in commercial football and ball games

Scope note: these notes come from about 18 search and fetch calls. Several primary pages did not give usable text: the EA pitch-note pages render as video or intro-only pages, operationsports returned 403, the glasp talk summary returned 403, the GDC Vault page is paywalled, and the PC Gamer Rematch article came back truncated. Some claims below therefore rest on search-result snippets of pages that could not be fully fetched. Those are marked "(snippet)". Anything without a source is under Inferences or Gaps.

## EA FIFA / EA Sports FC ball physics

### Takeaway
EA's only well-documented ball-physics fix was in FIFA 14. Their drag model was wrong: deceleration did not depend on speed, so the ball looked "floaty" and straight, and the Magnus curve was miscalculated. The fix was a correct speed-dependent drag plus Magnus force. Later FIFA and FC changes, such as FC 24's "AI True Flight", are described only in marketing terms ("dip, swerve, soar") and come with no published numbers. Passing "weight" is presented to players as discrete pass types with trap-error penalties, not as exposed physics.

### Cited Findings
- **FIFA 14's core bug was the drag coefficient.** Before FIFA 14, the ball "accelerated and decelerated at a fixed rate regardless of its starting speed". Senior gameplay producer Aaron McHardy said a ball at 30 or 50 mph slowed at "the same rate as if it were moving at five miles per hour." The ball looked "floaty" and followed an unrealistically straight path. — [Scientific American, Dec 2013](https://www.scientificamerican.com/article/fifa-physics-how-a-video-game-figured-out-air-resistance/)
- **How it was found.** A team of engineers and animators audited "all the projectile physics code in the game" and found "their drag coefficient was wrong". The drag error also produced unrealistic spin: a miscalculated Magnus effect limited curve variety. After the fix, McHardy said the ball "dips and swerves" and "we got so much more variety in the curve." The article gives no numeric coefficients. — [Scientific American](https://www.scientificamerican.com/article/fifa-physics-how-a-video-game-figured-out-air-resistance/)
- **The physicist's framing in the same article.** Physicist John Eric Goff explained that the ball is fastest right after it leaves the foot, air resistance slows it until the top of its arc, and it "should then pick up speed on its way down." In other words, the flight should be asymmetric rather than a parabola. — [Scientific American](https://www.scientificamerican.com/article/fifa-physics-how-a-video-game-figured-out-air-resistance/)
- **FIFA 14 marketing.** "Real Ball Physics" let the ball take less rigid paths in the air, with 3 new flight types (low rising, dipping, turbulent) whose trajectories depend on distance and force. It shipped alongside "Pure Shot", in which players adjust their approach and stride before striking and can shoot off-balance. (snippet) — [EA: Pure Shot & Real Ball Physics](https://ea.com/games/fifa/news/pure-shot-real-ball-physics); [Bleacher Report](https://bleacherreport.com/articles/1781263-fifa-14-real-ball-physics-explained-with-robert-lewandowski-and-isaac-newton)
- **FIFA 20 described ball-ground interaction in which pitch roughness affects bounce.** A rougher pitch makes the ball bounce more after ground contact. (snippet; the source page returned 403) — [Operation Sports FIFA 20 Pitch Notes breakdown](https://www.operationsports.com/fifa-20-pitch-notes-breakdown)
- **FC 24 "AI True Flight Ball Physics"** was built by "studying hundreds of real-world passes, shots, free-kicks, and crosses and tuning them to make the ball dip, swerve, and soar". EA published no mechanism or numbers. (snippet) — [EA FC 24 Gameplay Deep Dive](https://www.ea.com/games/ea-sports-fc/pitch-notes/news/fc-24-gameplay-deepdive)
- **FC 24 Precision Pass** respects the player's aim at a target location. "Swerve on demand" requires pass power above 40. (snippet) — [EA FC 24 Gameplay Deep Dive](https://www.ea.com/games/ea-sports-fc/pitch-notes/news/fc-24-gameplay-deepdive)
- **Ground-pass speed is a live tuning knob that EA patches.** FIFA 23 Title Update 2 increased ground-pass ball speed and published a before/after comparison (snippet) — [EA Pitch Notes FIFA 23 TU2](https://www.ea.com/games/fifa/fifa-23/news/pitch-notes-fifa-23-title-update-2). FC 26 TU 1.5.0 improved ground-pass accuracy and speed, with the effect "most noticeable on players with higher passing attributes" (snippet) — [EA Pitch Notes FC26 TU 1.5.0](https://www.ea.com/en/games/ea-sports-fc/fc-26/news/pitch-notes-fc26-title-update-1-5-0).
- **FIFA 16 ground-pass weights.** FIFA 16 offered three weights of ground pass: a cushioned short ball, a standard pass whose speed depends on the stick, and a harder pass. (snippet) — [FourFourTwo FIFA 16 passing guide](https://www.fourfourtwo.com/features/how-master-passing-fifa-16-fourfourtwos-essential-guide)
- **Pass speed trades against trap error.** On EA's mobile/"Advanced Passing" controls, the "Driven Ground Pass" is a faster ground pass. Driven passes add "trap error" to the receiver "based on the pass's distance and speed combined with the receiver's Ball Control". "Lob passes are now fully assisted." At power above 50%, ground passes and lofted through passes "consistently select the farther teammate." Tapping ahead of a receiver makes a through pass into space, while tapping on or behind them makes a pass to feet. — [EA: Advanced Passing](https://www.ea.com/able/news/gameplay-advanced-passing)

### Inferences
- The FIFA 14 symptom of the same deceleration at every speed is exactly what you get from constant (Coulomb-like) deceleration or a linear-in-v drag tuned at low speed. Real air drag is proportional to v², so a fast shot loses much more speed per second than a slow pass. For an indie game, use quadratic air drag (a = −k·|v|·v) in flight and a separate rolling-resistance model on the ground. The asymmetric "slows on the way up, steeper on the way down" arc follows from quadratic drag automatically.
- "Floaty" in the FIFA 14 sense means too little high-speed drag combined with weak, uniform Magnus. Increasing drag only at high speed, plus making Magnus scale with spin×velocity, gives dip and swerve.
- EA treats pass speed as gameplay balance rather than physics truth. The fact that they patch it suggests designers own a separate "pass speed" curve, a function of power, distance and attributes, layered on top of the ball sim. The trade-off EA exposes is speed versus receiver trap error, and that is a cheap, legible way to give pass "weight" meaning.

### Gaps
- No EA GDC talk with numbers on ball drag, Magnus or bounce coefficients was found. I also found nothing on ATTD/"Agile Dribbling" (FIFA 15–17) or on HyperMotion ball-touch syncing beyond marketing names ("Effort Dribble", "Controlled Sprint"). The FC 24 deep-dive page fetched only its intro.
- No published EA formula for assisted pass speed, meaning whether a pass is solved so the ball arrives at a target speed.

## Konami PES / eFootball

### Takeaway
Konami frames ball physics around reception. PES 2017's "Real Touch" makes the first touch depend on the pass's height and power and the receiver's ability, with the player able to steer the touch. Later eFootball patches tune "heavy touch" edge cases. No numbers are public.

### Cited Findings
- PES 2017 "Real Touch" "determines the many ways players receive a ball, dependent on the height and power of the pass, and the natural ability of the player." (snippet, Konami press text) — [DSOGaming PES 2017 first details](https://www.dsogaming.com/news/pro-evolution-soccer-2017-first-details-first-screenshots/); [Operation Sports](https://forums.operationsports.com/news/881996/pes-2017-announced-some-new-features-and-details-revealed/)
- Previews describe steering the movement stick during reception to take the first touch into space or drop back. (snippet) — [OSFTW PES 2017 details](https://www.osftw.com/news/883290/pes-2017-teaser-trailer-control-reality-plenty-more-details-revealed/?page=2)
- A PES 2018 review describes the first touch as "calculated by player attributes as well as the further inclusion of real ball physics directly controlled by the input ball touch." (snippet) — [Impulse Gamer PES 2018 review](https://www.impulsegamer.com/pes-2018-review/pes2018-005/)
- eFootball 2023 v2.4.0 fixed a case where, "when controlling a sudden oncoming ball, players may take an excessively heavy touch even if a Dash command is not performed". This shows that touch heaviness is tied to the sprint input. (snippet) — [Konami eFootball 2023 v2.4.0 Patch Notes](https://www.konami.com/efootball/en/page/2023/season3_patch-notes_v2-40)
- eFootball 2022 v1.0.0 had a "Player and Ball Behaviors" section, and reviewers noted "better ball tracking accuracy and response to L-input". (snippet) — [Konami eFootball 2022 v1.0.0 Patch Notes](https://www.konami.com/efootball/en/page/2022/season1_patch-notes_v1-0-0); [Operation Sports breakdown](https://www.operationsports.com/breaking-down-the-efootball-2022-v1-0-news)

### Inferences
- A consistent design pattern across PES and FIFA: first-touch quality = f(incoming ball speed, incoming height, receiver skill, whether sprinting). The output is a touch-away distance plus a direction error. This maps directly onto a utility/learning system, because control quality becomes a trainable stat.
- The "ball not glued" idea means dribble touches push the ball ahead by a distance that grows with run speed or sprint. Sprinting means bigger touches, which defenders can exploit. The eFootball patch confirms that sprinting enlarges touches in their model.

### Gaps
- I found no primary Konami source on a "ball is not glued" philosophy or on dribble-touch implementation details, and no numbers.

## Rocket League (Psyonix) — GDC 2018 "It IS Rocket Science!"

### Takeaway
Rocket League runs Bullet physics at a fixed 120 Hz, with its own gameplay overrides layered on rigid-body physics. Community reverse engineering gives these ball constants: radius about 91.25 cm (uu), max speed 6000 uu/s, gravity 650 uu/s² (about 2/3 of real g), restitution about 0.6, and ground friction about 0.285 applied to the contact-point slip velocity. A hand-tuned extra "hit impulse" on car touches makes ball hits predictable.

### Cited Findings
- The talk is "It IS Rocket Science! The Physics of 'Rocket League' Detailed" by Jared Cone (Psyonix), GDC 2018, Programming track. It covers "specific game design decisions and implementation details" behind the gameplay feel and networked physics. — [GDC Vault](https://www.gdcvault.com/play/1024972/); [Rocket League at GDC 2018](https://www.rocketleague.com/news/rocket-league-at-gdc-2018); [Game Developer video post](https://gamedeveloper.com/design/video-the-rocket-science-behind-i-rocket-league-s-i-physics)
- Psyonix chose Bullet because it is open source, so engineers could debug and modify it. Bullet ran gameplay physics alongside UE3's own physics. (snippet) — [Game Developer](https://gamedeveloper.com/design/video-the-rocket-science-behind-i-rocket-league-s-i-physics)
- **Physics runs at a fixed 120 Hz (about 8.3 ms).** A fixed step means physics frames are not computed with varying intervals when render performance changes, and the higher rate reduces inconsistencies from discrete collision detection, at extra CPU cost. (snippet of a talk summary; the page itself returned 403) — [Glasp summary of the talk](https://glasp.co/youtube/ueEmiDM94IE). `TICKS_PER_SECOND = 120` — [RLGym Game Values](https://rlgym.org/Cheatsheets/game_values)
- A community post says the server sends network updates at 60 Hz while simulating at 120 Hz. This is a user post, not official. (snippet) — [Steam forum](https://steamcommunity.com/app/252950/discussions/0/1693788202025800838)
- **Community-measured constants (not official Psyonix values):** `BALL_RADIUS 91.25 uu` (1 uu = 1 cm), `BALL_MASS 30` (arbitrary units), `BALL_MAX_SPEED 6000 uu/s`, `GRAVITY 650 uu/s²`. — [RLGym Game Values](https://rlgym.org/Cheatsheets/game_values). The rl_ball_sym library uses a drag constant of −0.0305 (a linear velocity damping term), mass 30, and a collision radius of 93.15 uu. (snippet) — [rl_ball_sym source](https://docs.rs/crate/rl_ball_sym/latest/source/src/simulation/ball.rs)
- **Bounce/friction model from Sam Mish's reverse engineering:**
  - Restitution C_R ≈ 0.6 ("a good approximation").
  - Friction μ = 0.285, slip-scaling Y = 2.0, spin constant A = 0.0003, R = 91.25.
  - Normal impulse: J⊥ = −m(1+C_R)v⊥.
  - Contact slip: s = v∥ + R(n×ω).
  - Tangential impulse: J∥ = −m·min(1, Y·‖v⊥‖/‖s‖)·μ·s, so friction is capped by the normal impact strength, like Coulomb friction.
  - Spin update: ω += A·R·(Δv∥ × n).

  — [smish.dev ball simulation 1](https://www.smish.dev/rocket_league/ball_simulation_1)
- Psyonix "applies an additional impulse to the center of ball that is more predictable than the one directly from the physics engine". A pure rigid-body model therefore does not match in-game car hits. (snippet) — [smish.dev ball simulation 3](https://samuelpmish.github.io/rocket_league/ball_simulation_3)

### Inferences
- Lessons for a small football game:
  - Use a fixed, high tick rate. WildPitch's deterministic fixed-point sim already does this.
  - Lower gravity to about 2/3 g for hang time and readability.
  - Add a designer-authored "kick impulse" on top of physics, so contact outcomes are predictable rather than emergent.
  - Make the bounce friction act on contact slip, so spin and velocity converge to rolling within one or two bounces. Backspin balls check up and topspin balls skid forward, for free.
- The friction term min(1, Y·|v⊥|/|s|) is cheap and fixed-point friendly. On a glancing bounce, friction is limited by the normal impulse, which prevents hard spin reversals.
- Rocket League's ball is intentionally oversized and floaty. That reads well for an arcade game, but a football game needs a much smaller relative ball and stronger ground friction. Don't copy the numbers wholesale.

### Gaps
- I could not access the talk transcript or slides, so I could not confirm Psyonix's own numbers or explanations, including client-side prediction/rollback details and the exact "hit impulse" formula. The talk is on GDC's YouTube channel per the search snippets, and the owner could watch it directly.
- I found no reverse-engineered rolling-resistance value for the RL ball on grass.

## Other games (Rematch, Football Manager, Sociable Soccer, SWOS, Mario Strikers, Captain Tsubasa, Inazuma, golf)

### Takeaway
Sloclap (Rematch, 2025) treats the ball as "the basic brick of the game... an atomic component" and tunes it with great care for game feel. Mechanically, Rematch uses 360° aimed "taps" and passive auto-control when near a ball that is not in a "Powerful" state. For FM, Sociable Soccer, SWOS, Mario Strikers, Captain Tsubasa, Inazuma and golf games, I found no citable ball-physics details within budget.

### Cited Findings
- Rematch devs (lead designer Dylan Allen and Pierre Tarno, per a secondary report) call the ball "the basic brick of the game, it's like an atomic component" and say that tweaks to it are handled with great care because changes are sensitive. Game feel is a core Sloclap value inherited from Sifu. (partial fetch plus snippet) — [PC Gamer](https://www.pcgamer.com/games/sports/rematch-devs-talk-balls-specifically-how-to-balance-the-balls-feel-with-function-its-the-basic-brick-of-the-game-its-like-an-atomic-component); [CPG summary](https://consolepcgaming.com/how-rematch-developers-balance-the-balls-feel-and-function-in-gameplay)
- **Rematch mechanics:**
  - Ball control is passive: it triggers when a player gets close to the ball, and the player automatically gains possession unless the ball is in a "Powerful" state. (snippet) — [Game8](https://game8.co/articles/latest/rematch-news)
  - Taps are aimed with the left stick, 360° around the player. (snippet) — [Rematch: Feature focus, tap mechanics](https://www.playrematch.com/post/feature-focus-tap-mechanics)
- Rematch's online ball sync across 10 players was a stated challenge in UE5 (snippet) — [Unreal Engine interview](https://www.unrealengine.com/developer-interviews/rematch-reinvents-football-using-unreal-engine-5). GamesRadar reports post-launch patching struggled with physics complexity (snippet) — [GamesRadar](https://www.gamesradar.com/games/sports/players-are-very-precise-and-very-demanding-rematch-was-a-very-strong-learning-curve-for-the-sifu-devs-but-sloclap-is-still-working-to-find-the-back-of-the-net).

### Inferences
- Rematch's "Powerful" ball state is a clean design pattern. Above a speed threshold the ball cannot be auto-controlled: it deflects or needs a deliberate action. Below the threshold, proximity gives auto-control. This turns pass and shot speed into a gameplay variable, much like EA's trap error, and fits WildPitch's utility-AI and learning framing.

### Gaps
- Football Manager 3D, Sociable Soccer / SWOS remakes, Mario Strikers, Captain Tsubasa, Inazuma Eleven and golf roll physics were not researched within the budget. No sources were found or fetched.

## Weighted passes, spin/rolling visuals, and player complaints mapped to parameters

### Takeaway
No commercial source I could reach publishes an "arrival speed" pass solver. The closest evidence is EA's patches to pass speed and its trap-error-versus-speed trade-off. The well-sourced complaint-to-parameter link is FIFA 14's: "floaty" plus straight flight plus little curve variety pointed to wrong (non-speed-dependent) drag and Magnus. "Heavy touch" is a reception-model issue, as in eFootball 2023 v2.4.0.

### Cited Findings
- "Floaty" ball with straight paths, caused by deceleration that did not depend on speed and by weak Magnus. Fixed with a correct drag coefficient and Magnus. — [Scientific American](https://www.scientificamerican.com/article/fifa-physics-how-a-video-game-figured-out-air-resistance/)
- Ground passes were too slow, fixed by raising ground-pass ball speed in FIFA 23 TU2. (snippet) — [EA FIFA 23 TU2](https://www.ea.com/games/fifa/fifa-23/news/pitch-notes-fifa-23-title-update-2)
- "Excessively heavy touch" on sudden incoming balls without sprint, fixed in the reception logic in eFootball v2.4.0. (snippet) — [Konami v2.4.0](https://www.konami.com/efootball/en/page/2023/season3_patch-notes_v2-40)
- Faster passes mean more trap error, scaled by distance and receiver Ball Control. — [EA Advanced Passing](https://www.ea.com/able/news/gameplay-advanced-passing)

### Inferences (uncited engineering guidance, not from sources)
- **Weighted-pass solver.** With ground deceleration a(v) = −(c_roll + k·v²), you can invert to find the launch speed that reaches distance d with arrival speed v_arr. With constant rolling deceleration alone, v0 = sqrt(v_arr² + 2·c_roll·d). Through balls aim at the receiver's predicted position after time t, then iterate once or twice. In fixed point, precompute a table of distance against v0 per v_arr bucket.
- **Mapping complaints to parameters:**

  | Complaint | Likely cause |
  |---|---|
  | "Beach ball / balloon" | Too much drag relative to mass, gravity too low, or restitution too high on grass |
  | "Ice rink" | Rolling resistance too low (the ball rolls forever) |
  | "Too heavy / brick" | Rolling resistance or drag too high, low restitution, or kick speeds too low |
  | "Glued" | Dribble-touch distance too small, or no sprint scaling |

- **Visuals.** Rotate the ball texture by ω (rolling: ω = v/R about the axis n×v), and blend toward real ω after bounces. Use a ground shadow that shrinks and sharpens with height; it is the main depth cue in a tilted 2.5D view. Add slight motion stretch or blur above some speed. None of this was sourced from a commercial game in this pass.

### Gaps
- I found no primary source on how FIFA, PES or Rematch compute assisted pass speed or through-ball lead, or on how they visualise spin (texture rotation, blur) or sync dribble touches to animation (ATTD, HyperMotion). These need GDC Vault access or dev-blog searches beyond this budget.
- I found no forum analyses of the "beach ball"/"ice rink" complaints that tie them to specific patches, apart from the cited items.
