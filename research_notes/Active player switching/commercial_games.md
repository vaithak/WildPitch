# Active player switching ("cursor change") in commercial football games

Scope: EA Sports FC / FIFA (FIFA 20-22, FC 25-27), eFootball / PES, plus Mario Strikers: Battle League and Rematch.
Researched 2026-11-17. Note: FC 27 (released Sept 2026) is the current EA title; its settings are documented below
alongside the FC 25/26 ones. "Documented" = an official EA/Konami page or a guide quoting in-game help text.
"Inferred" = my reading, marked as such.

## 1. Which auto-switching and switching settings exist, and what each does

### Takeaway
EA exposes a large set of switching controls. **Auto Switching** sets *when* the AI may switch for you: Manual,
Only on Loose Balls, Only on Air Balls, On Air Balls and Loose Balls, or Automatic. **Auto Switching Move
Assistance** keeps the new player running in his old direction for a short time. **Pass Receiver Lock** sets
when the pass target is fixed. Two settings govern manual switching: **Next Player Switching** (the L1/LB rule:
Classic, Closest to Ball or Goal Side) and **Right Stick Switching** (Classic, Adaptive or Player Rotation,
measured from the player or the ball, with a sensitivity value). **Reaction Time Calibration** compensates for
human reaction lag. The **Next Player Switch Indicator** shows who L1 will select, and **Player Lock** keeps
control of an off-ball player. Konami's eFootball offers fewer options: Cursor Change (Assisted / Semi-Auto /
Manual), L1 to switch to a player near the ball, and a right-stick flick to "the closest player in the chosen
direction".

### Cited Findings
**EA FIFA / FC: Auto Switching**
- FIFA 22 official accessibility text: "Select to decide how you will switch between players during certain
  passes while playing. This option includes Manual, Only on Loose Balls, Only on Air Balls, Automatic, and On
  Air Balls and Loose Balls. The default is set to On Air Balls and Loose Balls."
  — [EA Accessibility, FIFA 22 PS5 Customise Controls](https://www.ea.com/able/resources/fifa/fifa-22/ps5/customise-controls-settings)
- FC 25 descriptions as a guide quotes them:
  - Air Balls and Loose Balls: "AI will only switch for you on Lobs and Crosses, and Loose Balls."
  - Auto: "AI will switch to the best player for you." You can still override with the switch button.
  - Manual: "AI will never switch for you, you are on your own."
  — [FIFAUTeam FC 25 Controller Settings](https://fifauteam.com/fc-25-controller-settings/)
- Only on Loose Balls switches "on Loose Balls and Crosses". Only on Air Balls switches on "Lobs and Crosses".
  — [FIFPlay FC 25 Controller Settings](https://www.fifplay.com/fc-25-controller-settings/)
- FC 27 in-game text, quoted by a guide: "When auto is selected, the game will automatically switch to the best
  player for you." The options are the same five. — [FIFAUTeam FC 27 Game Settings](https://fifauteam.com/?p=337819)
- The FC 27 default is reported inconsistently. FIFAUTeam marks **Only on Air Balls**
  ([FIFAUTeam FC 27](https://fifauteam.com/?p=337819)). DTGRE says "The default is On Air Balls and Loose Balls"
  ([DTGRE FC 27 switching](https://www.dtgre.com/2026/09/ea-fc-27-player-switching-settings-wrong-defender.html)).
  FIFA 22's official default was On Air Balls and Loose Balls ([EA Accessibility FIFA 22](https://www.ea.com/able/resources/fifa/fifa-22/ps5/customise-controls-settings)).

**EA: Auto Switching Move Assistance**
- FIFA 22 official text: "Select between 'None,' 'Low,' 'Medium,' and 'High' to keep the player you AUTO switch
  to moving in his current direction for a short period of time. This will allow you to orient your intended
  direction for the new player. The default is set to High."
  — [EA Accessibility FIFA 22](https://www.ea.com/able/resources/fifa/fifa-22/ps5/customise-controls-settings)
- FC 27: "keeps the player who you have auto switched to moving in their current direction for a period of
  time". The options are None, Low and High, and higher levels keep him running longer. FIFAUTeam marks None as
  the default. — [FIFAUTeam FC 27](https://fifauteam.com/?p=337819)
- DTGRE: "Move Assistance determines how long the newly selected player continues moving in their previous
  direction". None gives control immediately. — [DTGRE](https://www.dtgre.com/2026/09/ea-fc-27-player-switching-settings-wrong-defender.html)

**EA: Pass Receiver Lock** (when the switch target of your own pass is fixed)
- FIFA 22 had two options, Early and Late, with Late as the default.
  — [EA Accessibility FIFA 22](https://www.ea.com/able/resources/fifa/fifa-22/ps5/customise-controls-settings)
- FC 25 has three:
  - Late: "The pass receiver is locked as late as possible, close to the time when the ball is kicked."
  - Animation Start: "locked early, at the time when the kicking animation begins."
  - Power Up: "locked as soon as the pass button is released or the power bar reaches full power."
  — [FIFAUTeam FC 25](https://fifauteam.com/fc-25-controller-settings/)
- FC 27 keeps all three, with Late as the default. — [FIFAUTeam FC 27](https://fifauteam.com/?p=337819)

**EA: Right Stick Switching** (type, reference point, sensitivity)
- FIFA 22: the options are "Player Relative, Ball Relative, and Player Rotation". The setting decides "whether a
  player or the ball will act as the center point of reference for any Right Stick switching". The default is
  Player Relative. — [EA Accessibility FIFA 22](https://www.ea.com/able/resources/fifa/fifa-22/ps5/customise-controls-settings)
- FC 25 text:
  - Player Relative: "When defending, the player you control will act as the center point of reference for any
    Right Stick switching."
  - Ball Relative: "When defending, the ball will act as the center point of reference."
  — [FIFAUTeam FC 25](https://fifauteam.com/fc-25-controller-settings/)
- FC 27 splits this into two settings:
  - **Right Stick Switching**: Classic (the default), Adaptive or Player Rotation.
    - Adaptive: "holding the right stick for longer will switch to more distant players as soon as you release it."
    - Player Rotation: a flick moves the Next Player Switch Indicator but does not switch. L1/LB then completes
      the switch.
  - **RS Switching Reference**: Player Relative (the default) or Ball Relative.

  — [FIFAUTeam FC 27](https://fifauteam.com/?p=337819); [DTGRE](https://www.dtgre.com/2026/09/ea-fc-27-player-switching-settings-wrong-defender.html)
- RS Switching Sensitivity runs from 0 to 10: "The higher the sensitivity, the more the game will respect your
  directional input when selecting the next player." At low values the game favours nearby players even if you
  are not pointing at them. — [FIFAUTeam FC 27](https://fifauteam.com/?p=337819); same text for FC 25 in [FIFAUTeam FC 25](https://fifauteam.com/fc-25-controller-settings/)
- DTGRE gives the sensitivity default as 4 (unofficial). — [DTGRE](https://www.dtgre.com/2026/09/ea-fc-27-player-switching-settings-wrong-defender.html)

**EA: Next Player Switching** (the L1/LB rule; present from FC 25 on)
- FC 27 text: "While defending, press L1/LB to switch to the next-best available player on the pitch."
  - Classic (the default) "weighs several factors" ([FIFAUTeam FC 27](https://fifauteam.com/?p=337819)), or
    selects "a player based on multiple positional factors" ([DTGRE](https://www.dtgre.com/2026/09/ea-fc-27-player-switching-settings-wrong-defender.html)).
  - Closest to Ball favours the defender nearest the ball.
  - Goal Side "favors players between the ball and your goal".
- FC 25 and FC 26 settings guides list the same three options.
  — [FIFAGameNews FC 25](https://www.fifagamenews.com/?p=19411); [FIFAGameNews FC 26](https://www.fifagamenews.com/?p=19910)

**EA: Reaction Time Calibration** (FC 27 settings list; first edition not established)
- In-game text quoted by FIFAUTeam: "switching you to the player that was indicated when you initially thought
  about switching". It does not affect right-stick switching or how fast input is processed. Default On. A
  separate sensitivity slider (0-10) "fine tune[s] the duration of the Reaction Time Calibration window". The
  guide notes that the switch icon can change during this reaction window before you press.
  — [FIFAUTeam FC 27](https://fifauteam.com/?p=337819)
- DTGRE, attributing this to EA: it "accounts for possible delay between deciding to switch and pressing the
  button … When enabled, it can ignore the currently recommended player and switch to the previous recommendation
  instead". Sensitivity default 6. — [DTGRE](https://www.dtgre.com/2026/09/ea-fc-27-player-switching-settings-wrong-defender.html)

**EA: Next Player Switch Indicator and Player Lock**
- Indicator: "shows which player you'll swap controls with when pressing L1 button while defending". Default On
  (FIFA 22). — [EA Accessibility FIFA 22](https://www.ea.com/able/resources/fifa/fifa-22/ps5/customise-controls-settings)
- FC 27 adds a third option, "Only in Single Player", alongside On and Off. — [FIFAUTeam FC 27](https://fifauteam.com/?p=337819)
- Player Lock: "lock to a player using the left stick + right stick". The CPU then takes over the ball carrier
  while you move an off-ball player, and you leave with a right-stick flick. Default On.
  — [EA Accessibility FIFA 22](https://www.ea.com/able/resources/fifa/fifa-22/ps5/customise-controls-settings); [FIFAUTeam FC 25](https://fifauteam.com/fc-25-controller-settings/)

**Konami eFootball / PES**
- eFootball 2023 v2.2.0 official patch notes:
  - "when Cursor Change is set to 'Assisted', the cursor will automatically switch to a player who is near the
    ball and more suited to the flow of the play".
  - "When Cursor Change is set to 'Assisted', the cursor will now automatically switch in a more frequent manner."
  - New option "'Semi-Auto', which is equivalent to the pre-v2.2.0 'Assisted' option". Users who had Assisted
    were migrated to Semi-Auto.
  - "various adjustments to other Cursor Change options … so that cursor switches will be performed in a more
    intuitive manner."

  — [Konami eFootball 2023 v2.2.0 Patch Notes](https://www.konami.com/efootball/en/page/2023/season2_patch-notes_v2-20)
- PES 2019 official manual: "press [LB] to move the cursor onto a player who is close to the ball, or push the
  [R stick] towards the direction of the player you wish to take control of."
  — [Konami PES 2019 manual, Other Controls](https://dds.konami.com/games/manual/pes2019/PC/en-us/control_other.html)
- eFootball mobile manual: manual cursor change moves to the closest player in the chosen direction (as
  summarised in search results; I did not fetch the full page). — [Konami eFootball Controls Manual](https://www.konami.com/efootball/en/page/mobile_controller)
- A third-party guide mentions cursor display settings such as "Own Team Cursor Type" (e.g. Username in co-op)
  and a toggle for opponent player names. I did not verify these against Konami docs.
  — [GamingOnPhone eFootball settings guide](https://gamingonphone.com/guides/efootball-guide-to-the-best-gameplay-and-game-plan-settings-for-a-smooth-experience/)

**Others**
- Mario Strikers: Battle League:
  - L/ZL ("Switch Character") moves control to a teammate, "usually the one closer to the ball", when not in
    possession.
  - With Auto controls, the game switches automatically to the nearest character or the ball holder. In Manual,
    ZL/L switches to the pass receiver or the ball holder.
  - In multiplayer you cannot take a character a teammate is controlling.

  — [Twinfinite, MSBL how to switch](https://twinfinite.net/2022/06/mario-strikers-battle-league-how-to-switch-characters/); [Shacknews MSBL controls](https://shacknews.com/article/131091/mario-strikers-battle-league-controls)
- Rematch (Sloclap, 2025): each human controls one footballer, including the goalkeeper, in third person (5v5),
  so there is no cursor switching at all. — [Rematch (Wikipedia)](https://en.wikipedia.org/wiki/Rematch_(video_game)); [Gamereactor](https://www.gamereactor.eu/rematch-1562023/)

### Inferences
- EA treats switching as three separate problems:
  1. *When* the AI may switch: Auto Switching, a set of event classes (air balls, loose balls, all).
  2. *Whom* a user request picks: the L1 rule, plus right-stick direction, reference point and sensitivity.
  3. *How the handover feels*: Move Assistance momentum carry-over, Pass Receiver Lock timing, and Reaction Time
     Calibration latency compensation.
- The sensitivity slider describes a blend: score = w·(angle match to stick direction) + (1−w)·(proximity),
  where w comes from sensitivity. This is my inference from the text, not documented internals.
- Konami's v2.2.0 change shows a design trade-off. "Assisted" became more frequent and context-aware, and the
  old, calmer behaviour was kept as "Semi-Auto" because some users preferred fewer auto-switches.

### Gaps
- I found no official EA text for FC 26 itself (FC 25 and FC 27 texts are quoted through guides). The FC 27
  default for Auto Switching and Move Assistance conflicts between guides.
- I found no official Konami definition of Semi-Auto beyond "equivalent to the pre-v2.2.0 Assisted", and no
  definitive current list of eFootball cursor options or cursor-display settings.
- I could not verify UFL, Sensible Soccer or Goal! switching rules in this pass. For Sensible Soccer, my
  training-knowledge belief that control auto-passes to the player nearest the ball is unsourced, so it is
  omitted here.

## 2. When the games switch automatically, and how they avoid flicker

### Takeaway
The documented triggers are your own passes (switch to the receiver, locked at Late, Animation Start or Power
Up), lofted balls and crosses, loose balls, and in "Automatic" any situation where the AI judges another player
"best". eFootball "Assisted" switches to a player "near the ball and more suited to the flow of the play", and
older PES "Assisted" mostly switched only on passes. I found no published anti-flicker rule such as minimum
dwell or a neutral-stick requirement. The documented stabilisers are:
- event-gated auto-switching (choose a mode that switches only on air or loose balls);
- Move Assistance, which keeps the new player moving so a sudden switch doesn't cause a stop;
- Reaction Time Calibration, which switches to the previously indicated player if the recommendation changed
  inside a short reaction window. This is a hysteresis of sorts on the L1 target.

### Cited Findings
- Pass receiver switching is locked at one of three moments (Late, near the kick; Animation Start; Power Up,
  on release or at full power). — [FIFAUTeam FC 25](https://fifauteam.com/fc-25-controller-settings/)
- "Only on Air Balls" means lobs and crosses. "Only on Loose Balls" means loose balls and crosses.
  — [FIFPlay FC 25](https://www.fifplay.com/fc-25-controller-settings/); [FIFAUTeam FC 25](https://fifauteam.com/fc-25-controller-settings/)
- FC 27 update v1.2 pitch notes have a section "Loose Ball Switching Improvements": "We've also improved
  auto-switching across several loose ball scenarios" (no further detail retrieved).
  — [EA FC 27 Update v1.2 Pitch Notes](https://www.ea.com/games/ea-sports-fc/fc-27/news/update-v1-2-pitch-notes)
- FIFA 20 pitch notes (via Twinfinite) listed as known issues "the issue where a player will lock onto a loose
  ball", and the "general accuracy of auto-switching and right stick player switching".
  — [Twinfinite, planned FIFA 20 changes](https://twinfinite.net/news/planned-fifa-20-gameplay-changes-detailed/)
- Move Assistance exists "to keep the player you AUTO switch to moving in his current direction for a short
  period of time. This will allow you to orient your intended direction for the new player."
  — [EA Accessibility FIFA 22](https://www.ea.com/able/resources/fifa/fifa-22/ps5/customise-controls-settings)
- Reaction Time Calibration: "can ignore the currently recommended player and switch to the previous
  recommendation instead". The window can be tuned from 0 to 10.
  — [DTGRE](https://www.dtgre.com/2026/09/ea-fc-27-player-switching-settings-wrong-defender.html); [FIFAUTeam FC 27](https://fifauteam.com/?p=337819)
- eFootball Assisted switches automatically to a player "near the ball and more suited to the flow of the play",
  "in a more frequent manner" after v2.2.0. — [Konami v2.2.0](https://www.konami.com/efootball/en/page/2023/season2_patch-notes_v2-20)
- PES 2019 user report: with "Assisted", the cursor changed to the closest player essentially only when the
  opponent passed. If a CPU attacker dribbled at goal without passing, the cursor stayed on a distant defender,
  unlike FIFA, which "automatically switches cursor control to your closest player".
  — [Evoweb thread](https://evoweb.uk/threads/closest-player-switching-flaw-in-pes-2019.79328/)

### Inferences
- From the PES 2019 report and Konami's wording, older PES Assisted appears to be pass-triggered: it re-evaluates
  the cursor on opponent passes, not continuously. eFootball's newer Assisted re-evaluates more often. This is
  inferred.
- Defaults across both publishers suggest the industry norm: auto-switch on discrete ball events (pass, lob,
  loose ball) rather than continuously, and leave the user's own runner alone otherwise. Event gating itself
  prevents flicker.
- Reaction Time Calibration implies the game keeps a short history of the "recommended next player". It is good
  evidence that EA's L1 candidate changes often enough to cause mis-switches.

### Gaps
- I found no official statement of a minimum dwell time, a rule against switching while the stick is held, or
  whether the opponent-pass switch targets the predicted interceptor rather than the nearest player.
- Exact rules for shot rebounds are not documented beyond "loose balls".

## 3. Manual switching: the L1 rule, right-stick direction, the indicator

### Takeaway
L1/LB goes to the "next-best" player. PES documents this as "close to the ball". In EA from FC 25 the rule is
selectable: Classic (multi-factor), Closest to Ball or Goal Side. An on-screen indicator previews the L1 target.
A right-stick flick picks the nearest player in the flick direction, measured from your player or from the ball.
Optional modes are Adaptive (a longer hold reaches further) and Player Rotation (the flick moves the indicator,
L1 confirms).

### Cited Findings
- PES L1: "move the cursor onto a player who is close to the ball". Right stick: "towards the direction of the
  player you wish to take control of". — [Konami PES 2019 manual](https://dds.konami.com/games/manual/pes2019/PC/en-us/control_other.html)
- EA L1: "switch to the next-best available player". Classic, Closest to Ball, Goal Side ("between the ball and
  your goal"). — [FIFAUTeam FC 27](https://fifauteam.com/?p=337819); [DTGRE](https://www.dtgre.com/2026/09/ea-fc-27-player-switching-settings-wrong-defender.html)
- EA indicator: "shows to which player you'll switch to when pressing L1/LB while defending" (On / Only in
  Single Player / Off). — [FIFAUTeam FC 27](https://fifauteam.com/?p=337819)
- Classic right-stick switching: a flick "switches toward the next player in the direction of the flick".
  Adaptive "uses the duration of your Right Stick input to determine how far the switch should reach". Player
  Rotation moves the indicator. — [DTGRE](https://www.dtgre.com/2026/09/ea-fc-27-player-switching-settings-wrong-defender.html)
- Player-relative versus ball-relative: in player-relative a flick up picks whoever is above *your* player; in
  ball-relative, whoever is above the *ball*. — [FIFAUTeam FIFA 23](https://fifauteam.com/fifa-23-controller-settings/); [EA Accessibility FIFA 22](https://www.ea.com/able/resources/fifa/fifa-22/ps5/customise-controls-settings)
- Mario Strikers BL: L toggles to the teammate "usually closer to the ball". — [Twinfinite MSBL](https://twinfinite.net/2022/06/mario-strikers-battle-league-how-to-switch-characters/)

### Inferences
- Nobody documents a "cycling order" for repeated L1 presses. Repeated presses seem to re-evaluate the
  "next-best" excluding the current player, rather than step through a fixed list (inferred, not sourced).
- Goal Side is a ready-made heuristic for a defensive candidate scorer: prefer defenders between the ball and
  their own goal.

### Gaps
- Classic's "multiple positional factors" are not enumerated anywhere I found.

## 4. Community complaints, fixes, and pro recommendations

### Takeaway
The recurring complaints are:
- auto-switch picking a random or far player on air and loose balls;
- L1 picking the "wrong defender";
- PES Assisted not switching to the nearest defender against a dribbler.

The fixes have been EA's switching-accuracy work (FIFA 20 notes, FC 27 v1.2 loose-ball improvements), new L1
modes (Closest to Ball, Goal Side), and Konami's v2.2.0 rework. Common advice is to auto-switch only on air
balls (or air and loose balls) and keep everything else manual.

### Cited Findings
- EA forum (FC 25): a user reports that auto-switching on air and loose balls "keeps switching a random player
  that is not even close to the ball" (thread title "Auto switching on air balls and loose balls"; seen as a
  search snippet, the page returned 403). — [EA Forums thread](https://forums.ea.com/discussions/fc-25-ultimate-team-en/auto-switching-on-air-balls-and-loose-balls/11443191)
- FC 27 guide on fixing "wrong defender selection": try Closest to Ball for L1. Try Reaction Time Calibration
  Off. Use RS sensitivity 4-5, lowering it if switches are accidental.
  — [DTGRE](https://www.dtgre.com/2026/09/ea-fc-27-player-switching-settings-wrong-defender.html)
- FC 27 design context:
  - "A manually-controlled defender will be able to reach further than an AI-controlled defender."
  - "We've also reduced AI defensive reach for interceptions."
  - The game is "encouraging you to switch to the player actively defending", and Teammate Contain is
    "requiring the user to switch to them and manually defend".

  These changes make switching quality matter more. — [EA FC 27 Gameplay Deep Dive](https://www.ea.com/games/ea-sports-fc/fc-27/news/pitch-notes-fc27-gameplay-deep-dive)
- Recommendations vary:
  - Guides recommend "Only on Air Balls" or "On Air Balls and Loose Balls".
  - For reference point, some prefer Player Relative ("feels more natural"), others Ball Relative with
    sensitivity 6.

  — [FIFPlay FC 25](https://www.fifplay.com/fc-25-controller-settings/); [Destructoid FC 25](https://www.destructoid.com/best-controller-settings-for-ea-fc-25/)
- eFootball community: some find Assisted too aggressive or poor on defence and prefer Semi-Auto or Manual. A
  2026 guide recommends Semi-Auto as the balance. — [GamingOnPhone eFootball guide](https://gamingonphone.com/guides/efootball-guide-to-the-best-gameplay-and-game-plan-settings-for-a-smooth-experience/); [Reddit r/eFootball (mirror)](https://axebps-redlib.hf.space/r/eFootball/comments/1hyv6wt/controller_setting_psconsole_which_is_the_most)

### Inferences
- Skilled players reject continuous auto-switching. The common complaint is losing control of a player mid-run
  ("switching away while running"). This is consistent with Move Assistance's existence, but I found no direct
  quote of that specific complaint.

### Gaps
- I found no EA staff explanation in the forums (pages blocked), and no pro-player interview with stated reasons.

## 5. Documented internal heuristics

### Takeaway
Little is public. The documented signals are:
- proximity to the ball (PES L1, eFootball Assisted, EA Closest to Ball);
- "suited to the flow of the play" (eFootball);
- goal-side position, between ball and own goal (EA Goal Side);
- "multiple positional factors" (EA Classic);
- angular match with the stick direction against proximity, weighted by sensitivity (EA right stick);
- a short history of recommendations to absorb reaction lag (EA Reaction Time Calibration).

I found no public source (GDC talk or dev blog) describing predicted interception time or distance to the ball
path as the selection metric.

### Cited Findings
- See the quotes above: [Konami v2.2.0](https://www.konami.com/efootball/en/page/2023/season2_patch-notes_v2-20);
  [FIFAUTeam FC 27](https://fifauteam.com/?p=337819); [DTGRE](https://www.dtgre.com/2026/09/ea-fc-27-player-switching-settings-wrong-defender.html).

### Inferences
- A sensible clean-room design consistent with all of the above:
  - Score candidates by time-to-ball or time-to-intercept, plus a goal-side bonus and a facing term.
  - Show the top candidate as an indicator.
  - Switch automatically only on events: own pass (receiver locked at kick), lofted ball (landing-spot
    reacher), loose ball.
  - Use the indicator as it was a few hundred ms before the press (cf. Reaction Time Calibration).
  - Carry the new player's momentum briefly (cf. Move Assistance).

  This is my synthesis, not documented internals.

### Gaps
- I found no developer interviews or GDC talks on football-game switching heuristics.
