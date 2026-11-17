import subprocess, time, os, sys, threading
B=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'wildpitch')
def rd(p):
    try: return int(open(p).read())
    except: return None
def sample(stop, out):
    while not stop.is_set():
        out.append((rd('/sys/class/hwmon/hwmon4/power1_input'), rd('/sys/class/hwmon/hwmon5/temp1_input'), rd('/sys/class/hwmon/hwmon4/temp1_input')))
        time.sleep(0.5)
def run(name, args, env_extra, secs=60):
    env=dict(os.environ); env.update(env_extra); env['SDL_AUDIO_DRIVER']=env.get('SDL_AUDIO_DRIVER','dummy')
    s=[]; stop=threading.Event(); t=threading.Thread(target=sample,args=(stop,s)); t.start()
    if args is None: time.sleep(secs); outp=''
    else:
        r=subprocess.run([B]+args+['--perf-seconds',str(secs)],env=env,capture_output=True,text=True,timeout=secs+60)
        outp=' | '.join(l for l in r.stdout.splitlines() if l.startswith(('avg','perf','audio')))
    stop.set(); t.join()
    s=s[6:]  # skip warm-up (3 s)
    ppt=[x[0] for x in s if x[0]]; tc=[x[1] for x in s if x[1]]; ge=[x[2] for x in s if x[2]]
    print(f"{name}: PPT {sum(ppt)/len(ppt)/1e6:.2f} W  Tctl {sum(tc)/len(tc)/1e3:.1f} C (max {max(tc)/1e3:.1f})  GPU edge {sum(ge)/len(ge)/1e3:.1f} C  || {outp}", flush=True)
    time.sleep(10)
# real launches driven by queued keys (card -> title -> setup -> kick-off); settings in a scratch dir (never the
# player's); audio through SDL's dummy driver (the mixer callback runs, nothing is heard)
import tempfile
X={'XDG_DATA_HOME': tempfile.mkdtemp(prefix='wp_perf_')}
run('idle desktop (no game)', None, {})
run('title screen, no input', ['--keys','Return'], X)
run('7v7 match 60 fps', ['--keys','Return Return Return'], X)
run('7v7 match 30 fps (battery)', ['--keys','Return Return Return','--battery'], X)
run('7v7 match, pause menu open', ['--keys','Return Return Return Escape'], X)
run('5v5 --autoplay harness 60 fps, audio', ['--autoplay','--audio'], {})
