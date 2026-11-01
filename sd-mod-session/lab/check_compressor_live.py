"""Local QEMU native UI smoke check. Never accesses a physical MPC or SD card."""
from pathlib import Path
import hashlib,json,struct,subprocess,sys,time,threading,urllib.request,urllib.error
sys.path.insert(0,'/opt/mpc-sample-lab')
import lab
ssh_options=lab.ssh_base
lab.ssh_base=lambda:[*ssh_options(),'-o','ControlMaster=auto','-o','ControlPersist=600','-o','ControlPath=/tmp/optocat-ssh-%p']
R=Path(__file__).resolve().parent.parent
baseline=json.loads((R/'compressor_base.json').read_text())
O=R/'build/compressor'/baseline['revision']/'live';O.mkdir(parents=True,exist_ok=True)
LIB=O.parent/'phone-input.so'
URL='http://127.0.0.1:8765'
def request(route,p=None):
    data=None if p is None else json.dumps(p).encode()
    try:return urllib.request.urlopen(urllib.request.Request(URL+route,data,{'Content-Type':'application/json'}),timeout=20).read()
    except urllib.error.HTTPError as error:raise RuntimeError(error.read().decode()) from error
def tap(name):
    request('/control',dict(name=name,action='press',value=100));time.sleep(.2)
    request('/control',dict(name=name,action='release'));time.sleep(1)
def snapshot(name):
    (O/(name+'.png')).write_bytes(request('/screen.png'))
def status():
    for command in ('systemctl show mpc-clone-trial-v1-clone.service -p MainPID --value',
                    'tail -n 16 /run/mpc-clone-trial-v1/events.txt',
                    'systemctl show mpc-clone-trial-v1-probe.service -p ActiveState -p SubState -p ExecMainStatus; tail -n 12 /run/mpc-clone-trial-v1/app/probe.txt; true',
                    'tail -n 12 /media/MPCSAMPLE/autorun/MPC_MOD_CONSOLE.txt'):
        try:print(lab.guest(command,timeout=30).decode(),flush=True)
        except Exception as error:print(str(error),flush=True)
    snapshot('current')
if '--reboot-stock' in sys.argv:
    lab.card_swap()
    lab.vm('systemctl reboot',check=False)
    print('Reboot requested with virtual SD ejected',flush=True)
    sys.exit(0)
if '--install' in sys.argv:
    lab.wait_app()
    lab.run_autorun(str(R.parent/('am-mod-'+baseline['revision']+'-optocat.zip')),reboot=False,confirm=False,log=lambda message:print(message,flush=True))
    sys.exit(0)
if '--status' in sys.argv:
    status();sys.exit(0)
if '--screen' in sys.argv:
    snapshot('current');sys.exit(0)
if '--wait' in sys.argv:
    deadline=time.monotonic()+900
    previous=''
    while True:
        events=lab.guest('cat /run/mpc-clone-trial-v1/events.txt',timeout=30).decode()
        if events!=previous:print('\n'.join(events.splitlines()[-3:]),flush=True);previous=events
        ready=int(lab.guest('systemctl show mpc-clone-trial-v1-clone.service -p MainPID --value'))
        if ready>1:break
        assert time.monotonic()<deadline,'Clone startup timed out'
        time.sleep(15)
    lab.wait_app()
    time.sleep(15)
pid=int(lab.guest('systemctl show mpc-clone-trial-v1-clone.service -p MainPID --value'))
assert pid>1
sha=hashlib.sha256(LIB.read_bytes()).hexdigest()
assert lab.guest('sha256sum /run/mpc-clone-trial-v1/phone-input.so').decode().split()[0]==sha
maps=lab.guest(f'cat /proc/{pid}/maps').decode()
status=lab.guest(f'cat /proc/{pid}/status').decode()
tracer=int(next(line.split()[1] for line in status.splitlines() if line.startswith('TracerPid:')))
base=int(next(line for line in maps.splitlines() if 'phone-input.so' in line).split('-')[0],16)
symbols=subprocess.check_output(['aarch64-linux-gnu-nm','-anC',str(LIB)],text=True).splitlines()
def symbol(name,kind):
    values=[int(line.split()[0],16) for line in symbols if line.split()[1:]==[kind,name]]
    assert len(values)==1,(name,values)
    return base+values[0]
settings=symbol('settings','d');owner=symbol('owner','b')
blocks=symbol('blocks','b');hits=symbol('hits','b');reduction=symbol('reduction','b')
meter_input=symbol('meter_input','b');meter_output=symbol('meter_output','b')
image=symbol('pixel_data','b')
def memory(address,n):
    if n>=4096:
        offset=address%4096;count=(offset+n+4095)//4096
        result=lab.guest(f'dd if=/proc/{pid}/mem bs=4096 skip={address//4096} count={count} 2>/dev/null',timeout=60)[offset:offset+n]
    else:result=lab.guest(f'dd if=/proc/{pid}/mem bs=1 skip={address} count={n} 2>/dev/null',timeout=30)
    assert len(result)==n
    return result
def q(address):return struct.unpack('<Q',memory(address,8))[0]
def u(address):return struct.unpack('<I',memory(address,4))[0]
def f(address):return struct.unpack('<f',memory(address,4))[0]
def until(predicate,message):
    for _ in range(50):
        if predicate():return
        time.sleep(.3)
    snapshot('failed');raise AssertionError(message)
def raster():
    data=q(image);assert data,'OptoCat native software image is missing'
    pixels=q(data+56);assert pixels
    return hashlib.sha256(memory(pixels,320*164*4)).hexdigest()
def capture_meters():
    # Keep real pad audio running while the slow TCG/SSH readback completes.
    # A one-shot can decay below the floor between separate remote reads.
    data=q(image);pixels=q(data+56);offset=pixels%4096;count=(offset+320*164*4+4095)//4096
    stopped=threading.Event();errors=[]
    def pulse():
        try:
            deadline=time.monotonic()+30
            while not stopped.is_set() and time.monotonic()<deadline:
                request('/control',dict(name='PAD 1',action='press',value=100));stopped.wait(.15)
                request('/control',dict(name='PAD 1',action='release'));stopped.wait(.4)
        except Exception as error:errors.append(error)
        finally:request('/control',dict(name='PAD 1',action='release'))
    thread=threading.Thread(target=pulse,daemon=True);thread.start()
    try:
        for _ in range(5):
            time.sleep(.6)
            commands=[f'dd if=/proc/{pid}/mem bs=1 skip={address} count={size} 2>/dev/null' for address,size in ((meter_input,8),(meter_output,8),(reduction,4))]
            commands.append(f'dd if=/proc/{pid}/mem bs=4096 skip={pixels//4096} count={count} 2>/dev/null')
            raw=lab.guest('\n'.join(commands),timeout=60)
            ins=struct.unpack('<ff',raw[:8]);outs=struct.unpack('<ff',raw[8:16]);gr=struct.unpack('<f',raw[16:20])[0]
            green=raw[20+offset:20+offset+320*164*4].count(struct.pack('<I',0xff00ffa7))
            if max(ins)>1e-5 and max(outs)>1e-5 and green:
                snapshot('opto-audio');return max(ins),max(outs),green,gr
        raise AssertionError(f'Meter readback: input={ins}, output={outs}, green pixels={green}')
    finally:
        stopped.set();thread.join(timeout=25);assert not thread.is_alive() and not errors,errors
tap('SAMPLE');snapshot('sample-before')
request('/control',dict(name='SHIFT',action='press'));time.sleep(.3)
tap('PAD 5')
request('/control',dict(name='SHIFT',action='release'));time.sleep(2)
snapshot('duck')
until(lambda:q(owner)!=0,'Native Compressor screen did not bind its master Colour processor')
if q(settings)&1:tap('SCREEN 2')
assert q(settings)&1==0
tap('SCREEN 3') # Native Duck bypass must not disable the independent OptoCat path.
tap('SCREEN 2');snapshot('opto')
until(lambda:q(settings)&1==1,'B2 did not select OptoCat')
time.sleep(1);watch=raster()
request('/control',dict(name='K3',action='set',value=127));time.sleep(1)
gain=raster();assert gain!=watch,'Gain did not change the rendered kitten'
snapshot('opto-gain')
request('/control',dict(name='K3',action='set',value=51));time.sleep(.5)
request('/control',dict(name='K1',action='set',value=79));time.sleep(.5)
before=q(settings)
request('/control',dict(name='K1',action='set',value=0));time.sleep(1)
after=q(settings);assert (before>>2)&255!=(after>>2)&255,'K1 did not change Opto threshold'
count_before=u(blocks);hit_before=u(hits);peak_gr=0
for _ in range(3):
    tap('PAD 1');peak_gr=max(peak_gr,f(reduction))
    time.sleep(.3)
assert u(blocks)>count_before and u(hits)>hit_before and peak_gr>.1,'Live audio did not reach OptoCat DSP'
peak_in,peak_out,meter_pixels,meter_gr=capture_meters();peak_gr=max(peak_gr,meter_gr)
tap('SCREEN 3');assert (q(settings)^after)&2,'B3 did not toggle Opto bypass'
time.sleep(1);sleep=raster();assert sleep!=watch and sleep!=gain
snapshot('opto-sleep')
tap('SCREEN 3');assert (q(settings)&2)==(after&2)
tap('SCREEN 2');assert q(settings)&1==0
tap('SCREEN 3') # Restore the native Duck bypass state.
snapshot('duck-return')
tap('SAMPLE');time.sleep(2)
before=q(settings);tap('SCREEN 2');assert q(settings)==before,'B2 outside Compressor changed its type'
assert int(lab.guest('systemctl show mpc-clone-trial-v1-clone.service -p MainPID --value'))==pid
proof=dict(passed=True,base_revision=baseline['revision'],library_sha256=sha,pid=pid,
    probe_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    master_bound=True,b2_switches_both_directions=True,opto_threshold=True,opto_bypass=True,
    duck_bypass_independent=True,
    b2_outside_compressor_preserved=True,physical_device_tested=False,audio_asserted=True,animation_verified=True,
    untraced=tracer==0,
    audio_blocks_before=count_before,audio_blocks_after=u(blocks),signal_hits=u(hits),peak_reduction_db=peak_gr,
    stereo_meters_verified=True,meter_peak_input=peak_in,meter_peak_output=peak_out,meter_green_pixels=meter_pixels,
    raster_watch=watch,raster_gain=gain,raster_sleep=sleep,
    scope='Full QEMU native app/audio: Shift+Pad 5, exact master binding, B2 Type, K1/K3, real pad audio through OptoCat with GR/hit telemetry, native image pixels change for Gain/Bypass, Duck return and screen exit.')
(O/'verification.json').write_text(json.dumps(proof,indent=2)+'\n')
print(json.dumps(proof,indent=2),flush=True)
