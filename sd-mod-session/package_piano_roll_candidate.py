"""Promote the exact VM-verified Piano Roll library into an SD candidate."""
from pathlib import Path
import hashlib,json,re,subprocess,sys,zipfile

R=Path(__file__).resolve().parent
VERSION=re.search(r'#define AM_MOD_VERSION "([^"]+)"',(R/'src/mod_build.h').read_text(encoding='utf-8')).group(1)
B=R/'build/piano-roll-dev'
O=R/'build/piano-roll-card'
O.mkdir(parents=True,exist_ok=True)
sha=lambda data:hashlib.sha256(data).hexdigest()
def read(path):return json.loads(path.read_text(encoding='utf-8'))
def verify_sources(report):
 assert report['passed']
 for name,digest in report['sources_sha256'].items():
  assert sha((R/name).read_bytes())==digest,'Stale source: '+name

compile_proof=read(B/'compile-verification.json')
model_proof=read(R/'build/piano-roll/verification.json')
for proof in (compile_proof,model_proof):verify_sources(proof)
library=(B/'phone-input.so').read_bytes()
assert sha(library)==compile_proof['library_sha256']
assert '#define AM_MOD_VERSION "'+VERSION+'"' in (R/'src/mod_build.h').read_text(encoding='utf-8')
assert compile_proof['full_preload_constructor'] and compile_proof['mixer_phone_routes']
assert '-DMPC_MIDI_EVENTS=1' in compile_proof['compiler_flags']

base=R/'build/r30b-palette-rear-fix/am-mod-r30b-palette-rear-fix.zip'
base_proof=read(base.parent/'package-verification.json')
assert base_proof['passed'] and sha(base.read_bytes())==base_proof['package_sha256']
with zipfile.ZipFile(base) as z:
 assert z.testzip() is None
 files={i.filename:z.read(i) for i in z.infolist() if not i.is_dir()}
launcher=files['autorun/mod-launcher.sh']
old=sha(files['autorun/phone-input.so']).encode()
assert launcher.count(old)==1
usb_proof=json.loads((R/'build/usb-drive/verification.json').read_text())
assert usb_proof['passed']
for name,digest in usb_proof['sources_sha256'].items():assert sha((R/name).read_bytes())==digest,name
helper=(R/'build/usb-drive/usb-drive-screen').read_bytes()
assert sha(helper)==usb_proof['screen_sha256']
old_helper=sha(files['autorun/usb-drive-screen']).encode()
assert launcher.count(old_helper)==1
updated_launcher=launcher.replace(old_helper,sha(helper).encode())
files['autorun/usb-drive-screen']=helper
files['autorun/phone-input.so']=library
files['autorun/mod-launcher.sh']=updated_launcher.replace(old,sha(library).encode())
entry=files['autorun/autorun.sh']
assert entry.count(sha(launcher).encode())==1
assert entry.count(b'revision=r30b-palette-rear-fix')==1
files['autorun/autorun.sh']=entry.replace(sha(launcher).encode(),sha(files['autorun/mod-launcher.sh']).encode()).replace(b'revision=r30b-palette-rear-fix',('revision='+VERSION).encode())
manifest=json.loads(files['autorun/autorun.json'])
manifest['value0']['scripts'][0]['message']='Am MOD '+VERSION
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
salt=b'f7bed1c9-7e90-4731-9194-c996666b4331'
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+salt).encode()

prefix='SOURCE/piano-roll-candidate/'
for proof in (compile_proof,model_proof,usb_proof):
 for name in proof['sources_sha256']:files[prefix+name]=(R/name).read_bytes()
for name in ('PIANO_ROLL_STAGE1_RU.md','PIANO_ROLL_MIDI_PLAN_RU.md','MIDI_FILES_RU.md','FEATURE_MERGE_RU.md','SEQUENCE_PAD_MUTES_DESIGN_RU.md','package_piano_roll_candidate.py'):
 files[prefix+name]=(R/name).read_bytes()
for suite in ('global-palette','midi-file','mixer-step1','piano-input','pad-colour-rgb','midi-tools','midi-archive','features-dev5'):
 extra=read(R/'build'/suite/'verification.json');verify_sources(extra)
 files[prefix+suite+'-verification.json']=(R/'build'/suite/'verification.json').read_bytes()
 for name in extra['sources_sha256']:files[prefix+name]=(R/name).read_bytes()
midi_proof=read(R/'build/midi-events/snapshot-verification.json');verify_sources(midi_proof)
files[prefix+'midi-events-verification.json']=(R/'build/midi-events/snapshot-verification.json').read_bytes()
for name in midi_proof['sources_sha256']:files[prefix+name]=(R/name).read_bytes()
files[prefix+'usb-drive-verification.json']=(R/'build/usb-drive/verification.json').read_bytes()
files[prefix+'compile-verification.json']=(B/'compile-verification.json').read_bytes()
files[prefix+'model-verification.json']=(R/'build/piano-roll/verification.json').read_bytes()
vm_reports=[]
for name in ('live-verification.json','navigation-verification.json','controls-verification.json','record-verification.json','record-history-verification.json'):
 path=B/'vm'/name;proof=read(path)
 assert proof['passed'] and proof['virtual_device_only'] and proof['library_sha256']==sha(library)
 files[prefix+'vm/'+name]=path.read_bytes();vm_reports.append(name)
 for evidence,digest in proof['evidence_sha256'].items():
  assert Path(evidence).name==evidence
  content=(path.parent/evidence).read_bytes();assert sha(content)==digest,evidence
  files[prefix+'vm/'+evidence]=content
live_path=R/'build/midi-live-dev4/vm/native-layers-verification.json'
live_proof=read(live_path);verify_sources(live_proof)
assert live_proof['virtual_device_only'] and live_proof['library_sha256']==sha(library)
files[prefix+'vm/native-layers-verification.json']=live_path.read_bytes();vm_reports.append('native-layers-verification.json')
for name,digest in live_proof['evidence_sha256'].items():
 assert Path(name).name==name
 content=(live_path.parent/name).read_bytes();assert sha(content)==digest,name
 files[prefix+'vm/midi-'+name]=content
for name in ('check_midi_layers_vm.py','inspect_midi_live_vm.py','finalize_midi_layers_vm.py'):files[prefix+'lab/'+name]=(R/'lab'/name).read_bytes()
feature_path=R/'build/features-dev5/vm/verification.json'
feature_proof=read(feature_path);verify_sources(feature_proof)
assert feature_proof['virtual_device_only'] and feature_proof['library_sha256']==sha(library)
files[prefix+'vm/features-verification.json']=feature_path.read_bytes();vm_reports.append('features-verification.json')
for name,digest in feature_proof['evidence_sha256'].items():
 assert Path(name).name==name
 content=(feature_path.parent/name).read_bytes();assert sha(content)==digest,name
 files[prefix+'vm/features-'+name]=content
for name in ('check_features_vm.py','inspect_features_vm.py','finalize_features_vm.py','features_pcm.py'):files[prefix+'lab/'+name]=(R/'lab'/name).read_bytes()
for path in sorted((R/'src').iterdir()):
 if path.is_file():files[prefix+'src/'+path.name]=path.read_bytes()
for name in ('inspect_piano_roll_vm.py','verify_piano_roll_live_vm.py','check_piano_navigation_vm.py','check_piano_controls_vm.py','check_piano_record_vm.py','check_piano_record_history_vm.py'):
 files[prefix+'lab/'+name]=(R/'lab'/name).read_bytes()
files['PIANO_ROLL_STAGE1_RU.md']=(R/'PIANO_ROLL_STAGE1_RU.md').read_bytes()
files['MIDI_FILES_RU.md']=(R/'MIDI_FILES_RU.md').read_bytes()
files['FEATURE_MERGE_RU.md']=(R/'FEATURE_MERGE_RU.md').read_bytes()
files['README.ru.md']=(
 '# Am MOD '+VERSION+'\n\n'
 'Разработческая сборка пианоролла, проверенная в полной виртуальной MPC.\n'
 'Вход: Shift + Seq → Events / Steps → Knob FX. Работает при FX Mode Stock и Am MOD.\n\n'
 'Навигация: K1 — Scroll rows; K2 — Grid; K3 — Scroll time.\n'
 'DATA / Fader: время / строки. Shift+DATA / Shift+Fader: зум времени / строк.\n'
 'Edit: K1 — позиция, K2 — пэд/нота/чоп, K3 — длина.\n'
 'Shift в Edit: K1 — точный сдвиг, K2 — velocity, K3 — точная длина.\n'
 'DATA / Fader в Edit перемещают выделение по двум осям; обзор следует за нотами.\n'
 'Shift+B1: просмотр событий; click / Shift+click: одиночное / множественное выделение.\n'
 'B2: прямоугольное выделение. B3: Edit выбранных нот. B1: отмена предпросмотра.\n'
 'Shift+B2: Follow. Shift+B3: операции, включая Import / Export MIDI.\n'
 'Shift− / Shift+: общие Undo / Redo. Seq Record + Play: штатная запись пэдов/L/C.\n'
 'Mod Settings → Cursor → Hide cursor on Play: скрывать выбор пэда при Play.\n\n'
 'MIDI-файлы: MIDI_FILES_RU.md. Live MIDI: Pads / Levels, Shift+K1 в Out Map.\n'
 'Sequence Pad Mutes: Mute+B3 — режим, Mute+B2 — снять мьюты секвенции.\n'
 'Seq Audition: Shift+B1 в браузере сэмплов; K1 — Orig Note файла.\n'
 'Новые функции и проверки: FEATURE_MERGE_RU.md.\n'
 'SD Card Access после подтверждения: все 16 пэдов янтарные до выхода.\n'
 'Vinyl: диск останавливается по Stop и продолжает вращение по Play.\n'
 'Подробности и границы проверок: PIANO_ROLL_STAGE1_RU.md.\n'
).encode()
for name in ('autorun.sh','mod-launcher.sh'):
 subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+name],check=True)
target=O/('am-mod-'+VERSION+'.zip')
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
 for name,content in sorted(files.items()):z.writestr(name,content)
sys.path.insert(0,str(R.parent/'installer'))
from mpc_mod_installer import Package
pkg=Package.from_file(target)
assert pkg.revision==VERSION and pkg.files['phone-input.so']==library
assert all('VM ONLY' not in name and 'VM_ONLY' not in name for name in files)
report=dict(passed=True,revision=VERSION,package_sha256=sha(target.read_bytes()),
 base_sha256=sha(base.read_bytes()),library_sha256=sha(library),vm_reports=vm_reports,
 payload_sha256={name:sha(content) for name,content in files.items() if name.startswith('autorun/')},
 hardware_tested=False,card_written=False)
(O/'package-verification.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
(O/'SHA256SUMS.txt').write_text(report['package_sha256']+'  '+target.name+'\n',encoding='utf-8')
print(target,report['package_sha256'],flush=True)
