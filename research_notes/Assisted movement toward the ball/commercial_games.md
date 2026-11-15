# How commercial football games assist the controlled player in reaching the ball

Scope: EA FIFA / EA Sports FC (FIFA 12–24, FC 24–26), PES / eFootball, classic games (Kick Off, Sensible Soccer, ISS / Winning Eleven). Researched 2026-11-15, 19 tool calls.

Source-quality legend: **[EA]/[Konami]** official; **[guide]** community settings guide (fifauteam, fifplay, pesmastery, etc.); **[forum]** player reports (community evidence, not verified); **[press]** interviews, previews and reviews. Several citations come from search-result snippets of pages that could not be fully fetched (EA redirected or returned 403/404). They are marked *(snippet)*.

---

## 1. FIFA / EA FC: what the movement-related assist settings actually do

### Takeaway
EA has no single "run to the ball" assist setting. Assisted movement is spread across several controller settings:
- **Auto Switching**: when the game takes over cursor selection.
- **Auto Switching Move Assistance**: a short window in which the newly switched player keeps running in his current direction before stick input takes over.
- **Pass Receiver Lock**: when the receiver is fixed during a pass.
- **Pass Block Assistance**: automatic interception attempts on nearby passes.
- **Ground / Through / Lob / Cross assistance**: these aim the ball to the receiver, not the receiver to the ball.

The receiver's actual run to meet the ball is built-in AI and animation behaviour. No setting exposes it, and EA has only described it loosely (for example, FC 24 Precision Pass receivers "attempt to move toward the ball's destination").

### Cited Findings
**Auto Switching** (FIFA 20–FC 26):
- Options are "Air Balls and Loose Balls", "Auto" and "Manual". The FIFA 21 default is **Manual** — [EA Accessibility, FIFA 21 controller settings](https://www.ea.com/able/resources/fifa/fifa-21/xbox-one/controller-settings)
- FC 26 option descriptions [guide]:
  - Air Balls and Loose Balls: "AI will only switch for you on Lobs and Crosses, and Loose Balls."
  - Auto: the AI picks the best player and you can override with the switch button.
  - Manual: "AI will never switch for you, you are on your own."
  - Source: [fifauteam FC 26 controller settings](https://fifauteam.com/fc-26-controller-settings/)
- Guides disagree on whether the air-ball option also covers loose balls. Some describe it as "Only on Air Balls" (lobs and crosses). The current menu label says "Air Balls and Loose Balls" — [fifauteam FC 25](https://fifauteam.com/fc-25-controller-settings/), [EA forum FC 25 thread](https://forums.ea.com/discussions/fc-25-ultimate-team-en/auto-switching-on-air-balls-and-loose-balls/11443191) *(snippet)*

**Auto Switching Move Assistance:**
- "keeps the player who you have auto switched to moving in their current direction for a period of time. This gives you time to orient your intended direction for the new player. The higher the assistance level, the longer the period of time the player will run." — [ginx FIFA 21 controller settings tutorial](https://www.ginx.tv/en/fifa-21-controller-settings-tutorial) *(snippet)*
- The FIFA 21 official list gives the options as None/Low/Medium/High, default **None** — [EA Accessibility FIFA 21](https://www.ea.com/able/resources/fifa/fifa-21/xbox-one/controller-settings)
- FC 25/26 guides list only None/Low/High. "None disables it entirely." — [fifauteam FC 26](https://fifauteam.com/fc-26-controller-settings/)
- A fifplay preset for FC 25 sets it to High under the high-assist layout and None/Low under manual — [fifplay FC 25 controller settings](https://www.fifplay.com/fc-25-controller-settings/) *(snippet)*
- FIFA 17 and FIFA 19 guides list the default as None — [fifplay FIFA 17](https://www.fifplay.com/fifa-17-manual-controls/), [fifplay FIFA 19](https://www.fifplay.com/fifa-19-manual-controls/) *(snippets)*
- EA has given no durations in seconds or ticks (see Gaps).
- An FC 24 forum thread is titled "auto switch move assistance setting doesn't work" — [EA forum](https://forums.ea.com/discussions/fc-24-technical-issues-en/auto-switch-move-assistance-setting-doesnt-work/8001377). The page returned 403, so only the title is known.

**Pass Receiver Lock** (added around FIFA 20). It sets *when* the intended receiver is fixed:
- **Late** (default): "locked as late as possible, close to the time when the ball is kicked"
- **Animation Start**: "locked early, at the time when the kicking animation begins"
- **Power Up**: locks when the pass button is released or the power bar fills, the earliest option
- Sources: [fifauteam FC 26](https://fifauteam.com/fc-26-controller-settings/); FIFA 20 had only Early/Late, per [player.one FIFA 20 guide](https://player.one/fifa-20-guide-customize-gameplay-new-controller-settings-130258) *(snippet)*. EA's FIFA 21 page says only "Sets when the pass receiver locks. Default: Late" — [EA Accessibility](https://www.ea.com/able/resources/fifa/fifa-21/xbox-one/controller-settings)

**Player Lock:**
- "Locks a player with the sticks; flicking the right stick switches away" (default On in FIFA 21) — [EA Accessibility FIFA 21](https://www.ea.com/able/resources/fifa/fifa-21/xbox-one/controller-settings)
- With it on, "the CPU AI will then take control of the on-ball player", so you can steer your off-ball player into space and call for the ball — [fifauteam FC 26](https://fifauteam.com/fc-26-controller-settings/)
- This is how FIFA lets a human make the receiving *run* themselves, in place of the auto-run.

**Pass Block Assistance:**
- On: AI and user-controlled players "attempt to intercept a pass automatically when the ball is close by". Off: "They will stay in position and you manually need to launch a block/tackle." — [fifauteam FC 26](https://fifauteam.com/fc-26-controller-settings/)
- Default On in FIFA 21 — [EA Accessibility](https://www.ea.com/able/resources/fifa/fifa-21/xbox-one/controller-settings)
- This is the clearest documented case of the game moving the *human-controlled* player toward a nearby ball without stick input.

**Pass assistance** (ball-side aim assist, not movement):
- Ground pass. Assisted: direction and power are assisted to reach the receiver's path. Semi: power assisted, direction only slightly. Manual: "The direction you point is the direction the pass goes."
- Through pass. Assisted, Semi ("partially assisted") and Manual.
- Lob pass. Assisted and Manual only, no Semi.
- Cross. Assisted (aimed toward a teammate, no power bar), Semi (aimed to front/back post areas, you set the power) and Manual.
- Sources: [fifauteam FC 26](https://fifauteam.com/fc-26-controller-settings/); in FIFA 21 the official text describes Lob Pass Assistance as setting "the consistency and strength of lob passes" — [EA Accessibility](https://www.ea.com/able/resources/fifa/fifa-21/xbox-one/controller-settings)

**Other settings:**
- Right Stick Switching: Adaptive, Classic or Player Rotation (FC 26) — [fifauteam FC 26](https://fifauteam.com/fc-26-controller-settings/). FIFA 21 offered "Player Relative" vs ball-relative reference — [EA Accessibility](https://www.ea.com/able/resources/fifa/fifa-21/xbox-one/controller-settings)
- No setting named "Trajectory assistance", "Receiver auto-movement" or "Assisted receiving" appears in the FIFA 21 official list or the FC 26 guide — [EA Accessibility](https://www.ea.com/able/resources/fifa/fifa-21/xbox-one/controller-settings), [fifauteam FC 26](https://fifauteam.com/fc-26-controller-settings/)

**Developer statements on the receiver:**
- **FC 24 Precision Pass:** "The player receiving a Precision Pass will attempt to move toward the ball's destination." — [EA FC 24 Gameplay Deep Dive](https://www.ea.com/games/ea-sports-fc/pitch-notes/news/fc-24-gameplay-deepdive) *(snippet; the full page fetch did not render this text)*
- **FC 24 PlayStyles:** Pinged Pass and Tiki Taka give "increased pass quality allowing pass receiver to have increased ball trapping control". Some PlayStyle passes travel faster "without impacting the trapping difficulty of the receiver". So pass quality feeds into the receiver's trap difficulty — same source *(snippet)*
- **FC 26 Semi-Assisted Through Passes:** "aiming at a specific passing lane/gap between two defenders will decide the target selection of your pass receiver" — [EA forums, FC 26 Pitch Notes Gameplay Deep Dive](https://forums.ea.com/blog/ea-sports-fc-game-info-hub-en/ea-sports-fc-26--pitch-notes---gameplay-deep-dive/12371925) *(snippet)*
- **FIFA 21 "Passing Lane Analysis":** AI teammates gain an "understanding of spaces to determine where the next best move to receive a pass is" — [EA FIFA 21 Gameplay Deep Dive](https://ea.com/news/pitch-notes-fifa-21-gameplay-features-deep-dive) *(snippet)*
- **FIFA Mobile, August 2022 (EA):**
  - "We've made improvements to defender movement when the ball is in air on long lobbed through passes, lobbed passes, and crosses."
  - The stated aim was to stop *switched* defenders "unintentionally running towards the ball instead of following the receiver."
  - The assist "will be enabled regardless of your switching setting."
  - Source: [EA FIFA Mobile gameplay improvements Aug 2022](https://ea.com/games/fifa/fifa-mobile/amp/news/gameplay-improvements-august-2022) *(snippet; the page now redirects to FC 27 news)*
  - This is a rare official admission that an auto-switched player's default behaviour is to run toward the ball, and that this can be wrong.
- **FIFA 13 First Touch Control (David Rutter, executive producer, 2012):**
  - "This year we wanted to remove that near perfect control, and introduce a system we call First Touch Control."
  - "when a ball comes towards you and it's got spin or velocity, and your body's not prepared... when it hits you, then it's going to be more difficult to control."
  - He called it the equivalent change to Tactical Defending the year before.
  - Sources: [extratime producer interview](https://extratime.com/articles/8893/fifa-13-the-producers-interview), [godisageek interview](https://godisageek.com/2012/05/interview-david-rutter-exec-producer-fifa-13/), [fifauteam First Touch Control](https://fifauteam.com/first-touch-control-fifa-13/) *(snippets)*
  - Preview summary: before FIFA 13, "even a level 50 defender could pluck the ball out of the air and bring it under control like Messi" — same sources.
- **FIFA 20 (community wish):** a commentator asked for an "early lock to pass receiver", i.e. "being able to control the receiver and actually move towards the ball, without having to manually switch". So before Pass Receiver Lock existed, the receiver was largely AI-driven until the ball arrived — [Operation Sports FIFA 20 Pitch Notes breakdown](https://www.operationsports.com/fifa-20-pitch-notes-breakdown) *(snippet)*

### Inferences
- FIFA's pipeline appears to work like this:
  1. Pass assist picks the target and leads the ball to the receiver's path.
  2. The receiver is locked (Late by default).
  3. The cursor moves to the receiver.
  4. Until the human gives meaningful stick input, AI or animation moves the receiver toward the ball's destination.
  5. Auto Switching Move Assistance holds the receiver's current run direction for a short, configurable time so a neutral or stale stick does not stop him.
  6. First-touch quality depends on pass pace and height, receiver stats and body readiness (FIFA 13+), with PlayStyles modifying it (FC 24+).
- This sequence is pieced together from the settings text and pitch notes. EA has never published it as a single algorithm.
- "Move Assistance" is effectively an **input-takeover delay**: the stick is ignored, or blended, for N ms after a switch. That points to a design lesson: after the cursor changes hands, the stick is still pointing where it pointed for the previous player, so stale input must not be applied at once.
- Pass Receiver Lock shows a trade-off: locking late lets the passer re-aim, but leaves less time for the receiver to start moving.

### Gaps
- No official numbers for Move Assistance duration per level, stick-deflection thresholds for takeover, or cone angles. No GDC talk on FIFA receiver movement was found. A GDC "Classic Game Postmortem: Kick Off" by Dino Dini exists ([GDC Vault](https://gdcvault.com/play/1019423/contactUs)) but its content was not retrieved.
- "Ball magnetism" or "magnet dribbling" is a community term. I found no EA statement defining it.
- HyperMotion receiving animations: no specific text was found.
- EA's own help article on passing ([help.ea.com how-to-pass](https://help.ea.com/nl/articles/ea-sports-fc/how-to-pass/)) was not fetched.

---

## 2. PES / eFootball

### Takeaway
Konami's documented assist is **Cursor Change**:
- **Assisted**: since eFootball 2023 v2.2.0 this switches automatically and often to a player near the ball who "suits the flow of play".
- **Semi-Auto**: the old Assisted.
- **Manual**: a manual switch moves the cursor to "the closest player in the chosen direction".

"Player Support" is a *team tactic* (how close teammates stay to the ball carrier), not a movement assist. Pass Support levels 0–4 are aim assist. Receiver movement is controlled with modifier buttons (trap direction, let the ball run, or abort the run to the ball) rather than an assist slider.

### Cited Findings
**Cursor Change (eFootball 2023 v2.2.0 patch notes, Konami):**
- "when Cursor Change is set to 'Assisted', the cursor will automatically switch to a player who is near the ball and more suited to the flow of the play"
- "Added a new Cursor Change option 'Semi-Auto', which is equivalent to the pre-v2.2.0 'Assisted' option"
- "the cursor will now automatically switch in a more frequent manner"
- Other options were adjusted so "cursor switches will be performed in a more intuitive manner"
- Source: [Konami eFootball 2023 v2.2.0 patch notes](https://www.konami.com/efootball/en/page/2023/season2_patch-notes_v2-20)
- The manual switch "will move to the closest player in the chosen direction" — [Konami eFootball controls manual (mobile)](https://www.konami.com/efootball/en/page/mobile_controller)
- On receiving, the manual's only line is "Face the Opponent's Goal | Release ⑩ → Press and hold ⑥ as the player goes to trap the ball". The phrase "as the player goes to trap the ball" implies the player moves to the ball on his own — same source

**Player Support** is a Team Style tactic: a higher value keeps players closer to the ball carrier, a lower value spreads them for long passing — [PES Mastery, Team Style](https://pesmastery.com/pes-team-style/) [guide]

**Pass Support level (0–4):**
- 0 = full manual. 4 = most assistance, but "your passes may not reach the intended teammate, especially if there are 2 player in the same generic direction". Default 2 — [PES Mastery passing tutorial](https://pesmastery.com/pes-passing-tutorial/) [guide, snippet]
- In an evoweb forum poll, about 60% of respondents used level 0 and nobody used level 4 — [evoweb thread](https://evoweb.uk/threads/what-pass-assistance-do-you-use.64594/page-2) [forum]

**Winning Eleven 9 / PES 5 era (2005) controls:**
- R1+R2 means "aborting the run to the ball, if we are running to the ball and we know it will leave off the pitch". This implies an **automatic run toward the incoming ball** that the player can cancel.
- R1 lets the ball through the legs ("letting the ball through") and R2 traps while turning toward goal.
- A direction held during receipt traps "and change[s] the posture in the selected direction without stopping".
- L1 switching follows the "manual, s-auto, fix" setting.
- Source: [gamepressure WE9 controls](https://www.gamepressure.com/worldsoccerwinningeleven9/control/zb281) [guide]

**ISS 64 era (1997):** "the computer will usually switch players for you, so only use [manual switching] if necessary" — [Mr. N64 ISS64 summary](https://www.winternet.com/~mr_n64/reviews/n64/iss64/iss64cont.html) *(snippet)*. On ISS 2 (import) defence, B moves the CPU-controlled player to the ball and L selects the nearest player — [Nintendo World Report](https://www.nintendoworldreport.com/review/3934) *(snippet)*

**Community evidence:**
- PES 2021 Steam: "When a normal ground pass is sent, the target stays still waiting for the ball to arrive at his feet." Workarounds suggested were through passes or manual control — [Steam discussion](https://steamcommunity.com/app/1259970/discussions/0/3048357185555532589) [forum]
- A Reddit user reported a bad experience with "assisted" cursor change, especially in defence (cited in the search summary; the thread was not fetched).

### Inferences
- PES treats moving toward the ball on receipt as a default AI action, with explicit buttons to *cancel* (R1+R2), *dummy* (R1) or *shape* the first touch (direction / R2). This is a "default assist plus override verbs" design rather than a slider.
- Konami's split of Assisted into "Assisted (more frequent)" and "Semi-Auto" suggests players disagree on switch frequency, so the game offers both.
- The PES 2021 complaint suggests ground-pass receivers often wait instead of attacking the ball, at least for some passes.

### Gaps
- "PES Shift" was not found as a real feature name. I found no source for it and treat it as nonexistent or misremembered.
- No Konami statement on how far receivers move or on any stick-tolerance numbers.
- No dedicated eFootball "assisted first touch" setting was found. First-touch quality appears to be stat- and animation-driven.

---

## 3. Classic games (Kick Off, Sensible Soccer, ISS, others)

### Takeaway
The classic Amiga games deliberately did **not** glue the ball to the player's feet. Kick Off (Dino Dini, 1989) and Sensible Soccer (1992) kicked the ball ahead of the runner and required the player to steer around it, so the control feel came from that looseness plus aftertouch (swerve applied just after the kick). Konami's ISS/WE line instead used heavy auto-switching and automatic runs to the ball with cancel buttons. I found no documentation of an explicit "snap toward the ball" steering assist in Kick Off or Sensible Soccer.

### Cited Findings
**Kick Off and Kick Off 2:**
- "the ball doesn't 'stick' to your players' feet, as it has been the convention in football games for years" — [Kick Off 2 fan page / MobyGames text, dazeland](https://dazeland.com/en/Amiga/Kick_Off2.html) *(snippet)*
- "players don't automatically dribble; you have to manipulate them around the ball. It's tricky, but fair and intuitive." — Kick Off 2 review *(snippet, same search)*
- Dino Dini (2016, on Kick Off Revival): "The ball is not stuck to your feet; you can shoot the way you want; aiming and swerving the ball will be 100% under your control." The article contrasts this with games where the ball would "magnetically stick to the foot of a player" — [TechRadar](https://techradar.com/news/gaming/forget-fifa-the-king-of-soccer-games-is-making-a-comeback-1306758) *(snippet)*
- "What made Kick Off so revolutionary was Dini's decision to detach the ball from the player's feet" — [nerdspan](https://nerdspan.com/the-kick-off-of-the-football-simulation-genre) *(snippet)*

**Sensible Soccer and SWOS:**
- "Where most other games favored the 'ball glued to feet' mechanic, Sensible Soccer preferred a looser system", so sudden turns at speed could leave the ball behind.
- Aftertouch: swerve "could be applied to the ball in varying degrees based on how soon after the kick they were pushed".
- Source: [Giant Bomb, Sensible Soccer series](https://www.giantbomb.com/wd/3030-27453) *(snippet)*
- Jon Hare credited Tehkan World Cup (1985) as the influence behind aftertouch — [Retro Gamer "History of Sensible Soccer"](https://gb.readly.com/magazines/retro-gamer-uk/2023-09-01/64eca5464e8df5a067aa1d3f) *(snippet)*
- A 2016 review said the original's dribbling "seems rather haphazard" — [forceforgood Sensible Soccer](https://forceforgood.co.uk/?p=9659) *(snippet)*
- SWOS players report the ball "sticks" more in the DOS version than the Amiga original. Others attribute this to a lower frame rate, which hints that stickiness is a per-tick, frame-rate-dependent capture check — [SWOS United forum](https://swosunited.com/forum/swos-2020/27633-ball-not-staying-with-player) [forum]

**Modern retro-styled game:** in Super Arcade Football, "there's an auto-pass at play, so the ball will tend to lock onto the player you're aiming at and land at their feet", and aftertouch still works in flight — [TheXboxHub review](https://www.thexboxhub.com/?p=137786) *(snippet)*

### Inferences
- In the classic games, reaching the ball is mostly a matter of **generous capture radii plus auto-switching to the nearest player**, not steering assists. Once the controlled player overlaps the ball it is "collected", or in Kick Off simply kicked forward.
- Running onto a long ball works because the game switches you to the nearest man and the 8-way stick only needs a rough direction.
- This is an inference from the "ball not stuck" descriptions and the SWOS frame-rate anecdote. No primary source states it.

### Gaps
- No primary documentation found for Super Sidekicks (SNK) or Goal! (Dini, 1993) on player steering.
- The Retro Gamer "Making of Kick Off 2" ([Everand](https://www.everand.com/article/581898500/The-Making-Of-Kick-Off-2)) and Dini's GDC postmortem were not readable here, though they may describe capture or steering details.
- Whether Sensible Soccer auto-moves the controlled player toward a loose ball when the stick is neutral is unverified.

---

## 4. Community complaints and praise; tuning trade-offs

### Takeaway
The recurring complaints mirror each other:
- **Auto-switch picks the wrong man**, often the one under the ball's flight rather than at its landing point.
- **Switched players run at the ball instead of tracking the runner.**
- **Receivers let passes roll past them or stand still.**
- **Auto-switch steals control during a run.**
- **Settings that seem not to work.**

Developers respond by narrowing when assists fire (air/loose balls only), adding takeover delays (Move Assistance), and letting players lock a man (Player Lock, Pass Receiver Lock options).

### Cited Findings
- FC 24 bug report: after an update, auto-switch on a pass went to a player "underneath the ball who is not close to landing point of the ball" — [EA forum FC 24 auto-select issues](https://forums.ea.com/discussions/fc-24-technical-issues-en/player-switching-auto-select-issues-after-latest-update/7985095) [forum, snippet]
- FC 25: air/loose-ball auto-switching "keeps switching a random player that is not even close to the ball"; "used to work 100% on EAFC 24" — [EA forum FC 25](https://forums.ea.com/discussions/fc-25-ultimate-team-en/auto-switching-on-air-balls-and-loose-balls/11443191) [forum, snippet]
- FC Mobile: the receiver lets the pass "roll right by him" until tapped — [EA forum FC Mobile passing bug](https://forums.ea.com/discussions/fc-mobile-general-discussion-en/passing-bug/8045033) [forum, snippet]
- FC 27 feedback: players "should not appear to allow the ball to pass through them without any attempt to interact with it" — [EA forum FC 27](https://forums.ea.com/idea/fc-27-bug-reports-en/fut---several-gameplay-issues/13741346) [forum, snippet]
- FC Mobile: on through balls "your player will try to jump over the ball", and the game "ALWAYS switches to the CB's or full backs" — [EA forum FC Mobile defense feedback](https://forums.ea.com/discussions/fc-mobile-limited-beta-3-en/defense-feedback-honest-feedback/12546076) [forum, snippet]
- FC 26: a thread titled "auto switching off but player still changes" — [EA forum FC 26](https://forums.ea.com/discussions/fc-26-general-discussion-en/need-an-explanation-auto-switching-off-but-player-still-changes/13567924). This is consistent with EA's Mobile note that some assists are on "regardless of your switching setting" [forum, title only]
- FIFA 19: players complained they "can't switch to the player nearest to the ball" — [GameRevolution FIFA 19 switching](https://www.gamerevolution.com/guides/486627-fifa-19-player-switching-nearest-player-not-working-glitch-fix) *(snippet)*
- EA's own fix for lobbed balls targeted switched defenders "unintentionally running towards the ball instead of following the receiver" — [EA FIFA Mobile Aug 2022](https://ea.com/games/fifa/fifa-mobile/amp/news/gameplay-improvements-august-2022) *(snippet)*
- Praise or preference: competitive PES players favour minimal assist (the evoweb poll above). FIFA guides recommend high Move Assistance for assisted layouts and None/Low for manual layouts — [fifplay FC 25](https://www.fifplay.com/fc-25-controller-settings/) *(snippet)*

### Inferences: tuning trade-offs relevant to a design
- **Target point:** aim at the ball's *landing or intercept point*, not its current position. "Under the ball" selection is the top FIFA complaint.
- **Stale stick after a switch:** without a short hold (Move Assistance), the new player obeys a stick direction meant for the old one. With too long a hold, the player "runs past the ball" or ignores the user.
- **Assist strength vs agency:** auto-running to the ball helps beginners but steals runs from experts. Games handle this with cancel buttons (PES R1+R2, R1 dummy) or lock modes (Player Lock).
- **Defensive air balls:** "go to the ball" is wrong for defenders who should track the runner, so the attractor should depend on context.
- **Receiver waiting vs attacking the ball:** a receiver standing still feels passive and lets defenders intercept. Moving toward the ball's destination (FC 24 Precision Pass) is the documented fix.
- **First touch quality:** this is a separate lever (FIFA 13 FTC). Assisting the run need not mean assisting the trap.

### Gaps
- No direct Reddit threads were fetched. Community evidence here comes from EA forums, Steam and evoweb.
- No quantitative stick-tolerance cones, takeover thresholds or assist durations were found for any game.

---

## 5. Quantitative details (angles, thresholds, durations)

### Takeaway
None of the commercial games publish numbers. The only discrete data points are the *levels* of each setting: Move Assistance None/Low/(Medium)/High; Pass Receiver Lock Late/Animation Start/Power Up; PES Pass Support 0–4.

### Cited Findings
- Move Assistance levels: None, Low, Medium and High in FIFA 21 ([EA Accessibility](https://www.ea.com/able/resources/fifa/fifa-21/xbox-one/controller-settings)); None, Low and High in FC 25/26 ([fifauteam FC 26](https://fifauteam.com/fc-26-controller-settings/)). The only stated rule is "the higher the level, the longer the run".
- Pass Receiver Lock timing events: at kick, at animation start, or at button release / full power — [fifauteam FC 26](https://fifauteam.com/fc-26-controller-settings/)
- PES Pass Support 0–4, default 2 — [PES Mastery](https://pesmastery.com/pes-passing-tutorial/) *(snippet)*

### Inferences
- Any values WildPitch picks (cone angle, hold time, takeover deflection) will need playtesting. There is no industry reference number to copy.

### Gaps
- No GDC/Game Developer article with FIFA/PES receiver numbers was found within this search budget. A future search could target GDC Vault "FIFA animation" / "Ignite" / "HyperMotion" talks and Konami Fox Engine talks.
