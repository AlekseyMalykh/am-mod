"""Check the compiled callback vector, including ELF function relocations.

This checks shipped ARM bytes rather than just the generated bridges. A new
hook must never shift a footer callback into a lighting callback's slot.
"""
from pathlib import Path
import json
import subprocess
import sys

R=Path(__file__).resolve().parent
FUNCTIONS={
    'REBUILD':'rebuild','DTOR':'editor_dtor','DELETE':'editor_delete',
    'PAD':'pad_event','CONTROLLER':'controller','BUTTONS':'buttons',
    'SAMPLE_PAINT':'sample_paint','PROJECT_PAINT':'project_paint','RGB':'rgb',
    'CHAR_INCREMENT':'char_increment','CASE':'case_change','MOVE':'move_action',
    'CLEAR':'clear_action','ERASE':'erase_action','PLUS':'plus_action',
    'MINUS':'minus_action','SCROLL':'scroll_action','RIGHT':'right_action',
    'SAMPLE_DTOR':'sample_dtor','PROJECT_DTOR':'project_dtor',
    'SIXTEEN_REFRESH':'sixteen_refresh','SIXTEEN_PROCESS':'sixteen_process',
    'MUSIC_SAVE':'music_save','MUSIC_LOAD':'music_load','MUSIC_READ':'music_read',
    'MUSIC_KEY':'music_key','NEW_PROJECT':'new_project',
    'SIXTEEN_CTOR':'sixteen_ctor','SIXTEEN_DTOR':'sixteen_dtor',
    'STEP_DTOR':'step_dtor','NOW_TICK':'now_tick','BANK_CTOR':'bank_ctor',
    'BANK_DTOR':'bank_dtor','BUTTON_LIGHT':'button_light',
    'CHOP_REFRESH':'chop_refresh','NOTE_FOOTER':'note_footer',
    'FILES_DTOR':'files_dtor','COPY_DTOR':'copy_dtor',
    'COPY_CONTEXT_CTOR':'copy_context_ctor','COPY_CONTEXT_DTOR':'copy_context_dtor','COPY_SOURCE':'batch_copy_source','COPY_RELEASE_EXIT':'batch_copy_native_release_exit','AUDITION_CTOR':'audition_ctor',
    'MUSIC_PROJECT':'music_ui_request_project','EVENT_LABEL':'step_native_event_label',
    'FILES_CTOR':'files_ctor','MASTER_VOLUME_CTOR':'master_volume_ctor','FILE_PREVIEW':'file_preview',
    'PRIVATE_PREVIEW_WORKER':'private_preview_worker',
    'PREVIEW_EVENT_GATE':'step_native_preview_suppressed',
    'SAMPLE_FILE_COPY':'sample_file_copy',
}

def verify(path):
    hooks=json.loads((R/'build/phone-hook-map.json').read_text())
    assert {r['name'] for r in hooks}==set(FUNCTIONS)
    symbols={}
    for row in subprocess.check_output(['aarch64-linux-gnu-nm','-a','-S',str(path)],text=True).splitlines():
        parts=row.split()
        if len(parts)==4 and (parts[2] in ('t','T') or parts[3].startswith('callbacks.')):
            symbols[parts[3]]=(int(parts[0],16),int(parts[1],16))
    vectors=[v for n,v in symbols.items() if n.startswith('callbacks.')]
    assert len(vectors)==1,'Expected one compiled named callback vector'
    start,size=vectors[0];assert size==len(hooks)*8
    relocations={}
    for row in subprocess.check_output(['aarch64-linux-gnu-readelf','-rW',str(path)],text=True).splitlines():
        parts=row.split()
        if len(parts)>=4 and parts[2]=='R_AARCH64_RELATIVE':
            relocations[int(parts[0],16)]=int(parts[3],16)
        elif len(parts)>=7 and parts[2]=='R_AARCH64_ABS64':
            relocations[int(parts[0],16)]=int(parts[3],16)+int(parts[6],16)
    for i,hook in enumerate(hooks):
        function=FUNCTIONS[hook['name']]
        assert relocations.get(start+i*8)==symbols[function][0],hook['name']+' callback mismatch'
    # Reproduce the discovered positional error: Chop, footer and Project must
    # have distinct targets; the swapped vector fails the same byte checks.
    indices={h['name']:i for i,h in enumerate(hooks)}
    a=start+8*indices['CHOP_REFRESH'];b=start+8*indices['NOTE_FOOTER']
    assert relocations[a]!=relocations[b]
    swapped=dict(relocations);swapped[a],swapped[b]=swapped[b],swapped[a]
    assert swapped[a]!=symbols['chop_refresh'][0]
    return {'callbacks_verified':len(hooks),'compiled_vector':hex(start),
            'chop_footer_project_slots':'distinct_correct_functions','swapped_vector_rejected':True}

if __name__=='__main__':
    print(json.dumps(verify(Path(sys.argv[1]) if len(sys.argv)>1 else R/'build/phone-input.so'),indent=2))
