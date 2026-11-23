"""Drive the real VM transport and trial queue; inspect native XPJ autosave.

No event insertion, recorder mock or handcrafted project in this test.
"""
from pathlib import Path
import gzip, hashlib, json, os, subprocess, sys, time, urllib.request
R=Path(__file__).resolve().parent.parent; O=R/'build/pad-midi-vm'
sys.path.insert(0,'/opt/mpc-sample-lab');import lab
assert '/'+os.environ.get('AM_MOD_VM_RUN_NAME','pad-midi-recording')+'/sdcard.img' in str(lab.qmp('query-block'))

def control(name,action,value=100):
    data=json.dumps(dict(name=name,action=action,value=value)).encode()
    urllib.request.urlopen(urllib.request.Request('http://127.0.0.1:8765/control',data,{'Content-Type':'application/json'}),timeout=10).read()

def tap(name):
    control(name,'press');time.sleep(.3);control(name,'release');time.sleep(.5)

def request(command):
    pid=int(lab.guest('systemctl show mpc-clone-trial-v1-clone.service -p MainPID --value',timeout=20));assert pid>1
    path='/proc/'+str(pid)+'/root/run/am-pad-midi-vm'
    lab.guest('cat > '+path+'.tmp && mv '+path+'.tmp '+path+'.request',data=(command+'\n').encode(),timeout=20)
    time.sleep(.3)

def snapshot(label):
    pid=int(lab.guest('systemctl show mpc-clone-trial-v1-clone.service -p MainPID --value',timeout=20))
    assert pid>1
    deadline=time.monotonic()+45
    while True:
        data=lab.guest('cat /proc/'+str(pid)+'/root/data/TempProjects/Temp-Project.xpj',timeout=20)
        try:
            plain=gzip.decompress(data);d=json.loads(plain[plain.index(b'{'):]);break
        except (ValueError,EOFError,OSError):
            if time.monotonic()>deadline:raise
            time.sleep(.5)
    (O/(label+'.xpj')).write_bytes(data);(O/(label+'.json')).write_text(json.dumps(d,indent=2)+'\n')
    return d

def events(d):
    result=[]
    for seq in d['data']['sequences']:
        for maps in seq['value']['trackClipMaps']:
            for clip in maps:
                for e in clip['value']['eventList']['events']:
                    if e['type']==3:result.append(e)
    return result

if __name__=='__main__' and '--verify-recorded' not in sys.argv:
    tap('STOP');tap('SAMPLE')
    before=snapshot('engine-before');assert not events(before), 'Use a fresh disposable project'
    tap('SEQ RECORD');tap('PLAY');time.sleep(5)
    request('down 0 60 54 1');time.sleep(.4)
    request('down 1 67 112 2');time.sleep(.4)
    request('up 0 60 0 1');time.sleep(.3)
    request('up 1 67 0 2');time.sleep(.3)
    tap('STOP');request('stop');time.sleep(2)
    after=snapshot('engine-recorded');notes=events(after)
    assert len(notes)>=2,notes
    assert all(n['note']['length']>0 for n in notes),notes
    assert any(n['time']>0 for n in notes),notes
    assert {round(n['note']['velocity']*127) for n in notes}=={54,112},notes
    assert len({n['note']['modifierValue0'] for n in notes})==2,notes
if __name__=='__main__':
    notes=events(json.loads((O/'engine-recorded.json').read_text()))
    assert len(notes)==2 and all(n['note']['length']>0 for n in notes)
    assert {round(n['note']['velocity']*127) for n in notes}=={54,112}
    assert {round((n['note']['modifierValue0']-.5)*240)+60 for n in notes}=={60,67}
    print(json.dumps(notes,indent=2),flush=True)
    log=lab.guest('cat /run/mpc-clone-trial-v1/app/clone.txt',timeout=30)
    (O/'engine-journal.log').write_bytes(log)
    assert b'PAD_MIDI_VM op=up' in log and b'faults=0 held=0' in log
    files=['engine-before.xpj','engine-recorded.xpj','engine-journal.log','phone-input.so']
    proof=dict(passed=True,native_engine=True,native_recorder=True,native_xpj_save=True,
       native_xpj_load=False,physical_pads=False,hardware_tested=False,
       notes=notes,sha256={f:hashlib.sha256((O/f).read_bytes()).hexdigest() for f in files})
    (O/'engine-verification.json').write_text(json.dumps(proof,indent=2)+'\n')
    print('PASS real Engine -> native recorder -> XPJ (load is a separate check)',flush=True)
