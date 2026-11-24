"""Surface overdub on the existing disposable VM; no project-memory writes.

Prepare a nonempty sequence and the desired pad/16 Levels mode in native UI.
Starts playback before engaging recording; compares all preexisting events.
"""
import argparse,collections,hashlib,json,time
from pathlib import Path
import check_pad_midi_engine_vm as engine
from check_pad_midi_engine_vm import lab,control,tap,events
p=argparse.ArgumentParser();p.add_argument('label');p.add_argument('--pad',type=int,default=1)
p.add_argument('--velocity',type=int,required=True);p.add_argument('--repeat',action='store_true');a=p.parse_args()
assert a.label.replace('-','').isalnum() and 1<=a.pad<=16 and 1<=a.velocity<=127
O=Path(__file__).resolve().parent.parent/'build/pad-midi-pads';engine.O=O
digest=hashlib.sha256((O/'phone-input.so').read_bytes()).hexdigest()
assert lab.guest('sha256sum /run/mpc-clone-trial-v1/phone-input.so').split()[0].decode()==digest
tap('STOP');before=engine.snapshot(a.label+'-before');old=events(before)
assert old,'Use a nonempty disposable sequence'
assert not any(round(e['note']['velocity']*127)==a.velocity for e in old),'Use a unique velocity'
try:
    tap('PLAY');time.sleep(2);tap('SEQ RECORD');time.sleep(1)
    if a.repeat:control('NOTE REPEAT','press');time.sleep(.3)
    control('PAD '+str(a.pad),'press',a.velocity);time.sleep(2 if a.repeat else .65)
finally:
    control('PAD '+str(a.pad),'release')
    if a.repeat:control('NOTE REPEAT','release')
    time.sleep(.3);tap('STOP')
time.sleep(8);after=engine.snapshot(a.label);new=events(after)
key=lambda e:json.dumps(e,sort_keys=True,separators=(',',':'))
missing=collections.Counter(map(key,old))-collections.Counter(map(key,new))
added=[e for e in new if round(e['note']['velocity']*127)==a.velocity]
report=dict(passed=not missing and len(added)>=(2 if a.repeat else 1) and all(e['note']['length']>0 for e in added),
    library_sha256=digest,input='surface PLAY then SEQ RECORD then pad',note_repeat_held=a.repeat,
    old_event_count=len(old),new_event_count=len(new),missing_events=list(missing.elements()),added=added,
    full_current_source_build=False,hardware_tested=False,
    xpj_sha256=hashlib.sha256((O/(a.label+'.xpj')).read_bytes()).hexdigest())
(O/(a.label+'-verification.json')).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='added'},indent=2),flush=True)
print('Recorded:',[(e['time'],e['note']['length']) for e in added],flush=True)
assert report['passed'],'Overdub did not preserve the prior event multiset and add a positive-length note'
