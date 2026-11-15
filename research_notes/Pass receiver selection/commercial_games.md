# Pass receiver selection in commercial football games

Scope: how commercial football games turn stick, power and button input into a choice of team-mate (EA FC/FIFA incl. Mobile, PES/eFootball, Rematch, SWOS, Mario Strikers). Research budget: about 28 tool calls. Several primary EA pages (forums.ea.com, help.ea.com, some ea.com news URLs) returned 403/404/timeout, so a few claims are **snippet-only** (taken from search-result summaries and not confirmed by a full page fetch). These are marked [snippet-only].

No vendor has published the actual scoring formula (weights, cone angles, tie-breaks). Everything below is either an official one-line description, a patch note that shows which levers exist, or community observation.

---

## 1. Assisted, semi and manual: how is the receiver picked?

### Takeaway
The EA FC/FIFA and PES/eFootball families use the same basic model. **Stick direction gives a cone of candidates, and pass power picks how far along that direction the receiver is.** "Assisted" means the game snaps to a receiver (and assists power). "Manual" means the ball goes exactly where the stick points with no receiver snap. Newer EA titles add situational inputs: a "passing lane" (the gap between two defenders) for semi-assisted through balls, and a bias toward "open" players. eFootball's optional Smart Assist re-targets passes to open players or runners. Sensible-style arcade games use a direction cone whose width scales with the passer's skill.

### Cited findings
**EA FC / FIFA**
- Assistance tiers, as described by a fan settings guide: on Semi, "pass power is assisted, but AI will only help you a little with the direction". On Manual, "the direction you point is the direction the pass goes". [snippet-only] — [FIFA UTeam FC 24 controller settings](https://fifauteam.com/?p=231484)
- **Power picks distance:** a community tutorial says FIFA's passing assistance is "more power based than direction". Holding power for 3 bars "makes the game think you want a player further away" than a 1-bar hold, even if you aim at a closer team-mate. The tutorial numbers candidates along the aim line from the "nearest player (no.1)" outward as 2, 3, 4. — [FIFA Addiction: How to master passing](https://fifaaddiction.com/fifa-tutorials/fifa-attacking-tutorial/how-to-master-passing-in-fifa)
- FIFA Mobile ("Advanced Passing" deep dive): a search snippet quoted "High Power will now consistently select the further teammate as the receiver", applying to ground and lofted through passes once power passes about 50%. [snippet-only: the page fetch returned unrelated landing content, and an exact-phrase search found nothing, so treat as unverified] — [EA FIFA Mobile Advanced Passing deep dive](https://ea.com/games/fifa/news/gameplay-advanced-passing)
- FC 25 Gameplay Refresh patch (official), confirming that power and receiver choice are coupled and get tuned: "Low powered Ground Passes were sometimes being delivered to farther players than intended." Also "Slightly increased accuracy of short Ground Passes to **open players**", which suggests open-ness enters the targeting or accuracy model. And "Sometimes, free kick passes were not hit towards the intended receiver if the receiver was moving too quickly", which suggests the pass leads a predicted receiver position. — [EA FC 25 Pitch Notes: Gameplay Refresh](https://ea.com/games/ea-sports-fc/fc-25/news/fc-25-pitch-notes-gameplay-refresh-update)
- FC 26 "Passing lanes" for semi-assisted through passes: "This is a new concept introduced for Semi-Assisted Through Passes in FC 26 … aiming at a specific passing lane/gap between two defenders will decide the target selection of your pass receiver … to make sure the ball goes to the player you intended." This is a paraphrase of EA's FC 26 Gameplay Deep Dive; the EA page itself 404'd for me. — [FUT.GG: Top 10 gameplay changes in FC 26](https://www.fut.gg/news/top-10-gameplay-changes-coming-to-ea-sports-fc-26/); original listed as [EA forums: FC 26 Pitch Notes – Gameplay Deep Dive](https://forums.ea.com/blog/ea-sports-fc-game-info-hub-en/ea-sports-fc-26--pitch-notes---gameplay-deep-dive/12371925) (not fetched, 403)
- The FC 26 deep dive as summarised by press: "Through passes have been overhauled too, with the receivers better selected for ground and through passes," and "Lob passes will see the ball go into areas where there are fewer obstructions." — [TheSixthAxis FC 26 deep dive](https://www.thesixthaxis.com/2025/07/22/ea-sports-fc-26-deep-dive-explains-the-gameplay-changes/)
- FIFA 20 patch 1.05 (official note via press): "Changed the CPU's passing logic to result in better passing target selection by the CPU AI Controlled Player". AI team-mates use their own target-selection logic, which is tuned separately. [snippet-only] — [GameRevolution FIFA 20 1.05 patch notes](https://www.gamerevolution.com/?p=607318)
- FC 24 Precision Pass (R1/RB plus pass) is a manual-aim mode with a trajectory indicator line. "The receiving player moves toward the ball's destination", so the receiver is assigned *after* the aim point, not before. It has a minimum pass power of 40. [snippet-only summary of the EA deep dive] — [EA FC 24 Gameplay Deep Dive](https://ea.com/games/ea-sports-fc/news/fc-24-gameplay-deepdive); [GGRecon precision pass guide](https://ggrecon.com/guides/ea-sports-fc-24-precision-pass)

**PES / eFootball**
- Passing assistance is a numbered level, **default 3**. "Level 1 gives you a high level of AI assistance, but your passes may not reach the intended teammate, especially if there are 2 player in the same generic direction. Level 4 means you pass with less assistance." Levels 0 and 2 are not described. Note the counter-intuitive wording: higher assistance means more snapping, which means more wrong-player snaps when candidates line up. — [PES Mastery eFootball passing tutorial](https://pesmastery.com/efootball-passing-tutorial/)
- The same tutorial's aiming advice: low pass, "Aim in the direction of the teammate". Through pass, aim "at a spot in front of your teammate, rather than directly at him". Pass power is charged by holding the button. — [PES Mastery](https://pesmastery.com/efootball-passing-tutorial/)
- **Smart Assist** (official Konami): when attacking, "the target of passes will change to players who are open depending on the situation, such as if an opponent is blocking a passing lane or a teammate is making a run behind the defense." It is an explicit override of the stick target based on lane-blocked and runner checks. — [Konami eFootball overview](https://www.konami.com/efootball/en/page/overview); [Konami v4.4.0 info](https://www.konami.com/efootball/en/page/v4/versioninfo_v4-40)
- Smart Assist is controversial in PvP. There was a player petition to remove it, and Konami later added matchmaking filtering by Smart Assist setting (May 2026, v5.5.0, per secondary reports). — [change.org petition](https://www.change.org/p/we-demand-that-konami-remove-smart-assist-from-efootball); [players.com.ua](https://players.com.ua/en/news/konami-to-test-matchmaking-based-on-smart-assist-settings-in-efootball/)

**Sensible World of Soccer (Jon Hare, Q&A)**
- "Passing skill opens up the length of the pass and also the width of the passing cone." The candidate cone is attribute-scaled. — [sensiblesoccer.de Jon Hare Q&A](https://www.sensiblesoccer.de/forum/swos-related/24921-i-bring-you-the-word-of-go-ehm-mr-jon-hare-gameplay-mechanics-q-amp-a-session)
- "Next player selection is made based on the direction that you are pressing the joystick in." With a neutral stick, the game may keep using the last pressed direction. — same source

**Rematch (Sloclap)**: there is no receiver selection at all. The pass ("tap") is aimed 360° by the left stick (the camera aims shots only). Holding adds power, up to 105 km/h. Tapping backward is weaker than forward, and tapping while sprinting is stronger. The official text mentions no auto-targeting. — [Play Rematch: Feature focus – Tap mechanics](https://www.playrematch.com/post/feature-focus-tap-mechanics)

**Mario Strikers: Battle League**: normal pass (B) and lob (Y) target a team-mate. Holding the button and moving the stick moves the target to another team-mate or a location, and the pass goes on release. "Free Pass" (ZL+B) and "Free Lob" (ZL+Y) deliberately send the ball into space with no receiver. [secondary guides, possibly outdated] — [Shacknews controls](https://shacknews.com/article/131091/mario-strikers-battle-league-controls); [The Nerd Stash free pass](https://thenerdstash.com/how-to-get-the-free-pass-in-mario-strikers-battle-league/)

### Inferences
- The common recipe is (a) candidate filter: team-mates inside an angular cone around the stick direction, with cone width possibly scaled by the passer's skill (SWOS), (b) power maps to an expected distance, and (c) the best candidate is the one whose distance and angle best match. A one-dimensional "rank along the aim line by power" (FIFA Addiction's 1-2-3-4) is the simplest form.
- Through balls target the *receiver's predicted run* (aim "in front"), and the receiver is chosen by the gap the stick points through (FC 26).
- Manual and precision modes avoid the selection problem: the ball goes to a point, and a receiver is assigned afterwards (the closest team-mate who can reach it).

### Gaps
- No published cone angles, weights or scoring formula from EA or Konami.
- I could not confirm from primary text whether EA uses passer facing, camera orientation or the Vision attribute in target selection. I found no source.
- eFootball levels 0 and 2 are undocumented in the sources I found.

---

## 2. Inputs used: predicted runs, open-ness, attributes, facing, camera

### Takeaway
Confirmed inputs are stick direction, power (button hold), pass type (ground, through, lob, driven), receiver motion (lead and prediction), open-ness and lane blocking (FC 25/26 and eFootball Smart Assist), and passer attributes (SWOS cone width; FC Mobile accuracy error scales with Long Passing). I found no source for camera or facing.

### Cited findings
- Receiver motion is used: FC 25 fixed free-kick passes missing "if the receiver was moving too quickly". — [EA FC 25 Gameplay Refresh](https://ea.com/games/ea-sports-fc/fc-25/news/fc-25-pitch-notes-gameplay-refresh-update)
- Open-ness is used: "accuracy of short Ground Passes to open players" was tuned in FC 25 (same source). eFootball Smart Assist checks lanes blocked by opponents and runs behind the defence — [Konami overview](https://www.konami.com/efootball/en/page/overview).
- Lane geometry is used: FC 26 semi-assisted through passes choose the receiver by the gap between two defenders — [FUT.GG](https://www.fut.gg/news/top-10-gameplay-changes-coming-to-ea-sports-fc-26/).
- Attributes:
  - SWOS: passing skill widens the cone and lengthens the range — [Jon Hare Q&A](https://www.sensiblesoccer.de/forum/swos-related/24921-i-bring-you-the-word-of-go-ehm-mr-jon-hare-gameplay-mechanics-q-amp-a-session).
  - FC Mobile 26: "the further and higher the pass, the greater the accuracy error", and mid-to-long lofted through passes have lower accuracy based on Long Passing. [snippet-only] — [EA FC Mobile 26 gameplay update](https://www.ea.com/games/ea-sports-fc/fc-mobile/news/fc-mobile-26-update-gameplay)
- Rematch: power depends on the passer's motion (sprint stronger, backward weaker) — [Play Rematch](https://www.playrematch.com/post/feature-focus-tap-mechanics).

### Inferences
- Attributes in EA/Konami mainly affect *execution error* (accuracy, pace) after the target is chosen, not which target is chosen. SWOS is the documented exception, where skill widens the cone.

### Gaps
- No source on the camera-relative versus pitch-relative stick mapping for pass targeting. All these games appear to map the stick to screen directions, but I found no citation.

---

## 3. Back-passes and passes to the goalkeeper

### Takeaway
I found **no primary source** describing a special back-pass or GK-avoidance rule (for example "require the stick nearly straight back to target the keeper") in any of these games. Searches for patch notes about accidental back-passes or GK passes returned nothing relevant.

### Cited findings
- Rematch weakens backward taps relative to forward ones, which is a physical, not a targeting, asymmetry — [Play Rematch](https://www.playrematch.com/post/feature-focus-tap-mechanics).

### Inferences
- If a game wants to prevent accidental keeper passes, a sensible design (not sourced) is a narrower cone or penalty for backward and GK candidates. Combined with the "power picks distance" rule, this would explain community complaints that a hard pass "skips" a near player and finds a deep one, sometimes behind.

### Gaps
- No found citation for EA or Konami GK or back-pass handling. The EA forum threads that might document it ("passes keep locking on to wrong player", "Passing AI consistently chooses incorrect and illogical recipients") returned HTTP 403 and could not be read.

---

## 4. No team-mate in the stick direction

### Takeaway
Manual and free-pass modes send the ball into space along the stick (Rematch always does; Mario Strikers has dedicated Free Pass and Free Lob buttons; EA FC Precision Pass goes to an aim point and the receiver then runs to it). For assisted modes I found no primary statement on the fallback.

### Cited findings
- Mario Strikers free pass: "kick the ball stronger towards a certain direction without having another teammate to receive it" — [The Nerd Stash](https://thenerdstash.com/how-to-get-the-free-pass-in-mario-strikers-battle-league/)
- FC 24 Precision Pass: the receiver moves toward the ball's destination [snippet-only] — [EA FC 24 deep dive](https://ea.com/games/ea-sports-fc/news/fc-24-gameplay-deepdive)
- FC 26: lob passes go "into areas where there are fewer obstructions", a space-targeted rather than player-targeted element — [TheSixthAxis](https://www.thesixthaxis.com/2025/07/22/ea-sports-fc-26-deep-dive-explains-the-gameplay-changes/)

### Gaps
- Whether assisted EA or eFootball passes fall back to the nearest team-mate, widen the cone, or kick into space when the cone is empty is undocumented in what I could access.

---

## 5. Receiver indicator / preview UI

### Takeaway
EA FC has a manual-aim trajectory line for Precision Pass and a "receiver lock" timing setting. Mario Strikers shows a movable target. I found no confirmed source for a named eFootball "target indicator".

### Cited findings
- FC 24 Precision Pass: "after pressing R1 / RB, a white indicator will appear". A guide also describes a "handy indicator line to show you the path and destination". [snippet-only] — [GGRecon](https://ggrecon.com/guides/ea-sports-fc-24-precision-pass); [Charlie Intel](https://charlieintel.com/ea-sports-fc/ea-fc-24-how-to-use-precision-passing-275867)
- EA controller settings include a receiver-lock option: the receiver locks "as soon as the pass button is released or the power bar reaches full power". This implies that while the power bar charges, the chosen receiver can change with stick and power. [snippet-only] — [FIFA UTeam FC 24 controller settings](https://fifauteam.com/?p=231484)
- Mario Strikers: moving the stick while holding pass moves the target marker among team-mates or locations, and the pass goes on release [secondary] — [The Nerd Stash](https://thenerdstash.com/how-to-get-the-free-pass-in-mario-strikers-battle-league/)
- SWOS: next-player selection is shown by direction (Hare). The red X and green tick markers are *not* pass indicators: they are "computer guesses" of AI performance — [Jon Hare Q&A](https://www.sensiblesoccer.de/forum/swos-related/24921-i-bring-you-the-word-of-go-ehm-mr-jon-hare-gameplay-mechanics-q-amp-a-session)

### Inferences
- The "receiver lock at release or full power" setting means EA re-evaluates the target continuously during charge-up, so the receiver is a function of (stick, current power) until it is locked.

### Gaps
- An exact EA "pass receiver indicator" setting name, and its eFootball equivalent, could not be confirmed from primary pages.

---

## 6. Known complaints and patch fixes

### Takeaway
The dominant complaint across EA titles is "the pass went to the wrong (often farther) player", driven by the power-to-distance coupling. EA's documented fix was FC 25's "low powered ground passes delivered to farther players than intended". Konami's documented weak point is ambiguity when two team-mates sit in the same direction at high assistance.

### Cited findings
- FC 25 bug-report thread "Passing AI consistently chooses incorrect and illogical recipients": the poster reports the game picks the wrong recipient roughly 8/10 times, ignoring near team-mates for long balls or far-side players. [snippet-only, page 403] — [EA forums](https://forums.ea.com/idea/fc-25-bug-reports-en/passing-ai-consistently-chooses-incorrect-and-illogical-recipients/12350344)
- Thread "Passes keep locking on to wrong player" (FC 25). Search summaries attribute it to too much power locking onto distant players; reducing power risks underhitting. [snippet-only, page 403] — [EA forums](https://forums.ea.com/discussions/fc-25-ultimate-team-en/passes-keep-locking-on-to-wrong-player/11458696)
- FC Mobile bug report titled "Power driven pass direction lock overrides manual input": the title alone shows that power-based target lock can override the stick [page 403, title only] — [EA forums](https://forums.ea.com/discussions/fc-mobile-bug-reports-en/power-driven-pass-direction-lock-overrides-manual-input/13575623)
- Official FC 25 fixes:
  - "Low powered Ground Passes were sometimes being delivered to farther players than intended."
  - Free-kick passes to fast-moving receivers.
  - Increased short-pass accuracy to open players.

  — [EA FC 25 Gameplay Refresh](https://ea.com/games/ea-sports-fc/fc-25/news/fc-25-pitch-notes-gameplay-refresh-update)
- FC 26 introduced passing lanes "to make sure the ball goes to the player you intended" for semi-assisted through balls — [FUT.GG](https://www.fut.gg/news/top-10-gameplay-changes-coming-to-ea-sports-fc-26/)
- eFootball: at high assistance (Level 1), passes may not reach the intended team-mate "especially if there are 2 player in the same generic direction" — [PES Mastery](https://pesmastery.com/efootball-passing-tutorial/). Smart Assist re-targeting is seen as taking control away in PvP — [change.org](https://www.change.org/p/we-demand-that-konami-remove-smart-assist-from-efootball).

### Inferences
- The failure modes come from the design: (1) collinear candidates make power the only tie-breaker, so a power mis-read gives the wrong player; (2) a fast-moving receiver needs lead prediction; (3) "smart" re-targeting to open players fights player intent. The fixes are narrower or lane-based selection, better lead, and power thresholds that favour the near player at low power.

### Gaps
- No found complaint or patch about "passes to the keeper" or "pass goes backwards" specifically (searches returned only real-world back-pass rule pages).
- Kick Off, Sociable Soccer and Football Manager 3D: no sources found within budget. A related indie note: Super Liquid Soccer's devlog copies Sensible Soccer's *player-switch* scheme (stick direction from the ball's viewpoint, closest player within a cone). That is switching, not passing, but the cone-plus-closest structure is the same — [Super Liquid Soccer devlog](https://punyrobot.itch.io/superliquidsoccer/devlog/352077/new-player-selection-scheme).
- A 2003 patent application, US20030171146 ("Quick passing feature for sports video games"), decouples the moving direction from the passing direction. The fetch was blocked (403), so its assignee and claims are unverified — [RPX insight](https://insight.rpxcorp.com/pat/US20030171146A1).
