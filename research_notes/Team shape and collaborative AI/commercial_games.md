# Team shape and collaborative AI in commercial football games

Legend: **[full]** = I fetched and read the page (via a summarising fetch tool, so quotes are as relayed by that tool). **[snippet]** = taken only from search-result snippets or search summaries; not verified on the page. Some useful pages (SWOS manual mirror, sensiblesoccer.de Jon Hare Q&A, EA "Tactical Defending explained" page, which returned 404) could not be fetched.

---

## 1. EA FIFA / EA SPORTS FC: team AI (Tactical Defending, teammate intelligence, FC IQ roles and tactics, pressing, defensive line)

### Takeaway
EA's published model is **formation = defensive shape, Role = attacking/off-ball deviation**. When possession is lost, players return to formation slots. A single "Defensive Approach" setting (line height 1–100 in four bands) controls both line height and how much the line tracks runs versus holding or stepping up for offside. Pressing is controlled by that setting, a "Team Press" quick tactic and specific press roles (such as the Stopper CB). By default only the controlled player engages the ball carrier: one AI "second man" contains on request, and CPU teams can choose "double contain" or multi-player pressing. Since FIFA 12, EA's design has deliberately moved ball-winning *away* from AI teammates (they position and cut lanes; the human tackles), and FC 27 goes further.

### Cited Findings
**FC 25 FC IQ (official Pitch Notes) [full]**
- FC IQ = three components: Player Roles, Team Tactics, Smart Tactics. Roles: 31 at launch, each with 1–3 Focuses, 52 combinations in total. A Role sets the player's off-ball behaviour, and a Focus modifies one or two traits of it — [EA FC 25 Pitch Notes: FC IQ Deep Dive](https://www.ea.com/games/ea-sports-fc/fc-25/news/pitch-notes-fc-25-fc-iq-deep-dive)
- **Formation controls defensive shape and Roles control attacking movement.** When possession is lost, players return to their formation positions. How fast they return depends on tactical settings, Familiarity and Focus. In possession, players can deviate from formation positions, so asymmetrical shapes are possible — [same](https://www.ea.com/games/ea-sports-fc/fc-25/news/pitch-notes-fc-25-fc-iq-deep-dive)
- **Defensive Approach bands (line height 1–100):** Deep 1–30 (default 25): line drops with runs, no pressure. Balanced 31–60 (default 50): line is flexible about tracking runs, no pressure. High 61–90 (default 70): line rarely tracks runs, minimal pressure. Aggressive 91–100 (default 95): line never tracks runs and sets offside traps. Aggressive also presses "immediately after losing possession", pressures heavy touches and drains extra stamina — [same](https://www.ea.com/games/ea-sports-fc/fc-25/news/pitch-notes-fc-25-fc-iq-deep-dive)
- **Build-up styles:** Short Passing (players "come short to support the ball carrier rather than make forward runs", patient transition). Balanced (a mix of runs and coming short). Counter (get in behind, quick transition) — [same](https://www.ea.com/games/ea-sports-fc/fc-25/news/pitch-notes-fc-25-fc-iq-deep-dive)
- FC 25 **removed** the Defensive Width/Attacking Width sliders (Roles now determine width), the Players-in-Box slider (replaced by set-piece overload and "Get In Box" in possession) and the standalone Pressure setting (pressing now comes from Defensive Approach, the Team Press quick tactic used only out of possession, and Roles such as Stopper). Work Rates and Player Instructions were also replaced — [same](https://www.ea.com/games/ea-sports-fc/fc-25/news/pitch-notes-fc-25-fc-iq-deep-dive)
- Example roles: Stopper CB "steps out of the back line to win the ball and lead the press". Centre Half drops between the CBs in possession as counter-attack insurance. Falseback moves into a central position when the team has the ball. Shadow Striker plays "in the hole" and runs into the box. False 9 (Build-Up focus). Poacher. Advanced Forward — [same](https://www.ea.com/games/ea-sports-fc/fc-25/news/pitch-notes-fc-25-fc-iq-deep-dive)
- **Role Familiarity** (base / Role+ / Role++ / Out of Position) is used "in every positioning formula" and "can account for 10% to 40% of the result". It affects attacking runs, support range, **marking reaction time, marking distance, cover distance, press tightness**, and transition speed ("The lower the Familiarity the longer it takes for players to transition") — [same](https://www.ea.com/games/ea-sports-fc/fc-25/news/pitch-notes-fc-25-fc-iq-deep-dive)
- Tactical changes take effect gradually. "players can't teleport into new positions and roles". Tactical Focus (Attacking/Defending) temporarily shifts build-up and defensive approach by one level — [same](https://www.ea.com/games/ea-sports-fc/fc-25/news/pitch-notes-fc-25-fc-iq-deep-dive)
- The article gives **no** number for how many players press at once — [same](https://www.ea.com/games/ea-sports-fc/fc-25/news/pitch-notes-fc-25-fc-iq-deep-dive)

**FC 26 (official Pitch Notes) [full]**
- Support: teams try "to always have someone available as a passing option". Attacking runs were increased "to lower reliance on triggered runs" and "more consistently run into open space". Roles were loosened so players can take the best run "instead of strictly following what their player Role dictates" (for example, a Winger cuts inside). Build-up gives "run options as well as close-support options". Attackers stay onside better and recover from offside positions faster — [EA FC 26 Pitch Notes: Gameplay Deep Dive](https://www.ea.com/en/games/ea-sports-fc/fc-26/news/pitch-notes-fc26-gameplay-deep-dive)
- Defending: defenders "man mark opponents when close and inside the box". The line "will not automatically push up as far when trying to keep players offside". There are two presets: Competitive ("Less AI defending, resulting in looser defensive lines") and Authentic ("Greater AI defending… tighter defensive lines… defenders reacting realistically when a ball is nearby", with tighter midfield positioning) — [same](https://www.ea.com/en/games/ea-sports-fc/fc-26/news/pitch-notes-fc26-gameplay-deep-dive)
- CPU: teams "will know better when and how to defend, press the player, stay back, or use double contain". Some teams "will try to overwhelm you by pressing with multiple players at once". Stoppers and CBs who step up do it "more safely" — [same](https://www.ea.com/en/games/ea-sports-fc/fc-26/news/pitch-notes-fc26-gameplay-deep-dive)
- Community complaint (FC 26/27 forum): AI defenders sprint backward and drop into the six-yard box, breaking the offside line, and abandon close marks in the box — [EA forums](https://forums.ea.com/discussions/fc-27-feedback-en/broken-defensive-positioning-ai-defenders-leave-attackers-free-in-box/13802465) **[snippet]**

**FC 27 (official Pitch Notes) [full]**
- In Competitive mode, AI defenders "will now play more of a supporting role through their on-pitch positioning". They win the ball only when "a loose ball comes within reach or when a dribbler runs directly into them". A manually controlled defender reaches further than an AI one. "AI teammates will mark the ball carrier from a slightly longer distance than before". Rationale: "Community feedback consistently highlighted a desire for less automation… we want tackling to be an intentional player-led action" — [EA FC 27 Pitch Notes: Gameplay Deep Dive](https://www.ea.com/games/ea-sports-fc/fc-27/news/pitch-notes-fc27-gameplay-deep-dive)
- AI attackers make "more contextual curved runs". Without a trigger, "a winger may run parallel to the defensive line while waiting for a pass". Manually triggered runs last only a limited distance — [same](https://www.ea.com/games/ea-sports-fc/fc-27/news/pitch-notes-fc27-gameplay-deep-dive)

**FIFA 12–15 history**
- FIFA 12 Tactical Defending replaced "hold button to auto-approach and tackle" with manual, positioning-based containment. FIFA 11 behaviour stayed available as "Legacy Defending" — [esports-news.co.uk](https://esports-news.co.uk/?p=2944); [pcgames.de](https://www.pcgames.de/FIFA-12-Spiel-23267/Tipps/FIFA-12-Tipps-Tricks-und-Cheats-zu-Virtual-Pro-Standardsituationen-und-Defensivspiel-849008/4/) **[snippet]**
- Tip: pulling a defender out of the line lets the CPU play a through ball into the gap (the line is a shared structure that the CPU attack reads) — [esports-news.co.uk](https://esports-news.co.uk/?p=2944) **[snippet]**
- Complaint: "the shift in tactical thinking hasn't been supported by the AI programming" — [thegaminggentleman blog](https://thegaminggentleman.blogspot.com/) **[snippet]**
- FIFA 13 "Attacking Intelligence": attackers "analyse space… and think two plays ahead". Teammates move "one or even two passes ahead based on where the ball is likely to go", and use a stutter step to stay onside instead of curving runs — [EA: FIFA 13 innovations](https://www.ea.com/en-ca/news/ea-sports-unveils-fifa-13-innovations); [GamesRadar](https://www.gamesradar.com/fifa-13-first-look-what-comes-after-revolution) **[snippet; preview/marketing material]**
- FIFA 14 "Teammate Intelligence": "Defensive decision-making will be based on multiple frames enabling defenders to continually analyze the play to search for the most dangerous player. This will improve run tracking and man marking." Defenders track runs "without over-committing" and "recognize opportunities to support and apply pressure from anywhere on the pitch" — [EA: FIFA 14 Teammate Intelligence](https://www.ea.com/news/fifa-14-teammate-intelligence) **[snippet; the fetched page no longer contains the text]**; also [FIFPlay mirror](https://www.fifplay.com/fifa-14-teammate-intelligence/) **[snippet]**
- FIFA 14 "Dynamic Defensive Pressure": the AI recognises time-wasting and raises pressure — [OSFTW first look](https://www.osftw.com/features/1627/first-look-at-fifa-soccer-14/?page=2) **[snippet]**
- Game Informer preview: defenders "man-mark tighter… and exhibit less confusion about which players to defend in open space" — [Game Informer](https://gameinformer.com/games/fifa_14/b/xbox360/archive/2013/04/17/a-fine-strike.aspx) **[snippet]**
- **Second-man press (teammate contain):** holding R1/RB asks *one* AI teammate to press the carrier. If the AI picks a centre-back as the second man, the striker is left free — [Gamereactor FIFA 15 guide](https://mtest.gamereactor.eu/articles/265684/How+to+Defend+Properly+in+FIFA+15/) **[snippet]**. An FC 26 guide advises never using it with CBs because it pulls them out of position — [games.gg (KO)](https://games.gg/ko/ea-sports-fc-26/guides/ea-fc-26-수비-가이드-포지셔닝-조키-스킬-카운터/) **[snippet]**
- An EA FC developer (Michael Jones) lists "CPU AI, player positioning, passing, set pieces…" among his core systems in a GDC speaker bio. I could not find talk content — [GDC schedule](https://schedule.gdconf.com/speaker/jones-mike/81744) **[snippet]**

### Inferences
- EA's pressing structure is effectively **1 engager (human-controlled or the CPU's chosen presser) + an optional 1 AI "second man" contain + everyone else holding formation slots and lanes**. "Double contain" and multi-press are explicit CPU choices, not the default. This is a coordination-token pattern (at most N engagers).
- The defensive line is a **team-level object**: a height parameter plus a policy for runners (drop with the run / hold / step up for offside). It is not per-player behaviour. The repeated complaints ("pushes up too far", "drops into six-yard box") show that the line-vs-runner policy is the hardest part to tune.
- Familiarity scales *distances and reaction times* inside positioning formulas (10–40%). This is a cheap template for "learned skill" in WildPitch: the same formula, with competence scaling the parameters.
- Even EA prevents ball-chasing mainly by having AI teammates **not tackle**: they position, mark from distance and cut lanes.

### Gaps
- No official EA numbers for marking distances, how often the press is triggered, how many players press in Team Press, or the internal positioning formula. The EA "Tactical Defending explained" page returned 404.
- FIFA 14's "Positioning Personality" / "Player Personality" could not be confirmed from a primary source in this pass.
- No public EA GDC talk with positioning internals was found.

---

## 2. PES / eFootball: team play, advanced instructions, fluid formation, who presses

### Takeaway
Konami exposes team shape as **sliders plus toggled "Advanced Instructions"** (up to 2 attacking and 2 defensive at once). Fluid Formation allows separate kick-off, in-possession and out-of-possession shapes. The player can call 1 presser, or 2–3 pressers ("pressure in numbers"), and the manual warns that this opens space. "Containment Area" is a team setting that chooses the zone (middle or flanks) where the team funnels opponents.

### Cited Findings
- PES 2019 manual: "Call for Pressure" (hold X) makes the teammate under the Next Player Indicator press. "Call for Pressure in Numbers" (press X twice and hold) gets **2 to 3 players** pressing, which "can potentially lead to large spaces opening up for your opponent to exploit". The D-pad moves the defensive line up or back, and a double-tap sets a short offside-trap push-up — [Konami PES 2019 manual: Team control](https://dds.konami.com/games/manual/pes2019/PC/en/control_team.html) **[full]**
- PES 2019 manual: Team Instructions let you "choose areas of the pitch to build up play in attack or to contain the opposition in defence". **Fluid Formation** switches between Kick-off, When in Possession and When out of Possession formations. Advanced Instructions: up to two attacking and two defensive, toggled in match. Some teams have built-in instructions that trigger automatically — [Konami PES 2019 manual: Game Plan](https://dds.konami.com/games/manual/pes2019/PC/en/plan.html) **[full]**
- Advanced Instructions (fan guide quoting in-game text): **Gegenpress**: "Players around the ball carrier will start to put pressure on him." **Tiki-Taka**: "players will move into space around you to provide passing options." **False No. 9**: the CF drops into midfield, pulling defenders. **Anchoring**: keep a player near his position. **Defensive**: one chosen player stays back and doesn't join the attack. **Counter Attack (target)**: one player stays up front while defending. **Tight Marking**: a target "will be marked by the player closest to him." **Deep Defensive Line**: pull the line toward goal to stop through balls. **Swarm the Box**: crowd the box. **Wing Back**: wide midfielders track back. **Attacking Full Backs**, **False Full Backs**, **Wing Rotation**, **Hug the Touchline**, **Centering Targets** — [PES Mastery: Advanced Instructions](https://pesmastery.com/pes-advanced-instructions/) **[full]**
- Slider semantics (fan guide): **Support Range**: at 50 "players will stay around ball carrier to provide support", at 100 they keep far away for a long-passing game. **Defensive Line**: 100 = high (pairs with offside trap), 0 = deep — [PES Mastery: Team Style](https://pesmastery.com/pes-team-style/) **[snippet]**
- **Containment Area** set to "Middle" forces opponents to attack centrally (the alternative is funnelling wide) — [Red Bull PES 2019 formation guide](https://www.redbull.com/ie-en/best-pes-2019-formation-guide) **[snippet]**
- **Compactness** = how narrow or wide the defensive shape is (PES 2015) — [OSFTW PES 2015 defensive tactics guide](https://www.osftw.com/features/2080/pes-2015-defensive-tactics-guide) **[snippet]**
- eFootball "Defensive Engagement" is a player attribute: willingness to take part in defending without the ball — [PES Mastery eFootball defending tutorial](https://pesmastery.com/efootball-defending-tutorial/) **[snippet]**. The eFootball team playstyle decides how AI teammates move and press across defence, build-up, attack and transition — [FIFPlay eFootball 2026 team playstyles](https://www.fifplay.com/efootball-2026-team-playstyles/); [gamemarket.gg](https://gamemarket.gg/news/efootball/efootball-team-playstyles-explained-how-each-tactic-works) **[snippet]**

### Inferences
- Konami's levers map cleanly onto parameters: line height (0–100), compactness (width), support range (distance of support players from the carrier), pressuring intensity, containment zone (which lane to force the carrier into), plus per-player overrides (anchor, stay back, stay forward, man-mark target).
- "Pressure in numbers = 2–3" with an explicit warning is a strong hint that the default engagement budget is 1, with 2–3 as the aggressive maximum.

### Gaps
- The official manual's slider definitions (Support Range, Pressuring, Containment Area, Defensive Style "Frontline Pressure / All-out Defence") were not on the fetched page. I relied on fan guides via snippets.
- I found no source on "Team Spirit"'s actual effect on positioning, or on "Player Index", in this pass.

---

## 3. Football Manager match engine: roles, duties, team instructions

### Takeaway
FM separates **where the block sits** (Line of Engagement, the height at which pressing starts) from **where the back line sits** (Defensive Line) and from **how compact the block is** (width plus vertical gaps). Role and duty modify each player's tendency to stay or roam. Pressing is triggered by "trigger press" on specific opponents and capped by the line of engagement. All sources here are community guides, not SI internals.

### Cited Findings
- A higher Line of Engagement lets you trigger-press opposition defenders. A lower one lets the team regain shape and only trigger-press attackers. Trigger Press (Always / Never / none): the team holds shape "until that player receives the ball" — [FM24 guide (search summary)](https://www.fmscout.com/a-how-to-press-in-football-manager.html) **[snippet]**
- Defensive line has four options (very high / higher / standard / lower). Standard line + higher LoE = a compact midfield block. The typical "gegenpress" combo is higher LoE + higher line + more urgent pressing + "step up more" — [mykhel FM24](https://www.mykhel.com/football/football-manager-2024-what-are-defensive-lines-255242.html); [passion4fm pressing](https://www.passion4fm.com/?p=17125) **[snippet]**
- Compactness = narrow horizontally plus short gaps between defence and midfield. Low-block guides pair it with many defend-duty roles — [SI community low-block guide](https://community.sports-interactive.com/forums/topic/567884-4-4-2-diamond-low-block-the-guide); [FourFourTwo: compact](https://fourfourtwo.com/features/compact-football-tactics-explained) **[snippet]**
- FM26 introduced separate in-possession and out-of-possession formations ("IPOOP") with OOP roles (for example, a wide-covering CM) that dictate defensive width — [FootballGPT FM26](https://footballgpt.co/guides/master-fm26-tactics-new-ipoop-roles-pressing-explained) **[snippet]**

### Inferences
- FM, FC 25/26 and eFootball all converged on **two shapes, one in possession and one out of possession**, with roles defining in-possession deviation. This is a stable industry pattern.
- A useful split for WildPitch tunables: `engage_line_x` (where the first presser activates), `def_line_x` (back line), `block_width` / `line_gap` (compactness).

### Gaps
- I found no official SI documentation of how the match engine computes positions (for example, "position zones" plus ball-relative offsets). The SI manual was not fetched.

---

## 4. GDC talks, dev write-ups and general techniques: anchors, ball-relative offsets, influence maps, role assignment, engagement limits

### Takeaway
I found no public GDC talk detailing FIFA/PES positioning. The best-documented techniques come from RoboCup and small indie write-ups. The core idea is that **formation positions are a function of ball position**, implemented in three ways: lookup tables (SWOS), interpolation over sampled ball positions with Delaunay triangulation (HELIOS), or ball-relative offsets. On top of this, **dynamic role exchange** swaps players into slots, and **influence/score maps** do local refinement. The "first man / second man / third man" ranking by distance to the ball is the simplest anti-ball-chasing rule.

### Cited Findings
- HELIOS (RoboCup 2D champion team) encodes each player's target position as a function of ball position. Sampled ball positions are the vertices of a Delaunay triangulation, each vertex stores every player's target, and positions in between are linearly interpolated inside the containing triangle. It is "light-weight", trainable from human instruction, and in a 2007 comparison the Delaunay+linear interpolation model gave the highest performance versus a Normalized Gaussian Network. HELIOS TDPs from 2008 to 2023 keep this model, and code is in helios-base — [HELIOS2013 TDP](https://staff.fnwi.uva.nl/a.visser/activities/robocup/RoboCup2013/Symposium/TeamDescriptionPapers/SoccerSimulation/Soccer2D/TDP_HELIOS2013.pdf); [Akiyama et al., JACIII 2007](https://www.fujipress.jp/jaciii/jc/jacii001100080998/); [Akiyama & Noda, RoboCup 2007 LNCS](https://rd.springer.com/chapter/10.1007/978-3-540-68847-1_38) **[snippet]**
- The alternative baseline, SBSP (Situation Based Strategic Positioning), takes the ball position as its main input. Dynamic Positioning and Role Exchange (DPRE) scores swaps using each player's distance to a strategic position and that position's importance — [arXiv 2312.16273 survey](https://arxiv.org/pdf/2312.16273) **[snippet]**
- Steel Circus (commercial 3v3 sports game) bots: influence maps are "the heart of our AI". The bot samples pseudo-random nearby points and scores them, moving "from one position to a nearby one that's a bit more advantageous". Ball-holder factors: toward the enemy goal or a clear shot, really avoid enemies, don't stand too close to teammates, keep clear pass lines, avoid the field edges. There are four states (own ball / teammate ball / enemy ball / free ball), and the teammate-ball state uses the "same rules… with slightly different weights". Tuning was done by printing scores live while playing. About a quarter of dev time went into "sensorium" helpers (ball travel prediction, best interceptor, threat evaluation). The total was about 90 h — [Philipp Seifried: AI in Steel Circus pt 1](https://philippseifried.com/blog/2019/12/ai-in-steel-circus-part-1/) **[full]**
- Rocket League rotation (human doctrine): first man challenges, second man supports, third man covers the long clear. Roles are assigned by distance to the ball and re-ranked continuously — [GINX 3v3 guide](https://www.ginx.tv/en/rocket-league-standard-guide-when-to-challenge-3v3-rotations-and-roles) **[snippet]**. Counterpoint: modern play is "situational, not rigid sequentiality" — [Dignitas: myth of the third man](https://dignitas.gg/articles/the-myth-of-the-third-man-in-rocket-league) **[snippet]**
- Mat Buckland, *Programming Game AI by Example*, includes a worked soccer example (often cited for "supporting spot" calculation). The forum pointer is in [gamedev.net thread](https://gamedev.net/forums/topic/567579-soccer-games-strategy/4631150) **[snippet]**
- SFU thesis: existing methods compute a player's desired position from the ball's location and their formation role. It studies optimal offensive positioning in a digital soccer game — [SFU etd2503](https://summit.sfu.ca/_flysystem/fedora/sfu_migrate/4118/etd2503.pdf) **[snippet]**

### Inferences
- A practical recipe consistent with all sources is: (1) shape target = f(ball position, phase) per slot, via a table or interpolation; (2) team-level line height and compactness clamp it; (3) assign at most 1 presser (+1 cover/second man, +N in an explicit press mode) by distance or time-to-ball ranking; (4) everyone else locally refines around their slot with a utility or influence score (pass lanes open, spacing from teammates, onside). Step 4 fits WildPitch's D-008 "candidate actions × utility" directly.
- Delaunay interpolation is deterministic and fixed-point friendly (barycentric weights), which is attractive for `src/sim/`.

### Gaps
- No GDC Vault talk on FIFA/PES/NBA 2K/Madden team positioning was found in this pass. I found no Game AI Pro sports chapter. I found no source on Madden zone coverage internals or NBA 2K help defence.
- I found no source for Rocket League's built-in bot logic, only human rotation doctrine.

---

## 5. Sensible Soccer / SWOS and Kick Off: ball-position lookup tables

### Takeaway
SWOS uses a pure lookup table per formation. The pitch is split into **35 ball zones**, and for each of the **10 outfield players** the table stores one target spot out of a grid of **240 positions**. The goalkeeper is excluded. Each player runs to the spot looked up for the ball's current zone. This is the classic, cheap, and very readable way to keep shape.

### Cited Findings
- SWOS manual (quoted in a forum analysis): tactics are "based on a grid which defines 35 separate areas of the pitch that the ball can be in at any one time" and "240 separate positions that any one player can be in at any one time" — [SWOS United: tactic file hex analysis](https://www.swosunited.com/forum/editing-programming/2136-tactic-file-hex-analysis) **[full]**
- .TAC file = 370 bytes: 8-byte name, then 350 bytes = 10 players × 35 ball areas ("define where the players should run to"), then a trailer the author suspects is a validation "magic number". Each player's 35 entries are contiguous. Offset = Player×35 − 26 + (BallArea−1). Each entry is one byte: high nibble X, low nibble Y. Example: player 8 (left winger) with the ball in area 35 has value 0x7F, which is "right in the opponents goal mouth" (an attacking run encoded in the table). The author says the area and position diagrams are only "approximately correct" — [same](https://www.swosunited.com/forum/editing-programming/2136-tactic-file-hex-analysis) **[full]**
- The SWOS manual is mirrored at worldofstuart (tactics chapter) — [manual6.htm](https://worldofstuart.excellentcontent.com/swos/manual6.htm) **[snippet; not fetchable]**
- A Jon Hare gameplay-mechanics Q&A exists on sensiblesoccer.de — [thread](https://sensiblesoccer.de/forum/swos-related/24921-i-bring-you-the-word-of-go-ehm-mr-jon-hare-gameplay-mechanics-q-amp-a-session) **[not fetchable; content unknown]**

### Inferences
- 35 zones × 10 players × 1 byte = 350 bytes per formation, so the whole "shape brain" is tiny and could be human-edited or *learned* (drills producing tables). Attacking runs (a winger into the goalmouth when the ball is in the corner) are baked into the table, so runs and shape come from one mechanism.
- Discrete zones cause snapping when the ball crosses a zone border. HELIOS-style interpolation is the smooth generalisation of the same idea.
- One byte per entry fits the X/Y nibbles exactly (16 values per axis = 256 slots). 240 usable positions suggests about 16×15 used, but that is my inference, not confirmed.

### Gaps
- I found no source on Kick Off / Kick Off 2 positioning logic.
- I couldn't access Jon Hare's own description of how the controlled player / ball chaser is chosen in SWOS, or how CPU teams decide to tackle.
