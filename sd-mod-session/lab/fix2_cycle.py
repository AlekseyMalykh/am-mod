"""Lab cycle for the r27q-fixN candidate (WSL, lab running, panel on 127.0.0.1:8765).

  python3 lab/fix2_cycle.py boot fix2-card     # insert image, reset, TCG timeouts, wait for the mod
  python3 lab/fix2_cycle.py status             # logs + screenshot
  python3 lab/fix2_cycle.py poweroff           # STOP STOP, power key, Yes, wait for clean power-off
  python3 lab/fix2_cycle.py coldboot fix2-card # boot the same card, wait for the SD project load,
                                               # screenshot without touching pads, list Projects
Between boot and poweroff, prepare the project by hand in the lab panel
(load a sample, Chop -> Extract, Save Project to SD). Output: build/r27q-fix2/lab-*."""
from pathlib import Path
import sys,time,json,urllib.request
sys.path.insert(0,'/opt/mpc-sample-lab');import lab
R=Path(__file__).resolve().parent.parent;O=R/'build/r27q-fix2';O.mkdir(parents=True,exist_ok=True)
URL='http://127.0.0.1:8765';CLONE='/run/mpc-clone-trial-v1/app/clone.txt'
def guest(cmd,timeout=30):
    try:return lab.guest(cmd,timeout=timeout).decode('utf-8','replace')
    except Exception as e:return f'<{type(e).__name__}: {e}>'
def snap(name):
    try:
        (O/f'lab-{name}.png').write_bytes(urllib.request.urlopen(URL+'/screen.png',timeout=15).read())
        (O/f'lab-{name}.json').write_bytes(urllib.request.urlopen(URL+'/state',timeout=10).read())
        print('screenshot',O/f'lab-{name}.png',flush=True)
    except Exception as e:print('screenshot failed',e,flush=True)
def control(name,action):
    data=json.dumps(dict(name=name,action=action,value=100)).encode()
    urllib.request.urlopen(urllib.request.Request(URL+'/control',data,{'Content-Type':'application/json'}),timeout=10).read();time.sleep(.4)
def tap(name):control(name,'press');control(name,'release');time.sleep(.7)
def wait_guest(limit=600):
    end=time.monotonic()+limit
    while time.monotonic()<end:
        try:lab.guest('true',timeout=10);return
        except Exception:time.sleep(5)
    raise SystemExit('guest SSH did not come up')
def wait_line(pattern,limit=1200):
    end=time.monotonic()+limit;last=''
    while time.monotonic()<end:
        log=guest(f'grep -a "{pattern}" {CLONE} | tail -n 1; tail -n 2 /run/mpc-clone-trial-v1/events.txt')
        if log!=last:print(log.strip(),flush=True);last=log
        if pattern in log.split('\n')[0]:return log.split('\n')[0]
        time.sleep(15)
    raise SystemExit(f'"{pattern}" not seen in {limit} s')
def boot(name):
    img=f'/opt/mpc-sample-lab/out/run/{name}.img'
    lab.card_swap(img);lab.qmp('system_reset');lab.qmp('cont');print('booting with',img,flush=True)
    wait_guest()
    subprocess=__import__('subprocess');subprocess.run([sys.executable,str(R/'lab/prepare_tcg.py')],check=False)
    line=wait_line('STARTUP_PROJECT NATIVE_LOAD_FINISHED')
    print('LOAD:',line,flush=True);time.sleep(20)
    print(guest(f'grep -a "AM_MOD\\|VERSION\\|r27q" {CLONE} | tail -n 3; sha256sum /run/mpc-clone-trial-v1/phone-input.so'),flush=True)
    return line
cmd=sys.argv[1] if len(sys.argv)>1 else 'status'
if cmd=='boot':
    boot(sys.argv[2]);snap('boot')
elif cmd=='status':
    print(guest(f'tail -n 25 {CLONE}; tail -n 5 /run/mpc-clone-trial-v1/events.txt'));snap('status')
elif cmd=='poweroff':
    tap('STOP');tap('STOP');time.sleep(3);snap('before-poweroff')
    lab.power_button();time.sleep(5);snap('poweroff-question');lab.yes_button()
    for i in range(30):
        time.sleep(10)
        if lab.guest_off():print('CLEAN_POWER_OFF',flush=True);break
        log=guest(f'tail -n 8 {CLONE}; ls /media/MPCSAMPLE/MPC-Sample/Projects',timeout=20);print(log[-800:],flush=True)
        (O/'lab-poweroff-log.txt').write_text(log)
    else:raise SystemExit('power-off did not complete in 300 s')
elif cmd=='coldboot':
    line=boot(sys.argv[2]);snap('cold-boot-no-touch')
    listing=guest('ls -la /media/MPCSAMPLE/MPC-Sample/Projects; ls -la /media/MPCSAMPLE/MPC-Sample/Projects/Mod-Exit-Last* 2>/dev/null; find /media/MPCSAMPLE/MPC-Sample/Projects -maxdepth 1 -name "Mod-Exit-saving-*"',timeout=60)
    (O/'lab-cold-projects.txt').write_text(listing);print(listing,flush=True)
    print('RESULT sd=1' if 'sd=1' in line else 'RESULT sd=0 (no saved SD project was loaded)',flush=True)
    print('errors=0' if 'errors=0' in line else 'LOAD REPORTED ERRORS: '+line,flush=True)
else:raise SystemExit(__doc__)
