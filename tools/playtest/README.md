# Headless playtest harnesses (passing & reception, CP0.2b → CP0.2c)

Written by a playtesting agent that "played" WildPitch by scripting seat inputs (8-way keyboard stick values),
to find unnatural passing/receiving behaviour. Findings: `research_notes/Through ball and pass reception/
playtest_findings.md`; the fixes and acceptance tests are D-020 / `tests/test_reception.c`.

Build any of them against the current sim library and run:
```sh
cc -std=c17 -Isrc -Itests -Itools/playtest tools/playtest/recv3.c build/libwp_sim.a -lm -o /tmp/recv3 && /tmp/recv3
```
- reception with the stick pressed: `recv.c recv2.c recv3.c trace72.c ft.c ft2.c`
- live sequences: `live2b.c` (midfielder on the ball, striker running; arg `np` = opposition on the pitch),
  `live.c live2.c through.c` (kickoff setup)
- power/pace: `misc.c` (power vs J hold), `tapcurve.c` (tap pace vs distance)
- through-ball geometry: `geom.c behind.c neargoal.c ng1.c`
- regressions found in the re-test: `own.c own2.c` (back-pass to own keeper), `ng1.c` (runner predicted off pitch)
- other: `onetwo.c onetwo2.c dbg.c`, `summ.py` (summarises live output)
They are tools, not tests (not built by CMake); numbers they print are what the findings quote.

CP0.3 harnesses (all take a format flag, 1 = 7v7): `shots.c` (live shot outcomes), `icpt.c` (live taps to the ringed
mate: received / intercepted), `human.c` (dispossession standing/jogging/sprinting, kickoff race), `mirror.c`
(mirror-symmetry divergence), `aimatch.c [matches] [minutes] [seven]` (AI vs AI balance and rates).

7v7 baselines (2026-11-15, before CP0.3b): shots 7-16 m: goal 19% / keeper 71% (catches weak, body-blocks firm);
taps to the ringed mate: 67% received, 21% intercepted (6% on clear lanes), launch 14.0 m/s; dispossessed standing,
jogging and sprinting; mirror divergence at tick 166 (rounding, same actions).
