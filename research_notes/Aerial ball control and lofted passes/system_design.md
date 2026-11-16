# System design for contextual ball-contact actions and chained touches (trap / chest / thigh / header / volley / first-time pass)

Scope: how games decide *which* ball-contact action happens (and with which body part) from context, how they buffer
inputs and chain touches, how animation is kept in sync with a contact tick, and what that means for WildPitch's
deterministic 60 Hz Q16.16 sim with a read-only presentation layer.

Source code read locally (clones in `/tmp/claude-1000/-home-vaithak-WildPitch/6ee2a09e-6816-4cfe-887a-b9e6ea79e320/scratchpad/repos/`):
- Google Research Football (GRF), commit `3d9e754`, `third_party/gfootball_engine/src/...` —
  https://github.com/google-research/football/tree/master/third_party/gfootball_engine/src
- YSoccer (Java), commit `043d0a5`, `java/core/src/main/java/com/ygames/ysoccer/match/...` — https://sourceforge.net/p/ysoccer/code/
- swos-port (Sensible World of Soccer reverse-engineered to C++), commit `5acc902`, `src/game/updatePlayers/...` —
  https://github.com/zlatkok/swos-port
Line numbers below are from those commits.

---

## Q1. How do GRF, YSoccer and swos-port choose between trap / header / volley, and handle "pass while the ball is in the air"?

### Takeaway
Three very different points on one spectrum. **SWOS and YSoccer** use a hard-coded decision tree over a few
**ball-height bands** (plus distance bands and "is there a stick direction") evaluated when fire is pressed; the body
part is implied by the band (kick only if the ball is near the ground, header if high). **GRF** has *no explicit
body-part decision at all*: the controller emits a **priority-ordered queue of intents** (buffered pass/shot → ball
control → trap → interfere → movement), and for each intent the humanoid searches an animation database for a clip
whose **authored touch point (3D, incl. height) at its touch frame** coincides with the **predicted ball position at
that frame**; whichever clip fits wins, so headers / chest traps / volleys "emerge" because those clips have touch
points at head / chest / knee height. A pass pressed while the ball is in the air simply succeeds if some pass clip
(e.g. a header-pass) can reach the predicted ball; otherwise the queue falls through to trap, and the buffered pass
remains pending and fires on a later touch (= the chain "control, then pass").

### Cited Findings

**swos-port (SWOS, 1994 logic, fixed point)**
- Every update, ball height is classified into exclusive bands on the team struct: `ballLessEqual4`, `ball4To8`,
  `ball8To12`, `ball12To17`, `ballAbove17` (ball.z in game units), and player-ball distance into
  `plVeryCloseToBall (<=32)`, `plCloseToBall (<=72)`, `plNotFarFromBall (<=2450)`; the comment says "Only one flag in
  each group is set... many later branches consult them without recomputing" —
  `src/game/updatePlayers/playerUpdateCore.cpp:104-138` ([repo](https://github.com/zlatkok/swos-port)). (The 32/72/2450
  thresholds look like *squared* distances — speculation, not verified.)
- The controlled-player update is an **ordered decision list**; the comment states "The order is significant: restart
  handling, ball acquisition, kick input, tackles, headers, and finally ordinary directional movement" —
  `src/game/updatePlayers/updateAllPlayers.cpp:112-221`.
- Kick (pass/shot) is only allowed when close and `team.ballLessEqual4` (i.e. ball at ground level) —
  `canControlledPlayerKick`, `updateAllPlayers.cpp:64-74`. So SWOS has **no volleys/first-time aerial passes** for
  outfield players: a ball above 4 can't be kicked.
- Tackle if fire + direction + ball `<= 8`; **jump (diving) header** if fire + direction + near + ball band `8To12`,
  `12To17` or `Above17` — `updateAllPlayers.cpp:195-217`. With **no stick direction**, fire at a ball in those bands
  starts a **static (standing) header** — `tryControlledPlayerStaticHeader`, `playerStates.cpp:502-513`.
- Dribble/"ball control" (ball sticks to player) only when very close and NOT `ballAbove17` / `ball12To17` —
  `updateAllPlayers.cpp:164-180` — i.e. a ball at chest/head height is not "controlled", it must be headed.
- The header **commits** to a state and contact is checked each frame inside a window: jump header connects only when
  `playerDownTimer >= 42`, `ballDistance <= 64` and ball z in `[8,15]` (`tryCompletingJumpHeader`,
  `playerUpdateCore.cpp:665-679`); static header similarly requires z in `[8,15]` each frame (`updateStaticHeader`,
  `:683-700`). Timing failure "simply leave[s] the player in the current jump-header state" — i.e. a whiff.
- A static header "can turn at most 90 degrees toward the controls" — `src/game/player.cpp:383` (aim is read at
  contact time, not at press time).

**YSoccer (Java remake of SWOS-style play)**
- Fire pressed in `PlayerStateStandRun.checkConditions()`: if player owns ball and `ball.z < 8` and moving → `stateKick`;
  else if `ballDistance < 120` and ball approaching → look up `ball.prediction[min(player.frameDistance, N-1)]`;
  if predicted `z > 18` → `stateHead`, else (moving, ball in front, distance > 12) → `stateTackle` —
  `PlayerStateStandRun.java:79-104` ([repo](https://sourceforge.net/p/ysoccer/code/)).
- `frameDistance` = earliest ball-prediction frame `f` the player could reach running at full speed
  (`dist(player, prediction[f]) < speed*f/refresh`) — `Player.java:283-305`. **Height is evaluated at the predicted
  interception frame, not the current frame** — the key idea for picking a contact type before the ball arrives.
- Possession/"trap" is implicit: `Player.getPossession` requires the ball inside a facing-oriented detection
  ellipse, `ball.z < PLAYER_H (22)`, and player-vs-ball velocity difference `< 320 + 10*control`
  (`Player.java:307-327`); I read the contested branch (ball owned by an opponent → tackling-skill roll); the branch
  that takes an unowned ball was not read, but the gating conditions above apply to both. No chest/thigh
  distinction is visible — anything below player height passes the height gate.
- Header state: jump after 0.05 s; contact if ball z within `[player.z+PLAYER_H-BALL_R, player.z+PLAYER_H+BALL_R]`
  and `ballDistance < 2*BALL_R`; outgoing direction is **read from the stick at contact time**
  (`ball.a = inputDevice.angle` else facing) — `PlayerStateHead.java:21-45`.

**GRF (gfootball engine; 100 Hz, 10 ms frames — every timer in the source steps by 10 ms)**
(Per-file blob URLs at the pinned tree: https://github.com/google-research/football/tree/3d9e754/third_party/gfootball_engine/src — commit
hash is the local clone's short hash.)
- **Intent queue.** `HumanController` builds a `PlayerCommandQueue` each frame: buffered pass/shot commands first
  (when the action buffer fires), then `_BallControlCommand`, `_TrapCommand`, `_InterfereCommand`, then movement —
  `onthepitch/player/controller/humancontroller.cpp:140-330`. `Humanoid::Process` iterates the queue "and pick[s] the
  first that is applicable" (`SelectAnim` returns false when no clip fits) —
  `onthepitch/player/humanoid/humanoid.cpp:230-246`.
  ([humancontroller.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/controller/humancontroller.cpp),
  [humanoid.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/humanoid/humanoid.cpp))
- **Action buffer (lifecycle verified in source).** A *fresh press* (edge: down now, up last frame) of short pass /
  long pass / high pass / shot sets `actionMode = 2` and remembers which button — `humancontroller.cpp:512-536`.
  While the button is **held**, `gauge_ms += 10` per 10 ms frame (clamped 10..1000) and `actionBufferTime_ms` is
  reset to 0; after **release**, `actionBufferTime_ms += 10` per frame ("button released, stay in this actionMode
  until actionBufferTime_ms becomes too big") — `:546-555`. So the 2 s lifetime counts **from release**, not press.
  Each frame while buffered, the pass/shot command is (re)built and pushed at the head of the command queue when
  the button is released, or still held with `gauge_ms > 500` ("allow anim to kick in before queue is complete...
  it will usually touch ball after the remaining time anyway, so we still have time to add more power"), or
  (redundantly) when not in possession and already released — `:121-135`. It keeps being re-pushed every frame
  until the humanoid actually performs it: the buffer is cleared only when the player's current function type is a
  pass/shot and its touch is no longer pending (`// clear buffer?`, `:57-67`), or **cancelled** if
  `actionBufferTime_ms > 2000`, or the player's time-to-reach-ball grew by > 700 ms vs the previous estimate, or he
  switched to an interfere action — `:102-112`. Cancels also exist for "shot then pass pressed" / "high pass then pass
  pressed" (`:76-90`). Power: `gaugeFactor = (gauge_ms - 60)/940` clamped 0..1 ("substract a little because we can't
  really press a button shorter than this"), `desiredPower = clamp(gaugeFactor^0.6, 0.01, 1)` — `:138-139, ~228`.
- While a pass/shot is buffered, the stick input fed to the trap/ball-control commands is **rewritten to bias braking
  toward the current facing** ("prefer braking, even on slight angles") so that the control touch sets up the pending
  pass — `humancontroller.cpp:~305-322` (quote at `:316`). This is an explicit "first touch prepares the buffered
  action" rule.
- If a pass/shot is in the queue, `preferPassAndShot = true` and trap/ball-control clips get their acceptance radius
  multiplied by 0.3 — so the system prefers a **first-time** pass/shot when one fits and only falls back to a control
  touch otherwise — `humanoid.cpp:237-244` and `GetBestCheatableAnimID` (`radiusFactor *= 0.3f`).
- **Clip filtering by function type, not body part.** `CrudeSelection` filters the anim DB by function type
  (Trap / BallControl / ShortPass / HighPass / Shot / Deflect / Interfere / Sliding), incoming velocity class, incoming
  body direction, and for Trap/Interfere/Deflect the **incoming ball direction** (from the ball prediction at 120→180 ms)
  — `humanoid.cpp:1225-1340`. Long pass reuses short-pass clips ("hax") — `:1236`.
- Header clips simply live inside the pass / shot / interfere folders (e.g. `data/media/animations/pass/idle/nonbase/000_header.anim`,
  `090_header.anim`, `070_header_jump.anim`, `shot/sprint/000_headerdive.anim`, `interfere/sprint/000_header_jump.anim`).
  A "header pass" is therefore just a pass intent whose best-fitting clip touches at head height.
- **Body part is derived offline**: at load time the engine poses each clip at its touch frame and stores the name of
  the body part closest to the authored ball position as `touch_bodypart` (`AddExtraTouches`,
  `onthepitch/player/humanoid/animcollection.cpp:886-919`); it's only used for bookkeeping
  (`SetLastTouchPlayer(..., GetTouchTypeForBodyPart(...))`, `humanoid.cpp:354,407,426`).
- **Contact solver ("cheatable anim").** For each candidate clip (in sorted preference order) and each of its touch
  variants (middle touch frame first, then fanning out), the engine predicts the ball at `touchFrame*10 ms`, transforms
  the clip's authored ball position into world space (scaling height by player height / default height), and computes
  the 3D miss `ballPos - animBallPos`. The **Z miss** gates: accepted if `< 0.22 m` after forgiveness — higher balls get
  more Z "cheat" ("else we would have to make 100000000 anims for high balls on different heights"), balls above 1.8 m /
  2.6 m are penalised ×2 / ×20, and balls under 0.5 m may use ground ("base") clips with 0.15 m extra slack. The XY miss
  is judged by `GetBodyBallDistanceAdvantage` with a radius that grows for passes/shots (×1.3, +0.15 m), deflects (×1.8),
  shrinks for off-default touch frames, earlier touches are favoured, and the radius shrinks right after a previous
  touch (scaled by ball-control stat). First clip passing the test wins — `GetBestCheatableAnimID`, `humanoid.cpp`
  (starts line 1791; continuous check ~1920; pass-preference ×0.3 at ~2013).
- `useContinuousBallCheck`: instead of comparing at one instant, it takes the closest point on the ball's path over the
  surrounding ~9 ms to emulate a continuous "close enough?" check — same function.
- **Warp the body, not the ball.** The XY miss becomes `actionSmuggle`: a positional offset applied to the body
  gradually over the frames *before* the touch frame with a cosine ease (`humanoid.cpp:654-671`), plus a rotation
  "smuggle" — i.e. GRF motion-warps the root so the authored contact lands on the predicted ball.
- **Touch execution.** At `touchFrame == frameNum` the engine re-checks the ball is within `touchableDistance = 0.4 m`
  (3D) and height within 1.0 m; it blends the intended outgoing ball velocity with the ball's current velocity by a
  "bumpyRideBias" proportional to the miss distance (a sloppy contact keeps more of the incoming velocity), then calls
  `Ball::Touch(vec)` — `humanoid.cpp:358-430`. For passes, the target is **re-evaluated at contact time** (`AI_GetPass`
  again) and accepted only if within 0.15π of the original direction (clamped up to 0.3π) — `:429-470`. High passes
  set a different `forwardness` constant (`-1.3f`) — `:504` (meaning of that variable not read; unverified).
- **Requeue rules (chaining/cancel windows).** Re-selection is throttled to every 20–80 ms depending on distance /
  possession (`humanoid.cpp:144-176`); a Trap may requeue into Trap/BallControl only while
  `frameNum + 6 <= touchFrame` ("after this # of frames remaining to touchframe, just let anim finish") —
  `humanoid.cpp:54, 1166-1167`; BallControl→Trap is disallowed (`:1165`).
- Global gates in `SelectAnim`: touch clips are not considered if the ball (predicted 200 ms ahead) is > 10 m away
  (`ballDistanceOptimizeThreshold`), or if the ball predicted at +80 ms is > 2.0 m away **and** moving away from the
  player — `humanoid.cpp:1114-1140`, `gamedefines.hpp:68`.
  `defaultTouchOffset_ms = 80` (`gamedefines.hpp:73`).
- "High or bouncy ball" flag: ball predicted height at +10 ms or +80 ms > 0.3 m, or |vz| > 1.0 m/s ⇒ loosen trap clip
  strictness (`HumanoidBase::_HighOrBouncyBall`, `humanoidbase.cpp:966-980`).
- A non-animated "controlled ball collision" path exists for dribbling (ball < 1.6 m high) that applies a trap vector
  when the ball would otherwise roll away — marked `EXPERIMENTAL` in source, `humanoid.cpp:312-356`.

### Inferences
- The decision "which body part" is best treated as **a consequence of where the ball will be at contact** (predicted
  height at a candidate contact tick), not as an input; the input only chooses the *intent* (control / pass / shot /
  clear) and maybe a modifier. All three engines agree on this; GRF just has many more candidate "clips".
- GRF's queue-with-fallback is a clean, generic chaining mechanism: buffered intent tried first every re-plan tick;
  if no contact solution exists for it, a control action runs; the intent stays buffered and is tried again after the
  control touch. This directly implements "chest control then pass" and "first-time when possible".
- SWOS/YSoccer show the minimal viable version: bands + evaluate height at predicted reach frame (YSoccer) + read
  stick at contact time (both). WildPitch's D-010 height bands already match this family.

### Gaps
- I did not read GRF's `_TrapCommand` / `_BallControlCommand` bodies in detail (they build movement targets toward
  the predicted ball); details on how the trap's outgoing direction (`GetTrapVector`) is chosen were not read.
- swos-port's exact header ball-output (`playerHittingJumpHeader`, `doLobHeader`) and SWOS "aftertouch" were not read.
- YSoccer has no chest/thigh/volley distinction at all (confirmed by the state list: no chest/volley states in
  `PlayerState*.java`).

---

## Q2. What do commercial games / talks say (FIFA, PES, Madden, Rocket League, motion matching, contact solvers, input buffers)?

### Takeaway
Public material is mostly marketing-level, but the consistent pattern is: (1) a large library of contact animations
selected by context (ball height/speed/direction, player velocity/facing, attributes), (2) **warping** the chosen clip
so the contact happens at the right place at the right time ("animation that must reach a specific point at a
specific time" — EA, GDC 2010), (3) attribute-gated special traps, (4) intent buttons pressed *while the ball is in
the air* (Madden catch types; FIFA first-time skills) and (5) small timing windows measured in frames (FIFA green
timing 2 frames; Smash buffer 9 frames). Rocket League sits at the other extreme: no contextual contact actions, pure
physics of a car hitbox on a ball.

### Cited Findings
- **EA FIFA, GDC 2010 "Animation Warping for Responsiveness in FIFA Soccer"** (Paul McComas, Jason Sanmiya): covers
  how turn animations are procedurally altered on the fly and how the approach extends to "any animation that must
  reach a specific point at a specific time" — [GDC Vault](https://gdcvault.com/play/1012342/Animation-Warping-for-Responsiveness-in)
  (abstract only; talk content not accessible).
- **FIFA 14 (Precision Movement / chest trap):** EA previews describe trapping with the chest as an explicit input and
  trapping "in a half-turn in one motion" instead of touch-then-turn —
  [EA FIFA 14 elite technique](https://ea.com/en-gb/games/fifa/news/fifa-14-next-gen-elite-technique-first-look-uk) (via search
  snippet; page not fetched); Precision Movement was described as touching "first touch, dribbling, shooting,
  positioning, and AI" — [EA FIFA 14 precision movement](https://www.ea.com/games/fifa/news/fifa-14-precision-movement-first-look) (search snippet).
- **FIFA 19 Active Touch:** trap variations incl. "disguise" traps; head, chest, knees and feet can flick the ball to
  change direction — [EA FIFA 19 Active Touch](https://www.ea.com/en-gb/games/fifa/news/fifa-19-gameplay-active-touch-system)
  (search snippet; fetched page had no body text — treat as unverified detail).
- **FIFA 22 pitch notes** (fetched): "Composed Ball Control" = "longer 2 touch animations" to control air balls,
  gated by: not sprinting, no opponents nearby, left stick still, and attributes (air/ground: ≥70 Ball Control and
  ≥60 Agility); "The Active Touch system can now evaluate more situations at once"; "Skill Moves can now be executed in
  first time situations as the ball is coming towards the player" (some excluded; harder at high incoming velocity;
  not from driven passes); "You can request a different type of pass halfway through a pass request" (the system tries
  to stop the current pass); ML-Flow "generate[s] ball approach animations in real time" (stride adjustment, cadence,
  pose matching, transitions); improved tackling of air balls "during an opponent's flick up or chest trap" —
  [EA Pitch Notes FIFA 22 deep dive](https://www.ea.com/ea-play/news/pitch-notes-fifa-22-gameplay-deep-dive).
- **HyperMotion (FIFA 22):** 4000+ new animations from 11v11 Xsens match capture; ML-Flow trained on 8.7M frames —
  [EA HyperMotion page](https://ea.com/games/fifa/fifa-22/hypermotion); [80.lv](https://80.lv/articles/ea-uses-machine-learning-for-animations-in-fifa-22) (marketing claims).
- **FIFA timed finishing:** second tap timed to a window; green = perfect; a patch narrowed the green window
  "from 2-4 frames to 2 frames for all shots" (reported via search snippet of
  [EA FIFA 20 follow-up pitch notes](https://ea.com/games/fifa/news/pitch-notes-fifa-20-gameplay-follow-up); my fetch of that URL
  returned no body text, so the exact quote is unverified). Too-late tap: "your shot will not be affected... as the
  ball is already traveling" — [EA FIFA 19 timed finishing](https://www.ea.com/news/fifa-19-gameplay-timed-finishing) (snippet).
- **PES Real Touch / Real Touch+:** PES 2017 Real Touch: first touch computed from player attributes plus ball physics
  ([Konami](https://www.konami.com/games/eu/en/topics/13633/), snippet); PES 2018 Real Touch+ adds trapping with chest,
  stomach and thighs "depending on the scenario" — [Operation Sports](https://operationsports.com/pes-2018-features-strategic-dribbling-real-touch-new-set-pieces),
  [Game Informer](https://www.gameinformer.com/games/pro_evolution_soccer_2018/b/playstation4/archive/2017/06/14/kicking-off-another-year-of-improvements.aspx).
  No technical description found.
- **Madden catch types (intent chosen while ball is in the air):** Possession / Aggressive / RAC catch are different
  buttons pressed as the ball approaches; the game then picks the catch animation for that intent —
  [EA Madden 26 catch guide](https://www.ea.com/games/madden-nfl/madden-nfl-26/tips-and-tricks-hub/m26-how-to-catch-the-ball),
  [madden-school](https://www.madden-school.com/how-and-when-to-use-rac-vs-possession-vs-aggressive-catch-in-madden-20/).
  Guides say to press as the ball approaches and that the catch goes through even with no prompt (search summary).
- **Rocket League:** GDC 2018 "It IS Rocket Science! The Physics of Rocket League Detailed" (Jared Cone) covers the
  design and implementation of its networked physics — [GDC Vault](https://www.gdcvault.com/play/1024972/),
  [Game Developer](https://gamedeveloper.com/design/video-the-rocket-science-behind-i-rocket-league-s-i-physics). A summary
  claims a fixed 120 Hz physics step ([Glasp summary](https://glasp.co/youtube/ueEmiDM94IE), low reliability).
- **Motion matching:** Clavet (Ubisoft, For Honor), GDC 2016 "Motion Matching and The Road to Next-Gen Animation":
  every frame search all mocap frames for the best match to current pose + desired trajectory and transition there;
  needs large data and CPU — [GDC Vault](https://www.gdcvault.com/play/1022985/Motion-Matching-and-The-Road),
  [Game Watch (JP)](https://game.watch.impress.co.jp/docs/news/749136.html); the "Learned Motion Matching" paper credits
  Clavet and Büttner with introducing the technique (attribution of the 2015 talk is unclear between sources)
  — [paper PDF](https://www.theorangeduck.com/media/uploads/other_stuff/Learned_Motion_Matching.pdf). No public source
  found on motion matching specifically for ball contact.
- **Local Motion Phases** (Starke, Zhao, Komura, Zaman, SIGGRAPH 2020; with EA): per-bone phase defined by contacts
  with external objects (ball), used for basketball dribbling, shooting, catching from gamepad control —
  [SIGGRAPH history](https://history.siggraph.org/?p=78734), [PDF](https://www.ipab.inf.ed.ac.uk/cgvu/basketball.pdf).
- **Motion warping (UE5):** warp windows are anim-notify-state intervals on a montage, bound to a named warp target;
  "Skew Warp" adjusts root motion so location/rotation match the target **at the end of the window** — set the window
  to end at the strike frame to land contact on the target —
  [Unreal docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/motion-warping-in-unreal-engine).
- **Input buffering norms:** Smash Ultimate buffer = 9 frames (10 in Brawl); held inputs are buffered (can be pressed
  arbitrarily early but must be held into the window); Smash 4 has a priority order for multiple buffered inputs;
  criticism: buffered actions fire unintentionally when the situation changed (e.g. jab becomes aerial after leaving
  ledge) and more so online — [SmashWiki Buffer](https://www.ssbwiki.com/Buffer). Secondary explainers (search
  summary did not say which page made which claim; unverified) put typical windows at 80–150 ms (≈5–9 frames @60) and
  recommend a timestamped queue that discards stale entries — candidates:
  [cyberly](https://www.cyberly.org/en/what-is-input-buffering/index.html), [Wayline](https://www.wayline.io/blog/art-of-input-buffering).
- **GRF's numbers for comparison** (from source, Q1): action buffer lives up to 2000 ms after button release
  (indefinitely while held), cancelled when time-to-ball worsens by > 700 ms; a trap clip can no longer be swapped in
  the last 6 frames (60 ms at 100 Hz) before its touch.

### Inferences
- Madden's catch buttons are the cleanest commercial precedent for "button = intent, context = animation/body part"
  (inference: the guides describe intents, not how animations are chosen).
- Rocket League is the opposite pole: as far as is commonly known (not sourced here), it has no contextual contact
  actions — every touch is a rigid-body collision of the car hitbox with the ball.
- Football needs **much longer buffers than fighting games** (GRF: up to 2 s) because the press often happens while
  the ball is still travelling to the receiver; the right cancellation rule is *situational* (ball no longer coming to
  me / time-to-ball got much worse / possession changed), not a fixed short frame count. A short frame-count buffer
  (~6–9 ticks) is appropriate only for "pressed slightly before I became able to act" (e.g. during a control touch's
  recovery).
- "Intent chosen by button, body part chosen by context" (Madden catches, GRF, FIFA contextual headers) is the
  dominant pattern; extra modifiers (FIFA "composed" traps, flick-ups, first-time skills) are layered as optional
  intents with gating conditions (stick still, no pressure, attributes).
- Every AAA approach reconciles animation and contact by **warping animation toward a sim-determined contact**, not by
  letting animation decide the contact.

### Gaps
- No primary technical description of FIFA's or PES's contact/trap selection logic was found (only marketing/pitch notes).
- NBA 2K contextual controls were not researched (no sources gathered).
- FIFA 18 "Real Player Motion" (RPM) was not researched (tool budget); only HyperMotion / ML-Flow and Active Touch are
  covered.
- The FIFA 20 "2–4 frames → 2 frames" quote could not be verified from the page body.

---

## Q3. Recommended architecture for WildPitch (deterministic 60 Hz Q16.16 sim, read-only presentation)

### Takeaway
Keep **all** contact decisions in the sim as a small **intent → contact-plan → contact-tick** pipeline: the input
layer records an *intent* (pass/shoot/loft/control + aim + power) into a per-seat buffer; each tick a per-player
**contact planner** scans the deterministic ball prediction for the earliest reachable tick where the ball is inside a
body-part height band, scores candidate (intent, body part, tick) tuples with the existing utility style (D-008),
and commits a **ContactPlan {kind, bodypart, contact_tick, contact_point}** into `WpPlayer`. At `contact_tick` the sim
applies the ball impulse (reading aim from the stick *at contact*). Presentation only reads the plan and warps a
procedural pose so the chosen limb reaches `contact_point` at `contact_tick`. Chains fall out of the buffer: a pass
intent that has no valid contact plan (ball too high/fast for a first-time pass) degrades to a control plan; the intent
stays buffered and executes at the first touch whose plan allows it.

(Everything in this section is design recommendation = inference, grounded in the sources above.)

### Cited Findings (the precedents each recommendation rests on)
- Height evaluated at the **predicted reach frame**, not now — YSoccer `PlayerStateStandRun.java:89-93`,
  `Player.java:283-305`; GRF predicts ball at each candidate clip's touch frame — `GetBestCheatableAnimID`.
- Exclusive height bands + ordered decision list — swos-port `playerUpdateCore.cpp:104-138`, `updateAllPlayers.cpp:112-221`.
- Intent queue with fallback, pass preferred when it fits (trap radius ×0.3) — GRF `humanoid.cpp:230-246`.
- Buffered-intent lifetime/cancel conditions (2000 ms after release, time-to-ball +700 ms; re-pushed every frame until performed) —
  GRF `humancontroller.cpp:58-139`.
- Control touch biased to set up the buffered action — GRF `humancontroller.cpp:296-312`.
- Commit window: no re-planning in the last 6 frames before contact — GRF `humanoid.cpp:54,1166`.
- Aim read at contact time; target re-evaluated with a max deviation — GRF `humanoid.cpp:429-470`; YSoccer
  `PlayerStateHead.java:36-40`; SWOS static header ≤90° turn `player.cpp:383`.
- Contact quality from miss distance (blend intended vs incoming velocity) — GRF "bumpyRideBias", `humanoid.cpp:367-395`.
- Body warped to the contact (cosine-eased offset before the touch frame) — GRF `humanoid.cpp:654-671`; UE skew warp
  ends at strike frame — [Unreal docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/motion-warping-in-unreal-engine).
- Existing WildPitch constraint: D-010 "ALL ball contact is resolved in height bands — feet ≤ 0.55 m, body ≤ 1.45 m,
  head ≤ 2.05 m + jump height" (`WP_BAND_FEET_MAX/BODY_MAX/HEAD_MAX` in `src/sim/sim.h:76-78`); pass/shoot at a
  reachable airborne ball triggers jump/stand header (no jump button) — `docs/DECISIONS.md` D-010/D-011.
- **What `src/sim/header.c` already does (read in full)** — it is a one-kind instance of the recommended pipeline:
  - `ball_approach()` = analytic closest-approach of the ball's flight to a point: `t = clamp(d·v / |v|², 0, 2 s)`,
    `z(t) = z + vz·t − g·t²/2` — explicitly "flight approx: no drag" (so it ignores the sim ball's drag/Magnus).
  - `header_wanted()` = the decision: ball not owned, player grounded, ball rising or above body band −0.25 m; contact
    within 1.2 s, XY miss ≤ 2.0 m, predicted z ≥ body max −0.15 m ⇒ header; jump iff predicted z > head (1.70 m)+0.20.
  - `header_start()` = commits an action (`WP_ACT_JUMP_HEADER` / `WP_ACT_STAND_HEADER`) with intent in
    `kick.foot` (0 pass / 1 shot) — the intent is latched at press time, aim is not.
  - `header_step()` = take-off timing so the apex meets the ball, cancel if the ball has passed; `WAIT_MAX = 70` ticks.
  - `header_contact_dist()` = per-tick 3D window (|Δz| ≤ 0.32 m, XY ≤ 0.45 m from head point); nearest head wins
    contests. `header_contact()` = **aim from the stick at contact**, pass target via `pass_pick`, shot via aim assist.
  - `header_assist_move()` = sim-side steering under the ball while waiting ("the stick only AIMS").

### Inferences — concrete design

**Framing:** this is a *generalisation of `header.c`*, not a parallel system: `ball_approach` → shared reach/contact
scan for all bands; `header_wanted` → one row of a band→kinds table; `header_start/step` → generic committed
contact action; `header_contact_dist` → generic band-aware contact window; `header_contact` → per-kind ball response.
Headers become the HEAD row; chest/thigh traps, volleys and first-time passes become other rows.

**A. Data (all fixed-size, hashed, no pointers)**
- `WpSeatInput` stays the only input path. Add to the per-seat/controlled-player sim state an `ActionBuffer`:
  `{u8 intent (NONE/PASS/LOFT/THROUGH/SHOT/CONTROL_MODIFIER), fx power, i16 aim_dir_q (or fv2), u16 age_ticks,
  i32 ttb_at_press (ticks-to-ball when pressed)}`. One slot is enough (GRF uses one `actionMode`); newest press
  overwrites (prefer-newer avoids stale buffered actions per SmashWiki criticism).
- Per player `ContactPlan`: `{u8 kind (TRAP_FOOT, TRAP_THIGH, TRAP_CHEST, HEADER_CONTROL, PASS_GROUND, VOLLEY_PASS,
  VOLLEY_SHOT, HEADER_PASS, HEADER_SHOT, CLEAR), u8 bodypart, u8 jump, u16 contact_tick, fv3 contact_point,
  u8 committed}` (much of this can reuse existing `p->action`, `p->kick.*`, `action_timer`). **Both** the
  `ActionBuffer` and the `ContactPlan` live in SimState, so both must be added to `sim_hash()` and
  `WP_SIMSTATE_SIZE_EXPECTED` (CLAUDE.md hard rule).
- Tunables in `src/sim/tuning.h`: band edges (reuse D-010), max ball speed per bodypart for a "clean" control,
  buffer lifetime (~120 ticks = 2 s counted from button release, cf. GRF), stale-cancel rule (ticks-to-ball grew by > ~42 ticks ≈ GRF's 700 ms),
  commit window (~4–6 ticks before contact, cf. GRF's 6 frames @100 Hz = 60 ms ≈ 4 ticks @60 Hz), contact tolerance.

**B. Pipeline per tick (new small module, e.g. `src/sim/contact.c` + `contact_internal.h`)**
1. **Predict once**: the ball module already substeps; build (or reuse) a fixed-length ball prediction ring
   (e.g. 90 ticks) once per tick for everyone — deterministic, no allocation. GRF/YSoccer/SWOS all keep such a
   prediction array.
2. **Reach scan** (YSoccer `frameDistance` generalised): for player i, find earliest tick t where
   `dist2(player_reach_pos(t), ball_xy(t)) <= reach(t)^2`; read `ball_z(t)` → candidate body part(s) from bands.
   Optionally also test t+1..t+k (later contact = more time to set up a chest trap).
3. **Candidate generation**: for the buffered intent (if any) and the default CONTROL intent, enumerate legal
   (kind, bodypart) per band: feet band → ground pass / volley / foot trap; thigh/knee band → thigh trap / volley
   (only if incoming speed ≤ T and facing OK); chest band → chest trap (drop to feet) / chest-pass (weak, short);
   head band → header control / header pass / header shot (stand or jump per D-010).
4. **Utility scoring** (D-008 style, integer/fx): feasibility (time to set body, incoming speed vs bodypart max),
   intent match (+big if it satisfies the buffered intent), first-time bonus when an intent is buffered (GRF's ×0.3 on
   traps), attribute/skill terms later (FIFA gates composed traps on Ball Control/Agility), pressure penalty for slow
   two-touch controls. Pick max; ties broken by fixed order (never by RNG unless drawn in its own statement).
5. **Commit**: write `ContactPlan`. Re-plan allowed each tick (or every N ticks like GRF's 20–80 ms throttle) until
   `contact_tick - now <= COMMIT_TICKS`, then frozen (prevents flicker in presentation and exploit of last-instant
   swaps).
6. **Contact tick**: verify 3D distance between ball and the plan's body point ≤ tolerance (band-aware). Success →
   apply outgoing ball velocity using **aim read now** (stick at contact, clamp deviation from planned direction like
   GRF's 0.15π), with quality = f(miss distance, incoming speed, bodypart) blending toward incoming velocity for poor
   touches (GRF bumpyRide). Failure → whiff (ball continues), plan cleared.
7. **Buffer resolution**: if the executed kind satisfied the intent → clear buffer. If it was a CONTROL touch while an
   intent is buffered → keep the buffer; on the following ticks the planner will find a feet-band ground contact
   (chest → ball drops → first ground/feet touch) and execute the pass there = **chest-then-pass chain**. Cancel the
   buffer on: age > lifetime, ticks-to-ball grew past threshold, opponent gained possession, player switched.

**C. Chains this gives for free**
- *Chest control → pass/volley*: pass buffered while ball at chest height → plan TRAP_CHEST (no legal pass at chest
  band except weak chest-pass) → ball drops → planner sees feet band at a near tick → PASS_GROUND or VOLLEY_PASS
  depending on ball height at that tick.
- *Header pass*: pass buffered, head band → HEADER_PASS (already in `header.c`), aim from stick at contact.
- *Flick-up then volley*: a CONTROL_MODIFIER intent (e.g. hold a key while dribbling) produces a kick-up contact with
  vz; a shot pressed afterwards is buffered; the planner finds the ball in the thigh/feet band in the air → VOLLEY_SHOT.
- *First-time pass*: intent buffered before the ball arrives; planner prefers PASS_* over TRAP_* when feasible.

**D. Presentation sync (render never decides contact)**
- Renderer reads `ContactPlan` (kind, bodypart, contact_tick, contact_point) and the player's sim position; it chooses
  a procedural pose family per kind and computes pose phase = f(ticks until contact_tick) so the limb peaks exactly at
  `contact_tick` (equivalent of UE skew warp ending on the strike frame; GRF's cosine-eased actionSmuggle). Interpolated
  render frames use `alpha` between ticks as usual; at contact the ball's rendered position and limb coincide because
  both derive from sim state at the same tick.
- Because the plan is frozen `COMMIT_TICKS` before contact, the wind-up pose has a stable target; earlier re-plans
  may change kind — renderer should cross-fade (cosmetic state only, outside SimState, per DECISIONS note).
- `--check-kick-pose`-style headless check can be extended: for each contact kind, assert the posed limb at
  `contact_tick` is within tolerance of `contact_point` (numeric avatar/sim agreement).

**E. Pitfalls**
- Picking body part from *current* ball height (ball rising/falling) → wrong action; always use height at the planned
  contact tick (YSoccer/GRF).
- Single-instant contact checks at 60 Hz can tunnel fast balls past a limb; use segment-vs-sphere over the tick
  (GRF `useContinuousBallCheck`; WildPitch ball already substeps).
- Stale buffers firing in a changed situation (SmashWiki); use situational cancels + prefer newest press.
- Aim latched at press time feels unresponsive; read at contact (GRF/YSoccer/SWOS) but clamp deviation.
- Letting animation/root motion move the sim body (root-motion-driven sim) breaks determinism and presentation/sim
  separation — keep sim kinematic and warp only the visuals (GRF smuggles offsets in its own deterministic-ish sim;
  WildPitch should keep warps render-side unless they must affect reach, in which case express them as sim
  "assist" movement like `header_assist_move()`).
- Prediction/sim mismatch: `header.c`'s `ball_approach` ignores drag (documented "no drag"); fine for a 0.45 m head
  window, but chest/thigh/volley windows are tighter and lofted passes travel longer — a planner should step the real
  ball code (or a cached prediction ring from it) rather than the analytic parabola.
- Determinism: candidate loops must be fixed-order; any tie-break RNG draw in its own statement; no float in
  prediction; prediction must call the *same* ball step code as the sim (otherwise plans miss).
- Too many special cases per body part → combinatorial branches; keep a table `{band → allowed kinds}` +
  per-kind tunables instead of nested ifs (SWOS shows the nested-if version scales poorly beyond 5 bands).

### Gaps
- No public source describes a commercial *deterministic* fixed-point implementation of contextual contact selection;
  the design above is synthesized from GRF/SWOS/YSoccer patterns and general warping docs.
- Exact tick values (commit window, buffer lifetime) should be playtested; GRF's are at 100 Hz and tuned for a
  mocap-clip engine.
