"""Live Song UI check in the QEMU lab (WSL root, lab running, panel 127.0.0.1:8765).

  python3 lab/song_cycle.py boot mix1-card   # insert image, reset, wait for the mod and project
  python3 lab/song_cycle.py song             # drive Song Mode, screenshots + logs + project dump
  python3 lab/song_cycle.py mixer-sound      # record pad 2, enter Mixer while it sounds
  python3 lab/song_cycle.py logs             # only copy logs (clone tail, events)
  python3 lab/song_cycle.py mixer            # enter Mixer (short Shift+Pad9), tabs, tracks, exit, long press
Writes PNG/JSON/logs to build/song-lab and copies them to the Windows project tmp/song-lab.
Only lab files; never a physical SD card."""
from pathlib import Path
import atexit,gzip,json,shutil,subprocess,sys,time,urllib.request
sys.path.insert(0,'/opt/mpc-sample-lab');import lab
R=Path(__file__).resolve().parent.parent;O=R/'build/song-lab';O.mkdir(parents=True,exist_ok=True)
WIN=Path('/mnt/c/Users/Алексей/Desktop/akai mpc sample/tmp/song-lab')
URL='http://127.0.0.1:8765';CLONE='/run/mpc-clone-trial-v1/app/clone.txt'
actions=[]
def guest(cmd,timeout=30):
    try:return lab.guest(cmd,timeout=timeout).decode('utf-8','replace')
    except Exception as e:return f'<{type(e).__name__}: {e}>'
def post(name,action,value=100):
    actions.append(dict(name=name,action=action,value=value,t=time.time()))
    data=json.dumps(dict(name=name,action=action,value=value)).encode()
    urllib.request.urlopen(urllib.request.Request(URL+'/control',data,{'Content-Type':'application/json'}),timeout=10).read()
def send(name,action,value=100):
    actions.append(dict(name=name,action=action,value=value,t=time.time()))
    data=json.dumps(dict(name=name,action=action,value=value)).encode()
    urllib.request.urlopen(urllib.request.Request(URL+'/control',data,{'Content-Type':'application/json'}),timeout=10).read();time.sleep(.4)
def tap(name):send(name,'press');send(name,'release');time.sleep(1.2)
def pad(n,hold=0.0):send(f'PAD {n}','press',100);time.sleep(hold);send(f'PAD {n}','release');time.sleep(1.2)
def shift(name):
    send('SHIFT','press')
    if name.startswith('PAD '):pad(int(name[4:]))
    else:tap(name)
    send('SHIFT','release');time.sleep(1.2)
def knob(k,v):send(k,'set',v);time.sleep(1.2)
def snap(name):
    time.sleep(1.5)
    try:
        (O/f'{name}.png').write_bytes(urllib.request.urlopen(URL+'/screen.png',timeout=15).read())
        (O/f'{name}.json').write_bytes(urllib.request.urlopen(URL+'/state',timeout=10).read())
        print('snap',name,flush=True)
    except Exception as e:print('snap failed',name,e,flush=True)
def wait_guest(limit=600):
    end=time.monotonic()+limit
    while time.monotonic()<end:
        try:lab.guest('true',timeout=10);return
        except Exception:time.sleep(5)
    raise SystemExit('guest SSH did not come up')
def wait_line(pattern,limit=1500):
    end=time.monotonic()+limit;last=''
    while time.monotonic()<end:
        log=guest(f'grep -a "{pattern}" {CLONE} | tail -n 1')
        if log!=last:print(log.strip(),flush=True);last=log
        if pattern in log:return log
        time.sleep(15)
    raise SystemExit(f'"{pattern}" not seen in {limit} s')
def songs_dump(name):
    """Native temp project: keep only the song part for inspection."""
    for _ in range(20):
        body=lab.guest('cat /run/mpc-clone-trial-v1/data-image/TempProjects/Temp-Project.xpj',timeout=30)
        try:
            text=gzip.decompress(body).decode();data=json.loads(text[text.index('{'):])['data']
            keep={k:v for k,v in data.items() if 'song' in k.lower()}
            (O/f'{name}-songs.json').write_text(json.dumps(keep,indent=1));print('songs',name,list(keep),flush=True);return
        except Exception:time.sleep(1)
    print('songs dump failed',name,flush=True)
def publish():
    (O/'actions.json').write_text(json.dumps(actions,indent=1))
    (O/'song-log.txt').write_text(guest(f'grep -a "SONG_UI\\|MIXER\\|STARTUP_PROJECT" {CLONE} | tail -n 120'))
    (O/'clone-tail.txt').write_text(guest(f'tail -n 400 {CLONE}',timeout=60))
    (O/'events-tail.txt').write_text(guest('tail -n 120 /run/mpc-clone-trial-v1/events.txt; ls -la /run/mpc-clone-trial-v1/ /run/mpc-clone-trial-v1/app 2>&1 | head -80',timeout=60))
    WIN.mkdir(parents=True,exist_ok=True)
    for f in O.iterdir():shutil.copy2(f,WIN/f.name)
    print('copied to',WIN,flush=True)

cmd=sys.argv[1] if len(sys.argv)>1 else ''
if cmd=='boot':
    img=f'/opt/mpc-sample-lab/out/run/{sys.argv[2]}.img'
    lab.card_swap(img);lab.qmp('system_reset');lab.qmp('cont');print('booting with',img,flush=True)
    wait_guest();subprocess.run([sys.executable,str(R/'lab/prepare_tcg.py')],check=False)
    wait_line('STARTUP_PROJECT NATIVE_LOAD_FINISHED');time.sleep(30)
    print(guest(f'grep -a "SONG_UI\\|r27r" {CLONE} | tail -n 5; sha256sum /run/mpc-clone-trial-v1/phone-input.so'),flush=True)
    snap('00-boot');publish()
elif cmd=='song':
    atexit.register(publish)
    tap('STOP');shift('PAD 12');snap('01-song-open')
    pad(1);tap('SCREEN 3');pad(2);tap('SCREEN 3');pad(1);tap('SCREEN 3');snap('02-three-steps')
    songs_dump('02')
    tap('MINUS');snap('03-row-selected')
    knob('K1',64);knob('K1',76);snap('04-k1-repeats')       # +12 positions = +3 repeats
    songs_dump('04')
    shift('SCREEN 2');snap('05-duplicated')
    songs_dump('05')
    shift('MINUS');snap('06-moved-up')
    pad(3);shift('SCREEN 3');snap('07-replaced')
    songs_dump('07')
    tap('SCREEN 3');snap('08-plain-b3-stock-insert')       # stock Insert still works
elif cmd=='mixer-sound':
    # Mixer readiness needs live Program audio: record a pad, then enter the
    # Mixer while that pad is sounding. Diagnostic only.
    atexit.register(publish)
    tap('STOP');tap('SAMPLE RECORD');knob('K3',0);snap('20-record-screen')   # threshold off: record even silence
    pad(2);time.sleep(5);tap('STOP');time.sleep(3);snap('21-recorded')
    tap('SAMPLE');snap('22-sample')
    post('PAD 2','press',120);time.sleep(1.5);snap('23-pad2-sounding')
    send('SHIFT','press');post('PAD 9','press',100);time.sleep(0.12);post('PAD 9','release');time.sleep(0.6)
    send('SHIFT','release');snap('24-mixer-while-sounding')
    time.sleep(2);post('PAD 2','release');time.sleep(2);snap('25-after-release')
    tap('SCREEN 1');snap('26-mix-tab');tap('SAMPLE');snap('27-exit')
elif cmd=='logs':
    publish()
elif cmd=='mixer':
    atexit.register(publish)
    tap('STOP');tap('SAMPLE');snap('10-sample')
    send('SHIFT','press');post('PAD 9','press',100);time.sleep(0.12);post('PAD 9','release');time.sleep(0.6)
    send('SHIFT','release');snap('11-mixer-overview')   # short Shift+Pad9 (well under 500 ms)
    tap('SCREEN 1');snap('12-mixer-mix');pad(2);snap('13-mixer-track-b')
    tap('SCREEN 2');snap('14-mixer-fx');tap('SCREEN 3');snap('15-mixer-route')
    pad(16);snap('16-mixer-master');tap('SCREEN 3');snap('17-mixer-route-same-tab-overview')
    tap('SAMPLE');snap('18-exit-to-sample')
    send('SHIFT','press');pad(9,0.9);send('SHIFT','release');snap('19-long-shift-pad9-stock-fader-settings')
    tap('SAMPLE')
else:raise SystemExit(__doc__)
