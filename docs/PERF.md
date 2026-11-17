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
AMD Ryzen 7 PRO 6850U (Radeon 680M), 1280x720 window, 60 Hz display, renderer `opengl`, on AC. Real launches driven
by `--keys` (card -> title -> Quick match -> Kick off!), default settings (7v7, 5-min halves) in a scratch
`XDG_DATA_HOME`, audio on through SDL's `dummy` driver (the mixer callback runs; nothing is heard).

| Case (60 s each) | fps | CPU (% of one core) | PPT avg | Tctl avg (max) |
|---|---|---|---|---|
| Desktop idle, no game (baseline) | - | - | 4.20 W | 47.1 °C (54.5) |
| Title screen, no input | 0 redraws | **0.6%** | 4.08 W | 46.6 °C (50.0) |
| 7v7 match, 60 fps (vsync) | 60.2 | **6.9%** | 6.31 W | 50.3 °C (56.1) |
| 7v7 match, **30 fps battery mode** | 30.0 | **4.2%** | 4.31 W | 49.0 °C (51.2) |
| 7v7 match, pause menu open | 0 redraws | **0.6%** | 4.30 W | 46.4 °C (49.0) |
| 5v5 `--autoplay` harness, 60 fps | 60.0 | 6.0% | 4.93 W | 47.7 °C (49.6) |

An earlier run of the same kind (harness only, part 3 commit): 5v5 autoplay 6.1% at 60 fps / 3.8% at 30 fps; pause
menu with audio *off* 0.0% (the 0.6% above is the audio callback mixing the quiet crowd while paused); vsync off
with the pacer alone 60.0 fps, 5.0%.

Reading it:
- Battery mode cuts the game's CPU by ~40% (6.9 -> 4.2% of one core in a real 7v7 match). The sim still steps at
  60 Hz; only drawing halves.
- Package power: in this run the 60 fps match sat ~2 W over idle and battery mode ~0.1 W over idle. Treat it as a
  trend, not a number: the desktop's own background load moves PPT by +-1.5 W between runs (in the first run the
  "idle" baseline was higher than two game cases). A clean figure needs a quiet machine on battery
  (`BAT0/power_now` while discharging), repeats, or root for RAPL.
- Temperatures: +3 °C at 60 fps, +2 °C at 30 fps over idle in this run.
- Menus really idle: the title and the pause menu draw nothing without input (0 frames in 60 s; they block in
  `SDL_WaitEvent`); the 0.6% left is the audio callback.
