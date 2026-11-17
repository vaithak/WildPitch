# Playtest feedback log

Owner's feedback after each checkpoint, verbatim where possible, plus what we did about it.
Format:

## CPx.y — YYYY-MM-DD
- **Feedback:** ...
- **Action:** what changed / which CP it went into / declined (why)

---

## CP0.0 — 2026-11-14
- **Feedback (verbatim):** "it looks okayish, I think we can have better and a bit more human looking avatars for
  the players, maybe taking inspiration from sensible soccer or maybe google research football like characters,
  think thoroughly on this, as it will be part of the user experience? Also, make sure the player can jump for
  headers also, even if it is in further checkpoints, but atleast make sure it is taken care of."
- **Action:** CP0.0b "Human avatars" inserted before CP0.1 — procedural 3D-skeleton players (D-009), presets to
  compare by eye, pose gallery + demo mode. Headers: jump state added to the sim now (D-010); playable header
  in CP0.2, AI aerial duels CP0.3, corners CP0.4.

## CP0.0b — 2026-11-14
- **Feedback:** "modern look, wide zoom, contextual headers fine." Asked whether the architecture supports (1) bigger /
  per-format pitch sizes and (2) future training of flair moves (bicycle kicks, rabonas, "Messi + Neymar" style).
- **Action:** defaults set (D-011). (1) answered: pitch is config; formations/markings/camera to be made format-aware
  (D-002 note). (2) answered with walkthrough; committed to parametric moves (D-012) starting CP0.1.

## CP0.1 — 2026-11-14
- **Feedback (verbatim):** "dribbling feels a bit off, which becomes worse while sprinting"
- **Diagnosis (traced tick by tick):** (1) sprint speed flickered 7.4<->8.0 m/s every tick (dribble slowdown toggled at
  the 1.0 m reach edge); (2) touches only fired when the ball lagged, so in a steady dribble the ball rolled ~1 m ahead
  with a touch every 1-2 s — detached, no rhythm; (3) at a steady sprint the ball parked at exactly reach (1.0 m), so
  cuts could abandon it. Tests missed it: they only sampled the first second of a dribble.
- **Action:** see CP0.1b in ROADMAP / D-014.

## CP0.1b — 2026-11-15
- **Feedback (verbatim):** "this looks better, but the friction looks a bit weird, a simple pass doesn't stop too, if
  it will be done later, no worries, but just bringing it up."
- **Diagnosis:** rolling friction was a constant 1.3 m/s² only → a 12 m/s pass rolled 55 m over 9 s (longer than the
  40 m pitch) at an unnaturally even slowdown; no drag on ground balls.
- **Action (CP0.1c, D-015):** decel = 0.9 + 0.35·v + 0.013·v² (rolling + grass + air drag). 12 m/s pass → 18 m / 4.5 s;
  3 m/s → 2.8 m; 22 m/s → 34 m. Dribble touch sizing uses the same model (feel unchanged; sprint max gap 0.88→0.91 m).
  Pass *weight* by distance (tap vs hold) still comes in CP0.2.

## CP0.1c — 2026-11-15
- **Feedback (verbatim):** "the ball physics still looks a bit weird, do a thorough deep research on how the ball
  physics is implemented in other soccer games, also think from physics point of view deeply."
- **Action:** research report `reports/Football game ball physics.md` (games, real physics, code audit). Owner: "yes,
  go ahead. it should step up and stop the ball. 0.7m sounds right" → CP0.1d (D-016): R1 rolling ball visuals, R2
  no-magnet possession, R3 impulse bounce.

## CP0.1d — 2026-11-15
- **Feedback (verbatim):** "looks really good, move ahead now"
- **Action:** start CP0.2, split into CP0.2a (shape, switching, aimed ground passes, clock) and CP0.2b (through ball,
  lofted pass, headers) so passing/switching feel gets its own playtest.

## CP0.2a — 2026-11-15
- **Feedback (verbatim):** "go ahead" (after CP0.2a + the dedication card; no specific passing/switching notes yet).
- **Action:** CP0.2b (through balls, lofted passes, headers). CP0.2a feel questions stay open for the next playtest.

## CP0.2b — 2026-11-15
- **Feedback (verbatim):** "there is something off about the through ball power and passing/through ball reception, do a
  deep research with other subagents to figure out how other games to do it correctly which feels more natural. Also,
  let an agent play the game for a while with different controls and configurations, and let if figure out the
  non-natural gameplay and report its findings."
- **Feedback (verbatim):** "like while a ball is being received, and the moving key is pressed, player moves ahead
  without reception, maybe this will be fixed later using AI on players, that is okay, I am just letting you know"
- **Action:** research `reports/Through ball and pass reception.md` + headless playtest agent findings (copied to
  `research_notes/Through ball and pass reception/playtest_findings.md`) → CP0.2c / D-020, all nine findings as
  acceptance tests in `tests/test_reception.c` (one-twos left to CP0.3). Owner: "go ahead and fix them".

## CP0.2c — 2026-11-15
- **Feedback (verbatim):** "keep deeper, no separate key, yes. Also, do a deeper research on the pass receiver
  selection method, whether our matchs with open source and good commercial games approaches or can we improve it?"
  (answers: J hold = deeper/softer stays; no separate through-ball key; YES a standing receiver should be easier to
  take over to make a run while the ball is coming.)
- **Action:** takeover rule for standing receivers; research `reports/Pass receiver selection.md`.

## CP0.2d — 2026-11-15
- **Feedback (verbatim):** "go ahead" (after CP0.2d receiver ring; no specific notes).
- **Action:** start CP0.3 "Instinct AI".
- **Owner decisions for CP0.3 (asked):** when an AI team-mate wins the ball, control jumps to him (held stick
  latched); when our keeper has the ball, the human controls the keeper (control jumps to him too).

## CP0.3a — 2026-11-15
- **Feedback (verbatim):** "the passes seem very slow now with opponents, they are intecepted almost everytime,"
- **Action:** measure pass speed/flight time and interception rate in live play; tune pass pace, AI reaction to a
  pass, interceptor reach (see STATUS/D-023).
- **Feedback (verbatim):** "one more thing allow the keybindings to be changeable with a config menu, and keep the
  following as default: arrow keys for movement, e for spring, s for pass, d for shoot, a for lobs, c for zoom change"
- **Action:** keymap module + Esc pause menu with a Controls screen (rebind, reset), saved to the user pref dir.
  Defaults as asked ("spring" read as sprint); chip W, switch Q, look P (my picks, rebindable).
- **Feedback (verbatim):** "now the shoot is getting intercepted in between"
- **Action:** measure live shot outcomes (goal / keeper / body block / defender control / wide), fix the cause.
- **Feedback (verbatim):** "also please increase now 7v7 and increase the pitch size, it is getting otugh to test with 5v5"
- **Action:** 7-a-side format (keeper + 2-3-1) on 55 x 35 m with 5 m goals as the game default (`--5v5` to go
  back); formations generalised per format; 5v5 regression tests unchanged + a 7v7 AI-match test.

## CP0.3a (7v7) — 2026-11-15
- **Feedback (verbatim):** "it is a good game, keep going ahead with further planned improvements"
- **Action:** CP0.3b: baselines in 7v7, keeper dives/parries, AI aerial duels, difficulty → playtest; then stats.

## CP0.3b — 2026-11-15
- **Feedback (verbatim):** "why are keepers not holding ball in hand once saved? also, why is throw-ins and corners not
  enforced properly? also, controlling defender should automatically switch. after these are done, we need to
  work/deep research on how to ensure players maintain formation and play like a team nicely and collaboratively?
  this needs to be done for both our team and their team, right now it is like everyone going towards the ball."
- **Action:** (1) auto-switch to the best defender when they have the ball; (2) keeper holds the ball in his hands
  after a save in his box, distributes (throw / punt), time limit; (3) real restarts: throw-ins, corners, goal
  kicks with takers and distance rules; (4) then deep research on team shape / collaborative play (both teams).
- **Feedback (verbatim):** "also, the size if small, I would say increase both for all formats by some factor (1.5) but
  ensuring proportiuon matches that of actual soccer fields,"
- **Action:** pitches x~1.5 with the real 105:68 (1.54) ratio: 7v7 80 x 52 m (goal 6 x 2.1), game 5v5 60 x 39 m
  (goal 5 x 2); formation / marking / pass-range distances scale with the pitch; the 40 x 25 m 5v5 stays as the
  regression-test format.
  Goals later set to the real goal-to-width ratio: 7v7 5.6 m, 5v5 4.2 m (D-031).

## 2026-11-15 — CP0.3b+ (team play D-030, bigger pitches D-031)
- **Feedback (verbatim):** "this is really good"
- **Action:** CP0.3b+ accepted. Next: CP0.3c player stats. Open (no complaint yet): AI shot conversion ~50%, pass
  completion 62%.

## 2026-11-15 — questions + fixes before CP0.3c
- **Feedback (verbatim):** "put thiese into the stats work of CP0.3c, but before that help me answer some questions and
  fix somr things: 1. what is the difference in the difficulty levels? 2. create an artifact explaining me the compete
  system design and main loop, also explain in the artifact how AI works? 3. fix these things: goalkeeper picking up
  ball when defender pass back should not be allowed, have corner flags, have billboards outside the field area like
  are generally on actual matches, don't allow players to cross those boards, the board should have banner of the
  game name with football logo at the end, then on the right corner of each board should be the dedication message in
  a smaller font"
- "one more question is the speed of AI opponent more than my player in normal mode?"
- "also make sure the kickoffs are alternate, and on goal, the other team should take the kickoff (not the one who
  scored the goal), and kickoff should only be a pass, no possibility of just taking the ball"
- **Action:** AI conversion / pass completion moved into CP0.3c. Answered difficulty (D-028) and speed (same top
  speeds; AI sprints freely until stamina in CP0.3c). Explainer artifact: https://claude.ai/artifact/YT1ERDs2HvaMoz3udPpqT6.
  Back-pass rule + kick-off set piece (D-032); boards, stands, corner flags (D-033).

## 2026-11-15 — go-ahead for CP0.3c
- **Feedback (verbatim):** "continue with next checkpoint"
- **Action:** CP0.3c player stats + AI pass choice (D-034/D-035); CP0.3b++ playtest still pending.

## 2026-11-15 — after CP0.3c
- **Feedback (verbatim):** "this looks good, but one feedback, first of all, the game should not of 90 minutes, it
  should be some scaled down version of it, like clock running at 9x, so 5 mins both halves, ensure that side changes
  after each half, also add the extra time thing. Secondly, the players need to be moved in the exact direction of
  ball if the ball is just there, or during long passes, is there any assust from system side to aid player towards
  the ball as long it is moving in the general direction of the ball? how does it work in games like fifa or other
  good games, do a deep research on this, as it will be an important user experience part"
- Clarified (AskUserQuestion): "extra time" = **stoppage time** (added minutes each half, e.g. 45+2').
- **Action:** match clock at 9x (two 5-min halves), sides switch at half time, stoppage time (D-036). Deep research
  "Assisted movement toward the ball" (4 researchers), then implement.
- Owner answers on the research (AskUserQuestion): AI-pass receive help "Yes, help"; released stick "No, stand still";
  setting "Menu, default Normal". Implemented as D-037.

## 2026-11-16 — after CP0.3d (clock + movement assist)
- **Answers:** defender cone on opponents' passes: "yes"; receive help still sprints on AI passes: "yes".
- **Feedback (verbatim):** "the throw-in should be from hands like done in actual matches with similar stance and
  during throw-in players should be spread out like in actual matches to receive the ball, similar for coner enforce
  minimum distance criteria, when goalkeeper passing, have cases for foot vs hand throw like in actual matches, the
  goal kick should be taken from the line as done in actual matches with everyone going out of D area and the team
  spreading out to receive the ball, when sprinting don't make the player's slow all the time instead when spring
  make it like the ball is touched a bit hard thus going a bit farther from player while sprinting, only during touch
  part is the speed a bit slow, otherwise it is still fast - now when turning direction during sprinting, it will
  first need to reach the ball again and only then the direction changes, the press of pass button for thrugh ball
  should have a bit bigger timer, only when pressed for that threshold should it convert to throughball with a meter
  and the meter's power should always be more than a normal pass power (like meter's 5% should also be more than
  that of a standard normal pass), also still there are sometimes when the pass is too slow for where the player is
  and during that times sometimes if key is pressed the receiver moves away instead of first receiving the ball,
  another thing the system assist to ensure defender goes towards the ball should also work for active player being
  controlled if we are generally moving in the direction of the ball"
- **Action:** CP0.3e (D-038..): restarts like real matches (throw-in stance from hands, distances, spread to receive;
  corners 9.15 m scaled; goal kicks from the goal-area line, opponents out of the box); keeper distribution
  roll / throw / punt; sprint dribble = longer knock-ons, slowdown only at the touch; through ball longer threshold +
  always firmer than a tap; receiver can't leave a slow incoming pass; chase cone also onto an opponent's ball.

## 2026-11-16 — go-ahead for CP0.4
- **Feedback (verbatim):** "continue with next checkpoint"
- **Action:** CP0.4 "A real match": fouls -> free kicks / penalties, offside (sim, this session); gamepad (hotplug)
  and instant replay (presentation, a parallel subagent); shout button if time allows.
- **Feedback (verbatim, mid-CP0.4):** "also add sliding tackle, think carefully with the advisor about which key
  should do this work, think of the choices taken by other popular games"
- **Feedback (verbatim, mid-CP0.4):** "there should be power meter in lofted pass also, that will decide how far to
  lob in air and we need to improve the receiving of air ball nicely - either through foot or chest or header with
  apt physics for each stance, also allow passing from header directly through air ball or like chest then foot pass
  type combination - make sure that is possible, do a deep research on this if required to make sure we do it nicely
  ensuring good system design approach. Next, currently there are no transitions available, for example, during
  outside giving a transition so that everyone goes into place, similarly during goal kick, allowing defenders to
  move back first if gone out of play - these can be skippable but atleast have a transition, same for corner."
- **Action:** slide = LOB key (advisor + EA FC / eFootball both use their lob/cross button); then CP0.5 with deep
  research on aerial reception.
- **Feedback (verbatim, mid-CP0.4):** "similarly good visual portray of offside like showing the line and writing
  offside on top and not just out, also during kickoff, why does opponent team start moving even before first pass?
  it doesn't look good, also during goal kicks the opponent team tries to come into the D, but stopped by rules, but
  still it doesn't look good. Also, when making passes by trajectory prediction of receiver, many times the ball is
  passed outside of the field because prediction goes out. Improve on these also."
- **Action:** D-044 (all four), committed with the restart transitions.

## 2026-11-17 — during CP0.5a (aerial)
- **Feedback (verbatim):** "continue with stages 2 and 3 after that"
- **Feedback (verbatim):** "twp more things to work post committing till stage 2 and 3 but before stage 4 and 5,
  firstly whenever the player being actively controlled is near the ball, always have some system assist so that
  player moves towards the ball as long as the direction of movement is somewhat in the direction of the where the
  ball is - whether it is for chasing a stray ball/defending/receiving a pass/throughball/loft ball etc.
  Secondly, the switching of active player needs improvement, currently I think it is just based on the vicinity of
  the ball, this leads to frequent switching sometimes when a couple of players near the ball leading to bad
  experience, do a deep research on this with other subagents if required on how other games handle this
  gracefully, implement it nicely and then update the artifact with a section on active player switching streategy
  in detail (before vs after)."
- **Owner answers (switching, reports/Active player switching.md):** menu setting "Auto switching: Manual / Events /
  Assisted" (default Events + 3x override); Q + direction picks the man that way (ball-relative), repeated Q cycles,
  no right stick (R3 stays zoom); keep the 1.5 s manual lock.
- **Action:** CP0.5b: chase assist for every ball situation near the controlled man; switching redesign (one ETA
  scorer, next-man indicator, dwell + return lock, stick-intent suppression, post-switch blend).

## 2026-11-17 — after CP0.5a/b (corners, keeper facing)
- **Question (verbatim):** "what is the light arrow vs dark arrow over player?" — answered: solid = controlled man,
  dim hollow V = who Q switches to, solid translucent = an automatic switch is about to happen (offered to make the
  third more distinct).
- **Feedback (verbatim):** "the corner kicks have some problem, the nearby player is too near and I am never able to
  send a long kick for header in the D-area. Either it is non-intuitive or not-doable right now, I tried with long
  press of lofted pass button"
- **Action:** D-053.
- **Feedback (verbatim):** "also, there is the problem of goalkeeper facing in random direction sometimes, like facing
  its own goal, I am not sure what the logic of goalkeeper moving direction is, but it should be improved"
- **Action:** D-054 (keepers face the ball).
- **Feedback (verbatim):** "once both of these are done and committed, continue with the next checkpoint as I am
  going to sleep and will test them together tomorrow"
