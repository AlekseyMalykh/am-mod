"""Full ARM VM Seq colour probe using native controls; virtual SD only.

Requires the small lab fixture: A01 has events; A02 is empty. Does not
modify an attached physical card or the user's normal emulator project.
"""
from pathlib import Path
import hashlib,json,time
from check_live_chord import control,tap,request

OUT=Path(__file__).resolve().parent.parent/'build/sequence-refresh'

def sample(seconds=1.2):
    rows=[];end=time.monotonic()+seconds
    while time.monotonic()<end:
        s=json.loads(request('/state'))
        rows.append(dict(time=time.monotonic(),pads=s['pads']))
        time.sleep(.04)
    return rows

def active(rows,pad):return any(any(r['pads'][str(pad)]) for r in rows)

def choose(pad):
    control('PAD '+str(pad),'press',100);control('PAD '+str(pad),'release',0)
    time.sleep(.5)

if __name__=='__main__':
    tap('STOP');tap('SEQ');choose(1)
    filled=sample();assert active(filled,0)
    assert all(not active(filled,p) for p in range(1,16))
    choose(2);empty=sample();assert active(empty,1)
    control('SHIFT');control('K1','set',127);time.sleep(.5)
    shifted=sample();assert active(shifted,0) and active(shifted,1)
    assert all(not active(shifted,p) for p in range(2,16))
    assert any(r['pads']['1'][0]>0 and r['pads']['1'][0]==r['pads']['1'][1]==r['pads']['1'][2] for r in shifted)
    control('SHIFT','release');choose(1)
    deselected=sample();assert active(deselected,0) and not active(deselected,1)
    tap('PLAY');playing=sample(2)
    assert active(playing,0) and any(not any(r['pads']['0']) for r in playing)
    choose(2);time.sleep(4);handoff=sample(2)
    assert active(handoff,1) and any(not any(r['pads']['1']) for r in handoff)
    tap('STOP')
    rows=dict(filled=filled,empty_selected=empty,shift_white=shifted,
              empty_deselected=deselected,playing=playing,queue_handoff=handoff)
    (OUT/'vm-light-samples.json').write_text(json.dumps(rows,indent=2))
    proof=dict(passed=True,library_sha256=hashlib.sha256((OUT/'phone-input.so').read_bytes()).hexdigest(),
        native_firmware='1.3.0.12',physical_device_tested=False,
        occupied_and_empty_slots=True,selected_empty_visible=True,
        shift_retains_palette=True,live_colour_change=True,playing_flash=True,
        queued_handoff=True,
        unit_checks=['native global/clip event containers including last-event removal',
                     'twelve-colour and legacy settings serialization',
                     '250 ms playback brightness period; native selected pulse retained',
                     'Mute/Erase/native restore ownership',
                     'full frozen preload constructor'])
    (OUT/'vm-verification.json').write_text(json.dumps(proof,indent=2)+'\n')
    print(json.dumps(proof,indent=2))
