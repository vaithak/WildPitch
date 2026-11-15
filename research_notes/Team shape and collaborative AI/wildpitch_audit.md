# WildPitch instinct-AI team shape audit (7v7): why everyone goes to the ball

Method and sources. The sources are the code (paths relative to `/home/vaithak/WildPitch`) and measurement harnesses
written for this audit in
`/tmp/claude-1000/-home-vaithak-WildPitch/6ee2a09e-6816-4cfe-887a-b9e6ea79e320/scratchpad/shape/`:
- `shape.c`: 7v7 (`sim_config_7v7()`, 55 x 35 m pitch, keeper + 2-3-1), 10 matches x 5 min, seeds 1..10, with
  ±0.5 m nudges after each reset (as in `tools/playtest/aimatch.c`). It samples every 6 ticks (0.1 s) in open play
  and skips set-piece setups. Phases: IN = our ball or our pass in flight, OUT = their ball or their pass, LOOSE =
  neither. Mode 0 is AI vs AI with every seat at -1 (`ai.txt`). Mode 1 puts seat 0 on team 0: it sprints at the ball,
  dribbles at goal and shoots inside 14 m, with the game's normal auto-switching (`human.txt`).
- `churn.c` measures how long each action lasts. `markpair.c` records who marks whom.
- Ablations: a scratch copy of `src/sim` with `#ifdef` switches in `brain.c` (`abl/`, built by `build.sh`, output in
  `abl_*.txt`). With no switches the copy reproduces `build/libwp_sim.a` output exactly (`diff` gave identical
  results). Project files were not edited.
- Roles: "defenders" = formation slots k1 and k2, "midfield" = k3-k5, "striker" = k6. Geometry is in attack terms:
  +x points toward the opponents' goal.
- Unless stated otherwise, numbers are for team 0. Team 1 is within a few percent on every metric (see `ai.txt`), so
  the behaviour is symmetric and is not a bias bug.

## Q1. How many players engage the ball (proximity and ball-directed actions)?

### Takeaway
"Everyone goes to the ball" is mostly not literal chasing. CHASE/PRESS normally involves exactly one man. The swarm
comes from SUPPORT (in possession) and MARK (out of possession). Both target points near the ball, so on average 4.1
of 6 outfielders (in possession) and 3.7 of 6 (out of possession) are within 10 m of the ball. Across both teams,
about 7.7 of the 12 outfielders are within 10 m of the ball at any moment.

### Cited Findings
- **Players within 10 m of the ball (AI vs AI, team 0).** In possession: mean 4.07 of 6. Distribution 0..6:
  0.3/3.3/9.6/20.4/24.6/25.7/16.1 %, so 5 or more players are within 10 m 42% of the time. Out of possession: mean
  3.68 (0.3/9.2/15.8/19.9/20.8/19.9/14.1 %). Loose ball: 2.78. Team 1: 4.13 in, 3.53 out. —
  [shape.c → ai.txt](scratchpad/shape/ai.txt)
- **Players within 5 m of the ball.** In possession: mean 2.12, with 3 or more inside 5 m 36% of the time. Out of
  possession: mean 1.35. — [ai.txt](scratchpad/shape/ai.txt)
- **CHASE/PRESS counts out of possession.** 0 players 7.0%, 1 player 79.1%, 2 players 11.3%, 3 or more 2.5%.
  Counting COVER as well: 1 player 54.5%, 2 players 32.9%, 3 or more 9.6%. Loose ball: 1 player chasing 66.5%,
  nobody 28.6%. In possession, almost nobody chases (99% zero). — [ai.txt](scratchpad/shape/ai.txt)
- **Non-first men in ball actions.** PRESS is done by a player who is NOT the team's first man to the ball (by
  `ball_rank` distance) 45% of the time. For CHASE the figure is 8% when out of possession and 8-14% on a loose ball.
  — [ai.txt](scratchpad/shape/ai.txt)
- **Action mix by role, out of possession.** Defenders: MARK 66%, PRESS 10%, CHASE 9%, COVER 7%, SHAPE 7%. Midfield:
  MARK 65%, SHAPE 7%. Striker: MARK 73%, SHAPE 3%. On a loose ball MARK is 70-78% for all roles. —
  [ai.txt](scratchpad/shape/ai.txt)
- **Action mix by role, in possession.** Defenders: SUPPORT 85%, SHAPE 2%. Midfield: SUPPORT 53%, RUN 26%,
  RECEIVE 13%, SHAPE 1%. Striker: SUPPORT 65%, RUN 15%, SHAPE 0%. — [ai.txt](scratchpad/shape/ai.txt)
- **The role-anchored action is almost never chosen.** SHAPE is the only action anchored to a player's role. Across
  all roles and phases it wins 0-16% of decisions. — [ai.txt](scratchpad/shape/ai.txt)
- **Human seat chasing the ball (team 0).** The AI team-mates are a bit less ball-centric: 3.48 within 10 m in
  possession, 3.09 out of possession. AI defenders choose SHAPE 27% of the time when out of possession (7% in AI vs
  AI). The AI opponent (team 1, in possession) still has 3.24 within 10 m. —
  [human.txt](scratchpad/shape/human.txt)

### Inferences
- The owner's "everyone going towards the ball" is the visible result of two whole-team behaviours:
  - Attackers pick SUPPORT points that are by construction 7-12 m from the ball.
  - Defenders man-mark those attackers wherever they are.
- Literal ball-chasing by 3 or more players is rare in the shipped weights (2.5%). It is a latent failure mode,
  though: Q5 shows it explodes when MARK is removed.
- About 1.3 players are inside 5 m of the ball out of possession, which is about right (one presser). The problem is
  the 5-10 m ring, where 2-3 more players stand.

### Gaps
- No per-event analysis of how fast the crowd forms after a turnover (time series). Only stationary distributions
  were measured.

## Q2. Team compactness, line structure and role adherence

### Takeaway
Both teams play as a tight blob, much tighter than WildPitch's own formation:
- Outfield length is about 11 m and width about 16 m, on a 55 x 35 m pitch.
- Convex hull area is about 90-100 m², against 221 m² for the formation's own kick-off shape.
- The defence-to-midfield-to-striker lines are collapsed to 2-3 m and 1 m gaps.
- Players average 12-18 m away from their ball-shifted formation spot.
- Out of possession, the "defenders" are ahead of the ball more than half the time.

### Cited Findings
- **Compactness, AI vs AI, team 0.**

  | phase | length | width | hull area | centroid to ball |
  |---|---|---|---|---|
  | in possession | 11.1 ± 7.0 m | 16.5 ± 4.6 m | 97 ± 75 m² | 5.7 m |
  | out of possession | 10.9 ± 7.5 m | 15.5 ± 4.7 m | 89 ± 77 m² | 7.7 m |
  | loose ball | 13.1 m | 15.7 m | 98 m² | 11.3 m |

  Team 1 is the same (10.8/16.5/92 in, 11.8/16.0/104 out). Area barely changes between in and out of possession, so
  the team does not "make the pitch big" when it has the ball. — [ai.txt](scratchpad/shape/ai.txt)
- **Reference: the formation's own shape.** `formation_frac` 7v7 places k1/k2 at x = -0.62·hl, the midfield at -0.34
  and -0.36, and the striker at -0.06. That spans 0.56 x 27.5 = 15.4 m long and 1.24 x 17.5 = 21.7 m wide, with a
  hull of 221 m² (computed). — [src/sim/sim.c `formation_frac`](src/sim/sim.c)
- **Line gaps (mean attack-x difference).**
  - Defence to midfield: 2.8 m in possession, 2.4 m out (formation: 9.6 m).
  - Midfield to striker: 1.1 m in, 0.9 m out (formation: 8.0 m).
  - Lines effectively do not exist. — [ai.txt](scratchpad/shape/ai.txt)
- **Distance from the dynamic home spot.** This is a replica of `home_spot()`: formation spot + 0.55·ball_x,
  + 0.3·ball_y, ±4/-1 m for possession.
  - In possession: defenders 14.2 m, midfield 12.2 m, striker 9.9 m.
  - Out of possession: defenders 17.9 m, midfield 15.3 m, striker 10.5 m.
  - Defenders are more than 10 m from their spot 68% of the time in possession and 77% out of possession. —
  [ai.txt](scratchpad/shape/ai.txt)
- **Defenders ahead of the ball.** Out of possession, a k1/k2 defender is more than 1 m ahead of the ball 57% of the
  time (team 1: 51%). In possession: 16%. — [ai.txt](scratchpad/shape/ai.txt)
- **Who marks whom.** Our defenders mark opponent defenders 40% of the time, midfielders 43%, the striker 17%. Our
  striker marks opponent defenders 50% of the time. No opponent is ever double-marked (crowd penalty works). The
  marked man is on average 25 m from our goal line, i.e. around halfway. —
  [markpair.c](scratchpad/shape/markpair.c)
- **Shifting with the ball (regression of team centroid on ball position).**
  - Out of possession: lateral slope 0.53 (r 0.78), vertical slope 0.86 (r 0.92).
  - In possession: lateral 0.62, vertical 0.91.
  - The designed `home_spot` shift is 0.3 lateral and 0.55 vertical. The team follows the ball almost 1:1
    vertically and about twice as strongly laterally as designed. — [ai.txt](scratchpad/shape/ai.txt);
    [brain.c `home_spot`](src/sim/brain.c)
- **Decision stability.** About 1.22 action changes per player per second. Mean stint: SHAPE 1.6 s, SUPPORT 1.7 s,
  MARK 1.5 s, CHASE 0.42 s, PRESS 0.40 s, COVER 0.34 s. MARK rarely switches its target (0.03 switches per second).
  — [churn.c](scratchpad/shape/churn.c)
- **With a human seat (team 0).** The AI team is longer: 17-18 m, hull 160-190 m², line gaps 7.4-8.1 m, defenders
  10.6-11.5 m from their spots. The AI opponent (team 1, in possession) also stretches to 18.4 m and 188 m², so a
  less ball-centric human opponent (one who carries the ball forward and shoots) loosens both blobs. —
  [human.txt](scratchpad/shape/human.txt)

### Inferences
- Role adherence is effectively zero in AI vs AI. Slot indices k1-k6 carry almost no positional meaning during play.
  Slot k1 is labelled a defender, but most of the time it is somewhere else and doing something unrelated.
- Even a perfect role-adherent team would only be 15.4 m long, because the formation table is short and every line
  shifts by the same 0.55·ball_x. The block cannot stretch in possession or squeeze out of possession except for a
  uniform ±4/-1 m.

### Gaps
- No measurement of "distance between the two teams' blocks" or of each player's space-control (Voronoi) area.

## Q3. In-possession support structure

### Takeaway
The attack has bodies near the ball but little structure:
- About 4 team-mates are within 3-15 m of the ball, but only about 2.2 have a pass lane clear by at least 1.5 m.
- Those open options cover only 1.5 of 4 directions.
- Width on both flanks almost never exists (2-3%).
- Nobody is more than 3 m ahead of the ball 58-61% of the time.
- No rest defence (nobody 5 m or more behind the ball) 41-43% of the time.

### Cited Findings
- **Pass options.** In possession, team-mates 3-15 m from the ball: 4.01. With a lane clear of 1.5 m or more:
  2.18. Sectors covered by open options (forward, left, right, back): 1.54 of 4. — [ai.txt](scratchpad/shape/ai.txt)
- **Depth.** Team-mates more than 3 m ahead of the ball: 0.89 on average, and nobody 58% of the time. RUN is chosen
  by midfield 26% and the striker 15% of the time, yet the runs do not hold depth. —
  [ai.txt](scratchpad/shape/ai.txt)
- **Rest defence.** Team-mates more than 5 m behind the ball: 1.37, and nobody 41% of the time. Team 1: 1.31 and 43%.
  — [ai.txt](scratchpad/shape/ai.txt)
- **Width.** Someone within 6 m of the left touchline 30% of the time, the right 43%, both at once 2% (team 1: 3%).
  — [ai.txt](scratchpad/shape/ai.txt)
- **Where SUPPORT targets are.** Mean SUPPORT target: 9.0 m from the ball and 16.1 m from the player's own home spot.
  — [ai.txt](scratchpad/shape/ai.txt)
- **How SUPPORT candidates are generated.** They are generated only around the BALL: 7 angles (0, ±30, ±60, ±110°)
  x 2 radii (7 m, 12 m) = 14 points. There are no candidates around the formation spot, none wider than 12 m from
  the ball, and no touchline-hugging or deep "rest" positions. — [src/sim/brain.c `gen_off_ball`](src/sim/brain.c)

### Inferences
- Every off-ball attacker that picks SUPPORT stands on the same 7-12 m ring around the ball. The ring itself bounds
  the shape at roughly 24 m by 24 m. The crowd penalty (team-mates' targets within 4 m) only spreads players around
  that ring, never away from it.
- There is no team-level concept of width, depth or rest defence. Each player independently maximises
  openness/lane, so the options are redundant (1.5 of 4 directions covered).

### Gaps
- Triangle structure is not measured directly (e.g. the share of carrier-plus-two-options triangles with no
  collinearity). Sector coverage is used as a proxy.

## Q4. Out-of-possession structure

### Takeaway
There is usually one presser, plus a nearby second man about 6 m away (79% of the time within 8 m). But only about 2
of 6 outfielders are goal-side of the ball. The rest are man-marking opponents around halfway, often ahead of the
ball. In effect the team defends man-to-man with no zone and no back line.

### Cited Findings
- **Pressure on the carrier.** Nearest defender 3.2 m from the carrier, second nearest 6.0 m (within 8 m 79% of the
  time). Within 3 m of the carrier: 0.75. — [ai.txt](scratchpad/shape/ai.txt)
- **Goal-side players.** Outfielders goal-side of the ball: 1.97 of 6 (team 1: 2.30). —
  [ai.txt](scratchpad/shape/ai.txt)
- **COVER.** COVER is a point 5 m goal-side of the ball. Only the rank-1 man gets the F_FIRST bonus (2.4). COVER
  appears in 7% of out-of-possession decisions, and CHASE|PRESS|COVER has 2 players 33% of the time. —
  [brain.c](src/sim/brain.c); [ai.txt](scratchpad/shape/ai.txt)
- **Defender shape.** Defenders are more than 1 m ahead of the ball 57% of the time and more than 10 m from their
  home spot 77% of the time. — [ai.txt](scratchpad/shape/ai.txt)

### Inferences
- First pressure exists and a second defender is usually close. However, "close" is the MARK ring rather than a
  deliberate cover position. The goal-side block behind them is mostly missing, which plausibly explains through
  balls and long runs at goal.
- Lateral shift (slope 0.53) is happening, but as a side-effect of following opponents, not as a zonal slide.

### Gaps
- No measure of defensive outcome versus shape (e.g. goals conceded after turnovers versus goal-side count).

## Q5. Code root causes (with ablation evidence)

### Takeaway
The root cause is structural, not one bad weight:
1. Every off-ball candidate except SHAPE is placed relative to the ball or to opponents.
2. SHAPE's utility drops faster with distance (-0.15/m) than its rivals' (-0.10 to -0.12/m), and its ceiling is low
   (bias 1.0) compared with SUPPORT, which typically scores 2-4.
3. MARK is man-to-man across the whole pitch for every player, with no zone or role restriction.
4. The formation spot itself is short and shifts rigidly.

Once a player is a few metres from his spot, SHAPE can never win again. That makes the blob self-reinforcing.
Ablations confirm the causal chain.

### Cited Findings
- **Weights** ([src/sim/brain.c W[][]](src/sim/brain.c)). Utility = W · features:

  | action | utility | notes |
  |---|---|---|
  | SHAPE | 1.0 - 0.15·d_home - 1.0·crowd | |
  | SUPPORT | 0.6 + 0.10·fwd + 0.35·open + 0.45·lane - 0.20·cost - 1.5·crowd | open capped at 8, lane at 4; cost = max(d_me, d_home/2) |
  | RUN | 0.2 + 0.15·fwd + 0.25·open + 0.30·lane - 0.10·d + 1.0·shot_quality - 1.5·crowd | |
  | CHASE/PRESS | 0.5 + 4.0·first - 0.10·d | first = 1 only if rank 0 |
  | COVER | 0.5 + 2.4·(rank==1) - 0.10·d - crowd | |
  | MARK | 0.4 + 2.0·threat - 0.12·d - 2·(others marking him) | threat = 1 - d(opp, own goal)/30 |

  Commitment adds +0.8 for the same action and target (within 2 m).
- **Arithmetic: SUPPORT beats SHAPE.** A typical open support spot (open 5 m, lane 3 m, fwd 0, cost 8 m) scores
  0.6 + 1.75 + 1.35 - 1.6 ≈ 2.1. SHAPE's maximum is 1.0, reached only when standing exactly on the spot. SHAPE
  therefore only wins in possession when no ring point is open. Measured: SHAPE 0-2% in possession. — derived from
  [brain.c](src/sim/brain.c); [ai.txt](scratchpad/shape/ai.txt)
- **Arithmetic: MARK beats SHAPE.** For an opponent at halfway (threat ≈ 0.17), MARK ≈ 0.73 - 0.12·d_mark. SHAPE =
  1 - 0.15·d_home. MARK wins whenever d_home > 1.8 m + 0.8·d_mark. The measured d_home is 15-18 m, so SHAPE scores
  about -1.3 to -1.7 and is beaten by any mark within about 20 m. — derived; [ai.txt](scratchpad/shape/ai.txt)
- **Arithmetic: a non-first CHASE beats SHAPE.** It scores 0.5 - 0.10·d, or 1.3 - 0.10·d with commitment while
  continuing. It beats SHAPE whenever d_home > 3.3 m + 0.67·d_ball (2 m + 0.67·d_ball while continuing). F_FIRST only
  separates rank 0 from the others; it does not penalise non-first men. — derived from
  [brain.c](src/sim/brain.c)
- **MARK candidate scope.** MARK candidates are generated for every opponent outfielder except the carrier, for every
  player, with no zone or role filter. The crowd term only counts others marking the same man. —
  [brain.c `gen_off_ball`](src/sim/brain.c); [markpair.c](scratchpad/shape/markpair.c) (defenders mark opponent
  defenders 40% of the time)
- **`ball_rank`.** It uses Euclidean distance to the ball predicted 0.4 s ahead, not time-to-meet, and ignores the
  goal-side PRESS point. It is recomputed by each player on his own staggered decision beat, so two players can both
  believe they are rank 0 or rank 1. Measured: 45% of PRESS samples are by a non-first man. —
  [brain.c `ball_rank`](src/sim/brain.c); [ai.txt](scratchpad/shape/ai.txt)
- **`home_spot`.** Every line shifts by the same 0.55·ball_x and 0.3·ball_y, with +4 m in possession and -1 m out of
  possession. The formation is 15.4 m long and is never stretched or squeezed per line. —
  [brain.c `home_spot`](src/sim/brain.c); [sim.c `formation_frac`](src/sim/sim.c)
- **No team-level coordination.** Each player chooses independently. Shared state is limited to `ai_act`, `ai_goal`
  and `ai_mate` of team-mates as read by `crowd()` and MARK duplication. There are no role assignments (presser,
  cover, holding/rest players, width holders) and no slot reservation. — [brain.c](src/sim/brain.c)
- **Ablations (team 0, AI vs AI, 10 x 5 min)** — [abl_*.txt](scratchpad/shape/)

  | variant | IN len / wid / hull | OUT len / wid / hull | ≤10 m IN / OUT | goal-side OUT | defenders ahead of ball OUT | ≥3 in CHASE\|PRESS OUT |
  |---|---|---|---|---|---|---|
  | baseline | 11.1 / 16.5 / 97 | 10.9 / 15.5 / 89 | 4.07 / 3.68 | 1.97 | 57% | 2.5% |
  | no MARK candidates | 11.2 / 18.2 / 114 | 11.7 / 15.0 / 98 | 3.85 / 3.67 | 2.27 | 56% | **54.5%** |
  | SUPPORT ring centred halfway between ball and home spot | 14.4 / 20.9 / 164 | 14.5 / 18.7 / 146 | 3.22 / 2.91 | 2.37 | 45% | 0.8% |
  | non-first PRESS/CHASE get no commitment bonus | ≈ baseline | 11.2 / 15.9 / 92 | 4.03 / 3.51 | 2.06 | 57% | 2.3% |
  | all three | 16.2 / 23.3 / 201 | 16.6 / 20.1 / 183 | 2.90 / 2.67 | 3.43 | 16% | 18.2% |
  | SHAPE bias 1.0 → 3.0 (only change) | 20.4 / 20.5 / 235 | 22.3 / 20.4 / 260 | 2.83 / 2.28 | 3.34 | 20% | 0.1% |

  More results:
  - SHAPE bias 3.0 also gives line gaps of 7.7-7.9 m (defence to midfield) and 3.9-4.1 m (midfield to striker), and
    defenders more than 10 m from their spot drop from 77% to 32%.
  - In possession with SHAPE bias 3.0: nobody ahead of the ball 42% of the time (baseline 58%), no rest defence 9%
    (baseline 41%), open-option sectors 1.87 (baseline 1.54).
  - Trade-off: the second defender within 8 m of the carrier falls from 79% to 52%.
  - Goals per 50 min (both teams): baseline 14-26, SHAPE bias 3.0 29-22, all three 30-16. The sample is small.

### Inferences
- MARK is currently masking a worse failure. Removing it, without making SHAPE competitive, sends 3 or more players
  into CHASE/PRESS 54% of the time, which is literally "everyone to the ball". The ball-chase pull comes from
  CHASE/PRESS being offered to every player, at a flatter distance slope than SHAPE.
- The single biggest lever is that the formation anchor cannot compete. Raising SHAPE's bias alone doubles team
  length and hull area. A redesign should make the role anchor part of every candidate, rather than a separate
  action that must outscore ball-relative ones. Options:
  - Generate SUPPORT/MARK/COVER candidates around or within the role zone.
  - Add a "distance from my zone" feature to all off-ball candidates.
  - Or use team-level role assignment: one presser, one cover, the rest zonal.
- The commitment rule makes little difference to shape. The 45% non-first PRESS is mostly a rank-definition issue
  (distance rather than time, staggered beats), not commitment.
- These ablations are diagnostic probes, not tuned proposals. All of them change match balance and passing
  behaviour, which was not evaluated here.

### Gaps
- Did not measure pass completion or other football-quality metrics under the ablations. Did not run the ablations
  in human-seat mode.

## Q6. Comparison with real-football reference numbers

### Takeaway
WildPitch's AI blocks are far more compact than real teams, both in absolute terms and as a share of the pitch.
Real teams are also clearly bigger in possession than out of it. WildPitch is the same size in both.

### Cited Findings
- **Elite 11v11 length and width.** Possession sequences (12 elite matches, 1,413 sequences): about 38 m team length
  and 43 m width against top opponents, with a 12% coefficient of variation. Against weaker opponents, length
  decreases and width increases. — [PMC6705862](https://pmc.ncbi.nlm.nih.gov/articles/PMC6705862)
- **Elite 11v11 surface area.** Team surface area (convex hull) is significantly larger in possession: about
  1,638-1,831 m² in possession versus 1,277-1,369 m² out of possession for one elite team. —
  [Springer chapter](https://springerlink.bibliotecabuap.elogim.com/content/pdf/10.1007/978-3-319-59029-5_5)
- **Defensive compactness.** Successful defensive plays show higher compactness of the defenders close to the ball,
  and stronger contraction of the whole team. —
  [KIT thesis](https://publikationen.bibliothek.kit.edu/1000173445/154142318)
- **Phase methodology.** Team length, width, length/width ratio and surface area are compared across in-possession,
  out-of-possession and transition phases. — [LJMU MPhil](https://researchonline.ljmu.ac.uk/19091/1/2022guangzezhangmphil.pdf)
- **Caveat.** These variables have limited direct coaching utility on their own. —
  [Revista RETOS](https://www.revistaretos.org/index.php/retos/article/download/117553/84526/506763)

### Inferences
- **Scaled by pitch dimension** (105 x 68 m to 55 x 35 m), the elite figures are about 36% of length (→ about 20 m
  in WildPitch) and 63% of width (→ about 22 m). WildPitch's AI measures 20% of length (11 m) and 46% of width
  (16 m). My scaling is approximate: 11v11 has 10 outfielders, WildPitch 6.
- **As a share of pitch area,** the elite hull is about 18-26% of the pitch (1,277-1,831 of about 7,140 m²).
  WildPitch's AI hull is about 5% (90-100 of 1,925 m²). The formation's own shape is 11.5% (221 m²). The SHAPE-bias-3
  ablation reaches 12-14%, closer to the formation but still under the 11v11 share.
- **In versus out of possession:** real teams expand in possession (about +25-35% area in the cited study). WildPitch
  baseline: 97 vs 89 m², essentially no expansion.
- **The formation itself** (15.4 m long, about 28% of pitch length) is probably shorter than a realistic 7v7 block
  of about 20 m. This is an inference from the scaled 11v11 numbers, not from 7v7 data.

### Gaps
- I found no reliable published team length, width or area data for 7v7 or youth small-sided match play. The
  comparison relies on scaled elite 11v11 figures and the cited caveats.
