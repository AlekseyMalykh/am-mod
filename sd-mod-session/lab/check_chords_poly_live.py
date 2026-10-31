"""Full native VM entry regression. Only the local QEMU lab; no physical SD.
Run once the r24c candidate is running in Sample with a loaded Mono sound.
"""
from pathlib import Path
import hashlib,json,struct,subprocess,sys,time,urllib.request
sys.path.insert(0,'/opt/mpc-sample-lab')
import lab
R=Path(__file__).resolve().parent.parent
O=R/'work/lab-tests';O.mkdir(parents=True,exist_ok=True)
LIB=R/'build/chords/phone-input.so'
URL='http://127.0.0.1:8765'
def request(route,p=None):
    d=None if p is None else json.dumps(p).encode()
    return urllib.request.urlopen(urllib.request.Request(URL+route,d,{'Content-Type':'application/json'}),timeout=20).read()
def tap(name):
    request('/control',dict(name=name,action='press'));time.sleep(.2)
    request('/control',dict(name=name,action='release'));time.sleep(.6)
def snapshot(name):
    (O/(name+'.png')).write_bytes(request('/screen.png'))
pid=int(lab.guest('systemctl show mpc-clone-trial-v1-clone.service -p MainPID --value'))
assert pid>1
sha=hashlib.sha256(LIB.read_bytes()).hexdigest()
assert lab.guest('sha256sum /run/mpc-clone-trial-v1/phone-input.so').decode().split()[0]==sha
maps=lab.guest(f'cat /proc/{pid}/maps').decode()
base=int(next(x for x in maps.splitlines() if 'phone-input.so' in x).split('-')[0],16)
symbols=subprocess.check_output(['aarch64-linux-gnu-nm','-an',str(LIB)],text=True).splitlines()
offsets=[int(x.split()[0],16) for i,x in enumerate(symbols[:-1]) if x.split()[-1:] == ['root'] and symbols[i+1].split()[-1:] == ['slider']]
assert len(offsets)==1
root_offset=offsets[0]
def mem(a,n):
    b=lab.guest(f'dd if=/proc/{pid}/mem bs=1 skip={a} count={n} 2>/dev/null')
    assert len(b)==n
    return b
def q(a):return struct.unpack('<Q',mem(a,8))[0]
def poly():
    r=q(base+root_offset);assert r
    things=q(r+0x148);assert things
    instrument=q(things+0x668);assert instrument
    return struct.unpack('<I',mem(instrument+0x1690,4))[0]
deadline=time.monotonic()+180
while True:
    try:before=poly();break
    except AssertionError:
        if time.monotonic()>deadline:raise
        time.sleep(1)
assert before==1,'Start with Mono so the native Auto Poly command is exercised'
snapshot('r24c-before-entry')
leds=json.loads(request('/state'))['leds']
if leds.get('40',0)<2:tap('16 LEVELS')
tap('CHOP')
for _ in range(30):
    if poly()==0:break
    time.sleep(.5)
else:raise AssertionError('Native model did not switch to Poly')
time.sleep(2)
snapshot('r24c-chords-after-entry')
assert int(lab.guest('systemctl show mpc-clone-trial-v1-clone.service -p MainPID --value'))==pid
leds=json.loads(request('/state'))['leds']
assert leds.get('75',0)>=2 and leds.get('40',0)>=2
def pad_rgb():return json.loads(request('/state'))['pads']['0']
idle=pad_rgb()
for _ in range(3):
    request('/control',dict(name='PAD 1',action='press',value=100))
    for retry in range(30):
        if pad_rgb()!=idle:break
        time.sleep(.2)
    else:raise AssertionError('Root did not light on press; set Orig Note before probe')
    request('/control',dict(name='PAD 1',action='release'))
    for retry in range(30):
        if pad_rgb()==idle:break
        time.sleep(.2)
    else:raise AssertionError('Root remained held after physical release')
# Back, then reenter with Poly already selected, and leave again.
tap('SCREEN 1');snapshot('r24c-after-back');assert poly()==0
tap('CHOP');snapshot('r24c-chords-reentry');assert poly()==0
tap('SCREEN 1');assert poly()==0
assert int(lab.guest('systemctl show mpc-clone-trial-v1-clone.service -p MainPID --value'))==pid
report=dict(passed=True,library_sha256=sha,pid=pid,mono_before=1,poly_after=0,
 native_model_changed=True,reentered=True,back_preserves_poly=True,root_retrigger_releases=3,
 physical_device_tested=False,audio_asserted=False,
 probe_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
 scope='Full QEMU system native app: loaded Mono sound with Orig Note, Chords entry applies Poly=0, three root strikes/releases restore idle LED, Back and already-Poly reentry; no audio assertion')
(O/'r24c-native-entry.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
