# Real-world football team tactics: principles and numbers a game AI can use

Scope: team shape in and out of possession, compactness, pressing, cover, marking and attacking support, for 5-, 7- and 11-a-side.
Research note (2026-11-15). Budget was about 15 tool calls. PMC (NCBI) pages were blocked, so a few tracking papers are known only from abstracts or secondary citations, and this is flagged where it applies. Numbers marked "coaching rule of thumb" come from non-peer-reviewed coaching sites.

## Defensive organisation: compactness, block shifting, line height, pressing roles, triggers, counter-press

### Takeaway
Elite 11v11 teams defend zonally. The back-to-front length out of possession is about 25–26 m (Sacchi's ideal 25 m; 2018 World Cup average 26 m). The defence-to-midfield gap is about 10 m (10.2 ± 3.8 m), against about 13 m (13.3 ± 4.2 m) from midfield to attack. Tracking data says compactness *near the ball* matters more for winning it back than whole-team compactness. In 7v7, the defending team measures roughly 25 m wide × 14–22 m long, with a stretch index of about 10–12 m. Pressing follows the pressure–cover–balance model, with the cover player about 45° behind and goal-side, and is set off by cues such as a back pass, a sideline pass, a receiver facing his own goal, or a poor touch. The counter-press is about 5 seconds, then the team drops into its block. The 5 s figure is a coaching convention, not an empirically derived optimum.

### Cited Findings
**Zonal defending and reference points**
- Sacchi's AC Milan (late 1980s) replaced man-marking plus a sweeper with zonal lines moving as one unit, up and down and side to side. Its four reference points were the ball, the space, the opponents and the teammates. — [Tactical Rant, "The Evolution of Defensive Compactness" (2021), citing Wilson, *Inverting the Pyramid*](https://tacticalrant.substack.com/p/evolution-defensive-compactness)

**11v11 vertical compactness (team length) and inter-line gaps**
- Sacchi's ideal maximum distance from the last defender to the centre-forward was 25 m. — [Tactical Rant (2021)](https://tacticalrant.substack.com/p/evolution-defensive-compactness)
- The FIFA 2018 World Cup technical report gives a tournament average of 26 m between the deepest defender and the highest attacker out of possession, more compact than 2014. — [Tactical Rant (2021), citing FIFA TSG report](https://tacticalrant.substack.com/p/evolution-defensive-compactness). This is secondary; I did not see the FIFA primary.
- Elite teams in the defensive phase: defence–midfield line distance 10.22 ± 3.76 m, midfield–attack 13.30 ± 4.22 m. — cited in [Sci. Reports 2025, "Characterizing the spatial structures of competing football teams"](https://www.nature.com/articles/s41598-025-97765-y) as a 2024 study. A second source in the same line of research describes the defence–midfield distance as "about 10.2 m". I could not open the Nature full text (paywall redirect), so this is the search-snippet figure.
- Forcher et al. (Bundesliga 2020/21, 153 matches, official tracking data): "compactness of players in areas close to the ball seems crucial for defensive success", but "compact organization of the entire team does not seem important to regain the ball". The same research line reports that a high press gives a smaller inter-team distance, a longer team (larger dispersion) and *larger* inter-line distances. — [Forcher et al., "Is a compact organization important for defensive success in elite soccer?" (abstract)](https://cir.nii.ac.jp/crid/1360861403644016128); [ResearchGate record](https://www.researchgate.net/publication/370827756_Is_a_compact_organization_important_for_defensive_success_in_elite_soccer_-_Analysis_based_on_player_tracking_data)
- Coaching rules of thumb, which disagree with each other: 10–15 m or 8–12 m between back line and midfield; 30–35 m from deepest defender to most advanced midfielder. — aggregated in the search results from [The Football Analyst](https://the-footballanalyst.com/compactness-and-shifting-drills-defending-as-a-unit/) and [Medium mid-block article](https://medium.com/@navidfaridi2014/mid-block-in-modern-football-how-compactness-shapes-elite-defenses-18322dcd95e6). Low reliability.
- Team width and length are defined as the distance between the two furthest-apart players across the pitch and along it. In La Liga, team length and the goalkeeper-to-defence distance have *declined* over recent seasons, and top teams show lower Length and GK–Def values. — [Biology of Sport, LaLiga eight-season analysis](https://visualize.jove.com/38416760); [termedia Biology of Sport](https://www.termedia.pl/Technical-and-tactical-evolution-of-the-offensive-team-sequences-r-nin-LaLiga-between-2008-and-2021-Is-Spanish-football-now-a-more-r-nassociative-game-,78,51582,1,1.html)
- 11v11 overall-shape figures: about 38 m game length and about 43 m game width against top opponents (12 elite matches, one top-5 club, CV 12%, split by possession). In the midfield transition phase, male matches were about 44.9 m wide and female matches about 41.8 m. — [PLOS One / PMC6705862 (search snippet)](https://pmc.ncbi.nlm.nih.gov/articles/PMC6705862); [PLOS One 0295787](https://dx.doi.org/10.1371/journal.pone.0295787). These are snippet values; I could not verify the full text.
- A thesis on elite tracking data finds the team "more dispersed and wider while attacking" than when defending, consistent with the offensive and defensive principles. — [LJMU MPhil thesis, Zhang (2022)](https://researchonline.ljmu.ac.uk/19091/1/2022guangzezhangmphil.pdf)

**7v7 compactness (the most directly usable data)**
- González-Rodenas et al. (2024, J. Hum. Kinet. 97:237–247) studied 7-a-side including goalkeepers, U19 sub-elite players, a 65 × 50 m pitch (232 m² per player) and 10 Hz GPS, with 2-3-1 and 3-1-2 each played against a 3-3. Medians, IQR in brackets:
  - **Attacking (own formation):** width 32.8 m (2-3-1) / 31.5 m (3-1-2); length 21.0 / 21.3 m; stretch index 12.8 / 12.7 m; surface area 395 / 381 m².
  - **Defending (own formation):** width 26.2 / 25.5 m; length 20.1 / 21.6 m; stretch index 11.1 / 11.7 m; surface area 306 / 336 m².
  - **Defending against an opponent's 2-3-1 vs 3-1-2:** length 14.2 m vs 18.1 m; width about 25 m in both; surface area 210 vs 269 m². A front-two opponent (3-1-2) stretches the defending team longer.
  - **Attacking against a 2-3-1 vs a 3-1-2 defence:** width 34.2 vs 30.0 m. Attackers spread wider against a narrower front-line structure.
  - So width shrinks by about 6–7 m (about 20%) and surface area by about 20–25% when going from attacking to defending. Length barely changes in the team's own formation.
  - Source: [González-Rodenas et al. 2024 (Europe PMC full text, PMC12127938)](https://www.ebi.ac.uk/europepmc/webservices/rest/PMC12127938/fullTextXML)

**Small-sided-game tracking metrics (definitions and pitch-size effects)**
- Frencken et al. (2011), 4v4 elite youth: the two teams' centroids show a strong positive linear coupling, strongest in the forward–backward direction. The expected counter-phase pattern of surface area (attack expands, defence contracts) was *not* clearly found in 4v4, which the authors attribute to the small number of players. — [Hanze summary and related citations](https://research.hanze.nl/en/publications/length-width-and-centroid-distance-as-measures-of-teams-tactical-/); [Trends review PDF](https://www.wbc.poznan.pl//Content/336757/PDF/4_Trends_2014_vol%2021_no%203_135.pdf)
- Frencken et al. (2013), 4v4: a shorter pitch gives a smaller longitudinal inter-team distance, a narrower pitch gives a smaller lateral inter-team distance, and a smaller area gives a smaller surface area. Shape scales with the pitch. — [Size matters (Hanze)](https://research.hanze.nl/en/publications/size-matters-pitch-dimensions-constrain-interactive-team-behaviou/)
- Folgado et al. (2012), U9/U11/U13 in GK+3v3+GK and GK+4v4+GK: younger teams show a higher length-per-width ratio (LPWR), meaning longer and narrower "bunched" shapes, and its variability falls with age. In 3v3 the centroid distance between teams is larger for older groups. — [Hanze record](https://research.hanze.nl/en/publications/length-width-and-centroid-distance-as-measures-of-teams-tactical-/)
- "The older, the wider" (elite U17 vs U19): U19 had a larger *lateral* stretch index and a lower LPWR. — [PubMed 25769114 annotation](https://pubannotation.org/docs/sourcedb/PubMed/sourceid/25769114?locale=ja)
- Larger pitches give a higher stretch index at any age. Lengthening the pitch raises the stretch index while width hardly changes. — [Kinesiology 2021](https://hrcak.srce.hr/en/265568); [Castellano et al. 2017 record](https://searchworks.jioinstitute.edu.in/articles/asx__124537480)
- Olthof et al. (2019): small-sided games on a match-like relative pitch area of about 320 m² per player produce larger inter-team distances and more high-intensity running than 100–200 m² per player. — [Frencken/Olthof search summary](https://research.hanze.nl/en/publications/size-matters-pitch-dimensions-constrain-interactive-team-behaviou/). I saw this only as a secondary citation.

**Pressing roles (pressure–cover–balance)**
- First defender: the nearest player to the ball. He closes down to "delay, make predictable, get tight", chooses an angle that shows the attacker away from goal, and takes away the entry ball into goal. — [Soccer Parenting](https://www.soccerparenting.com/blog/principles-defense-explanation-youth-soccer-parents/); [Minnesota Youth Soccer coaching points PDF](https://cdn1.sportngin.com/attachments/document/0112/0410/cp_defending.pdf)
- Second defender (cover): stands behind the presser at an angle and goal-side, close enough to engage if the first defender is beaten but far enough that the dribbler can't beat both at once. About 45° is suggested on a forum. There is no authoritative distance; it depends on attacker speed and field zone. — [GSL Soccer principles](http://gslsoccer.blogspot.com/2008/05/1st-2nd-and-3rd-defender-principles-and.html); [BigSoccer thread](https://www.bigsoccer.com/threads/how-to-teach-pressure-cover-balance.1549960/)
- Third defenders (balance): keep the shape away from the ball and read the switch. When the ball moves, the nearest player steps up to become the new first defender and the others slide into cover and balance. — [Soccer Parenting](https://www.soccerparenting.com/blog/principles-defense-explanation-youth-soccer-parents/); [FIFA Training Centre, zonal defending in small groups](https://www.fifatrainingcentre.com/en/practice/talent-coach-programme/zonal-defending/developing-defensive-coordination-in-small-groups.php)
- Pressing triggers: a pass to the sideline, a backward pass, a receiver with his back to goal, a poor first touch. The touchline acts as "an extra defender". Cover shadow means curving the run and using body position to block a passing lane while pressing. The common failure is a front line pressing while the midfield does not follow, which opens a central gap: "the press must be connected front to back". — [Sportsessionplanner mid-block triggers](https://www.sportsessionplanner.com/s/wURWi/Mid-Block-pressing-triggers.html); [Sportplan pressing triggers](https://www.sportplan.net/drills/Football/trends/pressing-triggers-winning-ball-back-2025-01.jsp); [FA Coachcast ep. 5 on pressing](https://community.thefa.com/coaching/podcasts/w/coachcast-transcripts/1207/episode-five-pressing-with-chris-welburn/revision/1)
- The 1970s Netherlands pressing structure left about 10–15 yards of open space beside the ball or behind the line. — [Tactical Rant (2021)](https://tacticalrant.substack.com/p/evolution-defensive-compactness)

**Counter-press and transition**
- The "5-second rule" is usually credited to Guardiola's Barcelona: maximum pressure for about 5 s after losing the ball, then drop into the block. — [arXiv 2307.04215, ball-recovery model](https://arxiv.org/pdf/2307.04215); [CoachNotes](https://coachnotes.co.uk/blogs/how-to-coach/how-to-coach-counter-pressing). Blog claims such as "elite regain in 5–7 s vs 9–11 s mid-table" and "35% press regain" are unsourced. — [Khara Sports Daily](https://kharasportsdaily.com/5-second-recovery-rule/) (low reliability)
- Bauer & Anzer (2021): successful counter-pressing significantly raises goal-scoring, and failed counter-pressing substantially raises goals conceded. Defensive transitions go better when **four or more players are behind the ball** at the moment the ball is lost. — cited in [Forcher et al. 2023, JSSM 22:707](https://www.jssm.org/volume22/iss4/cap/jssm-22-707.pdf)
- Forcher et al. (2023) define a successful defensive transition as **regaining the ball within 12 s** of losing it, and an unsuccessful one as an opponent's shot. They conclude that "regaining possession as fast as possible after a ball loss is the most important success factor in defensive transition". — [JSSM 2023](https://www.jssm.org/volume22/iss4/cap/jssm-22-707.pdf)

### Inferences
- For an 11v11 block, a sensible target is team length of about 25–35 m (deepest outfield player to highest), line gaps of about 10 m (defence–midfield) and about 13 m (midfield–attack), and width of about 40 m. That leaves the weak-side flank deliberately open on a 68 m-wide pitch; this follows from the numbers, I found no direct study of it.
- For 7v7 (about 55 × 35 m in the game, smaller than the 65 × 50 m study pitch), scale the study shape by the pitch ratio. Width out of possession of about 26 m on a 50 m-wide pitch is about 52%, so about 18–20 m on a 35 m pitch. Length of about 14–22 m on 65 m (22–33%) gives about 12–18 m on 55 m. Stretch index (mean player distance from the team centroid) is about 10–11 m defending and about 12–13 m attacking at study scale. This scaling is my inference, supported by Frencken 2013 and Castellano 2017 showing shape grows with pitch size.
- Weight compactness *around the ball*, meaning the 2–3 nearest defenders tight, above whole-team compactness (Forcher). A practical encoding: one presser, one cover player at about 45° goal-side a few metres back, and the remaining players in a zone-anchored shape shifted toward the ball.
- Counter-press for about 5 s (1–2 nearest players plus any nearby cover), then retreat to the block. Keep 4 or more players behind the ball in 11v11; scale to about 2–3 in 7v7.
- A curriculum signal from the youth findings: novice (young) teams have high LPWR and bunch around the ball, while skilled teams are wider (lower LPWR). A learning AI could start "young" and widen with skill.

### Gaps
- I found no peer-reviewed number for the **lateral shift of the block** (for example, the centroid's lateral displacement per metre of ball movement) or for ball-side overload counts. Centroid–ball coupling is documented qualitatively (Frencken 2011) but without a regression slope I could quote.
- I found no measured figure for **defensive line height relative to the ball**. The JSSM rest-defence figure (43.6 m from own goal while *in possession*) is the closest.
- I found no authoritative cover distance in metres for the second defender. Coaching sources give only qualitative guidance (about 45°, "close enough to step in").
- Forcher et al. (compactness paper) full numbers, such as surface area by success, were not accessible.

## Attacking organisation: width, depth, support, rest-defence, positional play

### Takeaway
Attacking shapes are wider than defending ones (7v7: about 31–34 m vs about 25–26 m width; surface area about 20–25% larger). The best-quantified attacking-side principle is rest defence. Elite Bundesliga teams keep about 3.7 defenders against about 2.0 opposing attackers (a +1.7 majority) in a line about 7 m deep and 28 m wide, positioned about 43.6 m from their own goal, with about 5 m to the nearest attacker. Common structures are 2+1 / 3+1 / 2+2 boxes. Positional-play lane and support-distance numbers are mostly coaching lore and were not found in measured form.

### Cited Findings
- Forcher et al. (2023, JSSM 22:707–725) combined interviews with 7 professional coaches and 2020/21 Bundesliga tracking data. Their definition: rest defence is "the positioning of the deepest defenders during ball possession to prevent counterattacks". The rest-defence area is the zone 10 m in front of the deepest defender. — [JSSM 2023](https://www.jssm.org/volume22/iss4/cap/jssm-22-707.pdf)
  - In the rest-defence area: **3.70 ± 0.76 defenders vs 2.01 ± 0.93 attackers**, a superiority of **+1.69 ± 1.00**. Rest-defence shape: surface area 86 ± 60 m², **7.35 ± 1.92 m long × 28.21 ± 7.43 m wide**, spread 13.9 ± 2.9 m.
  - Positioned **43.56 ± 10.00 m from their own goal line**. Closest defender to an attacker: 5.08 ± 2.23 m. Attacking team's space control in that zone: 11.5 ± 9.8%.
  - Expert consensus on who forms it: central defenders (7/7 experts), the central midfielder (7/7) and the weak-side full-back (3/7). Structures named: man-to-man coverage (7/7), a **+1 majority** (6/7; "sandwich" or flat), a +2 majority (2/7), space defending as 3+1 diamond (2/7) and 2+3 trapezium (1/7).
  - Numerical superiority was the 5th most important predictor of success, and low attacker space control near the goal was the 6th. "Controlling deep spaces and dangerous counterattackers" is the key behaviour.
  - Comparable to Vilar et al. (2013): about +1 superiority in centre-back sub-areas.
- Spielverlagerung frames the division of labour as rest defence for the players far from the ball and counter-pressing for those near it. The variants include man-to-man-plus-one and a **2-2 box** that gives strong positional cover against transitions. — [Spielverlagerung, "Tactical theory: the various forms of rest-defence" (2021)](https://spielverlagerung.com/2021/07/05/tactical-theory-the-various-forms-of-rest-defence/); [Spielverlagerung, "Rest-Attack" (2022)](https://spielverlagerung.com/2022/06/16/tactical-theory-rest-attack%ef%bf%bc/)
- In elite 11v11 tracking, teams are wider and more dispersed when attacking than when defending. — [LJMU thesis (2022)](https://researchonline.ljmu.ac.uk/19091/1/2022guangzezhangmphil.pdf)
- In 7v7, attacking width runs from 30.0 to 34.2 m depending on the opponent (65 × 50 m pitch), and the stretch index is about 12.7–13.8 m. — [González-Rodenas et al. 2024](https://www.ebi.ac.uk/europepmc/webservices/rest/PMC12127938/fullTextXML)
- FIFA futsal guidance (as relayed by a coaching site): unpredictable off-ball movement, the ball carrier must always have a nearby safe option, and runs come in pairs (decoy plus real threat). "A player who passes and watches has frozen the movement for everybody behind him." — [futsal.tech, 3-1 redondo](https://futsal.tech/en/positional-attack/3-1/redondo); [Teloframe 3-1 rotation](https://teloframe.com/training-drills/futsal-the-3-1-rotation-possession)
- Possession-style teams make more horizontal passes and more accelerations and decelerations. Counter-attacking teams make more long passes and more high-intensity running per minute of attack. Only xG (not goals or points) was linked to style. — [Forcher et al. 2023, Front. Psychol. 14:1197039](https://www.frontiersin.org/journals/psychology/articles/10.3389/fpsyg.2023.1197039/full)

### Inferences
- A rest-defence rule for the AI in 11v11: while in possession, keep (number of opponent forwards + 1) players in a band about 10 m deep, with the closest defender about 5 m from each forward. In 7v7 with one opposing striker, that means the 2 deepest outfielders, possibly 3 against a front two. In 5-a-side, 1–2 players.
- Support: a ball carrier should always have at least one short "safe" option. Coaching lore puts support distances at about 10–15 m in 11v11 with triangles or diamonds; I found no measured source this session, so treat it as a tunable.
- In-possession width should approach the pitch width (attacking width > defending width by about 20–30%). Depth comes from one player high (striker or pivot) and the rest-defence line.

### Gaps
- I found no measured data for **support angles or distances** (the commonly quoted "10–15 m" support distance was not found in a citable source), for **third-man runs**, or for **juego de posición lane/zone counts** (the 5 vertical lanes and half-spaces are common in tactics writing, but I did not retrieve a primary source this session).
- I found no study counting **how many players commit forward** in open play, beyond the rest-defence counts above.

## Small-sided specifics: 7v7 formations and futsal rotations

### Takeaway
7v7 is usually played 2-3-1 (default youth shape: natural width and triangles), 3-2-1 (more defensive) or 3-1-2. Formation measurably changes shape: 3-1-2 defends longer and with more surface area than 2-3-1, and opponents attack wider against 2-3-1. Futsal uses a 3-1 (three in separate lanes plus a pivot for depth, rotating in a figure-eight) and a 4-0 (all four rotate, no fixed positions), with rotation started by the passer moving.

### Cited Findings
- 2-3-1 roles: two defenders split wide when the goalkeeper has the ball and shift as a unit. The central midfielder is the connector who shields the back line. The wide midfielders give width and run most. The striker gives depth. Progressions are to 3-2-1 if conceding too much, or to a 2-1-2-1 diamond. The FA convention writes it 1-2-3-1, counting the goalkeeper. — [U9/U10 Systems of Play (7v7) 2-3-1 PDF](https://dt5602vnjxv0c.cloudfront.net/portals/29534/docs/systems%20of%20play%20-%207v7.pdf); [Areacopa 7v7 formations](https://areacopa.com/en/blog/7v7-formations-youth-football); [Cupello 7v7](https://www.cupello.com/blog/soccer-formations-7v7)
- Formation effect (7v7 study, 65 × 50 m): defending in 3-1-2 vs 2-3-1 gives length 21.6 vs 20.1 m and surface area 336 vs 306 m². Opponents attack wider against 2-3-1 (34.2 vs 30.0 m). — [González-Rodenas et al. 2024](https://www.ebi.ac.uk/europepmc/webservices/rest/PMC12127938/fullTextXML)
- Futsal 3-1: the back three hold separate lanes, and the pivot gives depth up to about the second penalty mark. The three circulate in a figure-eight; a diagonal run that breaks the figure-eight makes the rotation collapse. — [futsal.tech, figure-eight](https://futsal.tech/en/positional-attack/3-1/eight); [futsal.tech, redondo](https://futsal.tech/en/positional-attack/3-1/redondo)
- Futsal 4-0: no fixed positions; all four rotate continuously to disorganise the defence. Players used to fixed roles find it disorienting at first. — [SportsLabTools futsal systems](https://sportslabtools.com/en/blog/futsal-tactical-systems-3-1-2-2-4-0/); [Rockstar Academy futsal rotation](https://www.rockstaracademy.com/blog/futsal-rotation)

### Inferences
- A 7v7 game AI could encode formation slots (for example 2-3-1) as zone anchors scaled to pitch size, shifted toward the ball, and contracted when defending (width about 0.5× pitch width, length about 0.25–0.33× pitch length per the study ratios).
- For 5-a-side, a 3-1 with "positions are places, not people" rotation, where the passer moves after passing and others fill the vacated slot, fits candidate-action × utility AI well: score "fill the empty slot" highly.

### Gaps
- No official federation (FA, US Soccer, RFEF) document with metric 7v7 spacing was retrieved. The formation-role material is from commercial and club coaching sites.
- No tracking study of futsal team surface area or stretch index was retrieved this session.

## Tracking-data metrics: definitions useful for AI evaluation

### Takeaway
The standard collective metrics are team length and width (furthest players along and across the pitch), LPWR (length/width), stretch index (mean distance of players to the team centroid), surface area (convex hull area), centroid distance between teams, and inter-line distances. They are cheap to compute and work as both AI targets and debug overlays. Expected values: 7v7 stretch index about 10–14 m, 11v11 defence–midfield gap about 10 m, and younger or weaker teams show higher LPWR.

### Cited Findings
- Width and length are the distance between the two furthest-apart players across and along the pitch. — [LaLiga eight-season analysis](https://visualize.jove.com/38416760)
- Compactness is often measured by the convex hull (the polygon around all players). — [LaLiga search summary / Biology of Sport](https://pmc.ncbi.nlm.nih.gov/articles/PMC10955746)
- Stretch index, width, length and surface area for 7v7 by formation: see the González-Rodenas 2024 values above. — [Europe PMC](https://www.ebi.ac.uk/europepmc/webservices/rest/PMC12127938/fullTextXML)
- Centroids of opposing teams move in phase, with the strongest coupling longitudinally (Frencken 2011). LPWR and centroid distance are valid youth tactical measures (Folgado 2012). — [Hanze](https://research.hanze.nl/en/publications/length-width-and-centroid-distance-as-measures-of-teams-tactical-/)
- Controlled 11v11 manipulations: Effective Playing Space, LPWR and team separateness all rise with pitch size. — [PLOS One 0221368 (IDEAS record)](https://ideas.repec.org/a/plo/pone00/0221368.html)

### Inferences
- These metrics can act as regression tests for the game AI. Log the stretch index, length, width and inter-line gap per phase, and check that the defending stretch index is below the attacking one and that width contracts by about 20% on losing the ball.

### Gaps
- No team-synchronisation coefficients (for example relative-phase values from Folgado or Clemente) were retrieved.
- Several PMC full texts were blocked in this environment; values from them above are search-snippet level.
