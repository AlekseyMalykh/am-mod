"""Package a verified knob/OptoCat candidate without accessing a card.
Run verify_knob_events.py and build_chords_candidate.py first.
Usage: python3 package_knob_fix.py ../am-mod-r26j.zip r26k
"""
from pathlib import Path
import hashlib,json,re,subprocess,sys,zipfile
R=Path(__file__).resolve().parent;O=R/'build/knob-events';sha=lambda b:hashlib.sha256(b).hexdigest()
if len(sys.argv) not in (2,3):sys.exit(__doc__)
revision=sys.argv[2] if len(sys.argv)==3 else 'r26k'
assert revision in ('r26f','r26g','r26h','r26i','r26j','r26k')
tests=json.loads((O/'verification.json').read_text());compiled=json.loads((R/'build/chords/compile-verification.json').read_text())
for proof in [tests,compiled]:
 assert proof['passed']
 for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
assert tests['baseline_reproduced'] and compiled['full_preload_constructor']
if revision in ('r26g','r26h','r26i','r26j','r26k'):assert tests['scaled_baseline_reproduced']
compressor=None
if revision in ('r26h','r26i','r26j','r26k'):
 compressor=json.loads((R/'build/compressor/r26g/verification.json').read_text())
 assert compressor['passed'] and compressor['ubsan'] and compressor['animation_verified']
 for n,h in compressor['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
 assert compiled['compressor_included'] and '-DMPC_COMPRESSOR=1' in compiled['compiler_flags']
 assert {'src/compressor_dsp.c','src/compressor_ui.c','src/compressor_native.c','src/compressor_animation.c','src/compressor_image.cc'}<=set(compiled['linked_sources'])
 if revision in ('r26i','r26j','r26k'):
  assert compiled['native_takeover_preserved'] and 'src/takeover_native.c' in compiled['linked_sources']
  assert 'src/takeover_endpoints.c' not in compiled['linked_sources']
  assert any('native knob input thread' in case['test'] and case['ubsan'] for case in tests['tests'])
 if revision in ('r26j','r26k'):
  assert compressor['shift_controls_verified'] and compressor['paw_timing_verified'] and compressor['native_gain_compared'] and compressor['maximum_gain_db']==6
 assert any('display' in case['test'] and case['ubsan'] for case in tests['tests'])
restore=exit_copy=None
if revision=='r26k':
 restore=json.loads((R/'build/project-restore/verification.json').read_text())
 exit_copy=json.loads((R/'build/project-exit/verification.json').read_text())
 assert restore['passed'] and restore['ubsan'] and restore['modal_baseline_reproduced'] and exit_copy['passed']
 for proof in (restore,exit_copy):
  for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
lib=(R/'build/chords/phone-input.so').read_bytes();assert sha(lib)==compiled['library_sha256']
with zipfile.ZipFile(sys.argv[1]) as z:
 assert z.testzip() is None;old={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
files=dict(old);files['autorun/phone-input.so']=lib
def replace_once(text,old,new):
 assert text.count(old)==1,old;return text.replace(old,new,1)
launcher=old['autorun/mod-launcher.sh'].decode()
files['autorun/mod-launcher.sh']=replace_once(launcher,'SAVE_IO_SHA='+sha(old['autorun/phone-input.so']),'SAVE_IO_SHA='+sha(lib)).encode()
entry=old['autorun/autorun.sh'].decode();assert '# sd-filesystems=exfat,vfat\n' in entry
entry=replace_once(entry,sha(old['autorun/mod-launcher.sh']),sha(files['autorun/mod-launcher.sh']))
old_revision=re.findall(r'revision=(r\d+[a-z]+)',entry);assert len(set(old_revision))==1
files['autorun/autorun.sh']=replace_once(entry,'revision='+old_revision[0],'revision='+revision).encode()
manifest=json.loads(old['autorun/autorun.json']);manifest['value0']['scripts'][0]['message']='AM Mod '+revision+(' - project startup restore; experimental FAT32' if revision=='r26k' else ' - OptoCat controls; experimental FAT32' if revision=='r26j' else ' - takeover correction; experimental FAT32')
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
changed=sorted(n for n,b in files.items() if old.get(n)!=b)
assert set(changed)=={'autorun/phone-input.so','autorun/mod-launcher.sh','autorun/autorun.sh','autorun/autorun.json','autorun/autorun.check'},changed
for name in ['README.md','README.ru.md','CHANGELOG.md','docs/RELEASE_NOTES-'+revision+'.md','docs/RELEASE_NOTES-'+revision+'.ru.md','docs/RELEASE_NOTES-r26f.md','docs/RELEASE_NOTES-r26f.ru.md']:
 files[name]=(R.parent/name).read_bytes()
if revision in ('r26h','r26i','r26j','r26k'):
 files['COMPRESSOR_TYPE_RU.md']=(R/'COMPRESSOR_TYPE_RU.md').read_bytes()
 files['sd-mod-session/TAKEOVER_SCALED_RU.md']=(R/'TAKEOVER_SCALED_RU.md').read_bytes()
if revision=='r26k':
 for name in ['CONTRIBUTING.md','docs/FEEDBACK.md','docs/FEEDBACK.ru.md','docs/KNOWN_ISSUES.md','docs/KNOWN_ISSUES.ru.md','docs/RELEASE_NOTES-r26i.md','docs/RELEASE_NOTES-r26i.ru.md','docs/RELEASE_NOTES-r26j.md','docs/RELEASE_NOTES-r26j.ru.md']:
  files[name]=(R.parent/name).read_bytes()
files['README_RU.md']=('# AM Mod '+revision+(' — проекты и OptoCat\n\n' if revision=='r26k' else ' — OptoCat\n\n' if revision=='r26j' else ' — кандидат исправления ручек\n\n')+
 ('Автозагрузка ждёт закрытия штатных диалогов; сохранены OptoCat r26j и исправление K. Нужна проба на MPC.\n' if revision=='r26k' else 'OptoCat: Shift+K1 Attack, Shift+K2 Release, Shift+K3 Knee; Gain не выше +6 dB. Нужна проба на MPC.\n' if revision=='r26j' else 'Штатный путь ввода K1–K3 и расчёт takeover; сохранён OptoCat. Нужна проба на MPC.\n' if revision=='r26i' else 'Коррекция крайних положений Scaled; сохранён OptoCat с анимацией. Нужна проба на MPC.\n')+
 'Поддерживаются exFAT и экспериментальный FAT32; прошивка 1.3.0.12.\n'+
 'Скопируйте полную папку autorun в корень карты. Проекты и сэмплы можно оставить.\n'+
 'Подробности: README.ru.md и docs/RELEASE_NOTES-'+revision+'.ru.md. Кандидат не опубликован.\n').encode()
for n in ['autorun.sh','mod-launcher.sh']:
 subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+n],check=True)
out=R.parent/('am-mod-'+revision+'.zip');assert not out.exists(),out
with zipfile.ZipFile(out,'x',zipfile.ZIP_DEFLATED) as z:
 for n,b in sorted(files.items()):z.writestr(n,b)
with zipfile.ZipFile(out) as z:assert z.testzip() is None and all(z.read(n)==b for n,b in files.items())
report=dict(passed=True,revision=revision,package_sha256=sha(out.read_bytes()),library_sha256=sha(lib),changed_payload=changed,payload_sha256={n:sha(b) for n,b in files.items() if n.startswith('autorun/')},input_checks=tests,compressor_checks=compressor,project_restore_checks=restore,project_exit_checks=exit_copy,compile_verification=compiled,full_preload_constructor=True,hardware_warp_tested=False,full_native_warp_ui_tested=False,published=False)
(O/'package-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(dict(package=out.name,sha256=sha(out.read_bytes()),library_sha256=sha(lib),verified=True,hardware_warp_tested=False),indent=2))
