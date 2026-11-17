# Active player switching: design and engineering techniques

Scope: how team-sports games pick the human-controlled player, and how they stop the choice from flickering.
Main primary source: the open-source Google Research Football engine (gfootball, a fork of Bastiaan Konings'
GameplayFootball, Apache 2.0). Its code was downloaded and read directly; the quotes below are verbatim from
`master`. Commercial games (FIFA/EA FC, NHL, GOALS) are covered through their official settings text only. No
EA engine code or GDC talk on this topic was found.

Source files (all under `https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/`):
- team.cpp: `Team::Process`, `Team::UpdateSwitch`, `GetBestPossessionPlayer`, `SelectPlayer`
- AIsupport/AIfunctions.cpp: `AI_GetBestSwitchTargetPlayer`
- player/player.cpp: `UpdatePossessionStats`, the time-to-ball search
- player/controller/humancontroller.cpp: switch-bias steering blend

---

## 1. What do developers and open-source code actually do to pick the controlled player?

### Takeaway
gfootball uses a two-layer design. (a) Each frame the team keeps a **"designated possession player"**: the
teammate with the smallest *time to intercept the predicted ball path*. A **multiplicative hysteresis** guards
this choice, and so do bonuses for whoever has possession or human control. (b) The human cursor follows the
designated player **only** in three cases: the designated player owns the ball, a set piece is on, or the
designated player is **3x faster** to the ball than the human's current player. Otherwise the player switches
by hand with a button press. Commercial games (FIFA/FC) expose the same split as settings: auto-switch only on
ball events (air balls, loose balls), manual switching by button or right stick, and a visible next-player
indicator.

### Cited findings

**gfootball: time-to-ball is computed along the predicted ball path (intercept, not distance)**
- `Player` update (player.cpp) steps through the ball-prediction buffer. At each future time `ms` where the ball
  is low (`Predict(ms).coords[2] < 1.5f`, i.e. playable height < 1.5 m), it computes how long the player needs to
  run to that predicted ball position (`AI_GetTimeNeededForDistance_ms(GetPosition(), GetMovement(), ball@ms, GetMaxVelocity(), precise, ms)`).
  It takes the first `ms` where `timeNeeded <= ms`, i.e. the earliest point where the player can arrive no later
  than the ball. A coarse step speeds up the search: `timeStep_ms = clamp(round(balldist/50 m/s*1000),10,500)`.
  Once the intercept is bracketed it goes back and refines in 10 ms steps. It also keeps an "optimistic" variant.
  Fallback when no intercept is found inside the buffer: distance to the last predicted ball point divided by
  `GetMaxVelocity()*0.75`, starting from `GetPosition() + GetMovement()*0.2f` (position extrapolated 0.2 s).
  — [player.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/player.cpp)
- **The passer or shooter is excluded for 500 ms after the kick.** While the player's current function is
  ShortPass, LongPass, HighPass or Shot and no touch is pending, the search starts at `startTime_ms = 500`, and
  `hasPossession = false`. So the kicker cannot instantly win "closest to ball" back from the receiver. —
  [player.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/player.cpp)
- The full-precision time calculation runs only for the current designated player
  (`bool precise = (team->GetDesignatedTeamPossessionPlayer() == this)`), a cost-saving asymmetry. The ball
  retainer is forced to `timeNeededToGetToBall_ms = 1`. — [player.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/player.cpp)
- `Team::GetBestPossessionPlayer()` is a plain argmin of `GetTimeNeededToGetToBall_ms()` over active players. —
  [team.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/team.cpp)

**gfootball: hysteresis on the designated player (verbatim, team.cpp `Team::Process`)**
```cpp
if (designatedTeamPossessionPlayer != bestPlayer) {
  // switch only if other player is somewhat better, to overcome
  // possession-chaos
  int bestPlayerTime_ms = bestPlayer->GetTimeNeededToGetToBall_ms();
  float timeRating = (float)(bestPlayerTime_ms + 500) /
      (float)(designatedPlayerTime_ms + 500);
  if (bestPlayer->HasPossession()) timeRating *= 0.5f;
  if (designatedTeamPossessionPlayer->HasPossession()) timeRating /= 0.5f;
  if (bestPlayer->ExternalControllerActive()) timeRating *= 0.8f;
  if (designatedTeamPossessionPlayer->ExternalControllerActive())
    timeRating /= 0.8f;
  // current player can get to the ball before the closest opponent: less
  // need to switch
  if (!bestPlayer->ExternalControllerActive() &&
      designatedPlayerTime_ms < oppTime_ms - 100) {
    timeRating += 0.2f;
    timeRating *= 1.2f;
  }
  if (timeRating < 0.8f) {
    designatedTeamPossessionPlayer = bestPlayer;
  }
}
```
— [team.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/team.cpp)

What these numbers mean:
  - **Relative margin with an additive offset:** the challenger must satisfy (t_best+500)/(t_cur+500) < 0.8.
    The +500 ms offset stops tiny times (e.g. 40 ms vs 60 ms) from producing large ratios, so at short range the
    margin works out as about an absolute 100+ ms. At t_cur = 1000 ms the challenger needs t_best < 700 ms.
  - **Possession bonus x0.5 / /0.5:** a player who has possession is strongly favoured, and so is the incumbent if
    it has possession.
  - **Human-controlled bonus x0.8:** the player the human controls is "sticky".
  - **"Already winning the race" bonus:** if the incumbent beats the nearest opponent by 100 ms or more, the rating
    becomes (r+0.2)*1.2, which makes a switch much harder. A switch is only needed when the race is in doubt.

**gfootball: when the human cursor follows the designated player (verbatim, `Team::UpdateSwitch`)**
```cpp
// autoswitch on proximity
if (match->IsInPlay() && !humanGamers.empty()) {
  if (!designatedTeamPossessionPlayer->ExternalControllerActive() &&
      3 * designatedTeamPossessionPlayer->GetTimeNeededToGetToBall_ms() <
          HumanControlledToBallDistance() &&
      designatedTeamPossessionPlayer->GetFormationEntry().role != e_PlayerRole_GK) {
    SelectPlayer(designatedTeamPossessionPlayer);
  }
}
// team player in possession is not human selected
if (match->IsInPlay() && !humanGamers.empty()) {
  if (!designatedTeamPossessionPlayer->ExternalControllerActive() &&
      (designatedTeamPossessionPlayer->HasUniquePossession() ||
       match->IsInSetPiece())) {
    if (designatedTeamPossessionPlayer->GetFormationEntry().role != e_PlayerRole_GK) {
      SelectPlayer(designatedTeamPossessionPlayer);
    }
  }
}
```
`HumanControlledToBallDistance()` is the minimum time to ball over the human-selected players (starting at
10000 ms). — [team.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/team.cpp)
  - So the auto-switch fires in only three cases: (1) a teammate has **unique possession** (it received the pass or
    won the ball); (2) a set piece; (3) a "proximity" override when the designated player is **more than 3x
    quicker** to the ball than the human's player. Goalkeepers are never auto-selected.
  - For multiple humans on one team, `switchPriority` is a round-robin deque. `SelectPlayer` gives the new player
    to the human at the front and rotates the deque. A human who already controls the designated player "loses
    turn". — [team.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/team.cpp)

**gfootball: manual switch button (edge-triggered, with a guard)**
- Fires on the press edge (`GetButton(Switch) && !GetPreviousButtonState(Switch)`). It is refused when the human
  already controls the player who is both best and designated, *unless* the team's possession amount is below 1.0
  (opponent or loose ball). It is also refused while the selected player has unique possession. — [team.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/team.cpp)
- Target priority: designated possession player (if not human-controlled and the team is the best-possession
  team), then the best possession player, then `AI_GetBestSwitchTargetPlayer(...)`. Goalkeepers are rejected
  ("can not be goalie in current version, at least not during play"). — [team.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/team.cpp)
- `AI_GetBestSwitchTargetPlayer` picks a **positional** target, not a time-to-ball one:
  - action point = 0.5 * designated (global) possession player position + 0.5 * ball predicted 100 ms ahead.
  - defence point = action * (1, 0.8) + 4 m toward own goal, then blended 80/20 with the team's designated
    player.
  - offence point = action * (1, 0.8) + 8 m forward.
  - The two points are blended by `offenseBias`, computed from the fading and instant possession amounts and
    sharpened with `clamp((b-0.5)*2+0.5)` then `pow(b,1.5)` ("tend towards defensive").
  - It takes the 8 nearest AI-controlled outfield players. When defending, it picks the first one (nearest first)
    whose look-ahead position (`pos + movement*0.5`) is closer to its own goal than the opponent's, within a
    tolerance of `1.0 + clamp(oppGoalDist*0.1, 0, 3)` m. Otherwise it takes the nearest.
  - It takes `desiredMovement` (stick direction) as a parameter but **does not use it**, so manual switching
    ignores the stick. — [AIfunctions.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/AIsupport/AIfunctions.cpp)

**gfootball: post-switch steering blend (FIFA's "move assistance", in code)**
- On `SetPlayer`, `lastSwitchTime_ms` is recorded, with `lastSwitchTimeDuration_ms = 300`. For as long as
  `GetLastSwitchBias() > 0` (a decaying factor over that window; its exact body is in the header, which I did
  not read), stick input is blended with the player's current velocity:
  `switchBias = pow(bias, 0.7); result = current*switchBias*0.5 + manual*(1 - switchBias*0.5)`. At most 50%
  of the old motion is kept, fading over about 300 ms. This hides the "new player lurches toward a stale stick
  direction" artefact. — [humancontroller.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/controller/humancontroller.cpp)
- Holding Switch while in possession triggers `ApplyAttackingRun()` instead of a switch, a context-dependent
  reuse of the same button. — [humancontroller.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/controller/humancontroller.cpp)

**EA FIFA / FC (official settings text, no code)**
- FIFA 22 settings (EA accessibility manual):
  - **Auto Switching** is "how you will switch between players during certain passes". Options run from Manual
    through "Only on Loose Balls", "Only on Air Balls" and "On Air Balls and Loose Balls" to Automatic. Default:
    **On Air Balls and Loose Balls**.
  - **Auto Switching Move Assistance**: "keep the player you AUTO switch to moving in his current direction for a
    short period of time". Levels None/Low/Medium/High, default High.
  - **Right Stick Switching**: Player Relative / Ball Relative / Player Rotation, default Player Relative.
  - **Pass Receiver Lock**: Early / Late, default Late.
  - **Next Player Switch Indicator**: "shows you which player you'll swap controls when pressing L1 ... while
    defending", default On.
  - **Icon Switching**: R3 shows switching icons.
  — [EA Able: FIFA 22 PS5 controls settings](https://www.ea.com/able/resources/fifa/fifa-22/ps5/customise-controls-settings)
- Right-stick switching modes: "Classic" flicks to the next player in the pointed direction immediately. "Adaptive"
  means holding the stick longer selects more distant players. "Player Rotation" moves the indicator without
  switching until L1/LB is pressed. — summarised from community guides found via search (e.g.
  [FUT settings guide](https://fifauteam.com/fifa-23-controller-settings/),
  [Team Gullit FC26 settings](https://acc.teamgullit.com/ea-fc-26/controller-settings)). These are secondary
  sources, and their wording varies by year.
- FIFA Mobile (Aug 2022) added three switching modes: Auto (default; "selects the player most likely able to take
  the ball"), Semi Auto (auto only on air and loose balls, e.g. crosses and corners), and Manual. — EA news post as
  summarised by search ([ea.com](https://www.ea.com/games/fifa/news/gameplay-improvements-august-2022)). The page
  now redirects, so it could not be verified directly.

**GOALS (2024+ indie/AA football game, official settings guide)**
- "Advanced Switch Type": Player Relative ("Switches based on your currently selected player") / Ball Relative.
- "Auto Switch Type": "Air Balls → Auto-switch only on aerial passes" / "Off → No automatic switching".
- "Player Switch Indicator": "shows you exactly who you'll switch to next".
- "Pass Receiver Lock": "Controls how long you can change pass direction while charging" (Late = longer window).
— [GOALS game settings guide](https://playgoals.com/en/gamesettings-guide)

**Sensible Soccer-derived indie scheme (SuperLiquidSoccer devlog)**
- Selection is **stick-direction relative to the ball**: stick left means "the closest player to the _right_ of the
  ball (within a certain cone)". In other words, the player positioned so that running in the stick direction
  heads toward the ball. The dev credits "The Making of Sensible Soccer" (Read-Only Memory) and says it "really
  helps when the action gets frenetic". — [punyrobot devlog](https://punyrobot.itch.io/superliquidsoccer/devlog/352077/new-player-selection-scheme)
  (The ROM article itself returned 404.)

**NHL (community and forum only)**
- Users report that the switch button goes to the player closest to the puck, or the next closest if you already
  have that player. Right-stick aiming overrides this. Users ask for icon switching; an EA community manager
  replied that "not everyone wants the same player". — [EA forums, NHL 25](https://forums.ea.com/discussions/nhl-25-general-discussion-en/icon-player-switching-for-nhl-26-is-a-must/12322440)
  (forum, not official).

### Inferences
- The robust pattern is **two separate decisions**: (1) the "who should play the ball" oracle, used by the AI and
  smoothed by hysteresis; and (2) "when may the human's cursor move", which is gated by events and possession, not
  continuous. gfootball never auto-moves the cursor just because another player became a little closer. It does
  so only on possession, set pieces, or a 3x time advantage.
- The 500 ms passer exclusion plus "switch on unique possession" amounts to a **committed receiver**: during the
  pass flight the cursor stays put, and it jumps to whoever actually controls the ball. FIFA's default "air and
  loose balls" auto-switch is the same idea with an earlier trigger (at the kick, for lofted or loose balls only).

### Gaps
- No primary source for EA's actual scoring formula, dwell timers, or numeric thresholds. Madden, NBA 2K and NHL
  dev blogs on switching logic were not found.
- `GetLastSwitchBias()`'s exact decay curve (in humancontroller.hpp) was not read. It is presumably
  `1 - elapsed/300` clamped, but that is unverified.
- No GDC or Game Developer article specific to controlled-player selection was found. OpenFootball and Kick Off
  clones were not checked.

---

## 2. Which flicker-prevention methods are recommended, and what are their trade-offs?

### Takeaway
Six techniques show up, and they stack:
1. **A relative ratio threshold with an additive offset** (gfootball 0.8 on t+500 ms).
2. **An incumbent bonus**: possession x0.5, human-controlled x0.8, "already winning the race" (+0.2, x1.2).
3. **A large factor before overriding the human** (3x).
4. **Event-gated switching** rather than continuous switching (possession change, pass struck, air or loose ball).
5. **Excluding the kicker briefly** (500 ms).
6. **A post-switch input blend** (about 300 ms).

Each one buys stability by delaying a correct switch, so the margins should be smallest when it matters most:
loose ball, opponent close.

### Cited findings
- Ratio hysteresis plus incumbent bonuses plus "less need to switch if we already beat the opponent" — gfootball
  `Team::Process` (code above), [team.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/team.cpp).
  The comment states the purpose: "switch only if other player is somewhat better, to overcome possession-chaos".
- Human-override gate of 3x time ratio — `Team::UpdateSwitch`, [team.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/team.cpp).
- Manual-switch suppression when you already control the best and designated player while in possession —
  [team.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/team.cpp).
- Kicker exclusion (500 ms) — [player.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/player.cpp).
- Post-switch movement blend (300 ms, up to 50% old velocity) — [humancontroller.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/controller/humancontroller.cpp);
  FIFA calls the same idea "Auto Switching Move Assistance" (default High) — [EA Able FIFA 22](https://www.ea.com/able/resources/fifa/fifa-22/ps5/customise-controls-settings).
- Event-gated auto-switch as the shipped default (air and loose balls) — [EA Able FIFA 22](https://www.ea.com/able/resources/fifa/fifa-22/ps5/customise-controls-settings);
  GOALS offers only "Air Balls" or "Off" — [GOALS settings](https://playgoals.com/en/gamesettings-guide).
- The naive "closest player to ball is always controlled" design is reported as a bug cause in a fan-made game:
  "the closest player to the ball will be in your control, which causes rapid switching between teammates". —
  search snippet from [lexaloffle BBS](https://lexaloffle.com/bbs/?pid=15432) (not opened, low confidence).
- Players complain about auto-switch picking the wrong player after loose balls and about "switching back"
  unexpectedly. — EA forum threads surfaced in search (e.g.
  [FC 25 discussion](https://forums.ea.com/discussions/fc-25-general-discussion-en/fc25-switching-players-l1lb-without-ball-defending-mode/8009132));
  these are user reports only.

### Inferences
These are proposed engineering defaults for WildPitch, not from a source. They carry gfootball's structure into
Q16.16 fixed point.
- **Score:** `t_i` = earliest tick at which player i can reach the predicted ball, with the ball below a playable
  height and an arrival slack. The kicker is excluded for N ticks after the kick. Use the time-to-intercept, not
  the current distance: distance flips at the crossover of two runners even when their intercept times are stable.
- **Hysteresis:** switch the oracle only if `(t_new + K) * 5 < (t_cur + K) * 4`, i.e. a ratio below 0.8, with
  K of about 0.5 s in ticks. This works in integer arithmetic and has no divide. Optionally add an absolute floor
  (t_cur - t_new >= about 6 ticks), so very small times never flip.
- **Dwell / lockout:** an automatic switch at most once per about 250–400 ms. Manual presses bypass this (the user
  asked). A "pass struck" event can also bypass it once.
- **Intent-aware:** if the human's stick has a positive dot product with the vector from their player to the
  intercept point, and their own `t` is within some factor of the best (e.g. less than 1.5x), suppress the
  auto-switch. This copies gfootball's x0.8 human bonus and makes it stronger when the user is visibly chasing.
- **Event-driven:** re-evaluate the human cursor only on: pass or shot struck (go to the intended receiver if it
  is ours and the ball is lofted; otherwise keep the current player); deflection or loose ball; possession change
  (go to the new owner); set piece; the 3x proximity override.

### Gaps
- No published study quantifies the responsiveness-vs-stability trade-off, e.g. a preferred dwell in ms.
  Playtesting is needed.

---

## 3. HCI angle: predictability, user agency, cost of an unexpected switch, telegraphing

### Takeaway
Commercial games handle the agency problem with **telegraphing plus user override** rather than a cleverer
algorithm:
- a "Next Player Switch Indicator" showing exactly who L1 will select;
- optional icon switching (choose a specific player);
- stick-directed switching (player-relative or ball-relative);
- settings that let the user choose how much the game auto-switches, from Manual through air/loose only to
  Automatic.

An unexpected switch is costly because the new player inherits a stick direction meant for someone else. Both
gfootball and FIFA blend the old motion in for a short window after a switch for this reason.

### Cited findings
- Next-player indicator and icon switching are on by default in FIFA 22 — [EA Able FIFA 22](https://www.ea.com/able/resources/fifa/fifa-22/ps5/customise-controls-settings).
  GOALS: "shows you exactly who you'll switch to next" — [GOALS settings](https://playgoals.com/en/gamesettings-guide).
- "Player Rotation" right-stick mode separates choosing (moving the indicator) from committing (pressing L1). It
  is effectively a two-step, preview-then-confirm interaction. — community guides
  ([FUT settings](https://fifauteam.com/fifa-23-controller-settings/)).
- Stick-relative-to-ball selection ("closest player to the right of the ball within a cone" when the stick points
  left) makes the choice predictable from the user's own input. — [punyrobot devlog](https://punyrobot.itch.io/superliquidsoccer/devlog/352077/new-player-selection-scheme).
- An EA community manager on NHL switching: "not everyone wants the same player". This supports offering
  user-selectable modes. — [EA forums](https://forums.ea.com/discussions/nhl-25-general-discussion-en/icon-player-switching-for-nhl-26-is-a-must/12322440).
- Move assistance after a switch, in code and as a setting — [humancontroller.cpp](https://github.com/google-research/football/blob/master/third_party/gfootball_engine/src/onthepitch/player/controller/humancontroller.cpp),
  [EA Able FIFA 22](https://www.ea.com/able/resources/fifa/fifa-22/ps5/customise-controls-settings).

### Inferences
- Telegraphing makes an automatic switch read as intentional. Show a secondary marker on the "next" player
  (the oracle's choice) **before** the cursor moves, e.g. from pass-strike to reception. The switch is then
  confirmed rather than surprising. This is the same marker that the FIFA and GOALS indicator shows for manual
  switching.
- Input-buffer interaction: clear or re-map buffered actions (tackle, sprint, pass) when the cursor changes. A
  stale buffered press firing on the new player is an unexpected action that has been seen in practice. gfootball
  blends only movement; buffered actions are handled by `actionBufferTime_ms` in humancontroller.cpp (not
  analysed further).
- Ball-relative stick switching suits a 2.5D camera where the ball, not the selected player, is the visual
  anchor. Player-relative is FIFA's default.

### Gaps
- No academic HCI paper on control handoff in team sports games was found in this pass. No GDC talk on switching
  UX was found. Telemetry on how often players override auto-switch is not public.
