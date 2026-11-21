"""Finalize exact-build native import, history, PCM and cold-reload evidence."""
from pathlib import Path
import hashlib,json,math
R=Path(__file__).resolve().parent;O=R/'build/program-import-dev6';V=O/'vm'
sha=lambda b:hashlib.sha256(b).hexdigest()
def read(p):return json.loads(p.read_text())
compiled=read(O/'compile-verification.json');assert compiled['passed']
library=compiled['library_sha256'];assert sha((O/'phone-input.so').read_bytes())==library
for n,h in compiled['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
projects={n:read(V/(n+'-project.json'))['data'] for n in ['baseline','pgm_after','xpm_after','undone','redone','collision_after','named_save']}
def instruments(n):return projects[n]['tracks'][0]['program']['drum']['instruments']
def names(n,p):return [l['sampleName'] for l in instruments(n)[p]['layersv'] if l['sampleName']]
for n in projects:assert len(instruments(n))==128
for p in range(16,32):
 expected=['503 BD Tape (%02d)'%(p-15)]
 assert names('baseline',p)==[]
 for n in ['pgm_after','xpm_after','undone','redone','collision_after','named_save']:assert names(n,p)==expected,(n,p)
for n in ['xpm_after','redone','collision_after','named_save']:
 assert names(n,48)==['PI440','PI220'] and names(n,49)==['PI220']
 a,b=instruments(n)[48:50];assert a['coarseTune']==2 and b['coarseTune']==3
 assert [(l['velocityStart'],l['velocityEnd']) for l in a['layersv'][:2]]==[(0,63),(64,127)]
for n in ['baseline','pgm_after','undone']:assert names(n,48)==[] and names(n,49)==[]
assert names('collision_after',80)==['PI1']
# The baseline is an input XPJ; native first load normalizes its slice reference.
# Names, slices and synthesis parameters remain; subsequent snapshots match exactly.
a=instruments('baseline')[0]['layersv'];b=instruments('pgm_after')[0]['layersv']
assert a[0]['sliceIndex']==0 and b[0]['sliceIndex']==129
for x,y in zip(a,b):assert {k:v for k,v in x.items() if k!='sliceIndex'}=={k:v for k,v in y.items() if k!='sliceIndex'}
for n in projects:
 if n!='baseline':assert instruments(n)[0]['layersv']==b,n
for n in ['pgm_review','pgm_after','xpm_review','xpm_after','collision_review','collision_after','occupied_rejected']:
 d=read(V/(n+'.json'));assert d['library_sha256']==library
 if n.endswith('_after'):assert d['program_page']==0 and d['program_error']==''
assert 'D01 is occupied' in read(V/'occupied_rejected.json')['program_error']
checks=[*(f'pgm_audio_B{i:02}' for i in range(1,17)), 'xpm_audio_D01_low','xpm_audio_D01_high','xpm_audio_D02','collision_audio_F01','collision_original_D02','cold_reload_audio_D02']
for name in checks:
 d=read(V/(name+'.json'));assert d['passed'] and d['frames']>40000 and d['peak']>100
 assert (V/(name+'.wav')).stat().st_size>80000
 if 'expected_hz' in d:assert math.isclose(d['expected_hz'],d['dominant_hz'],rel_tol=.01)
saved=read(V/'named-save-assets-verification.json');assert saved['passed'] and len(saved['samples'])==19
assert sha((V/'named_save.xpj').read_bytes())==saved['project_sha256']
for name,d in saved['samples'].items():assert d['frames']>0 and d['channels'] in (1,2) and len(d['sha256'])==64
bootpath=R/'build/bootloader-compat/vm/sampleboot-verification.json';boot=read(bootpath)
assert boot['passed'] and boot['library_sha256']==library and boot['native_named_project_reloaded']
assert boot['verified_payload_files']==84 and len(boot['pads'])==18
for p in range(16,32):assert boot['pads'][str(p)]==names('named_save',p)
for p in (48,49):assert boot['pads'][str(p)]==names('named_save',p)
for n,h in boot['evidence_sha256'].items():assert sha((bootpath.parent/n).read_bytes())==h,n
sources=dict(compiled['sources_sha256'])
for n in ['verify_program_import_vm.py','lab/verify_bootloader_vm.py','lab/program_vm.py','lab/check_program_audio_vm.py','lab/check_program_collision_vm.py','lab/check_program_cold_audio_vm.py']:sources[n]=sha((R/n).read_bytes())
evidence={p.name:sha(p.read_bytes()) for p in V.iterdir() if p.is_file() and p.suffix in ('.json','.xpj','.wav') and p.name!='verification.json'}
report=dict(passed=True,virtual_device_only=True,hardware_tested=False,library_sha256=library,sources_sha256=sources,native_named_save_and_reload=True,pgm_imported_pads=16,xpm_imported_pads=2,xpm_velocity_layers=True,undo_redo=True,occupied_pad_rejected=True,same_name_sample_collision_preserved=True,pcm_captures=len(checks),saved_samples=19,saved_project_sha256=saved['project_sha256'],cold_reload_report_sha256=sha(bootpath.read_bytes()),evidence_sha256=evidence)
(V/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS exact-build imports, native Undo/Redo, 22 PCM captures, native save and cold reload')
