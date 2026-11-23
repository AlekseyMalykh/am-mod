"""Surface MIDI -> production pad hook -> Synth Keys -> native recorder.

Run after opening a fresh, empty project in the private full-system VM.
This is emulated physical-control input, not a hardware validation.
"""
import hashlib,json,sys,time
from pathlib import Path
from check_pad_midi_engine_vm import control,tap,snapshot,events,lab,request
import check_pad_midi_engine_vm as engine
R=Path(__file__).resolve().parent.parent;O=R/'build/pad-midi-pads';engine.O=O

def shot(name):
    target='/opt/mpc-sample-lab/out/run/'+engine.os.environ.get('AM_MOD_VM_RUN_NAME','pad-midi-recording')+'/'+name+'.ppm'
    lab.qmp('screendump',filename=target)
    (O/(name+'.ppm')).write_bytes(Path(target).read_bytes())

before=snapshot('pads-before');assert not events(before), 'Create an empty project through native New Project first'
tap('SAMPLE');control('PAD FX','press');time.sleep(1);control('KNOB FX','press');time.sleep(1)
control('KNOB FX','release');control('PAD FX','release');time.sleep(2)
tap('PAD 13');time.sleep(1);tap('16 LEVELS');time.sleep(1)
control('SHIFT','press');time.sleep(1);tap('SEQ RECORD');time.sleep(1);control('SHIFT','release');time.sleep(1)
shot('synth-keys-armed')
tap('SEQ RECORD');tap('PLAY');time.sleep(5)
control('PAD 5','press',73);time.sleep(.6);control('PAD 7','press',101);time.sleep(.6)
control('PAD 5','release');time.sleep(.5);control('PAD 7','release');time.sleep(1)
request('status');time.sleep(1)
tap('STOP');time.sleep(8)
after=snapshot('pads-recorded');notes=events(after)
assert len(notes)==2,notes
assert {round(n['note']['velocity']*127) for n in notes}=={73,101},notes
assert all(n['note']['length']>0 for n in notes),notes
assert len({n['note']['modifierValue0'] for n in notes})==2,notes
log=lab.guest('cat /run/mpc-clone-trial-v1/app/clone.txt',timeout=30)
(O/'pads-journal.log').write_bytes(log)
assert b'SYNTH_PAD_RECORD key=4 ' in log and b'SYNTH_PAD_RECORD key=6 ' in log
assert b'velocity=73' in log and b'velocity=101' in log
assert b'PAD_MIDI_VM op=status' in log and b'faults=0 held=0' in log
expected=json.loads((O/'compile-verification.json').read_text())['library_sha256']
actual=lab.guest('sha256sum /run/mpc-clone-trial-v1/phone-input.so',timeout=20).split()[0].decode()
assert actual==expected
proof=dict(passed=True,emulated_surface_pads=True,native_engine=True,native_xpj_save=True,
    native_xpj_load=False,hardware_tested=False,library_sha256=actual,notes=notes,
    sha256={n:hashlib.sha256((O/n).read_bytes()).hexdigest() for n in ['pads-before.xpj','pads-recorded.xpj','pads-journal.log','synth-keys-armed.ppm']})
(O/'pads-verification.json').write_text(json.dumps(proof,indent=2)+'\n')
shot('synth-keys-recorded')
print('PASS surface pads -> Synth Keys -> native Engine -> XPJ: velocities73/101, chord, release before Stop',flush=True)
