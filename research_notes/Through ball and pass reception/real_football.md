# Real-world football: pass weight, through balls and receiving (numbers for a 5-a-side sim)

Scope: measured pass, kick and roll speeds, pass distances (11-a-side and futsal), coaching rules for weighting passes and through balls, receiving technique, player speeds and reaction parameters, and the interception maths used in pitch-control models. Every number has a source. Anything worked out from those numbers is under **Inferences** and marked as derived. Unit conversion: 1 m/s = 3.6 km/h.

---

## 1. Measured pass speeds and distances, and how speed scales with distance

### Takeaway
I found no clean published table of real ball speeds by pass type. The usable anchors are:
- **Typical pass release speed in pro 11-a-side, from tracking data:** about 15–16 m/s.
- **Maximal kicks:** about 23–28 m/s.
- **Ball roll on regulation turf:** a slow-rolling ball decelerates by about 0.5–1.6 m/s².
- **Futsal pass distances:** about 3.6 m (short), 7.5 m (medium) and 14.5 m (long).

### Cited Findings
**Release speed from tracking data**
- Swedish Allsvenskan 2021 (Signality tracking, 25 Hz): average initial pass speed **16.20 m/s**, average deceleration **7.93 m/s²**. Method: estimate ball speed by central differences, move each pass's start to its peak-speed frame, keep only "good" passes (at least 20 frames = 0.8 s, frame-to-frame speed change under 2 m/s, first half of the pass faster than the second), then fit deceleration by linear regression. The authors say they "found little supporting research for the physics parameters involved in passing the ball". They also model 14.67 cm of ball travel while it is in contact with the foot (2/3 of a 22 cm ball diameter). — [Pfaff et al., "Controlling ball progression in soccer", arXiv 2210.16474](https://arxiv.org/pdf/2210.16474)
- The open-source Spearman-style pitch-control model (Laurie Shaw, Friends of Tracking) uses `average_ball_speed = 15.` m/s, commented "average ball travel speed in m/s". Ball flight is straight-line at constant speed. — [LaurieOnTracking Metrica_PitchControl.py](https://raw.githubusercontent.com/Friends-of-Tracking-Data-FoTD/LaurieOnTracking/master/Metrica_PitchControl.py)
- A pass-effectiveness paper assumes an average ball speed of **23.28 m/s** (citing Asai et al.) when speed is unavailable. This looks like a kick-speed figure, not an in-match pass average. — [Computational Modeling of Pass Effectiveness in Soccer (ResearchGate)](https://researchgate.net/publication/325318166_Computational_Modeling_of_Pass_Effectiveness_in_Soccer)
- Goes et al. (SportVU tracking at 10 Hz, professional matches) discarded passes faster than 40 m/s or longer than 90 m as corrupt, and passes under 2 m as accidental touches. The most and least effective passes differed in length, angle and velocity. I could not retrieve their speed distribution. — [Goes et al. 2019, Big Data](https://doi.org/doi:10.1089/big.2018.0067)

**Maximal kick speeds (upper bounds, not typical passes)**
- Futsal players kicking stationary and rolling balls: **24.2 ± 2.2 m/s** and **23.8 ± 2.7 m/s**, with foot speed about 17 m/s. — [Performance comparisons of kicking stationary and rolling balls in a futsal context (scilit)](https://www.scilit.net/publications/6e94471ff950cbe95f5872b63b606675)
- Elite female players, lab peak ball speed: about **25.1 m/s** with a standard ball and 26.5 m/s with a lighter ball. — [core.ac.uk manuscript](https://core.ac.uk/download/392170661.pdf)
- Instep kicks measured by radar: about **104 km/h (29 m/s)** mean in one Croatian study. Sterzing et al. compared full-instep, inner-instep and side-foot kicks; full-instep maximal kicks averaged about 103 km/h. I could not extract their side-foot figure. — search summary of [JSSM Suppl.10 p26-29](https://www.jssm.org/suppls/10/Suppl.10.p26-29.pdf)
- Elite shots average about 80 km/h (22 m/s). — [Frontiers fspor.2026.1766063](https://public-pages-files-2025.frontiersin.org/journals/sports-and-active-living/articles/10.3389/fspor.2026.1766063/pdf)

**Ball-roll physics on regulation surfaces**
- UEFA roll test: ball released down a ramp from 1 m must roll **5–8 m**. FIFA (EN 12234) measures the velocity drop over 1 m from an initial **2.5 m/s**; the requirement is ΔV = **0.4–0.75 m/s**, equivalent to a roll distance of **6.0–10.6 m**. Conversion: ΔV = −1.434·log10(DR) + 1.87. Deceleration formula: (Va² − Vb²)/(2·dist), and "the deceleration decreases the slower the ball roll". Ball speed at ramp exit is about **3.2 m/s**. — [Gabrielsen, ISSS Ball Roll (Norwegian Building Research Institute, 2004)](https://www.isss-sportsurfacescience.org/downloads/documents/A7ZD60AH55_Gabrielsen_Ball_Roll.pdf.pdf)

**Pass distances in futsal and small-sided play**
- Indonesia Pro Futsal League 2021 final, all outfield players (Kinovea video analysis): mean pass distance **3.58 m (short), 7.46 m (medium), 14.47 m (long)**. The page does not give category thresholds or counts. — [Analysis of passing distance, Indonesia Pro Futsal League (SPORT TK)](https://revistas.um.es/sportk/article/view/519451)
- Futsal biomechanics: inside-foot passing accuracy at 2 m was 91 ± 7%, and distance had a significant effect on accuracy (p = 0.001). This is a search-summary snippet that I did not verify in full. — [search result, futsal pass accuracy](https://revistas.um.es/sportk/article/download/519451/335841)
- 11-a-side: in a 3–5 s possession-window study, 75% of passes were under about 9.6 m. — [Flow motifs in soccer (ResearchGate)](https://www.researchgate.net/publication/334906807_Flow_motifs_in_soccer_What_can_passing_behavior_tell_us)
- Elite futsal (28 passing sequences): pass velocity was constrained by how fast the angle between the carrier→receiver and carrier→receiver's-nearest-defender vectors was changing. When a defender closes the lane faster, the pass is played faster. — [Corrêa et al., via futsal search](https://www.futsalfocus.net/wp-content/uploads/2022/02/Spatiotemporal-coordination-behaviors-in-futsal.pdf)

### Inferences (derived, not measured)
- **Rolling deceleration from the roll standards:** 3.2 m/s over 6–10.6 m gives a = v²/2d ≈ **0.5–0.85 m/s²**. FIFA's 2.5 m/s with ΔV of 0.4–0.75 over 1 m gives ≈ **0.9–1.6 m/s²**. A clean rolling football on good grass or turf therefore loses roughly 0.5–1.5 m/s per second, and loses more when faster, which fits a drag term roughly proportional to v.
- **The 7.93 m/s² tracking figure** is about 5–10 times the pure-rolling value. It probably includes the skid and bounce phase straight after the kick, air drag on lofted balls, tracking smoothing, and passes cut short by reception. A sim that applies 7.93 m/s² to ground passes would stop a 16 m/s pass within about 16 m, which is unrealistic for a driven 30 m pass.
- **Suggested sim model:** a short high-friction "skid" phase (around 2–4 m/s² for about 0.2–0.4 s), then a rolling phase (around 0.6–1.5 m/s²), plus a small linear drag term. The skid parameters are my estimate; I found no source for them (see Gaps).
- **Speed vs distance:** with rolling deceleration a ≈ 1 m/s², reaching distance d with arrival speed v_arr needs release speed v0 = sqrt(v_arr² + 2·a·d).
  - A 5 m pass arriving at 6 m/s needs only about 6.8 m/s.
  - A 15 m pass arriving at 8 m/s needs about 9.7 m/s.
  - Over 5-a-side distances (3–20 m), friction removes only about 1–3 m/s. Pass weight on a small pitch is therefore set mainly by the passer's chosen arrival speed, not by the need to overcome friction.
- **Plausible 5-a-side release speeds** (inference from the anchors above):
  - Short (3–6 m): about 5–9 m/s.
  - Medium (7–12 m): about 8–13 m/s.
  - Long or driven (14–25 m): about 12–18 m/s.
  - Shots: about 20–28 m/s.
- **Comfortable arrival speed for a controlled touch** (inference, no measured source): about 4–8 m/s on the ground. Above about 10 m/s is "heavy" for non-elite receivers.

### Gaps
- No peer-reviewed distribution of in-match pass release or arrival speed by pass length in 11-a-side or futsal was found. The pages I fetched gave only the single averages above.
- No measured value for the skid-to-roll transition distance or the sliding friction of a football on grass.
- Side-foot pass speed from Sterzing et al. was not extractable (PDF stream unreadable).

---

## 2. Weight of pass and through balls (lead, pace, timing)

### Takeaway
Coaching material is consistent but qualitative:
- Play the ball into space ahead of the runner so they meet it at full stride without slowing, checking or waiting.
- Hit it firmly enough that defenders cannot reach the lane, but softly enough that the runner can control it or the keeper cannot claim it.
- Release the pass while the run is starting and before the runner is level with the last defender.

I found no coaching source that states a lead distance in metres. Lead distance has to be derived from runner speed and ball travel time.

### Cited Findings
- A through pass needs the right "weight" so the receiver can meet it in stride. Hit it hard enough that defenders cannot cover the distance to block it, and adjust for ground conditions. Pass "into space ahead of the receiver so they don't have to slow down or change their run". Release "before the receiver is even with the defenders". The receiver "sprints into open space and intersects the ball there". Demo setup: defenders about 30 yd apart, receiver starting about 10 yd in front of them. — [Coaching American Soccer, "Intermediate Passing – The Through Pass"](https://coachingamericansoccer.com/?p=2380)
- A drill coaching point: put the ball in the runner's path "without him having to slow down or go get it", and weight it so the attacker can get onto it. — [Sport Session Planner (Patrick Snyder)](https://www.sportsessionplanner.com/s/AJxJh)
- Too much pace makes the ball hard to control, and too little lets it be cut out. Weight should be "neither too heavy nor too light". — [Cupello: playing through ball techniques](https://my.cupello.com/coaching/advice/playing-through-ball-techniques-or-skills)
- Rule of thumb: a straight ball for an angled (diagonal) run, and an angled ball for a straight run. The runner should start only when the passer can play it. — search summary of [Cupello](https://my.cupello.com/coaching/advice/technique-behind-playing-through-ball) and [World Class Coaching](https://worldclasscoaching.com/passing-the-ball-through-the-lines/)
- FIFA Training Centre on balls in behind:
  - The runner must anticipate the release and "wait patiently until the right moment to start the run".
  - "If the timing, direction and quality of both the run and the pass are right, these balls are basically undefendable."
  - The ball is often played on a diagonal into the runner's path. The angle and weight should make it impossible for the goalkeeper to intercept.
  - Runners curve onto the defender's blind side, and bending runs use the gap between defenders.
  - The passer should disguise the pass.
  - [FIFA Training Centre, "Balls in behind the defence"](https://fifatrainingcentre.com/en/game/game-insights/balls-in-behind-the-defence.php)
- Futsal: one session plan describes the sole pass as slow, so "the receiver needs to respond soon". Another sets the session aim as "weight of pass and receiving all with sole". — search summary of [Sport Session Planner futsal sessions](https://www.sportsessionplanner.com/s/2B8bb/Receiving-with-the-sole..html)

### Inferences (derived)
- **Lead point.** Aim at the point P on the runner's path where the runner and the ball arrive together:
  - t_runner(P) ≈ reaction (if the run hasn't started) + accel phase + distance / v_run.
  - t_ball(P) comes from v0 and the rolling deceleration.
  - "Into stride" means t_ball ≈ t_runner, ideally with the ball arriving slightly late (about 0.1–0.3 s) so the runner meets it moving forward rather than chasing a ball already past them. That margin is my estimate.
- **Lead distance ≈ v_run × t_ball.** Worked example: a runner at 6–7 m/s and a ball travelling 12 m at an average of about 11 m/s (≈1.1 s) gives a lead of about **7–8 m** ahead of where the runner was at release. In 5-a-side, with balls of about 0.5–1.0 s flight, leads of about **3–7 m** are expected.
- **Ball speed at the meeting point** should be close to or slightly above the runner's speed (about 6–9 m/s for sprinting outfielders). The relative speed between ball and foot is then small, so the first touch can push the ball ahead in stride. A ball much faster than the runner overruns them into the keeper's area. A ball much slower makes the runner check stride, which coaching explicitly forbids. This is an inference from the coaching rules plus kinematics.
- **The weight trade-off:** a faster pass reduces defender interception windows (they have less time than the ball's travel time). A slower pass makes a ball arriving near the runner's speed easier to take in stride. Futsal evidence ([Corrêa et al.](https://www.futsalfocus.net/wp-content/uploads/2022/02/Spatiotemporal-coordination-behaviors-in-futsal.pdf)) shows real players raise pass speed when a defender is closing the lane.

### Gaps
- No coaching manual (FA, UEFA, US Soccer) that I could access gives a lead distance in metres, or a ball-to-runner speed ratio, for through balls.
- No tracking study found that measures the ball–runner arrival-time gap for completed through balls.

---

## 3. Receiving: attacking the ball, body shape, back foot, directional first touch, touch distance, receiving at speed

### Takeaway
Standard coaching cues for receiving:
- Scan before the ball arrives.
- Get in line with the ball and move toward it (attack it).
- Open the body.
- Receive on the back foot, the one furthest from the passer.
- Cushion the ball by withdrawing the contact surface.
- Take a directional first touch away from pressure and into space.

Touch length depends on intent: a longer touch to run onto, a shorter one to dribble or keep it. I found no measured centimetre or metre values for first-touch length.

### Cited Findings
- Soccer Coach Weekly, "What is receiving":
  - Judge the ball's path and speed early to set body shape.
  - Receive with an open body so you can play forward without turning.
  - Receive on the foot furthest from the passer and nearest the next target, so the ball rolls across the body in the intended direction.
  - Scan to check no one is directly behind.
  - Direct the first touch away from pressure into space. "If you plan to run with the ball, push it further ahead to create room to accelerate; if dribbling past a defender, keep it closer. A firmer touch sends the ball further; a softer touch keeps it near you."
  - Cushion by moving the surface away from the ball on contact.
  - The inside of the foot is the default surface. The sole stops a slow ball dead. For ground balls, keep the foot only slightly off the floor.
  - [Soccer Coach Weekly](https://www.soccercoachweekly.net/coaching-advice/what-is-receiving)
- Back-foot sessions repeat the same cues: "Get in line with the ball. Check your shoulder. Open body shape. Receive on the back foot." They also ask for "a good, positive touch into space or out of pressure" and a "cushion touch to maintain possession". — search summary of [Sport Session Planner back-foot sessions](https://www.sportsessionplanner.com/s/xtQab/Receiving-On-The-Back-Foot.html)
- Scanning before receiving lets the receiver choose which direction to push the first touch. In a passing task, players who made more exploratory head movements before receiving responded with a passing action more quickly. — [Sportplan, first touch under pressure](https://www.sportplan.net/drills/Football/trends/first-touch-under-pressure-control-2025-04.jsp); [McGuckian 2019 thesis, ACU](https://acuresearchbank.acu.edu.au/download/860949a0e37b8d50cba781fec6988689fef0f99f7d2441929e1786ee9232f3ca/9804197/MCGUCKIAN_2019_An_examination_of_the_visual_exploratory.pdf)
- FIFA examples of balls in behind include poor first touches that were then corrected, finishes on the second touch, and first-time finishes. A first-time finish is a legitimate outcome of a well-weighted through ball. — [FIFA Training Centre](https://fifatrainingcentre.com/en/game/game-insights/balls-in-behind-the-defence.php)
- Running with the ball costs speed: in U16 players, linear sprint performance fell by **28%** while dribbling. Sprint times over 6 × 30 m were significantly slower with the ball than without it. — search summary of [JHK, Effects of Dribbling Constraints on Sprint Acceleration](https://jhk.termedia.pl/Effects-of-Dribbling-Constraints-on-Sprint-Acceleration-Performance-and-the-Force,204820,0,2.html) and [LIDA record](https://lida.sport-iat.de/ta/Record/4028189)

### Inferences (derived)
- **First-touch distance in a sim, by situation:**
  - Under pressure or a "keep it" touch: about 0.3–1 m.
  - Standing directional touch into space: about 1–2 m.
  - Sprinting onto a through ball: about 2–4 m ahead, so the ball stays ahead of the stride and the runner reaches it again within about 2–4 strides.

  These ranges come from the coaching rule "push it further to accelerate" combined with a 28% speed loss when dribbling. They are not measured values.
- **A ball "pushed ahead" while sprinting:** after the touch, the ball should roll at about runner speed or slightly faster (about 6–8 m/s), so the runner neither overruns it nor loses it.
- **"Attack the ball"** reduces interception time: each metre the receiver moves toward the ball cuts about 1/v_ball seconds from the defender's window.
- **Back foot and open body** means the first touch can go in the forward (play) direction without a turn. In a sim, a receiver who is set up well should be allowed a directional touch of up to about 90° from the ball's incoming direction with little time cost. A receiver facing the wrong way needs an extra touch or a turn. This is my proposal, not sourced.

### Gaps
- No biomechanics study found that reports first-touch ball displacement (cm or m), or post-touch ball speed, for standing versus running receptions.
- No measured success rates for controlling a ball as a function of incoming speed.

---

## 4. Player speeds, reaction times and interception maths

### Takeaway
Pitch-control models, the field's standard interception maths, use these parameters:
- Player top speed about 5 m/s.
- Reaction time about 0.7 s.
- Ball speed about 15 m/s.
- Logistic arrival-time uncertainty σ ≈ 0.45 s.
- Ball control rate λ ≈ 4.3 /s.

Real elite top sprint speeds are about 10 m/s. In tracking data, the top 1% of speeds exceed about 7 m/s.

### Cited Findings
- Friends-of-Tracking pitch-control defaults, quoted from code:
  - `max_player_accel = 7.` m/s² (not used).
  - `max_player_speed = 5.` m/s.
  - `reaction_time = 0.7` s, "time taken for player to react and change trajectory, roughly determined as vmax/amax".
  - `tti_sigma = 0.45` s, the standard deviation of the arrival-time sigmoid in Spearman 2018.
  - `lambda_att = lambda_def = 4.3` /s ball-control rate; Spearman's paper uses kappa_def = 1.72, giving defenders an advantage.
  - `lambda_gk = 3 × lambda_def`, because goalkeepers can catch.
  - `average_ball_speed = 15.` m/s.
  - Integration dt = 0.04 s.
  - [Metrica_PitchControl.py](https://raw.githubusercontent.com/Friends-of-Tracking-Data-FoTD/LaurieOnTracking/master/Metrica_PitchControl.py)
- Spearman et al. 2017, "Physics-Based Modeling of Pass Probabilities" (MIT Sloan):
  - Time-to-intercept is physics-based, wrapped in a statistical model.
  - Arrival-time uncertainty is logistic.
  - Ball control after arrival is exponential with rate λ.
  - Initial ball velocity is estimated by averaging ball movement over 0.4 s.
  - [xG Football Club summary](https://thexgfootballclub.substack.com/p/the-perfect-pass-formula-can-physics)
- Allsvenskan tracking:
  - Top 1% of player accelerations > **4.78 m/s²**; top 0.5% > 5.72 m/s².
  - Top 1% of speeds > **7.12 m/s**; maximum 11.13 m/s, probably including noise.
  - The model used max speed ≈ 10 m/s and max acceleration ≈ 4.98 m/s². The paper swaps the units in the text.
  - [Pfaff et al. arXiv 2210.16474](https://arxiv.org/pdf/2210.16474)
- Premier League (Opta tracking since 2020/21) top recorded speed: **37.38 km/h (10.4 m/s)**. — search summary, [CrunchSports / yardbarker sprint lists](https://www.yardbarker.com/soccer/articles/the_5_fastest_players_in_the_premier_league/s1_17738_44406476). Secondary source; figures vary between outlets.
- Skilled players showed faster and less variable reaction times on a pass-decision foot-pedal task. No absolute ms value was retrieved. — [ScienceDaily 2024 (Osaka Metropolitan Univ.)](https://www.sciencedaily.com/releases/2024/04/240418111823.htm)

### Inferences (derived)
- **Interception test per defender:** the defender intercepts if t_def(P) + reaction < t_ball(P) at some point P along the ball path, where t_def ≈ reaction + distance / v_max (or an acceleration-limited version).
  - With 0.7 s reaction, 5 m/s top speed and a 15 m/s ball, a defender 3 m from the lane needs about 1.3 s.
  - The ball covers about 19 m in that time, so a 10–15 m pass is safe from that defender.
  - In 5-a-side (small pitch, short passes), these margins shrink, so pass weight and pass speed matter more.
- **Pitch-control's 0.7 s reaction** is a model simplification standing in for limited acceleration. Pure perceptual reaction to a pass is shorter. A common sports-science figure is about 0.2–0.3 s, but I did not find a football-specific citation for it (Gaps). A sim could use about 0.2–0.35 s of perception delay plus explicit acceleration of about 4–6 m/s² (the tracking top-1% value is 4.8 m/s²).
- **Runner speed when meeting a through ball:** about 6–8 m/s for good outfielders (top-1% tracking speed > 7.1 m/s), with about 9–10 m/s as an elite peak. Receiving a normal pass while walking or jogging: about 1–4 m/s (no direct source).

### Gaps
- No football-specific measured simple reaction time (ms) to a pass was found in the sources I retrieved.
- I did not access the original Spearman 2017/2018 PDFs directly, so their exact paper parameters (other than κ = 1.72 as quoted in the code comments) are unverified.

---

## 5. Futsal and 5-a-side specifics

### Takeaway
Futsal passes are short: on average about 3.6, 7.5 and 14.5 m for short, medium and long passes. Futsal receiving uses the sole far more than the outdoor game: the sole gives control and stops a low-bounce ball dead on a small court, and players often receive with the foot nearest the pass. Kick speeds are similar to the outdoor game (about 24 m/s maximum).

### Cited Findings
- Pass distances 3.58 / 7.46 / 14.47 m (short / medium / long), pro futsal final. — [SPORT TK, Indonesia Pro Futsal](https://revistas.um.es/sportk/article/view/519451)
- Futsal kicks: about 24 m/s ball speed for both stationary and rolling balls. — [scilit futsal kicking study](https://www.scilit.net/publications/6e94471ff950cbe95f5872b63b606675)
- Futsal players use the sole far more than outdoor players. On the smaller court it "provides more control and less bounce". Players often receive with the foot closest to the pass, not the far-side inside foot taught outdoors. Players should scan 2–3 options before receiving. — search summary of [Sport Session Planner futsal plans](https://www.sportsessionplanner.com/s/h6GW/Recieving-With-The-Sole.html) and [5-a-side.com sole skills](https://www.5-a-side.com/skills/sole)
- FA Bootroom on the sole:
  - Lets players stay on the ball for the right amount of time.
  - Helps them shield (drag, turn, roll).
  - Lets them stop and start, change direction or speed, and move from a standstill (360° drag).
  - Sets up the next pass, shot or turn.
  - Disguises intent with late small movements.
  - Can "slightly slow the tempo".
  - [The FA Bootroom](https://thefa.com/bootroom/resources/coaching/how-using-the-sole-of-the-foot-can-benefit-players)
- Futsal pass velocity is coupled to defender closing angles: faster passes when the lane is closing. — [Corrêa et al. (futsalfocus PDF)](https://www.futsalfocus.net/wp-content/uploads/2022/02/Spatiotemporal-coordination-behaviors-in-futsal.pdf)

### Inferences (derived)
- In a 5-a-side sim, most passes should be 3–10 m with flight times of about 0.4–1.0 s. Through balls should be about 8–20 m.
- A "sole stop" reception can zero the ball's velocity (a dead stop) at a small time cost. A "directional" reception redirects the ball about 0.5–2 m. These are design suggestions from the coaching descriptions, not measured values.

### Gaps
- No measured futsal or 5-a-side pass-speed distribution was found. The futsal ball (size 4, low bounce) and court surfaces differ from grass, and I found no roll-deceleration standard for futsal courts.
