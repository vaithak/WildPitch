# Headless playtest findings — passing, through balls, reception (CP0.2b build, 2026-11-15)

Source: a playtesting agent drove seat 0 with 8-way keyboard inputs in the real configuration (opposition parked for
clean reception numbers unless noted), harnesses in the session scratchpad (`playtest/recv2.c, recv3.c, live2.c,
geom.c, through.c, ft.c, misc.c, trace72.c, onetwo.c, curves.c`). Pitch 40×25 m, 60 ticks/s. All numbers measured.

## 1. Pressing a direction while a pass arrives → receiver ignores the ball (owner's report, reproduced)
Misses/tries (recv3, mate 7–20 m, 6 placements; jog = sprint):

| input after pass | toward ball | sideways | forward/with ball |
|---|---|---|---|
| tap; 12-tick nudge right after hand-off, then release | 0/15 | 0/18 | 0/9 |
| tap; 12-tick nudge mid-flight, then release | 0/15 | 6/18 | 0/9 |
| tap; press ~200 ms before arrival and keep it | 0/15 | 11/18 | 7/9 |
| tap; hold a new direction from hand-off | 13/15 | 18/18 | 9/9 |
| through (hold 24); 12-tick nudge then release | 10/15 | 12/18 | 6/9 |
| through; press ~200 ms before arrival and keep it | 8/15 | 18/18 | 9/9 |

Live: holding forward after a tap pass → 0/8 receptions (striker ran 29–37 m, never met the ball). Releasing the
stick after a through ball → 0/20 receptions (receiver stood ~4.7 s).
Root cause: `seats.c:62-65` any stick change >48 units from the passer's stick at contact (`seats.c:43-44`)
permanently ends assisted receive — every 8-way key change is 90–180 units, and releasing the stick to (0,0) also
counts; afterwards `seats_assist_receive` returns early (`seats.c:118`). Nothing then pulls a stick-controlled
receiver toward the ball; possession needs the ball within T_CONTROL_REACH 0.75 m (`tuning.h:51`, `player.c:381`) —
smaller than dribble reach 1.0 and kick reach 1.15; players move before the control check and the ball after
(`player.c:443-451`). Trace (trace72): distances at the control check 0.82, 0.79, 0.78, 0.79, 0.82 m — missed by 3 cm.
Conversely holding the SAME direction as the pass (often "forward") = no input → the assist brakes him to a stop.
Suggested: receiving stays active until controlled or clearly gone; stick shifts the meet point/first touch rather
than cancelling; released stick = no opinion; receiver reach/lunge ~1.0–1.2 m; full manual only after a large
sustained opposite input or once the ball has passed.

## 2. Through balls land behind the runner and die to 3 m/s → he brakes, stops, turns back
Gap along the run at closest approach (negative = behind), runner is a co-op seat running straight (geom.c):

| runner | tap | hold 10 | hold 22 | hold 40 |
|---|---|---|---|---|
| 6 m ahead, jogging 6 m/s | -4.8 | -5.6 | -4.6 | -2.9 |
| 6 m ahead, sprinting 8 m/s | -6.1 | -6.8 | -6.8 | -5.9 |
| level with passer, 8 m wide, sprinting | -3.5 | -3.8 | -3.8 | -3.6 |
| 12 m ahead, sprinting | -9.9 | -11.2 | -10.3 | -8.8 |

No hold length puts the ball in front of a moving runner. Live (assisted receive, 20 through balls, holds 15–39):
receiver speed at reception 0.0 m/s in every run; waited 0.1–0.67 s; ran 3–10 m in 1.4–1.9 s; ball arrived
3.8–4.1 m/s; with sprint held he ran back the way he came (direction cosine −0.6…−1.0). Example: 7.9 m/s at t+10,
braking t+20, stopped t+45, moving back −1.0 m/s at t+50, receives at t+85.
Root cause: `through_velocity` (`player.c:165-176`) targets receiver's position AT CONTACT + lead 2.5–10 m along a
fixed direction ((2,0)+run), ignoring his speed and ball travel time; arrives at T_THROUGH_ARRIVE 3 m/s. A sprinter
covers 11–20 m in the 1.4–2.5 s flight (> max lead); the ball averages 5.5–7.4 m/s, slower than the runner.
`seats.c:107` runs to the first point where the ball slows to 3.5 m/s even if behind him.
Suggested: true intercept (time T where ball meets runner's predicted pos+vel·T plus a stride), arrive at about the
runner's speed or a bit less (6–9 m/s), hold scales lead+pace together, assisted run keeps his speed.

## 3. Tap passes to a running team-mate make him stop dead
Live: striker running 3.7–6.0 m/s; received in 0.85–0.90 s at ~6.1 m/s ball; his speed at reception 0.0–0.6 m/s
after standing 0.45–0.67 s; covered 0.5–1.3 m. Without assist the tap goes 3.5–9.9 m behind a running receiver.
Root cause: `pass_velocity` (`player.c:154-161`) leads only T_PASS_LEAD_S 0.25 s while flight is 0.9–2 s; the
assist picks the reachable point CLOSEST to him (`seats.c:108-109`) = stop and step back for a forward runner.
Suggested: lead by real travel time; "check back" only for standing/approaching receivers.

## 4. Through-ball hold isn't power (bar ≠ power)
- Minimum through ball is SOFTER than a tap: standing mate 10 m — tap 10.7 m/s; hold 10 → 9.9, rising to 12.8 at
  hold 36. Jogging runner: tap 10.1 vs hold-10 8.5. Live: min through 8.7–9.3 vs tap 9.8–10.1 and takes ~1.45 s vs
  0.88 s.
- Hold only moves the stopping spot (up to 7.5 m); every through ball arrives at 3 m/s.
- No mate in the 55° cone → hold does nothing (`player.c:211` uses T_PASS_SPEED): holds 14–38 → 13.4–13.9 m/s.
- Tap/hold boundary 9 ticks (150 ms) (`tuning.h:79`, `player.c:250`): a deliberate 160–200 ms tap silently becomes a
  soft through ball 2.5 m past a standing mate.
Suggested: hold scales pace and lead together from ≥ tap strength; longer tap window or separate through-ball
button; untargeted hold scales power too.

## 5. Deflected pass → stale stick on the receiver, then control ping-pong
Ball clips passive opposition #9 standing in the lane (~(0.4,0.1)); `seats.c:53-55` ends the pass
(last_touch ≠ pass_from); striker under the held pass stick sprints away from the loose ball 0.5 s; auto-switch after
T_SWITCH_COOL 30, flips back ~t+96–110 and again ~t+228. 3/24 live through balls lost this way; some passes struck
straight into #9. Suggested: on deflection switch immediately to the best-placed mate or keep assisting toward the
loose ball; passes should consider the lane.

## 6. Through balls near byline/touchline roll out
Target clamped 1.5 m inside goal line / 1 m inside touchline (`player.c:171-172`), arrives 3 m/s and rolls ~2.77 m
more → out. In live2 every through ball to a striker at x 12–15 ended at x ≈ 18.3–18.8; all unreceived ones went out.

## 7. Ground pass pace fixed, gentle, uncontrollable
Arrives 5.5–6.2 m/s regardless of distance (T_PASS_ARRIVE). Launch 4 m → 7.8, 12 m → 12.4, 20 m → 17.2 (cap 22).
Flight: 8 m 1.07 s, 12 m 1.43 s, 20 m 1.98 s — slow vs FIFA/PES/Rematch (~1 s for 15–20 m); easy to intercept once
CP0.3 opponents move. No power input on taps. `pass_pick` (`player.c:64-81`) has no max distance (beyond ~27 m soft,
>34 m stops short). Suggested: arrival scales with distance (~6–10 m/s) and/or a little hold = firmer; cap search.

## 8. Sprinting into a firm pass can bounce it off you
One run: relative speed 17.1 m/s > T_CONTROL_MAX_REL 16 → body deflection. Worse if pace rises.

## 9. One-twos impossible (placeholder AI)
After passing, the midfielder parks 2.8 m behind the ball (`team.c:193`); return through ball finds nobody in the
cone. CP0.3 scope.

## What felt right
First touch once controlled (ft.c): ball goes the stick's way within 1 tick; gap 0.6–0.73 m jog, 0.83–1.0 m sprint.
Hand-off on the contact tick and receiver choice matched expectations. Taps to a standing mate clean (~5.9 m/s).

## Not covered
Lofted passes and headers (same takeover mechanism likely applies); analogue-stick sweep (48-unit threshold more
forgiving on gamepads); most reception runs had opposition parked.
