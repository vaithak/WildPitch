# Performance / "runs cool" (CP0.6)

## Method
- `wildpitch --perf-seconds N` quits after N seconds and prints the process CPU time (user + system from
  `/proc/self/stat`) as a % of one core over the wall time, the achieved fps, the renderer and the audio driver.
- `tools/perf_measure.py` runs each case for 60 s (audio through SDL's `dummy` driver so the mixer callback runs
  without making noise) while sampling, every 0.5 s, without root:
  - **PPT** = APU package power (CPU + GPU) from amdgpu hwmon `power1_input` (µW),
  - **Tctl** = CPU temperature from k10temp `temp1_input`, **GPU edge** from amdgpu `temp1_input`.
  The first 3 s of each run are dropped (warm-up); 10 s gap between runs. Paths are for this laptop (hwmon numbers
  differ per machine). RAPL `energy_uj` needs root (not used); `BAT0/power_now` reads the charge rate while on AC.
- `WP_NO_VSYNC=1` turns vsync off to test the deadline pacer alone.

## Results — 2026-11-17, dev laptop
AMD Ryzen 7 PRO 6850U (Radeon 680M), 1280x720 window, 60 Hz display, renderer `opengl`, on AC.
`--autoplay` (5v5 harness match: AI + rendering + audio mixer), CP0.6 part 3 build.

| Case (60 s each) | fps | CPU (% of one core) | PPT avg | Tctl avg (max) |
|---|---|---|---|---|
| Desktop idle, no game (baseline) | - | - | 5.22 W | 46.3 °C (47.8) |
| Match, 60 fps (vsync), audio on | 60.3 | **6.1%** | 4.97 W | 47.4 °C (48.8) |
| Match, **30 fps battery mode**, audio on | 30.4 | **3.8%** | 6.44 W | 47.3 °C (50.2) |
| Match, 60 fps, vsync off (pacer only) | 60.0 | 5.0% | 3.73 W | 47.2 °C (48.8) |
| Pause menu open (no input) | 0 redraws | **0.0%** | 4.45 W | 46.3 °C (48.1) |

Reading it:
- Battery mode cuts the game's CPU by ~38% (6.1 -> 3.8% of one core). The sim still steps at 60 Hz; only drawing
  halves.
- **Package power is below this method's noise:** the desktop's own background load moves PPT by ±1.5 W between
  runs (the "idle" baseline is higher than two of the game runs), so the game's power cost here is well under ~1.5 W
  and the 60 vs 30 fps difference can't be resolved. A clean measurement needs a quiet machine on battery
  (`BAT0/power_now` while discharging), several repeats, or root for RAPL.
- Temperatures stay within ~1-2 °C of idle in every case.
- The pause menu really is idle: 0 frames drawn and 0.0% CPU over 60 s (it blocks in `SDL_WaitEvent`). The title
  screen uses the same loop.
- Older figures (ARCHITECTURE "Performance notes"): ~5% of one core for the CP0.1 loop; the 7v7 AI + audio now
  costs about the same.
