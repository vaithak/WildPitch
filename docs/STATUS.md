# Status

_Last updated: 2026-11-17 by Claude session (CP0.6 part 1, audio)_

## Current checkpoint
**CP0.5a (aerial play, stages 0-5, D-046..D-052) + CP0.5b (chase assist D-049, switching D-050) + D-053 (a human's
corner) + D-054 (keepers face the ball): ✅ done, all committed, awaiting the owner's playtest** (he asked to test
them together with the next checkpoint). Their playtest guides are below (stage 5 down to CP0.5b part 1, D-053/D-054
in DECISIONS).

**CP0.6 "Juice and cool" (ROADMAP's CP0.5, renamed): in progress.** Presentation only - goldens unchanged all
checkpoint (5v5 `b23b8e7d1e2a481d`, 7v7 `980606f7054393f4`).
- **Part 1 - audio (D-055): done.** Every sound synthesised at startup (no files, no new dependency): kicks graded by
  kind and power, header thud, bounce, post ping, net swish, whistles (short: fouls / offside / kick-offs / goals;
  long: half time; three blasts: full time), slide / tackle scuff, crowd murmur that swells near a goal, roar on a
  goal, "ooh" on a post or a goalward miss. Stereo pan by the ball's x against the camera. Esc menu: Master / Effects
  / Crowd volume (0-10, saved in settings.cfg); **M** mutes (saved). New `settings.c` (one struct, key = value file;
  old files still load). Verified: `--check-audio` (ctest `audio`), `--check-settings` (ctest `settings`), and an
  end-to-end `--autoplay --audio` run through SDL's *disk* driver (crowd bed ~0.013-0.02 RMS, kicks / whistles as
  spikes on the kick ticks, peak 0.34); no-device paths (`SDL_AUDIO_DRIVER=bogus` / `dummy`) run silently. SDL here
  has ALSA + PulseAudio (no rebuild was needed). **Not verified by ear** (the agent has no speakers): timbre and the
  balance between sounds are guesses - owner, please judge.
- **Part 2 - visual juice: done** (`src/game/juice.c`, fixed pools, time-based): a fading ribbon behind fast balls
  (> 15 m/s, or high and > 9 m/s: shots, long lobs, punts) on top of the old half-frame streak; the net bulges out
  where the ball hit it and ripples back (~0.9 s, strength by ball speed); dust puffs on slide tackles (a trail
  while he skids), landings from jumps / headers, keeper dives (~0.3 s in, the dive is a pose) and hard bounces; a
  goal flashes the screen once and a band in the scorer's kit colour sweeps in with a popping "G O A L !" and the
  score. Trail / puffs clear on teleports and replay jumps. Checked in screenshots (Modern + Retro) on the
  `--autoplay` goal (frames 140-185).
- Part 3 battery mode + PERF.md, part 4 title screen + quick match: next.

### Playtest guide (CP0.6 part 1, audio) - listen for
1. Launch normally: a short whistle at kick-off, a soft crowd murmur under everything.
2. Pass (tap S) vs shoot (hold D) vs lob (A): three different boot sounds; harder shots are louder and sharper.
   Dribble touches are quiet taps. Headers are a dull thud.
3. Shoot at the post / bar: a metallic ping and the crowd's "ooh". A shot just wide: "ooh". A goal: net swish, a roar,
   then the whistle; the crowd stays up for a few seconds.
4. Take the ball near the box: the crowd gets louder and brighter; back in midfield it settles.
5. Commit a foul / get caught offside: one short whistle. Half time: one long blast; full time: short-short-long.
6. Ball on the left / right of the screen: the sound comes from that side.
7. Esc: the crowd drops back; change the volumes (a kick plays so you hear the level); M mutes / unmutes in game.
8. Tell me: too loud / quiet? whistle too shrill? crowd too hissy? any sound that's annoying after 10 minutes?

### Playtest guide (CP0.6 part 2, visual juice) - look for
1. A hard shot or a long lob leaves a short white ribbon behind the ball; passes along the ground don't.
2. Score (or shoot into the side netting): the net pushes out where the ball hit and wobbles back.
3. Slide tackle (A without the ball near an opponent's ball): dust behind him. Keeper dives and jump landings: a puff.
4. Goal: one white flash, then a band in the scorer's colour sweeps in with "G O A L !" and the score.
5. Tell me: too much / too little? Anything distracting in Retro style (P)?

### History (CP0.4 - CP0.5b, kept for reference)
**CP0.4 — "A real match" (D-040..D-045): ✅ done, waiting for the owner's playtest.** Fouls / advantage / free kicks
/ penalties, offside (line + banner), gamepad, replay, slide tackle (A), restart transitions, kick-off hold, passes
that stay in, lofted-pass meter (hold A).
**Next: CP0.5a — aerial reception** (owner: foot / thigh / chest / head with apt physics, header passes, chest-then-
pass chains, "good system design"). Research done: `research_notes/Aerial ball control and lofted passes/`
(commercial games, physics, system design incl. GRF/YSoccer/SWOS source, WildPitch audit); report:
`reports/Aerial ball control and lofted passes.md` (being written). Direction from the notes: choose the contact
by height AT THE PREDICTED CONTACT, one buffered intent per seat (newest press wins, ~2 s after release, cancelled
when the situation changes), a per-player contact plan that freezes ~5 ticks before contact, a fallback order
(buffered pass/shot > header > trap) so chains fall out; generalise header.c rather than add a parallel system;
impulse contact model (head M~9.9 kg e~0.7; cushioned chest e~0.1-0.3 pops it up 30-60 deg, ~0.5-1.2 m ahead).
Open: AI attack with offside (49 goals / 110 shots per 100 AI min); AI never slides.

**CP0.5a stage 0 (report's staged plan, row 0): done, golden unchanged.** header.c / gain_apply literals moved to
`tuning.h` (T_HEAD_Z, T_HEAD_FWD, T_HEAD_REACH_XY/Z, T_HEADER_WAIT_MAX, T_HEADER_PASS_K/BASE/MIN/VZ/NOBODY_D,
T_FIRST_TOUCH_SPILL, T_GAIN_TOUCH_COOLDOWN; same values). `ball_flight_to(vh, vz, z0, z_end, &dist, &ticks)` in
ball.c (ball_carry_distance now calls it; test_ball checks it against the integrator: <= 0.04 m, same tick).
Aerial counters: `tests/aerial_metrics.h` (definitions frozen there, shared by tests) + `tools/playtest/aerial.c`.
**Aerial baseline** (7v7, `WP_STATS=1`, 20 x 5 min AI vs AI, `aerial 20 5 1`):

| Metric | Baseline (pre stage 1) |
|---|---|
| Goals / shots | 0.49 / 1.10 per min |
| Lobs (feet, not from hands) | 1.11/min, completed 70% (78/111) |
| Crosses (lobs from wide in the final third) | 0.49/min, completed 43% (21/49) |
| Lob distance at the kick (completion) | <10 m 1 (0%), 10-15 m 8 (12%), 15-20 m 13 (54%), 20-25 m 69 (93%), 25-30 m 10 (30%), 30-40 m 10 (30%) |
| First aerial contact, feet band (ball already down) | 67 gains, control success 94% |
| First aerial contact, body band (0.55-1.45 m) | 73 gains + 3 body hits + 1 header, control success 91% (69/76) |
| First aerial contact, head band (> 1.45 m) | 21 headers (no gains possible), keepers 0 |
| Headers | 0.23/min: 6 shots, 17 passes completed 35% (6/17) |

bench_sim 16.2 us/tick (1031x real time).

**CP0.5a stage 1 (D-046): done, goldens updated on purpose** (5v5 `51ad9c0dcc50f6a7`, 7v7 `41955f92797c21c8`).
Lofted passes come from one D-indexed flight table (`src/sim/lob.c`): vz = 5 + 0.245 D unclamped (floats, no more
drives), meter 12 m .. min(45 m, 0.62 x pitch), snap on team-mates' PREDICTED points, led to the ball coming down
through 1.0 m, the drop point handed to the receive assist, landings kept inside the pitch, AI lobs on the same solve
(balls into the box met at 2.3 m for a header), avatar lob pose = meter h. Throws from the hands unchanged.
Stage-1 numbers (same harness, `aerial 20 5 1`; in brackets 80 matches, baseline -> stage 1):

| Metric | Stage 0 baseline | Stage 1 |
|---|---|---|
| Lob completion | 70% (78/111) [69%, 309/447] | 70% (79/113) [70.5%, 315/447] |
| Cross completion | 43% (21/49) [45%] | 47% (27/57) [49%] |
| Lobs lost to an opponent's header | 9 [25] | 8 [35] |
| Headers / min | 0.23 (6 shots, 17 passes 35%) | 0.43 (19 shots, 24 passes 38%) |
| Control success, body band | 91% | 95% |
| Goals / min | 0.49 | 0.44 [0.43 -> 0.49] |

Meter landing error <= 0.37 m on 40 / 60 / 80 m pitches; lobs to moving mates come down within 1.0 m of P (z <= 1.6)
42/42, the mate within 2 m of P 38/42 (sprinters still accelerating miss by 2-4 m). bench_sim 15.6 us/tick.
**Next: stage 2** (prediction ring + contact planner, CONTROL intent only) - after the owner's playtest.

**Stage 2/3 baselines (at `570b31c`, before the contact planner):** `aerial 20 5 1` (WP_STATS=1): goals 0.44/min,
lobs 70% (79/113), crosses 47% (27/57), headers 0.43/min (19 shots, 24 passes 38%), new counter "lob receptions"
(<= 30 m, first contact a gain/body hit by the kicker's team, controlled) 51/52; 80 matches: goals 0.49/min, lobs 70%
(315/447), crosses 49%, headers 0.43/min (82 shots, 91 passes 32%), receptions 218/219. `aimatch 20 5 1`: goals 16+28,
passes 75%/77%, tackles 135/148, outs 0.8/min. bench_sim 15.6 us/tick (median of 5).

**CP0.5a stage 2 (D-047): done, goldens updated on purpose** (5v5 `6f21d636d56b9644`, 7v7 `8925d4ceea2ff5c7`).
Aerial balls are controlled by the body part their height asks for at the planned contact (instep / thigh / chest /
head-control for slow balls), with a graded first touch (faster = heavier, unprepared or pressed = heavier) and the
ball settling at his feet. New `src/sim/contact.c`, `tests/test_contact.c`. Numbers (baseline -> stage 2):

| Metric | Stage 1 (20 / 80 matches) | Stage 2 (20 / 80) |
|---|---|---|
| Lob completion | 70% / 70% | 69% / 74% |
| Cross completion | 47% / 49% | 51% / 53% |
| Lob receptions controlled (<= 30 m, our control attempt) | 51/52 / 218/219 | 45/45 / 202/203 |
| Control success feet / body / head band (80) | 98% / 97% / - | 98% / 95% / 89% (51 head-control gains) |
| Headers / min (80) | 0.43 | 0.44 |
| Goals / min (80) | 0.49 | 0.48 |
| aimatch 20x5: goals, passes | 16+28, 75/77% | 15+29, 74/76% |

Clean touches: instep 0.45 m, thigh 0.84 m / 0.48 s, chest 0.98 m / 0.58 s, head-control 0.90 m / 0.68 s; lofted
passes 10-47 m (8-17 m/s) controlled, drop 0.8-1.0 m; a chest ball at 20 / 23 / 26 m/s drops 2.0 / 3.5 / 5.8 m.
bench_sim 16.8 us/tick (+8%). Avatar stance poses: stage 5.

**CP0.5a stage 3 (D-048): done, goldens updated on purpose** (5v5 `438935178956b429`, 7v7 `b84701f83e76ea56`).
Press early and it happens when the ball arrives: buffered presses per seat (`src/sim/intent.c`), first-time volleys /
half-volleys / lay-offs, chest -> pass and chest -> volley chains, headers by the measured law with a real header-pass
solve. New `tests/test_intent.c`. AI vs AI (stage 2 -> stage 3, 80 matches; 20 in brackets): goals 0.48 -> 0.47/min
[0.44 -> 0.44], lobs 74% -> 71% [69 -> 71%], crosses 53% -> 48% [51 -> 52%], headers 0.44 -> 0.42/min, AI header
passes completed 38% -> 40% (stage 1: 32%); aimatch 20x5: goals 15+29 -> 18+26, passes 74/76% -> 74/76%.
Tests: 12/12 buffered passes fire; stale presses cancelled; chest -> pass at the ground pace (within 2%); chest ->
volley from 12 m 20/20 on target (no keeper); header passes to a jogging mate 6-10 m 15/20 within 1.5 m.
bench_sim 18.7 us/tick (stage 2 16.8; stage 1 15.6: +20% over the two stages - profile before RL; first suspects the
header-pass bisection + its per-mate retarget and meet_choose in the buffer's eta check). Chest band now 1.0-1.6 m
(a ball at 1.5-1.6 m with a button pressed was neither headed nor controlled).

**CP0.5b (owner requests after stage 3: chase assist everywhere near the ball + switching redesign) - in progress.**
- **Part 0 (perf), done, goldens unchanged:** callgrind showed 63% of all instructions in `fv2_len` (isqrt), ~half of
  the tick in `meet_choose` (team plan x 10 players every 3 ticks, brain, intents, seats - every call re-integrated
  the same ball path). `meet_choose` now predicts the ball's 80-step path once per ball state (static cache keyed
  on pos/vel/z/vz, like contact.c's ring) and skips the square root wherever an exact squared bound decides the
  step. Bit-identical results. bench_sim 18.7 -> 10.6 us/tick (1578x real time).
  AI-vs-AI reference before CP0.5b (`WP_STATS=1 WP_RULES=3`, 20 x 5 min 7v7): aimatch goals 18+26, passes 74/76%;
  aerial goals 0.44/min, lobs 71% (76/107), crosses 52% (27/52), headers 0.40/min.

- **Part 1 (D-049, owner request 1), done, 7v7 golden updated on purpose (`64e4492ca8981d6f`; 5v5 Off unchanged):**
  the chase cone now helps whenever the controlled man is near the ball (it or his meeting point within 12 m): no
  more "a team-mate is quicker" fade near the ball, balls he can't catch at his pace (steer to where they end up),
  late meeting points (long lofts), aerial balls steered to his contact-plan point, our pass to another mate only if
  he is quicker than the receiver. Contract unchanged (neutral stick, <= F turn, no sprint, Off = raw).

- **Part 2 (switching redesign, D-050), done, 7v7 golden updated on purpose (`8954bf043b6bc076`; 5v5 LEGACY
  unchanged):** new `src/sim/switching.c`; per-seat `switch_mode` (LEGACY for tests / regression format; the game:
  Esc > Auto switching Manual / Events / Assisted, default Events, saved in settings.cfg, applies on Restart).
  One intercept-ETA oracle (sw_next, hysteresis), forced hand-overs as before, unprompted switches only through event
  gates / the 3x override with dwell, return lock and stick-intent suppression; Q goes to the man the new chevron
  shows, Q + direction picks the man that way from the ball, repeated Q cycles; 0.3 s post-switch heading blend; team
  plan stops the old man being the AI winner while the lock runs. Harness `tools/playtest/switchrate.c` (7v7, assist
  Normal, seat 0 scripted, 3 seeds x 5 min; reasons inferred from the state; R walks onto a still ball within 5 m so a
  passive seat can't freeze the match):

  | Script | unprompted / min before -> after | A->B->A in 60 ticks (both unprompted) | all switches / min |
  |---|---|---|---|
  | R stick released | 34.2 -> 12.7 (37%) | 19 -> 0 | 45.7 -> 25.9 |
  | H stick held toward the ball | 17.3 -> 0.47 (3%) | 10 -> 0 | 31.1 -> 19.1 |
  | Manual (after) | 0 / 0 | 0 / 0 | 13.1 / 16.7 |
  | Assisted (after) | 22.7 / 1.9 | 0 / 0 | 38.1 / 21.2 |

  The chevron (while drawn): the oracle's man changes ~31-37/min (it must move after every switch), never A->B->A
  within 30 ticks (it holds >= 36 ticks).

  Before, most unprompted switches happened while their pass travelled (D-029's distance spot moving at pass speed).
  After, the remaining released-stick ones are the defensive switch onto the best man against their carrier.
  AI vs AI byte-identical (no seats). Cost: bench_sim (5v5 LEGACY) 10.5 us/tick unchanged; the 7v7 seat harness
  2.36 -> 2.50 s for the same 45 minutes of scripted play (+6%, one run; the play differs).
  Known: a seat man who never moves, within 4 m of a still loose ball, can freeze play (D-030 winner rule; also before).

**CP0.5a stage 5 (D-052): done, presentation only (goldens unchanged).** Players now show the touch the sim plans:
chest (lean back, arms out), thigh (raised), instep (foot lifted), head control (a little hop), volley / half-volley
(side-on swing with backswing and follow-through), lay-off, and headers whose forehead meets the ball (a header pass
glances toward its target). Each stance is a pose family fitted every frame so the limb is on the ball at the
contact tick; it ramps in over 0.3 s and eases out with a cushion or follow-through. `--check-kick-pose` measures
every stance at 3 offsets: worst limb -> ball 0.104 m (volley), the rest <= 0.033 m (limit 0.15). `--gallery` has
the stance poses with their ball (bottom right of the close-up rows). No lob ghost ring (owner).
**Next:** owner playtest of CP0.5a stages 4-5 (and CP0.5b) - then the roadmap's next checkpoint.

### Playtest guide (CP0.5a stage 5, aerial poses)
1. Lob to a team-mate and watch him take it: the chest / thigh / foot should meet the ball as it arrives (not a
   generic run cycle), then cushion it down.
2. Press D early on a dropping ball (volley) or S (half-volley / chest lay-off to a near mate): the strike's leg
   swings through the ball side-on.
3. Headers: the forehead meets the ball; a header pass turns the head toward the receiver.
4. `--gallery` shows each stance with its ball.
Questions: do the stances read at match zoom? Too much lunge (the body steps to reach a wide ball)?

**CP0.5a stage 4 (D-051): done, goldens updated on purpose** (5v5 `1c1bb280e057dcd5`, 7v7 `df26b28a9aa471c9`).
The AI plays aerial balls through the same contacts a human gets: new `src/sim/aerial_ai.c` chooses head shot / header
pass / clearance header / volley / half-volley / lay-off / control (candidates x features x weights) for the receiver
and each team's ball-winner; a contested receiver attacks it with his head, a free one controls it. The AI's header
shots now get the human header error (D-048 law); it compensates by aiming with the law (no flicks at goal, the
corner with the smaller turn, inside the post by the spread, no aim-assist re-pick). Driven crosses (1.8 m) were
built and measured but are OFF (`T_CROSS_DRIVEN 0`): the marker 0.5 m in front heads every one (scripted duels 0/72
vs 58/72 lofted); in matches they were level (0.43 goals/min, 47% crosses vs 0.41 / 51%).
Numbers (7v7, WP_STATS=1, rules on; baseline = HEAD before stage 4; `aerial` 80 x 5 min on seeds 1-80 / 81-160):

| Metric | Baseline | Stage 4 |
|---|---|---|
| Goals / min | 0.47 / 0.50 (0.48) | 0.43 / 0.46 (0.44, -8%) |
| Cross completion | 48% / 53% (50.4%) | 47% / 48% (47.3%) - below baseline, ~1 sigma |
| Header shots per cross | 0.39 / 0.41 | 0.42 / 0.43 |
| Aerial turnovers / min (new counter) | 0.37 / 0.45 | 0.36 / 0.45 |
| Lob completion | 71% / 73% | 67% / 70% |
| Header shots -> goals | 75 -> 50 / 84 -> 61 (no AI error) | 86 -> 39 / 84 -> 39 |
| Scripted cross duels (`tools/playtest/duel.c`) | 70/72 | 58/72 |
| aimatch 20x5: goals, passes | 18+26, 74/76% | 18+28, 74/76% |

bench_sim 10.57 -> ~10.6-10.9 us/tick. New counters in `tests/aerial_metrics.h` (aerial turnovers, header goals,
first-time air strikes; header shots now count sub-kind 1 only). Known: AI first-time foot strikes (volleys /
lay-offs) are candidates but almost never chosen (~0-4 per 400 min); cross completion is 3 points under the baseline
(the defender now attacks the ball: an owner call, see D-051).

### Playtest guide (CP0.5a stage 4, AI in the air)
1. Watch AI crosses into the box: the striker waits where the cross is played and attacks it with his head when his
   marker is close; free, he may take it down instead. Their defender reads it late and comes for it in the last 0.6 s.
2. AI headers at goal miss sometimes now (same error as yours) - aimed at the corner that needs less of a turn.
3. A long ball to an AI team-mate: he stands just beyond where it drops and takes it on the chest / thigh.
4. Defending a cross with your man: the AI attacker beats a marker who stands behind him, not one in front.
Questions: do AI aerial duels look like football? Should defenders read crosses earlier (better defending, fewer
headed goals) or later?

### Playtest guide (CP0.5b part 2, switching)
1. Esc > Auto switching: Events (default). Defend: a dim yellow "V" over a team-mate shows who Q gives you; it turns
   into a solid triangle just before the game switches for you (their tackle / clearance / lofted ball, or a man 3x
   better placed). Does control still jump between two men near the ball? (It shouldn't: 0 ping-pongs measured.)
2. Hold the stick at the ball while chasing: the game only takes him off you if you're hopeless (4x).
3. Q: goes exactly to the man with the V. Tap Q again within ~0.7 s: the next man, then the next (cycle).
4. Q while holding a direction: the team-mate that way from the ball (e.g. hold up + Q = the man above the ball).
5. After any switch with the stick held, the new man curves out of his run instead of snapping round.
6. Try Manual (only passes, gaining the ball and deflections move control) and Assisted (more eager).
Questions: is the V readable but quiet enough? Events vs Assisted - which feels right? Is 1.5 s after Q too long?

### Playtest guide (CP0.5b part 1, chase assist near the ball)
1. A loose ball 5-10 m away with a team-mate closer to it: hold roughly toward it (even the neighbouring arrow
   key) - your man now bends onto it (before: only if he was the quickest).
2. A ball rolling away from you faster than you jog: hold roughly after it - he bends onto its line.
3. A long lob / cross dropping near you: hold roughly toward it - he heads for the spot where his chest/thigh meets
   it (the same spot the receive assist uses), not just the bounce.
4. Their pass across you: hold roughly at it - he bends onto the interception even with a team-mate near it.
5. Release the stick: he stops, every time. Point well away (> 55 deg): untouched. Esc > Movement assist Off = raw.

### Playtest guide (CP0.5a stage 3, first-time play)
1. Header pass: as a cross comes in, press S (pass) early with the stick at a team-mate - he heads it down to him
   (a long one off a slow cross falls short). D (shoot) = header at goal; A (lob) = a looped header.
2. Chest then pass: a lob dropping at your chest, press S early (mate further than 8 m): he chests it down and
   passes as it reaches his feet. A mate within 8 m in front: a chest lay-off straight away.
3. Chest then volley: same, press D early: he chests it and volleys it as it drops.
4. Volley: a ball arriving at knee/thigh height with D pressed: struck first time (less accurate than a set shot).
5. Press S a second before a pass reaches you: it is played first time (or after one touch).
6. Pressing too early (> 1.5 s) or if they win it: nothing happens later.
Questions: is the error readable? Do early presses feel responsive or surprising?

### Playtest guide (CP0.5a stage 2, aerial control)
1. Lob (A) to a team-mate and do nothing with him: he runs to where the ball will be at chest / thigh height and
   takes it down; the ball drops just in front of him. Hold a direction as it arrives: he cushions it that way.
2. Long switch (full meter): still controlled, a bit further away. A driven/fast ball at the chest (a clearance)
   bounces off him.
3. A slow ball at head height with no button: he cushions it down with his head (pressing S/D still heads it).
4. Under pressure (an opponent close), or meeting it without time to set, the touch is heavier.
Question: does the touch feel too clean or too heavy?

### Playtest guide (CP0.5a stage 1, lofted passes)
1. Hold A (lob) longer and longer into space: the ball lands where the violet meter says - and floats (high, ~2-3 s
   for long switches) instead of being driven. Full meter = 45 m on the 7v7 pitch.
2. Hold A aiming at where a running team-mate WILL be: the ball snaps to him and drops onto his run; he runs to it.
3. Tap A: the quick lob to the ringed mate, now led onto his run.
4. Lob toward a touchline/byline: it never lands outside the pitch.
5. Watch the AI cross: the ball is put up for a header in the box.
6. An AI team-mate crosses to YOUR man in the box: the ball comes over at ~2.3 m where he stands; do nothing and he
   takes it as it drops (scripted: 24/24), or press S/D for a header.
Questions: do long switches now float rather than fizz? Is 45 m enough? Is 2-3 s of hang time too slow? (flatter:
vz = 5 + 0.2 D is one constant).

### Playtest guide (CP0.4)
1. Mistime a tackle (S near their carrier) and take the man: FOUL - free kick to them (in your box: PENALTY). If
   they keep the ball: ADVANTAGE, play on. Defending a free kick near goal, the AI builds a wall.
2. Offside: pass to a team-mate beyond their last defender - OFFSIDE, indirect free kick. Your AI team-mates wait
   on the line and time their runs.
3. A gamepad (if you have one): plug it in any time; A pass, B shoot, X lob, Y chip, RB sprint, LB switch, Start menu.
4. Press R (or pad Back) during play: the last 10 s replay (R again = slow motion; any other key = back to play).
5. Slide tackle: A (pad X) with their carrier within 3 m - win it from the side; from behind = foul.
6. Restarts: after a throw-in / corner / goal kick / free kick everyone walks into place (press S/D/A/W to skip);
   at a kick-off nobody moves until it's played; offside shows the line and a big OFFSIDE.

### Playtest guide (CP0.3e)
1. Throw-in: the taker holds it over his head; S throws it (two hands, feet planted), it drops into your mate;
   opponents stay 2 m off; your team-mates spread out (down the line, inside, back, long).
2. Corner: defenders stay 7 m (9.15 m scaled) away. Goal kick: from the goal-area line, opponents out of the box,
   your men split wide to receive.
3. Your keeper with the ball: S rolls it to a near mate / throws overarm to a far one; A overarm; D punt. Watch
   the AI keeper do the same.
4. Sprint with the ball (E): it's knocked further ahead, you stay fast between touches; turning sharply means
   reaching the ball first.
5. Through ball: hold S past ~1/3 s - the meter appears; even a short meter is firmer than a pass.
6. Receive a slow pass while holding a direction away: he takes the ball first.
7. Hold the stick roughly at an opponent on the ball: your man bends onto him (Esc > Movement assist to compare).

### Playtest guide (CP0.3d)
1. Match clock: two 5-minute halves (90' at 9x); added minutes at 45'/90' (+N); half time: ends change, blue
   kicks off; FULL TIME: Esc > Restart match.
2. Movement assist (Esc > Movement assist, default Normal; applies on Restart): hold a direction roughly at a loose
   ball (even the neighbouring arrow key) - he bends onto it; let go and he stops; point well away (> 55 deg) and
   nothing changes. Try a long clearance, a ball at your feet, and an AI team-mate's pass to you (he meets it).
3. Compare Off / Normal / High on the same drills: which one feels like your own player?
4. Questions: should the defender also get bent onto an opponent's pass lane? Should AI passes to you still force
   a sprint now that stamina exists?

### Playtest guide (CP0.3c)
1. Bottom-left: your player's stats (PAC CTL PAS SHO TCK STA; keepers KEEP) and an ENERGY bar. Switch players (Q)
   and see them change: strikers shoot harder/closer to the post, defenders tackle better, quick men are quicker.
2. Hold sprint (E) for ~15 s: the bar drains and, once low, your sprint fades to a jog; ease off and it refills.
3. Watch the AI: fewer hopeful long balls (more passes arrive), carriers pass when pressed instead of dribbling into
   tackles, keepers come off their line when someone runs through.
4. Is the stats spread noticeable but fair? Is scoring still fun (AI ~2-3 goals per 5 min between them)?

Numbers (7v7, 20 x 5 min AI vs AI, stats on): passes 78% complete (was 61%), >20 m passes 47% (was 28%), goals
~0.5/min, shots ~1/min, conversion ~45-50% (was ~60%); AI energy settles ~75%. Goldens: 5v5 `a034042ea9bdd9a8`,
7v7 `a462a0410e97ee05` (after D-036). bench_sim 17.7 us/tick.

### Playtest guide (CP0.3b++)
1. Kick-off: you can't dribble off - press S (pass) to start; blue waits outside the circle. After a goal the team
   that conceded kicks off (the HUD says KICK-OFF - RED/BLUE).
2. Pass back to your keeper with your feet: he plays it with his feet (no pick-up). Head it back: he may pick it up.
3. Run at the side or end of the pitch: the boards stop you; a rolling ball bounces back off them.
4. Boards: WILDPITCH + football panels, the dedication at the right-hand end of each board; corner flags at the
   four corners. Zoom in (C) to read the dedication on the near board.
5. Still in from CP0.3b+: does the team play feel right on the 80 x 52 m pitch?

## What works now
- **CP0.3c (D-034/D-035):** player stats (pace, control, passing, shooting, tackling, stamina, keeping) generated per
  slot with equal squad totals; energy that drains/recovers; HUD card; neutral mode = the old sim bit for bit. AI
  judges pass lanes by time, passes under pressure, keepers narrow the angle: 78% of AI passes arrive (was 61%).
- **D-032/D-033:** back-pass rule (keeper can't handle a team-mate's kicked pass), kick-off = pass-only set piece
  (own halves, opponents outside the circle), advertising boards players can't cross (ball rebounds), stands,
  corner flags; boards carry WILDPITCH + football and the dedication. `--boards --screenshot F.bmp`.
- **Team play (D-030):** both teams play from a team plan: one ball-winner, a cover, capped markers, rest defence,
  near-support and a far-side runner; formation blocks for in/out of possession slide with the ball. AI vs AI 7v7:
  team length ~25 m, ~2 players within 10 m of the ball (was 4 = the swarm), AI vs AI 7v7 (30 × 5 min): goals 58–53, shots
  109–99, ~0.74 goals/min, 62% passes arrive; back line stays goal-side of their deepest man (no offside yet).
- **Pitches (D-031):** 7v7 80 × 52 m (goal 5.6 m), 5v5 60 × 39 m (goal 4.2 m) — real 1.54 proportions, markings scale.
- **Owner's CP0.3b requests (D-029):** keepers hold the ball in their hands after a save in their box (throw S /
  high throw A / punt D, 6 s limit); real throw-ins, corners and goal kicks with takers and a 4 m distance rule;
  control auto-switches to the best defender when they have the ball.
- **CP0.3b:** keepers dive (catch or parry; pose), shots keep their aim (corner shots beat him), tackles have a
  lunge pose; AI players win headers (shots near goal, clearances, nod-ons) and cross from wide; difficulty
  Easy/Normal/Hard for the opposition (Esc menu, Restart match). 7v7 has its own golden + harnesses. 7v7 shots
  7–16 m: goal 36% (weak 13%, medium 43%, hard 52%) [pre-D-031; on the 80 m pitch 4–13 m: ~53%]. AI vs AI 7v7: ~2 headers/min.
- **7v7 (D-025):** the game now starts 7-a-side (keeper + 2-3-1) (now 80 × 52 m, D-031)
  (`--5v5` for 5-a-side).
- **CP0.3a (D-022/D-023):** the blue team plays: presses, marks, covers, tackles, passes, runs in behind, shoots;
  your red team-mates support, run, cover and mark; keepers position and catch. **J near their ball = tackle**
  (lunge; well-timed wins it, mistimed commits you). Bodies block. If a team-mate wins it, you get control.
  AI vs AI (20 × 5 min): ~0.75 goals/min, 77% passes arrive, possession 50/50, ~5 tackles/min, nobody stuck.
  Your taps to the ringed mate in live play: 69% received, 20% intercepted (6% when the lane is clear), out ~5%.
  Human-vs-AI checks: standing/jogging/sprinting on the ball all get dispossessed (block tackles go to the tackler);
  on our kickoff we reach the ball first; one-twos now happen (the passer runs past). Receiver sweep with brain
  team-mates: pick rate 72%, untargeted out 5%, but 35% of (all-direction) taps are intercepted - defenders defend.
  The golden hash covers an all-AI last quarter (AI shots, tackles, keepers on the ball).
- **CP0.2d (D-021):** a white ring under the team-mate a pass would go to right now (amber = an opponent near that
  lane) — the pass always goes to the ringed man; diagonals and wide team-mates are easier to hit on a keyboard;
  the keeper is reachable but only as a last resort; a pass with nobody there rolls into space and stops inside
  the pitch (77% → 12% went out); releasing the stick just before J keeps the last direction for 0.5 s.
- **CP0.2c (D-020):** through balls land just ahead of a running receiver (hold = deeper/softer, never weaker than a
  tap), taps lead runners by the real flight time and pace grows with distance, receivers keep running and are never
  abandoned when you press or release keys (the stick picks where he meets it), 1 m swept reach, deflections switch
  you to the best-placed man at once, hold J with nobody ahead = harder ball. Verified by re-running the playtest
  agent's harnesses (`tools/playtest/`): stick held through a pass 162/240 failed → 0/240; through-ball receivers
  6.0 m/s (were 0–0.5) never turning back; no-deflection pass failures 12 → 0; its 4 new findings fixed + tested.
- **CP0.2b:** hold J = through ball (white bar = how far ahead; receiver runs onto it), L = lofted pass over
  defenders, J/K at a ball coming in high = header (stand or timed jump; K at goal, J to a team-mate).
- **CP0.2a:** full 5-a-side team. J = pass to the best team-mate toward the stick (weighted to arrive ~6 m/s), control
  jumps to the receiver when the ball is struck and he runs to meet it; Q = switch; auto-switch to the team-mate
  nearest a loose ball; team-mates hold shape + make runs (placeholder, team.c); opposition = passive shuffling
  "cones"; match clock in the HUD. Receiving at a sprint = heavier first touch.
- Build: CMake + Ninja, SDL 3.4.8 vendored and statically linked.
- **Sim (modular, D-012/D-013):** `sim.c` orchestration · `ball.c` substepped physics (drag, Magnus curl, bounce,
  roll), posts/crossbar, player bodies, net box, interpolated goal-line crossing, outs · `player.c` movement with
  turn rate, parametric kicks (pass / charged shot / charged chip), height-band control, dribbling · `tuning.h`.
  Goals → 2 s celebration → kickoff reset (conceding team kicks off). Temporary out rule: ball placed back inside.
  Per-tick events (kick/touch/bounce/post/net/goal/out/reset/body).
- **Game:** Modern look, wide zoom (defaults), power bar while charging, leg draws back as you charge, kick pose
  driven by kick params (foot/kind/wind-up), GOAL banner, OUT label, no interpolation across resets, net drawn as
  the physics box, F3 shows ball speed/height/spin/owner/phase/events, control hints on screen.
- **Dedication card on launch (D-018)**: "In the memory of my brother Shubham Thakkar" + red heart, ~4 s, any key
  skips (not shown in --screenshot/--autoplay/--demo/--gallery; `--dedication --screenshot F.bmp` captures it).
- Flags: `--5v5`, `--menu` (with --screenshot: capture the Controls screen), `--check-keymap`, `--autoplay-header H` (scripted cross arriving at height H, K pressed: header visual check), `--dedication`, `--demo`, `--gallery`, `--autoplay`, `--autoplay-chip`, `--autoplay-dribble`, `--check-kick-pose`, `--style N`, `--zoom N`, `--screenshot F.bmp [--frames N]`.
- **Tests (21 ctest entries, all passing):** test_intent, test_contact, test_lob, test_rules, gamepad, replay, test_chase_assist, test_clock, test_stats, test_fixed, test_sim, test_ball, test_ball_control, test_passing, test_reception, test_ai_rules, test_ai_match, test_scoring, test_determinism, avatar_kick_pose, keymap
  (golden 5v5 `438935178956b429`, 7v7 `b84701f83e76ea56` after D-048; scripted seat chases the ball + taps/holds pass, lobs, switches, heads: ~140 kicks, 360 touches, 10 goals, 750 switches, 5 headers, owner invariant per 10 min, asserted),
  avatar_kick_pose. `tools/check_determinism.sh`: identical across gcc/clang × -O0/-O2.
- Perf: full game loop ~5% of one core; `bench_sim` ~930× real time (17.9 µs/tick after the team plan; was 15 with 10 AI players; ~0.1% of a core in play). RL training will want a
  faster brain (fewer candidates / squared distances) — measure before optimising. Callgrind: 47% in `fv2_len` (fixed-point sqrt), players_step 13%,
  receiver prediction ~4% → perf pass = a faster EXACT isqrt (bit-identical, golden unchanged); was 22,700× before team logic + pass solver — fine for play, revisit with a
  lookup table before RL training).

### Feel numbers (from test_ball_control; update when tuning changes)
Dribble numbers (D-014). Direction-change rows are the WORST case over 16 start offsets and over 40–180-tick run-ups:
| | jog | sprint |
|---|---|---|
| touch rhythm (steady) | every 16 ticks (0.27 s) | every 20 ticks (0.33 s) |
| ball gap, steady / max | ~0.45 / 0.56 m | ~0.75 / 0.91 m (reach 1.0) |
| player speed change per tick at top speed | 0.00 m/s | 0.00 m/s (was 0.30 flicker) |
| 90° cut: ball going the new way after | 3 ticks (50 ms) | 3 ticks |
| 180° turn: ball heading back after / max gap | 3 ticks / 0.63 m | 3 ticks / 1.00 m (lose at 2.2) |
| stop (stick released): ball trapped at | 0.40 m | 0.40 m (steps up if out of reach; never through the body) |
| balls lost after long run-ups + cuts | 0 | 0 (was 2) |
| shot release → ball leaves foot | ≤ 6 ticks (100 ms) | ≤ 6 ticks |

Ground roll (D-015, `test_ball`): 3 m/s → 2.8 m / 2.2 s · 6 m/s → 7.6 m / 3.3 s · **12 m/s pass → 18 m / 4.5 s** ·
22 m/s → 34 m / 5.5 s (was 55 m / 9 s for a pass).

Bounce (D-016): 2 m drop → 0.69 m; full chip stops after ~30.6 m (was 39.7). Possession "no magnet" checks: ball
never closer than 0.40 m to the player's centre on stops/receptions/gathers; no braking from beyond reach.

Scoring sweep (`test_scoring`, keyboard 8-way, statue keeper): natural positions (x ≥ 10 m, |y| ≤ 4 m, stick toward
goal) score **51%** vs statues; in the **live** setup (CP0.3 keeper + defending AI) **40%**
(asserted ≥ 25%). Live passes to moving team-mates: 23/24 complete; misses are mostly the keeper/kickoff statues. Floor asserted at 45%.
Frame pacing: 61.9 fps on a 60 Hz display (vsync on). 120/144 Hz not tested here.

| kick | speed | carry to 1st bounce | peak | flight |
|---|---|---|---|---|
| shot 0% | 14 m/s | 4.6 m | 0.07 m | 0.35 s |
| shot 50% | 22 m/s | 7.9 m | 0.18 m | 0.38 s |
| shot 100% | 30 m/s | 20.7 m | 0.81 m | 0.80 s |
| chip 0% | 8 m/s | 7.7 m | 1.25 m | 1.00 s |
| chip 50% | 11.5 m/s | 13.7 m | 2.15 m | 1.32 s |
| chip 100% | 15 m/s | 20.9 m | 3.28 m | 1.62 s |

## Next step (exact)
1. Finish CP0.6 (parts 2-4 in the "Current checkpoint" block), then stop for the owner's playtest of CP0.5a/b +
   D-053/D-054 + CP0.6 together. Log feedback in PLAYTEST_FEEDBACK.md.
2. Open from earlier: AI attack with offside, the AI never slides, a seat receiver at a contested corner header.

## Known issues / environment notes
- Audio: SDL is built with ALSA + PulseAudio (dev packages installed). If a fresh machine has no sound, install
  `libasound2-dev libpulse-dev`, then `rm -rf build && cmake -S . -B build -G Ninja && cmake --build build`;
  the game prints `audio: driver ...` (or `audio: off (...)`) at launch.
- No fouls/offside/free kicks/halves yet (CP0.4).
- Headers: height bands exist; the trigger is CP0.2.
- No gamepad mapping yet (CP0.4). Keeper doesn't dive (CP0.3).
- Avatar nits: eyes only on large heads; back number is a generic "11".
- Agents: never `pkill -f` a pattern that also appears in your own shell command (it kills the shell).

## Controls (D-024 — defaults; Esc → Controls to rebind, saved automatically)
Arrows move · **E** sprint · **S** pass (hold = through ball; near their ball = tackle) · **D** shoot (hold) ·
**A** lofted pass · **W** chip (hold) · **Q** switch · **C** zoom · **P** look · F3 debug · **Esc** pause menu.
Older playtest guides below still say J/K/L/I/WASD — read them as Pass/Shoot/Lob/Chip/Move.

## Playtest guide (CP0.3b — focus: keeper, headers, difficulty)
Keepers now dive: power and placement beat them (corners), soft or central shots are caught/parried. Blue heads the
ball (crosses come in from wide; they clear headers from their box). Esc → Difficulty (Left/Right) → Restart match.
Judge: keeper fair? Do headers feel right (you can still win them with S/D as the ball drops)? Easy/Hard noticeable?

## Playtest guide (CP0.3a — focus: a real match vs AI)
`./build/wildpitch`. The blue team now attacks and defends. Defend: get goal-side and press **J** when the blue
carrier's ball is close in front of you to tackle (too early/far = you lunge past and lose a moment). Your red
team-mates cover, mark, support and make runs; if one wins the ball, control jumps to him. Their keeper positions
and catches — scoring from 10–16 m is now ~22% in tests (was 39% vs a shuffling keeper).
Notes: control does NOT auto-switch while blue has the ball or is passing — press **Q** to pick the defender you
want. Tackles have no avatar pose yet (the player lunges). Keepers can't catch above 1.45 m yet (high shots bounce
off them). The ~22% scoring figure includes being tackled while charging a shot, not just saves.
Judge: are your team-mates helpful (in good spots, making runs)? Is blue beatable but not dumb? Tackling feel?
Too many/few tackles or turnovers (AI vs AI ~7/min)? Keeper too good/bad?

## Playtest guide (CP0.2d — focus: who gets the pass)
Watch the white ring: it sits under whoever J would pass to right now (amber when a blue player is near the lane).
Try diagonals and wide team-mates on the keyboard, letting go of the stick just before J, passing with nobody in
that direction (the ball should roll into space and stay in), and back toward your keeper (only if nobody else).
Questions: does the ring ever surprise you? Is it too busy / should it only show while J is held? Amber useful?

## Playtest guide (CP0.2c — focus: does passing/receiving feel natural now?)
Same controls. Try: through balls (hold J) to a running striker — he should keep running and meet it just ahead,
in stride; longer hold = deeper and softer (the bar = how far ahead). Taps to a running team-mate lead him. While a
pass travels, press or release keys freely: he still gets it, and the direction you hold picks where he meets it
(toward = attack it, along = take it on the run); only holding straight back for 1/4 s gives you full manual. A tap
is now anything up to 200 ms. Deflected passes switch you to the best-placed man and he chases the ball.
A standing receiver: hold a direction (not toward the ball) for 1/4 s to take over and make a run instead.
Owner decided: bar = deeper (kept), no separate through key.

## Playtest guide (CP0.2b — focus: through balls, lofts, headers)
New on top of CP0.2a: **hold J** = through ball — the white bar fills = how far ahead of your team-mate it goes;
control jumps to him and he runs onto it (move the stick to take over). **L** = lofted pass to the team-mate toward
your stick, over anyone in between. **Headers:** when a ball comes in high (try L to a team-mate near goal, then
press K as it drops to him), press **K** to head at goal or **J** to head it to a team-mate — he steps under it
and jumps if it's high; the stick aims. Note: a J *tap* now fires when you release it (~1/20 s later than before).
Judge: is the tap/hold split natural, or would you rather have a separate through-ball key? Lob height/pace OK?
Headers: easy enough to time? Too strong/weak?

## Playtest guide (CP0.2a — focus: passing & switching)
`./build/wildpitch`. You're red, attacking right. New: **J** passes to the team-mate most in line with your stick
(nearer + straighter wins); the camera/marker moves to him as the ball is struck and he runs to meet it — move the
stick to take over. **Q** switches to the team-mate nearest the ball. When the ball is loose, control auto-switches
to whoever's nearest. Team-mates spread out and the striker runs beyond the ball; blue are passive cones.
Judge: do passes go to who you expected? Pace right (not too slow/fast to receive)? Is auto-switching helpful or
jumpy? Is the hand-off moment clear? Does the receiver's run feel natural?

## Playtest guide (CP0.1d — focus: does the ball look physical now?)
`./build/wildpitch` (try `Z` for the close zoom). Watch: the ball visibly rolls (panels turn, blur when fast, streak
on hard shots; shadow slides out when it's in the air). Let go of the stick while dribbling/sprinting: you step up
and trap it under the sole — it should never pass through you or get "reeled in". Pass back/sideways while
sprinting: the foot meets the ball where it is (awkward ones are a bit weaker). Shots/chips: lively first bounce
(~0.7 m from 2 m), low drives skip on, chips sit down sooner. Anything still weird → describe the moment.

## Playtest guide (CP0.1c — focus: ball roll)
Pass (J) on open grass and watch it slow: quick at first, creeping to a stop (~18 m). Loose touches, rebounds and
deflections also die sooner now. Too short/long? One number each in `src/sim/tuning.h` (`T_ROLL_DECEL`, `T_ROLL_LIN`).

## Playtest guide (CP0.1b — focus: dribbling)
Same build/controls as below. Dribble at jog and with Shift held: straight runs, sharp cuts, reversals, stops.
Judge: does the ball now feel carried (regular touches, flick visible), at a comfortable distance? Sprinting: ball a
bit further ahead but always yours? If still off, say which: ball too close / too far / touches too frequent / too
rare / turns too sharp / too sluggish — each is one number in `src/sim/tuning.h`.

## Playtest guide (CP0.1)
Run `./build/wildpitch`. You control the red #11 striker (yellow marker), attacking the goal on the RIGHT.
Everyone else stands still (no AI until CP0.3): statues block the ball but you can walk through them.
- **WASD/arrows** move, **Shift** sprint, **J/Space** pass (ground pass toward the stick, or facing if none),
  **K** hold to charge a shot, release to strike; **I** same for a chip. **F3** debug, **P** look, **Z** zoom.
- **How to score:** get inside ~10 m, avoid shooting straight at the keeper: hold **up or down** with right while
  releasing to pick that side of the goal, or just shoot straight and it goes to the far post from where you stand.
  Aim assist is on (target-based, D-013) and tunable. Release → strike takes 100 ms.
- Try: quick cuts/stops at jog vs sprint, shots at 50% vs 100%, curl (strike across your run), chip the keeper from
  10–15 m (full-power chips from close go over), hit the post/bar, watch it hit the net.
- Judge: dribbling responsive but not glued? Shots weighty? 100 ms OK? Aiming too easy/hard? Chips/curl right?
  Camera OK at wide zoom? Anything that felt "wrong" — describe it and I'll turn it into a number to tune.
- Known/temporary: going out places the ball back inside after 0.75 s; no headers yet (CP0.2); no gamepad yet;
  kickoff has blue statues right in front of you; if your monitor is >60 Hz, the exit line prints the real fps.
