"""Real surface input close to loop end. Prepare sample/16 Levels via UI.

The VM helper only reads the app's published clock to time the surface pad;
it never writes project memory or creates a recorder event.
"""
import argparse,hashlib,json,subprocess,time,threading
from pathlib import Path
import check_pad_midi_engine_vm as engine
from check_pad_midi_engine_vm import lab,control,tap,events
p=argparse.ArgumentParser();p.add_argument('label');p.add_argument('--pad',type=int,default=1)
p.add_argument('--velocity',type=int,required=True);p.add_argument('--hold',type=float,default=.65)
p.add_argument('--before-end',type=int,default=96);a=p.parse_args()
assert a.label.replace('-','').isalnum() and 1<=a.pad<=16 and 1<=a.velocity<=127 and .02<=a.hold<=3
R=Path(__file__).resolve().parent.parent;O=R/'build/pad-midi-pads';engine.O=O
library=O/'phone-input.so';digest=hashlib.sha256(library.read_bytes()).hexdigest()
metadata=lab.guest('sha256sum /run/mpc-clone-trial-v1/phone-input.so; systemctl show mpc-clone-trial-v1-clone.service -p MainPID --value',timeout=20).decode().splitlines()
assert metadata[0].split()[0]==digest;pid=int(metadata[1]);assert pid>1
maps=lab.guest('cat /proc/'+str(pid)+'/maps',timeout=20).decode()
base=int(next(l for l in maps.splitlines() if 'phone-input.so' in l and l.split()[2]=='00000000').split('-')[0],16)
symbols={v[2]:int(v[0],16) for l in subprocess.check_output(['aarch64-linux-gnu-nm','-an',str(library)],text=True).splitlines() if len(v:=l.split())==3}
address=base+symbols['clock_tick']
helper=(O/'watch-native-clock').read_bytes()
lab.guest('cat > /run/am-watch-clock && chmod 700 /run/am-watch-clock',data=helper,timeout=20)
before=engine.snapshot(a.label+'-before');seq=before['data']['sequences'][0]['value'];length=seq['lengthPulses']
tap('STOP')
window=(length-a.before_end,min(length-1,length-a.before_end+72))
assert 0<=window[0]<=window[1]<length
cmd='/run/am-watch-clock '+str(pid)+' '+format(address,'x')+' '+str(length)+' 0 '+str(length-1)+' 30 trace'
trace=subprocess.Popen(['ssh',*lab.ssh_base(),'root@'+lab.HOST,cmd],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
samples=[]
def read_trace():
    for line in trace.stdout:
        if line.startswith('TICK '):samples.append((time.monotonic(),int(line.split()[1])))
reader=threading.Thread(target=read_trace,daemon=True);reader.start()
time.sleep(2);tap('SEQ RECORD');tap('PLAY');deadline=time.monotonic()+20
while not samples or not window[0]<=samples[-1][1]%length<=window[1]:
    if time.monotonic()>deadline:raise RuntimeError('Clock window timeout')
    time.sleep(.002)
on_tick=samples[-1][1];started=time.monotonic()
control('PAD '+str(a.pad),'press',a.velocity);time.sleep(a.hold);control('PAD '+str(a.pad),'release');released=time.monotonic();held=released-started
time.sleep(.1);off_tick=min(samples,key=lambda s:abs(s[0]-released))[1]
trace.terminate();trace.wait(timeout=5);reader.join(timeout=2)
(O/(a.label+'-clock.json')).write_text(json.dumps(samples,indent=2)+'\n')
time.sleep(.7);tap('STOP');time.sleep(8)
after=engine.snapshot(a.label);notes=[n for n in events(after) if round(n['note']['velocity']*127)==a.velocity]
assert notes, 'No recorded note at the test velocity'
clock_delta=(off_tick-on_tick)%length
expected=clock_delta
short=any(n['note']['length']<max(1,expected-240)*.35 for n in notes)
report=dict(label=a.label,hardware_tested=False,library_sha256=digest,read_only_clock_probe=True,
    on_clock_tick=on_tick,off_clock_tick=off_tick,loop_length=length,hold_wall_seconds=held,expected_native_ticks=expected,
    quantiser=after['data']['quantiser'],notes=notes,short_gate=short,
    xpj_sha256=hashlib.sha256((O/(a.label+'.xpj')).read_bytes()).hexdigest())
(O/(a.label+'-verification.json')).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ['label','on_clock_tick','off_clock_tick','hold_wall_seconds','expected_native_ticks','short_gate']}),flush=True)
print('Recorded:',[(n['time'],n['note']['length']) for n in notes],flush=True)
if short:raise SystemExit('REPRODUCED suspiciously short gate near loop end')
