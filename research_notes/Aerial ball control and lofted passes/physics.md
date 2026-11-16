# Aerial ball control and lofted passes: physics and biomechanics for sim parameters

Scope: restitution and rebound for ball-body contacts (foot, thigh, chest, head), which body part is used at which height, header speeds and redirection, lofted-pass ballistics (drag crisis, backspin), volleys. Numbers are tagged **[measured]** (from a cited study), **[derived]** (my arithmetic on cited numbers), or **[estimate]** (my judgement, no direct source).

Reference ball: mass 0.42–0.45 kg, diameter about 0.218 m, cross-section A ≈ 0.0375 m², air ρ = 1.2 kg/m³, so β = ρA/2m ≈ 0.053 m⁻¹ — [Goff & Carré 2009, AJP 77(11)](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)

---

## Q1. Restitution and rebound speed for ball-body contacts (cushioned vs. firm)

### Takeaway
Firm, passive contacts sit around e ≈ 0.7–0.8: the head, or the ball against a rigid surface. A deliberately swung contact adds the body segment's speed. The header and kick formulas below reproduce the measured data well with "impact mass" values of about 10 kg for the head-torso and roughly 2.4 kg for the foot-shank (derived). I found **no study that measures the effective restitution of a cushioned chest, thigh or foot trap**. Those values below are physics-based estimates.

### Cited Findings
- **Ball vs. rigid steel plate (FIFA Quality Pro):** the ball is dropped from 2 m onto a steel plate 10 times. Rebound must be **135–155 cm**, with at most 10 cm between the highest and lowest bounce **[measured standard; secondary source: retailer listing]** — [Decathlon IE, FIFA Pro ball spec](https://decathlon.ie/p/12895-327588-thermobonded-size-5-football-f900-fifa-pro-white-green-blue.html). That gives e = √(h/2) ≈ **0.82–0.88** [derived]. I did not retrieve FIFA's own test manual.
- **Ball vs. artificial turf (FIFA Quality Programme):** vertical rebound from 2 m is **0.6–1.0 m** for "Quality" and **0.6–0.85 m** for "Pro". One sample product measured 0.83 m dry and 0.79 m wet — [test report via mtender.gov.md](https://storage.mtender.gov.md/get/f5b17e67-1911-4e6c-a655-7abb4d27b3ff-1736150790509). That gives an effective e of about **0.55–0.71** [derived]. An older FA handbook lists **angle rebound 45–70 % dry (45–80 % wet)** and **ball roll 4–10 m** (outdated edition) — [FA AGP conditions 2007/08](https://www.thefa.com/GetIntoFootball/Facilities/~/media/Files/PDF/Get%20into%20Football/AGP_Conditions_of_use_in_FA_Comps_2007_08.ashx)
- **Head (passive / standing header):** the ball's e against a force plate standing in for the skull was **0.70 ± 0.11**, against earlier reports of **0.8** (Townend 1988; Schneider & Zernicke 1988). The head-torso **impact mass was 9.94 ± 1.5 kg**, against a head mass of 6.4 kg, because neck and torso muscle bracing adds mass. Ball 3.8 m/s in, head moving 2.23 m/s toward the ball, ball **6.42 m/s out** **[measured]** — [Kristensen & Terp, ISBS 2001](https://ojs.ub.uni-konstanz.de/cpa/article/view/3900/3617)
- **Foot (maximal instep kick):** foot speed **18.6 m/s** (preferred) / 17.0 m/s (non-preferred). Ball speed **24.7 ± 2.5 m/s** (range 20.3–28.4) / 21.5 ± 2.0 m/s. Foot-ball coefficient of restitution (as defined in Dörge's momentum model) **0.58** (0.46–0.68) / 0.50 (0.32–0.66) **[measured]** — [Dörge et al. 2002, J Sports Sci](https://people.stfx.ca/smackenz/Courses/HK474/Midterm/Dorge%202002%20Biomechanical%20differences%20in%20soccer%20kicking%20with%20the%20preferred%20and%20non-preferred%20leg.pdf). Ball speed / foot speed ≈ **1.33** [derived].
- **Foot COR falls as impact speed rises** (more energy is lost at harder impacts) — [Andersen, Kristensen & Sørensen 2008, via search summary](https://www.jstage.jst.go.jp/article/jssfenfs/5/0/5_45/_article/-char/en). A soccer ball struck by a foot at 16.7 m/s gave COR 0.60 (futsal ball 0.51) — [ISJS 2020 (secondary)](https://doaj.org/article/7de653052ac342b9a0b5b376a93798ad)
- **Absorbent layers reduce e:** a gel layer between ball and impact surface cut COR by **21.7 %** while lowering peak force through a longer contact time — [Taha et al. 2013](https://libmast.utm.my/Record/UMPir5533/Description). This is the same mechanism as cushioning with soft tissue or by withdrawing the body part.
- **Coaching description of cushioning:** thigh: lower the thigh as the ball arrives so it drops within control. Chest: pull the chest back at contact or arch slightly backwards — [soccerxpert chest traps](https://www.soccerxpert.com/tips/details/chest-traps); [MOJO thigh](https://mojo.sport/coachs-corner/how-to-receive-the-ball-with-your-thigh); [MOJO chest](https://mojo.sport/coachs-corner/how-to-receive-a-soccer-ball-with-your-chest)

### Inferences (sim model and parameters)
- **Unified contact model (rigid-body impulse, verified against the data above):** for incoming ball speed u along the contact normal (toward the body), body-segment normal speed v_b (positive toward the ball), impact mass M, ball mass m and restitution e:
  `v_out_n = ((e·M − m)·u + (1+e)·M·v_b) / (M + m)`.
  Check against Kristensen: M = 9.94, m = 0.43, e = 0.7, u = 3.80, v_b = 2.23 gives 6.0 m/s (measured 6.42) [derived]. Cushioning means **v_b < 0** (segment withdrawing) and/or a lower e. Setting v_b = −e·M·u/((1+e)·M) gives v_out = 0 (a "dead" trap).
- **Suggested effective normal restitution, e_eff = |v_out_n| / u [estimate unless noted]:**

  | Contact | Firm / passive | Cushioned (good technique) | Notes |
  |---|---|---|---|
  | Ground (grass/turf) | 0.55–0.71 [derived from FIFA turf] | — | tangential: ball keeps ~45–70 % of speed on angled bounce [old FA spec] |
  | Instep / laces, cushioned "take down" | ~0.5 (passive foot) | **0.05–0.20** | foot withdrawn; ball should die within ~0.5–1 m [estimate] |
  | Sole trap (ball pinned against ground) | — | **~0** normal; tangential speed mostly killed | treat as "stop" if ball is near ground at contact [estimate] |
  | Inside-of-foot (volley-cushion) | ~0.5 | 0.10–0.25 | [estimate] |
  | Thigh | ~0.5–0.6 (muscle, ~3–5 kg effective) | **0.10–0.25** | ball should pop up ≤ 0.3–0.5 m and drop at feet [estimate] |
  | Chest | ~0.5–0.6 | **0.10–0.30** | see Q4 for direction [estimate] |
  | Forehead | **0.7 ± 0.11** [measured] | 0.2–0.4 (cushioned header to feet, rare) | impact mass ~10 kg [measured] |

- **Tangential component:** for all body contacts use a friction-limited tangential impulse (Coulomb μ ≈ 0.4–0.6 for leather/PU on skin/fabric [estimate]). At glancing incidence the ball slides, so most of the tangential speed is kept. Rod Cross's general bounce model separates normal e_y from tangential e_x; a ball slides throughout the bounce only at incidence angles of about ≤ 20° to the surface, and otherwise grips — [Cross, Physics of Bounce](https://www.physics.usyd.edu.au/~cross/BOUNCE.htm); [Cross, Physics Today 2008](https://physicstoday.aip.org/article/61/9/84/413456)
- **Error and skill:** the trap's control radius (distance the ball runs after the touch) should scale with incoming speed × e_eff × (1 − skill). With u = 15 m/s and e_eff = 0.15 the ball leaves at about 2.3 m/s, which is roughly the "loose first touch" boundary [estimate].

### Gaps
- I found no published measurement of rebound speed or effective restitution for chest, thigh or cushioned-foot traps. All cushioned values above are estimates from impulse physics and coaching descriptions.
- I did not retrieve exact grass rebound values (natural turf varies widely), or the current FIFA Test Manual angle-rebound limits.

---

## Q2. Which body part by ball height; setup time

### Takeaway
Coaching literature is qualitative: **ball height at arrival decides the surface**. The thigh is for a dropping ball below chest height, the chest for a dropping or driven ball above the abdomen, the head for a ball above the shoulders, and the instep, inside of the foot or sole for low and dropping balls. Numeric bands have to come from body proportions.

### Cited Findings
- "The instep, thigh, chest, and head are the common surfaces, and the height of the ball as it reaches the player usually decides which one is used". Thigh: a dropping ball coming in below chest height. Chest: a dropping or driven ball above the abdomen. Chest drills are run between waist and head height — [search-summarised coaching sources: Soccer Coach Weekly](https://soccercoachweekly.net/coaching-advice/what-is-receiving); [MOJO thigh](https://mojo.sport/coachs-corner/how-to-receive-the-ball-with-your-thigh); [soccerxpert](https://www.soccerxpert.com/tips/details/chest-traps); [FA Boot Room receiving](https://www.thefa.com/bootroom/resources/coaching/how-to-help-players-improve-their-receiving-skills)
- The choice also depends on trajectory (steeply dropping vs. driven) and on nearby opponents — same sources.

### Inferences (height bands for a 1.80 m player, ball-centre height at contact) [estimate, scale with player height H]
| Band (ball centre) | Fraction of H | Surface | Notes |
|---|---|---|---|
| 0–0.25 m | 0–0.14 H | sole / inside foot (ground or half-volley) | ball dropping onto ground near the foot |
| 0.25–0.55 m | 0.14–0.30 H | instep / inside-foot cushion, or volley | knee height is about 0.28 H |
| 0.55–1.0 m | 0.30–0.55 H | thigh | best for a *dropping* ball (descent angle > ~30°) |
| 1.0–1.45 m | 0.55–0.80 H | chest | driven or dropping, ball above the abdomen |
| 1.45–1.6 m | 0.80–0.88 H | awkward gap: shoulder; use chest with a lean-back or head with a duck | prefer head if the player is moving forward |
| > 1.6 m (standing), to ~2.4–2.6 m (jumping) | > 0.88 H | head | jump reach adds 0.4–0.7 m [estimate] |

- The segment fractions are standard anthropometric proportions as I recall them (Drillis & Contini, as tabulated in Winter's *Biomechanics and Motor Control of Human Movement*): knee ≈ 0.285 H, hip ≈ 0.53 H, shoulder ≈ 0.82 H, eye ≈ 0.94 H. **These come from memory and I did not re-verify them this session.**
- **Setup time [estimate]:** chest and thigh control need the body square to the ball and set about 0.3–0.5 s before contact (a step to line up plus withdrawal timing). A volley or one-touch header needs about 0.2–0.3 s. A sole trap needs about 0.2 s. When the time available is shorter, the sim should raise e_eff and the error (a heavier touch). These figures are not sourced.

### Gaps
- I found no biomechanics study that quantifies surface-choice thresholds or the preparation time for chest and thigh control.

---

## Q3. Header speeds and redirection

### Takeaway
Measured lab headers add about **1.5–3 m/s** to an 8 m/s ball (skilled players add more), giving exit speeds of about **9.5–11 m/s**. With an impact mass of about 10 kg and e ≈ 0.7, the exit speed is ≈ 0.63·u_in + 1.63·v_head. A powerful jumping header off a 13 m/s cross therefore reaches about 15–19 m/s. Header passes are about 6–12 m/s.

### Cited Findings
- **Standing headers, 8 m/s delivery (24 female collegiate players):** frequent headers added **3.00 ± 0.93 m/s** to ball speed; infrequent headers added **1.52 ± 0.43 m/s**. Every participant added speed. Trunk angular velocity at contact was **282 vs. 147 deg/s**, neck about 396 vs. 187 deg/s, trunk range of motion 27° vs. 17°. Trunk velocity was the only significant predictor of the speed added (R² = 0.58) **[measured]** — [Ludwig et al., ISBS 2001](https://ojs.ub.uni-konstanz.de/cpa/article/view/3901/3618)
- **Low-speed header:** ball in 3.8 m/s, out 6.4 m/s, head 2.2 m/s, impact mass about 10 kg, e = 0.7 **[measured]** — [Kristensen & Terp 2001](https://ojs.ub.uni-konstanz.de/cpa/article/view/3900/3617)
- **Hand-served header:** in 4.29 ± 0.74 m/s, returned **5.48 ± 1.18 m/s** **[measured]** — [Dezman et al., PMC3899908](https://pmc.ncbi.nlm.nih.gov/articles/PMC3899908)
- **Shewchenko et al. 2005 (BJSM):** headers tested at ball speeds of **6 and 8 m/s**. Closing speed, including head speed, reached about **11 m/s** — [Shewchenko Part 1, PMC1765311](https://pmc.ncbi.nlm.nih.gov/articles/PMC1765311); Part 3 tested 6–7 m/s — [BJSM 39 suppl 1 i33](https://bjsm.bmj.com/content/39/suppl_1/i33)
- **Jumping headers** with ball-cannon delivery at about **13.1 m/s**: "powerful heading" produced a resultant ball speed after impact higher than before impact (exact exit speeds not retrieved) — [ISBS, via search summary](https://ojs.ub.uni-konstanz.de/cpa/article/view/726/648)
- In match play the most common header type is a pass (38.7 %), and the most common delivery before a header is a pass (47.4 %) — [Heads Up! iMG pilot, Applied Sciences 2023](https://mdpi.com/2076-3417/13/4/2639)

### Inferences (sim parameters)
- **Header exit speed:** `v_out ≈ 0.63·u_n + 1.63·v_head` (M = 10 kg, e = 0.7, from the formula in Q1) [derived]. v_head ranges [estimate, consistent with the Ludwig and Kristensen data]:
  - Cushioned / knock-down header (to own feet or a nearby teammate): v_head ≈ −1…0 m/s, e ≈ 0.4. Exit ≈ 0.2–0.5·u_in, so 2–5 m/s.
  - Header pass / flick-on: v_head 1–2.5 m/s. For a 10–15 m/s ball, exit ≈ **8–13 m/s**.
  - Defensive clearance (jump, full trunk snap): v_head 3–5 m/s. For a 15–20 m/s long ball, exit ≈ **15–20 m/s**, with an upward launch of 30–50°.
  - Headed shot off a cross: v_head 3–5 m/s, u_n 12–16 m/s, so exit ≈ **13–18 m/s** (well below kicked shots of 20–30 m/s).
- **Redirection:** only the normal component is reflected and amplified. The tangential component is mostly kept (sliding, low friction). The maximum useful deflection with full power is therefore when the ball meets the forehead nearly head-on (deflection up to ~150–180°, as in a clearance). A **glancing flick-on** (contact normal 60–80° off the ball's path) changes direction by about 10–40° and keeps 80–95 % of the speed [estimate from the impulse model]. A diving or side header redirects 45–90° with a moderate speed loss. Header aim error should grow with incoming speed and with the deflection angle requested.
- The lab speeds (3.6–13 m/s) are below match long-ball speeds. Extrapolating the linear model to 20 m/s inputs is an estimate.

### Gaps
- I found no study reporting **match** header exit speeds, or a measured maximum header deflection angle versus speed retained.

---

## Q4. How a chest trap drops the ball

### Takeaway
I found no measured data. Coaching says to arch back and withdraw at contact so the ball pops up slightly and drops at the feet, or angle the chest to direct it to one side.

### Cited Findings
- Chest technique: arch slightly back and pull the chest back as the ball is touched — [soccerxpert](https://www.soccerxpert.com/tips/details/chest-traps); [MOJO chest](https://mojo.sport/coachs-corner/how-to-receive-a-soccer-ball-with-your-chest)

### Inferences [estimate]
- **Chest contact normal:** tilted 20–40° upward from horizontal for a dropping ball (the player leans back), and close to horizontal for a driven ball (then the player "sinks" the chest, giving a downward-forward normal).
- **Post-contact ball:** speed 0.1–0.3 × incoming (e_eff), so 1.5–4 m/s for a 12–15 m/s incoming ball. Direction: up 30–60° and forward 0.3–1.0 m from the chest. The ball lands about 0.4–0.6 s later about 0.5–1.2 m in front, which is the window for the next touch (volley or ground control).
- A directional chest (turning the torso 20–40°) rotates the horizontal component of the normal by the same angle.

### Gaps
- No measured chest-trap rebound speeds or angles.

---

## Q5. Lofted pass ballistics: speed, angle, apex, flight time, drag and backspin

### Takeaway
A soccer ball goes through its **drag crisis at about 10–14 m/s** (Re ≈ 1.4–3 × 10⁵). Above that, C_D ≈ 0.15–0.25; below it, C_D ≈ 0.4–0.5. Lofted passes launch at 15–30 m/s, so they start in low drag and decelerate toward high drag near landing. The result is a range shorter than the vacuum range, a later and steeper descent, and a landing speed of about 70–80 % of launch speed. Backspin adds lift (C_L up to ~0.3), which flattens and lengthens the flight. The descent is about 25° for a 20° launch and about 45–55° for a 40° launch.

### Cited Findings
- **Drag crisis:** wind tunnels show the transition at **Re ≈ 1.4 × 10⁵ (≈ 9.9 m/s)** in one study and **> 2 × 10⁵ (≈ 14 m/s)** in another. C_D drops by about a **factor of 2** through the transition. A no-spin ball launched at **18 m/s, 22°** matched **C_D = 0.2** (range 21.9 m). A maximum-speed launch gave **C_D = 0.17** for Re 2.23–2.76 × 10⁵. Spinning balls with Sp 0.25–0.49 gave **C_D ≈ 0.27–0.30** and **C_L ≈ 0.27–0.31** (a lift plateau with spin). Spin decays only **4–8 % by the apex** (about 10 % by landing). Game speeds are roughly **4.5–31 m/s** (Re 0.7–4.9 × 10⁵) **[measured]** — [Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)
- **Equations of motion:** `a = β v² [−C_D v̂ + C_L l̂ + C_S (l̂ × v̂)] + g`, with β ≈ 0.053 m⁻¹, spin parameter Sp = rω/v — [Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)
- **Free-kick example:** 36 m/s launch with ω ≈ 63 rad/s (Sp 0.19) slowed to **19 m/s** over 27 m. Sidespin coefficient C_S ≈ 0.2 — [Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)
- **Critical Re by ball:** Asai et al. 2007 measured **2.2–3.0 × 10⁵**. Newer 8-panel balls measure 2.2–3.3 × 10⁵, with Roteiro around 2.2 × 10⁵ and C_D ≈ 0.12 above it. The 2026 Trionda has the lowest critical Re (≈ 1.78 × 10⁵), but supercritical **C_D ≈ 0.22** — [Goff, Physics Today "Power and spin"](https://physicstoday.aip.org/quick-study/power-and-spin-in-the-beautiful-game); [Sci. Rep. 2026](https://link.springer.com/10.1038/s41598-026-56275-1); [Asai/Hong, SpringerPlus](https://link.springer.com/doi/10.1186/2193-1801-2-171). The Physics Today piece notes the crisis starts at about **12 m/s**, near medium-pass speed.
- **Backspin on long throws (a proxy for lofted balls):** throw distance rose about **0.6 m per +1 rev/s** of backspin. Observed backspin was 0.4–1.3 rev/s. The optimum release angle falls from about 40° (no spin) to about 35° (2 rev/s) — [Linthorne & Thomas 2016, Brunel](https://bura.brunel.ac.uk/handle/2438/12719). Long throw-ins: **12–19 m/s at 22–40°**; mean 13.4 m/s at 32.1° — [review citing Bray & Kerwin; Linthorne & Everett](https://digitum.um.es/digitum/bitstream/10201/151565/1/586031.pdf)
- **Kick spin:** spin rates of about **22.6 and 36.4 rad/s** (≈ 3.6 and 5.8 rev/s) for two kick types, with a strong negative correlation between ball speed and spin (r = −0.81) — [search summary of ISBS kick study](https://ojs.ub.uni-konstanz.de/cpa/article/view/4446/4135) (not verified in full text)
- **Max instep kick speed:** 18–35 m/s depending on conditions; lower when players aim for accuracy — [review via JSSM](https://jssm.org/jssm-06-154.xml-abst); 24.7 m/s mean in Dörge 2002 (above)

### Inferences: computed trajectory table [derived from a simulation of my own, not measured]
Model: m = 0.43 kg, A = 0.0375 m², ρ = 1.2. C_D = 0.45 below 8 m/s, ramping linearly to 0.22 at 13 m/s and above (smoothed drag crisis). Backspin lift C_L = min(Sp, 0.3). Launch from the ground; "range" is the first-bounce distance. The spin column of 40 rad/s ≈ 6.4 rev/s is a plausible driven or lofted backspin [estimate].

| Range | Launch angle | Backspin (rad/s) | Launch speed (m/s) | Apex (m) | Flight (s) | Landing speed (m/s) | Descent angle | Vacuum launch speed (m/s) |
|---|---|---|---|---|---|---|---|---|
| 15 m | 20° | 0 | 16.1 | 1.4 | 1.08 | 13.7 | 22° | 15.1 |
| 15 m | 30° | 0 | 14.1 | 2.3 | 1.38 | 11.6 | 34° | 13.0 |
| 15 m | 40° | 0 | 13.5 | 3.5 | 1.68 | 10.9 | 46° | 12.2 |
| 20 m | 30° | 0 / 40 | 16.4 / 15.0 | 3.1 / 3.5 | 1.60 / 1.87 | 13.3 / 11.8 | 34° / 37° | 15.1 |
| 25 m | 20° | 0 / 40 | 21.7 / 17.9 | 2.5 / 2.7 | 1.43 / 1.77 | 16.6 / 13.9 | 24° / 25° | 19.5 |
| 25 m | 30° | 0 / 40 | 18.7 / 16.8 | 4.0 / 4.5 | 1.80 / 2.16 | 14.6 / 12.8 | 35° / 38° | 16.8 |
| 25 m | 40° | 0 / 40 | 17.9 / 17.1 | 5.9 / 6.9 | 2.20 / 2.59 | 13.8 / 13.0 | 47° / 50° | 15.8 |
| 30 m | 30° | 0 / 40 | 21.0 / 18.5 | 4.9 / 5.5 | 1.99 / 2.43 | 15.6 / 13.8 | 36° / 39° | 18.4 |
| 40 m | 20° | 0 / 40 | 29.3 / 23.2 | 4.2 / 4.7 | 1.86 / 2.43 | 19.1 / 15.6 | 26° / 29° | 24.7 |
| 40 m | 30° | 0 / 40 | 25.4 / 21.8 | 6.8 / 7.7 | 2.34 / 2.94 | 17.2 / 15.2 | 38° / 40° | 21.3 |
| 40 m | 40° | 0 / 40 | 24.2 / 22.8 | 9.9 / 12.3 | 2.83 / 3.55 | 16.8 / 15.9 | 49° / 53° | 20.0 |
| 50 m | 30° | 0 / 40 | 29.9 / 25.2 | 8.8 / 10.2 | 2.67 / 3.44 | 18.4 / 16.4 | 41° / 43° | 23.8 |

Key points for the sim:
- **Launch speed needed:** 15–20 m passes need 13–17 m/s; 25–30 m passes need 17–21 m/s; 40 m switches need 22–29 m/s. Drag adds about 5–20 % over the vacuum speed. Backspin cuts the needed speed by up to about 20 % on flat (20°) launches.
- **Apex:** about 0.15 × range at 30°, about 0.24 × range at 40°, about 0.10 × range at 20°. A driven cross at 15–20° reaches about 1.5–3 m (head height at the far post). A lofted chip at 40–45° reaches 4–10 m.
- **Flight time:** 1.1–1.7 s for 15 m; 1.8–2.6 s for 25 m; 2.3–3.5 s for 40 m. This is the window receivers and defenders have to read the ball.
- **Landing:** landing speed is about 70–85 % of launch speed. The descent angle is 2–10° steeper than the launch angle (drag asymmetry), and backspin steepens it a little more. On landing, use e_ground ≈ 0.55–0.71 normal. Backspin reduces the forward skid and "checks" the ball (the friction impulse opposes the contact-point velocity, per Cross). Topspin or no spin lets it skip on.
- **Fixed-point implementation:** a two-regime C_D (high below ~10 m/s, low above ~13 m/s, with a linear blend), C_L ∝ Sp capped at ~0.3, and spin decay of about 3–5 %/s is enough to reproduce these numbers. Precompute β·C_D and β·C_L tables as Q16.16 values indexed by speed.

### Gaps
- I found no match-tracking study reporting launch speed, angle and spin distributions for real crosses or long passes. The table is model output, anchored on Goff & Carré's measured coefficients, the throw-in data and kick speeds.
- Backspin rates on chipped or lofted passes are not directly measured in anything I found. The 3.6–5.8 rev/s kick figures come from shots, not lofts.

---

## Q6. Volleys vs. ground shots

### Takeaway
I found no study comparing volley ball speed with instep ground-kick speed. Physics suggests the volley exit speed is the instep kick speed plus a small contribution from the incoming ball's speed. Sim volleys should therefore be about as fast as ground shots when cleanly struck, but with a much larger error.

### Cited Findings
- Ball speed is driven mainly by foot speed at impact — [Theoretical study, lida.sport-iat.de](https://lida.sport-iat.de/dfb/Record/4026359?lng=en). Knee extension velocity is the strongest kinematic contributor (adj. R² 0.39) — [Sinclair et al., Herts](https://uhra.herts.ac.uk/handle/2299/16219). Maximal instep: 18–35 m/s — [JSSM review](https://jssm.org/jssm-06-154.xml-abst)
- Instep kick: foot 18.6 m/s gives ball 24.7 m/s, COR 0.58 — [Dörge 2002](https://people.stfx.ca/smackenz/Courses/HK474/Midterm/Dorge%202002%20Biomechanical%20differences%20in%20soccer%20kicking%20with%20the%20preferred%20and%20non-preferred%20leg.pdf)

### Inferences [estimate]
- Back-calculating from Dörge's ratio of 1.33 with e ≈ 0.58 and m = 0.45 gives an effective foot impact mass M ≈ 2.4 kg in the Q1 impulse formula [derived; Dörge's own e definition differs slightly, so treat M as approximate]. Then v_out ≈ 0.36·u_in + 1.36·v_foot. An incoming 10 m/s ball meeting the foot head-on therefore adds about 3–4 m/s. In practice the incoming ball rarely meets the foot along the strike direction, so the gain is smaller.
- Volleys usually come with lower foot speed (balance, timing, side-on technique), so in practice they are **about equal to or 0–15 % slower** than a set ground shot. The aim error should be about 1.5–3× larger, and the vertical error grows sharply when the ball meets the foot above knee height (contact below the ball's centre sends it over the bar).
- Half-volleys (struck just after the bounce) are close to ground-shot speed, with medium error.

### Gaps
- No direct measurement of volley ball speed or accuracy versus ground shots.
