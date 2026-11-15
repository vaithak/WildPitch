# Inferring the intended pass receiver: HCI target selection + football pass analytics

Scope: (A) game/HCI techniques for picking a target from coarse/noisy direction input; (B) football
analytics models of receiver choice and pass risk. Aimed at a deterministic 2.5D football game with an
8-way keyboard or analogue stick (WildPitch). Source-checked notes; inferences are marked as such.

## (A1) Target selection under coarse/noisy direction: cones vs scoring functions, bubble/area cursors, target expansion

### Takeaway
The HCI literature favours **"always pick exactly one best target by a continuous score"** (bubble cursor;
cone + scoring function as in IntenSelect) over hard fixed-size zones (area cursor / fixed cone), because
fixed zones either capture several targets (ambiguity) or none (dead space). Enlarging each target's
*effective width* — e.g. giving every target its Voronoi cell — is what produces the speed/accuracy gain,
and Fitts' law still predicts performance with effective width substituted for actual width.

### Cited Findings
- The bubble cursor (Grossman & Balakrishnan, CHI 2005) is an area cursor that "dynamically updat[es] its size based on the proximity of surrounding targets, such that there is always exactly one target inside the hotspot." — [Grossman & Balakrishnan 2005, PDF](https://www.dgp.toronto.edu/papers/tgrossman_CHI2005.pdf)
- Its algorithm: IntD_i = shortest distance from cursor centre to target i's border; ConD_i = longest such distance. Pick i = closest target by IntD, j = second closest; **radius = min(ConD_i, IntD_j)**. In practice they leave some empty space before the second-closest target, and add a visual "second bubble" that envelopes the captured target as reinforcing feedback. — [Grossman & Balakrishnan 2005](https://www.dgp.toronto.edu/papers/tgrossman_CHI2005.pdf)
- A **circular** activation shape is used because it "ensures that the target closest to the cursor centre is always captured first" (a square captures a farther target first) — i.e. the shape of the zone defines the implicit distance metric. — [Grossman & Balakrishnan 2005](https://www.dgp.toronto.edu/papers/tgrossman_CHI2005.pdf)
- Fixed area cursors fail when too large (multiple targets captured → effective width regresses to actual width, behaving like a point cursor) or too small (empty space, no target). — [Grossman & Balakrishnan 2005](https://www.dgp.toronto.edu/papers/tgrossman_CHI2005.pdf)
- The bubble cursor's activation regions equal the targets' **Voronoi cells**, which "makes maximal use of the effective width." — [Grossman & Balakrishnan 2005](https://www.dgp.toronto.edu/papers/tgrossman_CHI2005.pdf)
- Results: bubble cursor significantly faster than point cursor and object pointing (p < .0001), still faster even when effective width was only 33% larger than actual (EW/W = 1.33); performance modelled by Fitts' law; error rates 1.58% (bubble) vs 2.98% (point) vs 2.31% (object pointing) in the 2D study; distracter density had a significant but smaller effect on movement time. — [Grossman & Balakrishnan 2005](https://www.dgp.toronto.edu/papers/tgrossman_CHI2005.pdf); DOI [10.1145/1054972.1055012](https://dx.doi.org/10.1145/1054972.1055012)
- IntenSelect (de Haan, Koutek & Post, EGVE 2005): objects inside a user-controlled **conic selection volume** are scored by a scoring function; **scores accumulate over time** giving a dynamic ranking; the top object is indicated by bending the selection ray toward it. Improved performance over ray-casting, especially for small objects. — [IntenSelect, Eurographics DL](https://diglib.eg.org/items/54676737-8fc7-4188-9131-73a8b6e16118/full)
- IntenSelect's score is based on the angle of the object centre from the cone's centre line, computed from the perpendicular distance to the line and the distance along it; an exponent *k* "narrows" scoring with depth; β = cone opening angle; temporal hysteresis with two parameters, "stickiness" and "snappiness", prevents rapid target switching. — [Kopper/LenSelect paper, Frontiers in VR 2021](https://www.frontiersin.org/articles/10.3389/frvir.2021.684677/full)
- In the 2021 comparison, IntenSelect was best (fastest, fewest errors) in tidy scenes with fast-moving objects and distant objects, but worse than LenSelect/ray-casting in near, cluttered, dynamic scenes, and rated worse subjectively. — [Frontiers in VR 2021](https://www.frontiersin.org/articles/10.3389/frvir.2021.684677/full)
- IntenSelect+ overhauled the scoring function to simplify per-object parameterisation; 42-participant study showed significant gains over IntenSelect in performance, task load and UX. — [IntenSelect+ record](https://katalog.dhi-paris.fr/vufind/Record/NLM369269705/Description)
- Target-expansion rationale: enlarging effective target width lowers Fitts' Index of Difficulty, reducing time and errors (LenSelect enlarges objects near the ray). — [Frontiers in VR 2021](https://www.frontiersin.org/articles/10.3389/frvir.2021.684677/full)

### Inferences
- A pass picker is a 2D "angular bubble cursor": the input direction is the cursor, team-mates are targets.
  Instead of a fixed cone (fixed area cursor → "nobody in the cone" or "three people in the cone"), always
  pick the best-scoring team-mate (argmax), optionally with a hard outer cutoff (e.g. ±90°) beyond which a
  pass goes "into space" in the raw direction.
- In angle space, the bubble-cursor rule means each team-mate owns the angular sector halfway to its
  angular neighbours (a 1-D Voronoi partition on the circle). For 8-way input that alone solves "can't
  select the player at 20°": if no one is nearer in angle, the 0° or 45° press both land in his sector.
- Pure angle-Voronoi ignores distance; IntenSelect's depth exponent *k* shows the standard fix: make angular
  tolerance depend on distance (perpendicular offset matters more for near targets, less for far ones).

### Gaps
- Exact IntenSelect formula could not be extracted from the fetched pages (formulas lost in extraction;
  the RWTH "Revisited" PDF exceeded fetch size). From memory (UNVERIFIED): s_contrib = 1 − atan(d_perp / d_proj^k)/β,
  with 0 < k ≤ 1, and accumulated score s_t = s_{t−1}·c_stickiness + s_contrib·c_snappiness. Check the
  original [EGVE 2005 paper](https://diglib.eg.org/items/54676737-8fc7-4188-9131-73a8b6e16118/full) before quoting.
- No source found giving 2D angular-selection experiments specifically with 8-way (45°) input.

## (A2) Aim assist in games: slowdown, magnetism, snapping, priority by angle vs distance

### Takeaway
Console aim assist is a set of nested angular zones around the aim direction (outer = slowdown/friction,
inner = magnetism/bending), tuned per weapon and per range; it is attached to the aim, not the target, and
is exposed to tuning numbers (e.g. "magnetism 35% vs 50%"). Football games expose the same idea as pass
assistance levels and a "receiver lock" timing option.

### Cited Findings
- GDC talk "Techniques for Building Aim Assist in Console Shooters" (Nick Weihs, Insomniac) covers camera acceleration, dead-zoning, magnetism, centering and friction. Slides/content not retrievable. — [GDC Vault](https://gdcvault.com/play/1017942/Techniques-for-Building-Aim-Assist)
- Witchfire (The Astronauts): stickiness via two outer circles — slowdown begins when the larger circle touches an enemy, full slowdown at the smaller; bullet magnetism via inner circles (impact point shifted toward the circle's edge; anything inside the smallest circle hits); a critical-hit priority circle; every weapon has its own circle set; strength grows up to the weapon's effective range then drops quickly, so in side view the zones are (non-perfect) cones; assist traces key bones only (spine) so a briefly moving hand doesn't cause odd slowdowns; not implemented as enlarged hitboxes; a debug red cross shows exactly where the bullet will land. No numeric values given. — [Witchfire aim assist blog](https://www.theastronauts.com/2019/02/witchfire-aim-assist)
- Taxonomy of aim-assist types: magnetism (camera turn speed matched to target), bullet magnetism (shots bend toward target), snapping, friction, acceleration. — [Aim assist design notes](https://sites.google.com/site/doreturnend/design/design_mechanics/design_mechanics_aim-assist); [Halo 1 aim assist write-up](https://goooooouwa.github.io/gaming/2020/02/07/aim-assist-on-halo-1.html)
- Aim-assist strength is a live-tuned balance parameter: The Finals reduced camera magnetism from 50% to 35%. — [PC Gamer](https://www.pcgamer.com/the-finals-drops-major-nerf-on-controller-aim-assist-targets-controller-spoofing-on-pc)
- EA FIFA/FC: ground-pass assistance modes Assisted / Semi / Manual; Assisted adjusts both direction and power toward the receiver's path and away from defenders; Semi assists power but only slightly direction; Manual = ball goes where you point. — [EA Accessibility: FIFA 22 controls](https://www.ea.com/able/resources/fifa/fifa-22/pc/customise-controls-settings); [Ginx guide](https://admin.ginx.tv/en/fifa-21/fifa-21-controller-settings-tutorial)
- EA FC "Pass Receiver Lock" option: Late / Animation Start / Power Up — "Late" locks the receiver as late as possible, near the moment of the kick. — [FC 25 controller settings guide](https://fifauteam.com/fc-25-controller-settings/)
- GOALS: collects all teammates in the aimed direction; **charge power disambiguates** — low charge picks the closest, high charge a farther one; receivers "lock onto the ball instantly". No cone angle published. — [GOALS passing guide](https://playgoals.com/en/passing-guide)

### Inferences
- "Angle vs distance priority" in games is resolved by an extra input channel: power/charge (GOALS) or
  late-binding (FC "Late" lock lets the stick keep refining until the kick). With 8-way input both are
  valuable: late binding uses the *last* direction pressed, charge picks depth along that direction.
- Assist strength should be a tunable (in `tuning.h`) and probably skill/difficulty-dependent, mirroring
  per-weapon circles and the Assisted/Semi/Manual split.
- Witchfire's "trace the spine, not the hand" maps to using the receiver's body position (or short-horizon
  predicted position), not noisy instantaneous velocity spikes.

### Gaps
- No public numbers found for EA FC / eFootball pass-cone angles or scoring weights; Insomniac talk content
  not available as text.

## (A3) Hysteresis, intent from recent input history, and 8-way (45°) input

### Takeaway
Temporal accumulation of scores (IntenSelect "stickiness/snappiness") is the documented HCI mechanism for
stability; game evidence favours late-binding of the receiver and buffers that keep the most recent input.
No authoritative source was found with numbers for direction-smoothing windows or 8-way cone widening.

### Cited Findings
- IntenSelect uses temporal hysteresis (stickiness, snappiness) specifically "to avoid rapid switching between targets". — [Frontiers in VR 2021](https://www.frontiersin.org/articles/10.3389/frvir.2021.684677/full)
- Fighting-game input buffers (e.g. Tekken 8) are concerned only with the last input in storage. — [Hotspawn Tekken 8 buffer guide](https://www.hotspawn.com/fgc/guide/tekken-8-taking-advantage-of-the-input-buffer)
- Players report analogue-stick diagonals needing to be "pretty perfect" in some fighting games vs forgiving in others — i.e. diagonal acceptance width is a felt design parameter. — [Steam discussion, UNIB](https://steamcommunity.com/app/801630/discussions/0/3140616601479999314) (anecdotal)
- Some "analog" sticks are actually implemented as 49-way digital. — [Wikipedia: Arcade stick](https://wikipedia.com/wiki/Arcade_stick)
- FC "Late" receiver lock = choose as close to the kick as possible. — [FC 25 controller settings guide](https://fifauteam.com/fc-25-controller-settings/)

### Inferences (design suggestions, not sourced facts)
- 8-way keyboard: diagonals are often produced by two keys pressed a few ticks apart, and released
  non-simultaneously (e.g. release Up a tick before Right turns "up-right" into "right" at the kick).
  A short **direction latch** (keep the last *diagonal* seen within ~N ticks of the pass press/kick) is the
  analogue of fighting-game buffering. N ≈ 3–6 ticks at 60 Hz (50–100 ms) is a reasonable starting guess —
  no source; playtest.
- Because 8-way input has 45° resolution, true intended angle is uniform within ±22.5° of the pressed
  direction. A scoring function's angular tolerance should therefore be at least ~±22.5° flat-ish (no
  penalty) before decaying, or simply use the angular-Voronoi rule from A1 so any team-mate is reachable
  by the nearest of the 8 directions.
- Hysteresis for a *previewed* receiver: keep the current highlighted receiver unless a challenger's score
  exceeds it by a margin (e.g. +10–15%), or use IntenSelect-style exponential accumulation in fixed-point
  (`s = s*c_stick + contrib*c_snap`). Both are deterministic and cheap.
- Analogue stick: use the stick's own angle (radial dead zone), optionally smoothed over last few ticks;
  same scoring function, just narrower flat zone.

### Gaps
- Could not retrieve a dead-zone reference (Third Helix "Doing Thumbstick Dead Zones Right" fetch failed — DNS error).
- No published numbers for smoothing windows / latch durations in sports games.

## (A4) Preview / feedback: highlighting the would-be receiver

### Takeaway
Showing the currently-selected target before commit is a consistent feature of the successful techniques
(bubble cursor's enveloping second bubble; IntenSelect's bent ray; Witchfire's debug cross) — the user can
correct the stick before the commit instead of after a misplaced pass.

### Cited Findings
- Bubble cursor morphs to envelop the captured target "as a reinforcing visual cue to the user that the target is indeed captured". — [Grossman & Balakrishnan 2005](https://www.dgp.toronto.edu/papers/tgrossman_CHI2005.pdf)
- IntenSelect indicates the active (highest-ranked) object by bending the selection ray toward it. — [IntenSelect, Eurographics DL](https://diglib.eg.org/items/54676737-8fc7-4188-9131-73a8b6e16118/full)
- Witchfire shows a red cross where the bullet will actually land (debug aid). — [Witchfire aim assist blog](https://www.theastronauts.com/2019/02/witchfire-aim-assist)
- GOALS: receivers lock on instantly "so the intended target is clear to both players". — [GOALS passing guide](https://playgoals.com/en/passing-guide)

### Inferences
- A receiver marker (ring under the would-be receiver while the pass key is held / charging) is the
  football analogue; it is presentation-only (reads sim state), so it doesn't conflict with determinism if
  the sim exposes the "current candidate receiver" computed from the same function used at kick time.
  Even better: the sim stores the candidate in SimState each tick (hashed) so preview == outcome.

### Gaps
- No controlled study found isolating the usability effect of a receiver indicator in a sports game.

## (B1) Football analytics: models predicting the receiver / pass destination, and which features matter

### Takeaway
The best-documented interpretable receiver model is Spearman et al. 2017 (physics-based: each team-mate's
probability of being first to control the ball along the pass; predicted receiver = argmax), which got
~68% receiver accuracy. Data-driven models (SoccerMap, xPass) rely on distance, angle, positions and
velocities of attackers and defenders; even a single-logistic-unit baseline is decent and well calibrated.

### Cited Findings
- Spearman, Basye, Dick, Hotovy, Pop (Sloan SAC 2017) "Physics-Based Modeling of Pass Probabilities in Soccer": physics time-to-intercept inside a statistical model; intercept times from motion equations with velocity/acceleration limits, compared with ball flight time. — [Powers, tracking lecture slides](https://saberpowers.github.io/teaching/smgt432/lecture/tracking/slides.pdf); [xG Football Club summary](https://thexgfootballclub.substack.com/p/the-perfect-pass-formula-can-physics)
- Spearman 2017 formulas (secondary summary): control is exponential, P(t) = 1 − e^(−λt); receiving probability for player j: P_j(t) = ∫₀ᵗ (1 − Σ_k P_k(T)) · P_int,j(T) · λ dT; pass success = Σ over teammates (excl. passer); pass value V_j = p_j·f(x_succ) − (1 − p_j)·f(x_fail), f = negative exponential of distance to goal. Fitted σ = 0.45 ± 0.01 (stat) ± 0.04 (syst); λ = 4.30 ± 0.28 (stat) ± 1.10 (syst) s⁻¹. Data: 38 Crystal Palace matches 2015–16, 25 Hz, 10,875 passes. Intended receiver ground truth = player making the next controlled touch; **predicted receiver = team-mate with highest receiving probability → 67.9% accuracy** (12-game test set; 73.1% on one match); pass-success accuracy 80.5% (81.9% with threshold 0.27). Authors note players do not always attempt to receive passes the model favours. Ball drag modelled; spin/rolling friction not. — [xG Football Club summary (secondary)](https://thexgfootballclub.substack.com/p/the-perfect-pass-formula-can-physics)
- Spearman 2018 "Beyond Expected Goals" (Sloan) is the pitch-control / off-ball scoring paper reusing the same intercept machinery. — [Search-level summaries; Friends of Tracking code](https://www.github.com/Friends-of-Tracking-Data-FoTD/LaurieOnTracking/blob/master/Metrica_PitchControl.py)
- Power, Ruiz, Wei & Lucey (KDD 2017) "Not All Passes Are Created Equal": a pass has **risk** (likelihood of executing it in the situation) and **reward** (likelihood the pass creates a chance), estimated from two seasons of tracking data. Full model details not retrieved. — [VideoLectures KDD 2017](https://videolectures.net/videos/kdd2017_power_tracking_data); [DOI 10.1145/3097983.3098051](https://unpaywall.org/10.1145%2F3097983.3098051)
- Fernández & Bornn, SoccerMap (ECML PKDD 2020): fully-convolutional net producing a pass-success probability for **every pitch location** (a surface); 13 input channels: attacker/defender locations and velocities (sparse), distance to ball and to goal, sin/cos of angles to goal/ball, angle to goal, and sin/cos of the angle between the carrier's velocity and each attacking team-mate. Pass success log-loss: naive 0.545, logistic (single sigmoid unit) 0.384 (ECE 0.021), 2-layer dense 0.349, SoccerMap 0.217 (ECE 0.0225). Same architecture adapted to pass *selection* likelihood (no quantitative selection metrics reported). No feature-importance ranking. — [SoccerMap, arXiv 2010.10202](https://ar5iv.labs.arxiv.org/html/2010.10202)
- Opta/Stats Perform xPass features: start x, mirrored start y, pass angle and distance (from end location), foot/head, cross, open play/set piece, ground/air, number of passes in possession, carry distance before, vertical ball speed over last 3 actions, previous action type/owner, previous pass distance/angle. — [The Analyst: xP explained](https://theanalyst.com/articles/expected-pass-completion-explained)
- StatsBomb xPass uses the pass target as input (receiver location for completions; collector-tagged target for failures); xPass 360 adds freeze-frame (all-player positions) features. — [Hudl StatsBomb xPass](https://www.hudl.com/blog/statsbomb-models-expected-pass-xpass-and-pass-clustering); [xPass 360](https://www.hudl.com/blog/xpass-360-upgrading-expected-pass-xpass-models)
- Anzer & Bauer (Data Mining & Knowledge Discovery 2022) reconstruct the *intended* receiver of each pass by modelling ball flight and player movement (potential positions within a time window) at the moment of the pass, then train an xPass model. Full text paywalled; details not retrieved. — [Springer DOI 10.1007/s10618-021-00810-3](https://link.springer.com/content/pdf/10.1007/s10618-021-00810-3.pdf)
- Honda et al. (CVPRW 2022) receiver prediction from video + trajectories (3D conv + LSTM per player, transformer for interactions); trained on successful passes only because the intended receiver of failed passes is hard to label; video improves accuracy. — [CVF Open Access](https://openaccess.thecvf.com/content/CVPR2022W/CVSports/html/Honda_Pass_Receiver_Prediction_in_Soccer_Using_Video_and_Players_Trajectories_CVPRW_2022_paper.html)
- A 2026 GNN preprint (Masella) reports top-3 receiver accuracy of 97.80% (a talk page by the same author gives 65.22% top-1 / 95.65% top-3 — versions differ). — [arXiv 2605.25696](https://arxiv.org/html/2605.25696); [PyData Eindhoven 2025](https://cfp.pydata.org/pydata-eindhoven-2025/speaker/TNURBM/)
- McHale et al. work (as summarised) uses an "intended receiver" feature based on distance and angle to the passing line. — secondary search summary only; not verified.

### Inferences
- Top-1 receiver accuracy from rich models is only ~65–73%; top-3 is ~95%+. So a game should NOT pick
  the receiver purely from "where the pass would be best" — the *player's* direction input must dominate,
  with analytics features used only to break ties inside the directional neighbourhood (top-2/3 candidates).
- Features most consistently used and cheap in fixed point: distance to receiver, angle (vs input
  direction / forward), receiver velocity relative to carrier direction (SoccerMap channel), nearest
  defender to the lane / to the receiver, forward progression (x gain / distance to goal).

### Gaps
- Power et al. 2017 model specifics (features, coefficients) not retrieved.
- No published feature-importance ranking for receiver choice found.

## (B2) Simple closed-form pass risk / interception estimates usable in real time

### Takeaway
The standard real-time-friendly primitive is **time-to-intercept (TTI)**: reaction-then-straight-run time
for each player to a point on the ball path, compared with ball travel time, softened by a logistic with
σ ≈ 0.45 s. A pass is risky if any defender's TTI to some point on the lane is ≤ the ball's arrival time
there (plus margin), and safe if the receiver's TTI to the reception point beats every defender's.

### Cited Findings
- Friends-of-Tracking implementation of Spearman 2018 pitch control (`Metrica_PitchControl.py`): player keeps current velocity for reaction time then runs at vmax: r_reaction = p + v·t_react; **TTI = t_react + ‖r_target − r_reaction‖ / vmax**. Defaults: vmax = 5 m/s, t_react = 0.7 s, tti_sigma = 0.45, λ_att = λ_def = 4.3 s⁻¹, goalkeeper λ = 3×4.3 = 12.9, average ball speed 15 m/s (ball time = distance/15), integration dt = 0.04 s, max 10 s, convergence tolerance 0.01. — [Metrica_PitchControl.py](https://www.github.com/Friends-of-Tracking-Data-FoTD/LaurieOnTracking/blob/master/Metrica_PitchControl.py)
- Arrival probability: f(T) = 1 / (1 + exp(−(π/√3)/σ · (T − TTI))) (logistic with s.d. σ). Control integration: dP_i/dT = (1 − P_att − P_def) · f_i(T) · λ_i, starting at ball arrival time. — [Metrica_PitchControl.py](https://www.github.com/Friends-of-Tracking-Data-FoTD/LaurieOnTracking/blob/master/Metrica_PitchControl.py)
- Shortcut: if one team's nearest player arrives ≥ time_to_control earlier than the other's, assign full control without integrating; time_to_control = 3·ln(10)·(√3·σ/π + 1/λ) ≈ 3.3 s with defaults. — [Metrica_PitchControl.py](https://www.github.com/Friends-of-Tracking-Data-FoTD/LaurieOnTracking/blob/master/Metrica_PitchControl.py)
- Spearman 2017 fitted σ = 0.45 s and λ = 4.3 s⁻¹ from data (see B1), i.e. these defaults are empirically grounded. — [xG Football Club summary](https://thexgfootballclub.substack.com/p/the-perfect-pass-formula-can-physics)
- EA FC "Assisted" passing adjusts direction/power toward the receiver's path and away from defenders — i.e. commercial games already fold lane safety into the assisted target. — [EA Accessibility](https://www.ea.com/able/resources/fifa/fifa-22/pc/customise-controls-settings)

### Inferences (fixed-point friendly recipe)
- Per candidate receiver R (≤4 in 5v5), per defender D (≤5): sample K points along the lane (e.g. K = 4–6,
  or analytically find the closest-approach point). For each point x at distance s from the ball:
  t_ball(x) = s / v_pass; t_D(x) = t_react + max(0, ‖x − (p_D + v_D·t_react)‖ − reach) / vmax_D.
  margin_D = min over x of (t_D(x) − t_ball(x)). Lane risk ≈ logistic(−margin/σ') or a piecewise-linear
  ramp (cheaper in Q16.16: risk = clamp(0.5 − margin/(2·w), 0, 1)). Cost ≈ 4×5×K distance evaluations per
  tick — trivial; integer sqrt only.
- Receiver space: margin_R = min_D t_D(x_rec) − t_R(x_rec) (Spearman-style first-to-ball test).
- Suggested combined score (to tune by playtest; weights are not from any source):
  score_R = w_angle·A(Δθ) + w_dist·Dist(d) + w_safe·(1 − risk_R) + w_prog·Prog(Δx) + w_hyst·[R == prev],
  where A(Δθ) is flat to ±22.5° for 8-way input then falls to 0 at ~±90°, and the angle term weight is
  large enough that the safety term can only re-order candidates within roughly one 45° sector — keeping
  the player in control (cf. FC "Semi" assistance) while avoiding obvious interceptions.
- Determinism: all of the above is closed-form, branch-light, and uses only positions/velocities already
  in SimState; no RNG needed. Ties should break by a fixed order (e.g. lowest player index).

### Gaps
- No public closed-form for interception risk with ball deceleration (rolling friction) was retrieved; the
  FoT code uses constant average ball speed (15 m/s). Spearman 2017 used drag but no coefficient was found.
- No validated published weighting between "angle to input" and "pass safety" for game receiver selection.
