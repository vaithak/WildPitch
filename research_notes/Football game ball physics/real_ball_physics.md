# Real-world physics of a size-5 football, for a game model

Reference constants used throughout. Values are from FIFA rules and the cited papers. Derived numbers are marked as inference.
- m ≈ 0.43 kg (Goff & Carré measured 0.424 kg), r ≈ 0.11 m, D ≈ 0.218–0.22 m. Cross-section A = πr² ≈ 0.0380 m² (Goff & Carré use 0.0375). ρ_air = 1.2 kg/m³, ν ≈ 1.54×10⁻⁵ m²/s — [Goff & Carré, AJP 77 (2009) 1020](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)
- Handy conversion: Re×10⁻⁵ ≈ v / (7 m/s). Match play spans about 4.5–31 m/s, or Re ≈ 70,000–490,000 — [Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)
- Aero constant: K = ½ρA/m = 0.0530 m⁻¹. The drag constant is then k = K·C_D: **k = 0.0106 m⁻¹ at C_D = 0.20, 0.0133 at 0.25, 0.0239 at 0.45, 0.0265 at 0.50** (inference, my arithmetic).

---

## 1. Aerodynamics: drag vs Re and the drag crisis, Magnus/lift vs spin parameter, spin rates, spin decay

### Takeaway
Use these equations of motion:
- **a = −K·C_D·|v|·v + K·C_L·|v|²·(ω̂×v̂) + g**
- C_D: about 0.45–0.55 below the critical speed. It drops to about 0.14–0.20 above it, with a slight rise at high speed.
- Critical speed: about 12.5–13 m/s for 32-panel, Teamgeist and Brazuca balls; about 18.7 m/s for the Jabulani.
- C_L: about 1.15·Sp^0.83 in the turbulent regime (Sp = rω/v). It tends to plateau near 0.3 for Sp ≥ 0.25 and collapses toward 0 once the flow is laminar.
- A curling free kick: about 36 m/s, ω ≈ 63 rad/s, Sp ≈ 0.19.
- Spin decays slowly, about 4–8 % by the apex of a ~1 s flight.

### Cited Findings
**Force model**
- Drag F_D = ½C_D ρ A v², acting opposite to v. Lift (Magnus) F_L = ½C_L ρ A v², acting along ω̂×v̂. With β = ρA/(2m), Goff & Carré write the vector form a = βv²[−C_D v̂ + C_L l̂ + C_S(l̂×v̂)] + g. For a pure top- or backspin launch C_S = 0; for pure sidespin C_L = 0 — [Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)
- Spin parameter Sp = rω/v, the equatorial surface speed divided by the centre-of-mass speed — [Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf); [Kiratidis & Leinweber, arXiv:1710.02784](https://arxiv.org/pdf/1710.02784)

**Drag crisis (non-spinning)**
- In the turbulent (high-speed) regime the boundary layer separates late and drag is low. Below the critical speed the flow becomes laminar, separates early, and drag is high. Roughness (seams) lowers the critical speed but raises supercritical drag — [Kiratidis & Leinweber](https://arxiv.org/pdf/1710.02784)
- Kiratidis & Leinweber fitted wind-tunnel data from other groups (their ref. [8], Asai/Tsukuba-style data) with this formula:
  - **C_D(v) = (a − b_min)/(1+exp((v−v_c)/v_s)) + b_min + (v−v_min)/(1+exp(−(v−v_min)/v_s)) · (b_max−b_min)/(v_max−v_min)**
  - Fitted parameters (a, v_c [m/s], v_s, b_min, b_max, v_min, v_max) — [arXiv:1710.02784, Table 1](https://arxiv.org/pdf/1710.02784):

    | Ball | a | v_c | v_s | b_min | b_max | v_min | v_max |
    |---|---|---|---|---|---|---|---|
    | Tango 12 (32-panel) | 0.5452 | 12.86 | 1.304 | 0.1657 | 0.1953 | 16.22 | 35 |
    | Teamgeist | 0.4927 | 12.58 | 1.071 | 0.1440 | 0.1540 | 23.17 | 35 |
    | Jabulani | 0.4839 | **18.69** | 1.377 | 0.1413 | 0.1780 | 23.29 | 35 |
    | Brazuca | 0.4740 | 12.92 | 1.000 | 0.1657 | 0.2112 | 14.61 | 35 |
- The "unpredictable" (knuckle-prone) speed band is about **15–24 m/s for the Jabulani** and **about 10–17 m/s for the others**. Shots leave the boot at up to about 35 m/s and reach the goal at about 15–20 m/s, so the Jabulani was erratic at exactly the speeds keepers face — [Kiratidis & Leinweber](https://arxiv.org/pdf/1710.02784)
- Seam geometry (their ref. [30]):

  | Ball | Total seam length | Seam depth |
  |---|---|---|
  | Jabulani | ~203 cm | ~0.48 mm |
  | Brazuca | ~327 cm | ~1.56 mm |
  | Teamgeist | ~345 cm | not stated |
  | Conventional 32-panel | ~400 cm | ~1.08 mm |

  Brazuca and 32-panel balls are therefore expected to behave alike — [Kiratidis & Leinweber](https://arxiv.org/pdf/1710.02784)
- Sources conflict on the transition point of a standard 32-panel ball. Goff & Carré cite two wind-tunnel studies:
  - one puts the laminar→turbulent transition at **Re ≈ 1.4×10⁵ (≈9.9 m/s)**;
  - another puts it **slightly above Re = 2×10⁵ (≈14 m/s)**.

  Through the transition, C_D drops by about a factor of 2 — [Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)
- Trajectory analysis (Goff & Carré, 32-panel stitched ball, no spin):
  - launch ≈18 m/s at 22°, range 21.9 m, best fit **C_D = 0.2**;
  - C_D = 0.1 or 0.3 would misplace the range by about 1.5 m;
  - a ≈19.4 m/s launch gave **C_D = 0.17**;
  - for comparison, the computed range with C_D = 0 is about 25.5 m and with C_D = 0.5 about 18.3 m.

  [Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)
- Hong & Asai (Sci. Rep. 4:5068, 2014) tested five balls by wind tunnel and kick robot: Brazuca, Cafusa, Jabulani, Teamgeist 2 and a conventional ball. Snippet-level information only (the full text returned HTTP 503):
  - the size of the drag drop at the crisis was smallest for the Brazuca;
  - the Jabulani's drag varied strongly with panel orientation at Re 3–5×10⁵.

  [Hong & Asai 2014 (repository PDF)](https://tsukuba.repo.nii.ac.jp/record/31038/files/SR_4-5068.pdf). The Brazuca's critical speed is lower than the Jabulani's, and its supercritical C_D is higher — [Tsukuba JSET paper](https://tsukuba.repo.nii.ac.jp/record/32041/files/JSET_228-3.pdf) (search-snippet level).

**Spin effect on drag**
- Teamgeist wind-tunnel data in the turbulent regime fit **C_D(Sp) = 0.144 + 0.514·Sp**. The general model is C_D(Re,Sp) = C_D|Sp=0(v) + b·Sp, with b up to 5 % larger for rougher balls — [Kiratidis & Leinweber](https://arxiv.org/pdf/1710.02784)
- "A typical spin parameter for a swerving free-kick is approximately 0.2 rad, while spin parameters above 0.4 for flight trajectories are rare" — [Kiratidis & Leinweber](https://arxiv.org/pdf/1710.02784)

**Lift (Magnus) coefficient**
- Turbulent-regime fit at Re = 333,793 (Teamgeist data): **C_L = 1.15·Sp^0.83**. Reference points: C_L = 0.33 at Sp = 0.31, 0.29 at Sp = 0.19, about 0.14–0.15 at Sp = 0.06 — [Kiratidis & Leinweber](https://arxiv.org/pdf/1710.02784)
- C_L falls as the ball enters the drag crisis and is **near zero at Re = 1.3×10⁵ (laminar)**. Lower supercritical drag goes with higher lift. Their model scales C_L linearly with how far C_D sits between its minimum and its subcritical maximum (eq. 9) — [Kiratidis & Leinweber](https://arxiv.org/pdf/1710.02784)
- Goff & Carré measured topspin launches (launcher, Re ≈ 2.6–3.1×10⁵):

  | Sp | C_D used | \|C_L\| |
  |---|---|---|
  | 0.49 | 0.30 | ≈0.29 |
  | 0.25 | 0.27 | ≈0.30 |
  | 0.71 | 0.30 | ≈0.31 |
  | 0.24 | 0.27 | ≈0.27 |

  They suggest **|C_L| levels off as Sp increases**. Their launcher reached up to about 180 rad/s, though most runs were below 125 rad/s — [Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)
- Goff & Carré also report lift coefficients for Sp 0–1 and Re 1.3–3×10⁵ — [Goff et al., Procedia Eng. 2012 (White Rose)](https://eprints.whiterose.ac.uk/id/eprint/98035/1/1-s2.0-S1877705812016414-main.pdf) (search-snippet level; numbers not extracted)

**Real kick numbers**
- Beckham's free kick vs Greece (2001), from TV footage:
  - left the foot at about **36 m/s** (Re ≈ 5.1×10⁵), 27 m from goal;
  - average **ω ≈ 63 rad/s (Sp ≈ 0.19)**;
  - moved about **3 m laterally**;
  - arrived at about **19 m/s**.

  [Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf). Their back-of-envelope sideways coefficient is C_S ≈ 0.2 — [Physics World](https://physicsworld.com/a/bend-it-like-beckham/)

**Spin decay**
- In Goff & Carré's launches, spin rate fell **about 4–8 % by just past the apex**. They assume about 10 % by landing and a maximum speed drop of about 20 %. They cite prior work that spin decay is roughly exponential — [Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)

### Inferences
- **Magnitudes** (my arithmetic):
  - drag at 30 m/s with C_D ≈ 0.19 → K·C_D·v² ≈ 9 m/s² (about 1 g);
  - drag at 12 m/s, near the subcritical peak → about 3.2 m/s²;
  - Magnus at 25 m/s with C_L = 0.25 → 0.053·0.25·625 ≈ 8.3 m/s² sideways.

  Using the Tango 12 fit, aerodynamic deceleration is about 2.7 m/s² at 10 m/s, 3.1 at 13, 2.7 at 15, 3.7 at 20, 6.0 at 25 and 9.0 at 30. It is **not monotonic** through the crisis. A ball can "hang" at 12–15 m/s, which is a known real effect.
- **Deterministic fixed-point model:** a lookup table of C_D(v) and C_L(Sp, v) over speed is adequate. For example:
  - C_D: 0.50 below ~11 m/s, a ramp to ~0.17 by ~15 m/s, then a slow rise to ~0.20 at 35 m/s;
  - C_L: about min(1.15·Sp^0.83, 0.32) above ~15 m/s, scaled down toward 0 by ~9 m/s.
- **Spin decay:** "4–8 % by apex in about 0.5–0.8 s" implies an exponential time constant of roughly **τ ≈ 7–15 s**, so ω(t) = ω₀·e^(−t/τ). This is inference from one study's numbers; there is no direct measurement.
- **Knuckle effect:** a near-zero-spin ball passing through the critical band gets asymmetric separation that flips with seam orientation. A game can model this as a small random lateral force, applied only when |ω| is small and v is inside the ball's critical band (10–17 m/s for a 32-panel-type ball). Any random draw must come from the sim RNG.

### Gaps
- **Al Rihla (2022)** and **Telstar 18 (2018)** drag-crisis data: no reliable source found in this pass.
- **Hong & Asai 2014** exact C_D and Re_crit values per ball were not extracted (server returned 503). A search snippet said "Re_crit of soccer balls ranged from 2.2×10⁵ to 3.0×10⁵" ([Springer doi 10.1007/bf02844207](https://doi.org/10.1007/bf02844207)), but I did not read the text, so treat it as unverified.
- **Spin rates from snippets only (unverified):**
  - curve kicks about 4–10 rev/s (25–63 rad/s);
  - an Asai in-front curve kick averaging 7.8 rev/s (≈49 rad/s) — [Asai et al., SHU repository](https://shura.shu.ac.uk/2206);
  - knuckle shots 0–2 rev/s, with one Asai study using <12.56 rad/s as its knuckle cutoff — [Tsukuba JFS paper](https://tsukuba.repo.nii.ac.jp/record/25080/files/JFS_27-5.pdf).

  These are consistent with Beckham's measured 63 rad/s, but I did not verify them.
- **Spin decay time constant:** no direct measurement found. The value above is inferred.

---

## 2. Rolling on grass: Coulomb, speed-proportional, or both? Deceleration, roll distances, FIFA ball-roll test

### Takeaway
Measured rolling deceleration on turf is **"constant + linear in speed"**: dec = a + b·v. This is the form the Kolitzus/IST method and FIFA's reduced-roll method use. Air drag adds on top at higher speed.

The FIFA test rolls a ball from a 1 m-high, 45° ramp (it hits the turf at about 3.2 m/s). Required roll distances:

| Certification | Required roll |
|---|---|
| FIFA Quality | 4–10 m |
| FIFA Quality Pro | 4–8 m |
| After wear | up to 12 m |
| UEFA (2004) | 5–8 m |

That means an average deceleration of about **0.5–1.3 m/s²** at walking-pace speeds.

### Cited Findings
**FIFA test setup**
- FIFA Test Manual I (2024), Method 2024-02, ball roll:
  - ramp of two smooth parallel rails;
  - **ramp angle 45°±2**;
  - ball released with its lowest point **1,000±5 mm above the surface**;
  - roll measured from first contact to rest;
  - ball pressure adjusted so it rebounds **1.45±0.03 m on concrete** from a 2 m drop.

  [FIFA Test Manual I](https://www.sporteimpianti.it/wp-content/uploads/2024/04/FIFA-TTM-I-Layout-v8.pdf)
- Reduced ball roll (Method 2024-02a):
  - **timing gates 1.5 m apart** and four release heights, the top one 1.0 m;
  - vend is fitted as a quadratic, **v_end = a·v_start² + b·v_start + c**;
  - this is iterated in 1.5 m steps until v ≤ 0 to predict a full-length roll;
  - the final phase uses the deceleration from the last iteration.

  This is an explicitly speed-dependent deceleration model — [FIFA Test Manual I](https://www.sporteimpianti.it/wp-content/uploads/2024/04/FIFA-TTM-I-Layout-v8.pdf)
- A FIFA lab certificate for one Quality-level product:

  | Test | Requirement | Measured |
  |---|---|---|
  | Reduced ball roll, initial, dry | 4–10 m | 7.9 m |
  | Reduced ball roll, after simulated wear, dry | 4–12 m | 8.7 m |
  | Reduced ball roll, after simulated wear, wet | 4–12 m | 8.9 m |

  [FIFA certificate (Qgrass)](https://qrubber.cl/wp-content/uploads/2022/07/Pasto-Qgrass-50mm-04mt-20mt-QRubber-cert-FIFA_.pdf). A 2021 report lists Pro as 4–8 m (search snippet) — [same search](https://qrubber.cl/wp-content/uploads/2022/07/Pasto-Qgrass-50mm-04mt-20mt-QRubber-cert-FIFA_.pdf). The FIFA natural-pitch rating gives 4.0–10.0 m as its "excellent" ball-roll band (snippet) — [FIFA natural pitch rating](https://digitalhub.fifa.com/m/58aa765dd3e85f26/original/FIFA-natural-pitch-rating-system_EN.pdf)

**Norwegian Building Research Institute (Gabrielsen, ISSS 2004)**
- UEFA manual: release height 1000±5 mm; **UEFA requirement 5–8 m**; wind must be ≤1 m/s — [Gabrielsen, ISSS 2004](https://www.isss-sportsurfacescience.org/downloads/documents/A7ZD60AH55_Gabrielsen_Ball_Roll.pdf.pdf)
- Measured: "From a release height of 1 m … the speed of the ball when striking the surface is **3.2 m/s**" — [Gabrielsen](https://www.isss-sportsurfacescience.org/downloads/documents/A7ZD60AH55_Gabrielsen_Ball_Roll.pdf.pdf)
- **Kolitzus/IST method:** "the deceleration decreases the slower the ball roll." Deceleration from timing gates, dec = (Va² − Vb²)/(2·dist), plotted against mean speed gives "a more or less straight line of form **dec = a + b·v**." Roll distance is then integrated from that line — [Gabrielsen](https://www.isss-sportsurfacescience.org/downloads/documents/A7ZD60AH55_Gabrielsen_Ball_Roll.pdf.pdf)
- UEFA/NBI study, 2003:
  - 6 third-generation turfs, 3 natural grass fields, and 1 lab turf at 3 fill heights;
  - 360 measurements;
  - gates 2 m apart, 0.5 m after the ramp;
  - trendline **BRD = 4.3404·VC^(−0.9789)** (R² = 0.87), where VC is the speed lost between the gates;
  - examples: VC 0.40 → 10.6 m; 0.50 → 8.6 m; 0.60 → 7.2 m; 0.75 → 5.8 m.

  [Gabrielsen](https://www.isss-sportsurfacescience.org/downloads/documents/A7ZD60AH55_Gabrielsen_Ball_Roll.pdf.pdf)
- The older FIFA/EN 12234 velocity-change test (as of 2004):
  - ball at about 2.5 m/s, gates 1 m apart;
  - requirement **ΔV = 0.4–0.75 m/s**;
  - converted via ΔV = −1.434·log₁₀(DR) + 1.87, which gives **roll distance 6.0–10.6 m**.

  [Gabrielsen](https://www.isss-sportsurfacescience.org/downloads/documents/A7ZD60AH55_Gabrielsen_Ball_Roll.pdf.pdf)
- Natural vs synthetic: significant differences were found between some synthetic turfs and natural turf — [Gabrielsen](https://www.isss-sportsurfacescience.org/downloads/documents/A7ZD60AH55_Gabrielsen_Ball_Roll.pdf.pdf). Ball roll is "one of the first criteria which cause artificial grass pitches to fail" as they wear (search snippet) — [Sheffield/SHU](https://shura.shu.ac.uk/37177/1/Mills-A_Field-based_Analysis%28VoR%29.pdf)
- Theory: a rigid ball on a deformable (grass) surface decelerates non-linearly in time — [Brearley & de Mestre, ANZIAM J.](https://www.cambridge.org/core/journals/anziam-journal/article/rolling-of-a-rigid-ball-on-a-horizontal-deformable-surface/3BD3E45457768883C4F2D31AE7E1EB41) (abstract-level)

### Inferences
- **Constant-deceleration equivalent:** a 3.2 m/s ball stopping in 4 / 6 / 8 / 10 m means a mean deceleration of 1.28 / 0.85 / 0.64 / 0.51 m/s². As a rolling-resistance coefficient, a/g ≈ 0.05–0.13 (my arithmetic).
- **Fit of dec = a + b·v to the NBI trendline.** I solved for (a, b) so that the ball starts at 3.2 m/s, stops at the stated roll distance, and loses the trendline's VC between x = 0.5 and 2.5 m. Closed form: x(v) = (v₀−v)/b − (a/b²)·ln((a+bv₀)/(a+bv)). My fit:

  | Roll distance | a (m/s²) | b (1/s) |
  |---|---|---|
  | 5 m | 0.70 | 0.16 |
  | 6 m | 0.53 | 0.16 |
  | 8 m | 0.37 | 0.14 |
  | 10 m | 0.29 | 0.11 |
  | 12 m | 0.23 | 0.10 |

  So for a "medium" pitch (8 m), dec ≈ 0.37 + 0.14·v, about 0.5 m/s² at 1 m/s and 0.8 m/s² at 3.2 m/s. These fits already include air drag at ≤3.2 m/s.
- **Conflict:** the old FIFA 1 m-gate requirement (0.4–0.75 m/s lost at 2.5 m/s) implies 0.92–1.59 m/s². That is about twice the mean deceleration of the 6.0–10.6 m rolls the same document maps it to. No a + b·v law with a ≥ 0 starting from 3.2 m/s reproduces both. Likely causes are that the ball has not settled into pure rolling just after the ramp, and a log-formula conversion. Trust the NBI distance-based fit more.
- **Extrapolation to game speeds (not measured).** Method:
  - remove about 0.08 from b to avoid double-counting low-speed drag;
  - add the Tango 12 aero drag K·C_D(v)·v²;
  - assume the ball is already rolling (no skid).

  Predicted roll distance and time to stop:

  | Pitch class | 5 m/s | 10 m/s | 15 m/s | 20 m/s | 25 m/s |
  |---|---|---|---|---|---|
  | 6 m "slow" | 11.7 m / 5.8 s | 25 m / 7.7 s | 39 m / 8.8 s | 57 m / 9.8 s | 73 m / 10.5 s |
  | 8 m "medium" | 15 m / 7.8 s | 31 m / 10.0 s | 46 m / 11.2 s | 66 m / 12.4 s | 84 m / 13.2 s |
  | 10 m "fast" | 19 m / 9.8 s | 36 m / 12.4 s | 53 m / 13.7 s | 76 m / 15.0 s | 96 m / 15.9 s |

  These are plausible for match play: a hard 20 m/s ground ball rolls 55–75 m. **Air drag dominates above about 8–10 m/s** (2.7 m/s² at 10 m/s vs about 0.6–1.0 from turf), so a pure-Coulomb model with no drag badly over-predicts long rolls. For example, with a constant 0.64 m/s² and no drag, a 20 m/s ball rolls 312 m.
- **Wetness:** the one certificate shows slightly *longer* roll when wet after wear (8.9 vs 8.7 m). That is too small and too single-sample to generalise.

### Gaps
- **Measured rolling deceleration above ~3 m/s on real pitches:** no reliable source found. All test data are at ≤3.2 m/s, so game-speed rolls are extrapolation.
- **Natural vs artificial a, b values separately:** not found (the NBI trendline pools both).
- **Grass length and mowing height:** no reliable source found with numbers.
- **Wet natural grass vs dry:** no reliable source found.
- **"Reduced ball roll" vs full ball roll equivalence:** the manual describes the reduced roll as a prediction "comparable to a standard full-length ball roll," but I found no validation data.

---

## 3. Slide-to-roll transition of a kicked ground ball

### Takeaway
A ball struck along the ground without matching spin skids. Kinetic friction μ_k·g (about 4–6 m/s² if μ ≈ 0.4–0.6) cuts its speed hard until v = rω. For a thin shell (α = I/(mr²) = 2/3) with no initial spin, it then rolls at **3/5 of the launch speed**; for a solid sphere it would be 5/7. Topspin shortens the skid and keeps more speed; backspin does the opposite. That is the "fast, then rolling" look. The equations are textbook rigid-body mechanics, derived here as inference; ball–grass μ for footballs was not found.

### Cited Findings
- For a solid sphere I = 0.4·M·R². The bottom-point (contact) speed is v_x − Rω, and a rolling ball has v_x = Rω. Friction at the bottom is the only source of spin change if the normal force passes through the centre — [Cross, "Enhancing the bounce of a ball", Phys. Teach. 48 (2010) 450](https://www.physics.usyd.edu.au/~cross/PUBLICATIONS/48.%20EnhanceBounce.pdf)
- Proxy friction value from tennis, not football: grass court coefficient of sliding friction about **0.5–0.6** (Wimbledon). This is search-snippet level from [Cross, Physics Today "Tennis physics, anyone?"](https://physicstoday.aip.org/quick-study/tennis-physics-anyone) and not read in full.

### Inferences
Derivation assumes a rigid ball, continuous contact (no hops), constant μ_k, and no air drag during the skid.
- **Definitions:** α = I/(m r²). Thin shell α = 2/3; solid sphere α = 2/5. A real football is a shell of finite thickness, so α is slightly below 2/3; I'd use about 0.62–0.66. ω > 0 means topspin (rolling-forward sense). Slip speed u = v − rω.
- **During the skid (u > 0):**
  - dv/dt = −μg
  - dω/dt = +μg/(α r)
  - du/dt = −μg(1 + 1/α)
- **Skid end:**
  - skid time t_s = α·u₀ / ((1+α)·μg)
  - final rolling speed **v_roll = (v₀ + α·r·ω₀)/(1+α)**
  - with ω₀ = 0: v_roll = v₀/(1+α) = **0.6·v₀** for a shell (0.714·v₀ for a solid sphere)
  - skid distance s = (v₀² − v_roll²)/(2μg)
- **Worked numbers** (shell, ω₀ = 0, μ = 0.5, μg = 4.9 m/s²):

  | v₀ | v_roll | Skid time | Skid distance |
  |---|---|---|---|
  | 10 m/s | 6 m/s | 0.82 s | 6.5 m |
  | 20 m/s | 12 m/s | 1.63 s | 26 m |

  With μ = 0.4, the 20 m/s case skids 33 m. Adding air drag (about 3–4 m/s² at 15–20 m/s) makes the skid shorter and slower.
- **Energy:** 40 % of the initial speed is lost to sliding, which is 64 % of kinetic energy before counting the rotational share. That is why a fully skidding model may look too sluggish for driven passes.
- **Real kicks are not ω₀ = 0.** A pass struck slightly above centre gets topspin. With ω₀ = v₀/r there is no skid at all. Contact with turf fibres and small hops also shorten the effective skid. Use a "kick spin fraction" f = rω₀/v₀ in [−0.5, 1]:
  - v_roll = v₀(1 + α f)/(1+α);
  - t_s = α(1−f)v₀/((1+α)μg).
- **Backspin** (f < 0, chip or stabbed pass): longer skid, lower final speed. With f = −0.5 the shell ends at 0.4·v₀.
- **Topspin greater than rolling** (f > 1, rare on the ground): u < 0, friction accelerates the ball forward until v = rω.
- **Visual cue:** during the skid the rendered rotation rate rω is less than v (the panels barely turn), then it locks to v/r.

### Gaps
- **Ball–natural-grass and ball–artificial-turf μ_k for a football:** no reliable source found. The 0.5–0.6 figure is tennis-on-grass, and snippet-level.
- **Measured α for a size-5 ball:** no reliable source found.
- **High-speed video of skid length for real passes:** no reliable source found.

---

## 4. Bounce on grass: COR, tangential friction, spin change, grip vs slide, top/backspin effects

### Takeaway
Treat the bounce as an impulse with:
- a normal COR e_n ≈ 0.55–0.71 on FIFA-compliant turf at about 6 m/s impact;
- a tangential model that is either "slide throughout" (Coulomb, Δv_t = μ(1+e_n)v_n) or "grip" with tangential COR e_x ≈ 0.1–0.2.

Pick whichever gives the smaller tangential impulse. Grip is the usual case at steeper angles (above about 20° from horizontal); sliding happens at glancing angles, with backspin, or on slippery surfaces.

### Cited Findings
**FIFA turf rebound**
- Vertical ball rebound test: dropped from **2.00±0.01 m** (underside of the ball). The ball is pre-set to rebound 1.45±0.03 m on concrete. Rebound height is computed from the time between bounces: **H = 1.23·(T−0.025 s)²·100 cm** — [FIFA Test Manual I](https://www.sporteimpianti.it/wp-content/uploads/2024/04/FIFA-TTM-I-Layout-v8.pdf)
- Certificate results:

  | Test | Requirement | Measured |
  |---|---|---|
  | Vertical rebound, initial, dry | 0.6–1.0 m | 0.94 m |
  | Vertical rebound, initial, wet | 0.6–1.0 m | 0.85 m |
  | Vertical rebound, after wear | 0.6–1.0 m | 1.00 m |
  | Angle ball rebound, dry | 45–70 % | 56 % |
  | Angle ball rebound, wet | 45–80 % | 65 % |

  [FIFA certificate](https://qrubber.cl/wp-content/uploads/2022/07/Pasto-Qgrass-50mm-04mt-20mt-QRubber-cert-FIFA_.pdf)
- FIFA ball certification: on a steel plate from 2 m, **115–165 cm (Quality)** and 120–165 cm (Pro) — [sportsballshop summary](https://www.sportsballshop.co.uk/pages/how-do-footballs-become-fifa-approved) (secondary source)

**Rod Cross bounce model**
- Definitions:
  - the normal COR is the ratio of normal contact-point speeds;
  - the tangential COR is **e_x = −(v_x2 − Rω₂)/v_x1**, defined on the contact-point speed.
- Solid sphere, no incident spin:
  - **ω₂ = 5(1+e_x)v_x1/(7R)**;
  - e_x ≈ 0 on slippery surfaces and cannot exceed 1, so spin varies by up to a factor of about 2 depending on e_x.

  [Cross 2010](https://www.physics.usyd.edu.au/~cross/PUBLICATIONS/48.%20EnhanceBounce.pdf)
- Grip: "At angles of incidence less than about 50° [from vertical], the bottom of the ball comes to a complete stop … causing the ball to grip." Elastic tangential energy is then partly returned, giving positive e_x, and "the ball bounces with Rω > v_x when e_x is positive" (it leaves with more topspin than rolling) — [Cross 2010](https://www.physics.usyd.edu.au/~cross/PUBLICATIONS/48.%20EnhanceBounce.pdf)
- Slide throughout (F/N = μ) happens only at glancing angles, typically about 20° or less. At larger angles the ball grips and friction can reverse during the bounce. Horizontal COR is typically about 0.1–0.2 for most balls and about 0.5–0.6 for a superball. Footballs tend to bounce "in the direction they point," modified by spin — [Cross, Physics of Bounce page](https://www.physics.sydney.edu.au/~cross/BOUNCE.htm)
- Measured examples (golf ball and superball at 25° from vertical) — [Cross 2010, Table I](https://www.physics.usyd.edu.au/~cross/PUBLICATIONS/48.%20EnhanceBounce.pdf):

  | Ball and surface | COR | e_x |
  |---|---|---|
  | Golf ball on granite | 0.87 | 0.03 |
  | Golf ball on rubber sheet | 0.59 | 0.51 |
  | Superball on granite | 0.78 | 0.49 |
- With backspin or a slippery surface, the ball slides throughout the bounce over a wider range of angles. Measured grip-regime e_x = 0.14±0.02 for angles of 30–44° (search-snippet level) — [Cross, Physics Today 61(9) 84](https://physicstoday.aip.org/article/61/9/84/413456)
- Tennis proxy: on grass courts, vertical COR is about 0.6–0.75 depending on grass length and soil softness (snippet) — [Cross, "Tennis physics, anyone?"](https://physicstoday.aip.org/quick-study/tennis-physics-anyone)

### Inferences
Generalisation of Cross's equations to any α and incident spin. Assumes a rigid-body impulse at the contact point; v_x is tangential speed along the travel direction and ω > 0 is topspin.
- **Normal:** v_n2 = −e_n·v_n1.
- **Slip:** u₁ = v_x1 − rω₁.
- **Grip candidate impulse** (per unit mass): J_g = α(1+e_x)u₁/(1+α).
- **Slide candidate impulse:** J_s = μ(1+e_n)|v_n1|·sign(u₁).
- **Choose:** J = J_s if |J_s| < |J_g| (slides throughout), else J = J_g.
- **Update:**
  - v_x2 = v_x1 − J
  - ω₂ = ω₁ + J/(α r)
- **Checks:**
  - α = 0.4, ω₁ = 0 reproduces Cross's ω₂ = 5(1+e_x)v_x1/(7R);
  - grip gives **v_x2 = v_x1 − α(1+e_x)(v_x1 − rω₁)/(1+α)**.
- **Thin shell (α = 2/3), grip, e_x = 0.15:**
  - v_x2 = v_x1 − 0.46·u₁;
  - a no-spin ball loses about 46 % of its horizontal speed in one gripping bounce and leaves spinning slightly faster than rolling.
- **Spin effects:**
  - **Topspin** (u₁ small): little horizontal loss, so the ball "skips on."
  - **Backspin** (u₁ > v_x1): large loss, and the ball can "check" or even kick back if rω₁ is large.
  - **Rolling into a bounce** (u₁ = 0): no tangential impulse.
- **e_n estimates:**
  - FIFA turf: rebound 0.6–1.0 m from 2.00 m gives e_n ≈ √(0.30…0.50) = **0.55–0.71** at impact ≈ √(2·9.81·2) ≈ 6.3 m/s. Drag over 2 m is ignored; it is small.
  - Ball on concrete (1.45 m): **e ≈ 0.85**.
  - Ball on steel (FIFA ball certification): e ≈ 0.76–0.91.
  - So the turf absorbs a large share of the energy.
- **Wet turf:** the one certificate shows lower vertical rebound wet (0.85 vs 0.94 m, e_n 0.65 vs 0.69) and a higher angle-rebound percentage wet (65 vs 56 %). That is consistent with a more slippery surface keeping more horizontal speed. This is a single sample.
- **Speed dependence:** e_n typically falls with impact speed for inflated balls, but I found no football-on-grass data. Use a constant, or a mild decline.

### Gaps
- **FIFA "angle ball rebound" test conditions:** the incident speed and angle, and exactly what the 45–70 % is a ratio of, were not found in the text I extracted. Don't convert it to a friction coefficient.
- **Football-specific oblique bounce data** (μ, e_x, spin change on grass): no reliable source read. Rezaei et al. 2011 ("Finite element modelling and experimental study of oblique soccer ball bounce," J. Sports Sci.; [PubMed 21777166](https://www.pubmed.gov/21777166/)) is the lead, but the abstract was not retrievable. A snippet says higher friction can reverse the horizontal impact force.
- **e_n vs impact speed for footballs:** no reliable source found.
- **Ball deformation and contact time:** no reliable source found in this pass.
- **Grass length effects on bounce:** only the tennis proxy above.

---

## 5. Ball rotation while rolling and how it looks

### Takeaway
A rolling ball spins at **ω = v/r ≈ 9.1·v rad/s** (r = 0.11 m). That is 1.45 rev/s per m/s of speed: about 7 rev/s at 5 m/s and 29 rev/s at 20 m/s. Above about 5 rev/s, panel patterns alias at 60 fps. During skids ω < v/r; after gripping bounces ω is slightly above v/r; in flight it is whatever the kick gave, decaying slowly.

### Cited Findings
- Rolling condition: v_x = Rω, so the contact point is at rest — [Cross 2010](https://www.physics.usyd.edu.au/~cross/PUBLICATIONS/48.%20EnhanceBounce.pdf)
- After a gripping bounce with positive e_x, Rω > v_x — [Cross 2010](https://www.physics.usyd.edu.au/~cross/PUBLICATIONS/48.%20EnhanceBounce.pdf)
- Flight spin magnitudes:
  - Beckham free kick: 63 rad/s ≈ 10 rev/s — [Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)
  - launcher range 0–180 rad/s — [Goff & Carré 2009](https://wiki.penfieldrobotics.com/wiki/images/c/c0/Goff_Carre_AJP_2009.pdf)

### Inferences
- **Rates at 60 fps** (my arithmetic):

  | Ball speed | Spin | Rotation per frame |
  |---|---|---|
  | 2 m/s | 2.9 rev/s | ≈17° |
  | 5 m/s | 7.2 rev/s | ≈43° |
  | 10 m/s | 14.5 rev/s | ≈87° |
  | 20 m/s | 29 rev/s | ≈174° (looks static or backwards from aliasing) |

  For rendering, clamp the visual spin rate or add motion blur above about 8–10 rev/s. Keep the sim's ω true.
- **Rolling spin axis:** horizontal and perpendicular to the velocity, so ω = (ẑ × v)/r with z up.
- **A curling shot's spin axis:** roughly vertical (sidespin), which gives the lateral Magnus force. A mixed axis gives a dipping curl.
- **Tracking ω separately from v** is needed for the slide-to-roll transition, the bounce spin transfer and Magnus. At minimum the sim needs a 3-component ω, or a per-axis scalar spin plus a "rolling" flag.

### Gaps
- No perceptual studies were found on how players read ball spin visually.
