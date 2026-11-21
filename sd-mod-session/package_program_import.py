"""Verified dev5 envelope + exact dev6 library; no source WAVs in releases."""
from pathlib import Path
import hashlib,json,re,subprocess,sys,zipfile
from package_bootloader_compat import add_bootloader_entry
R=Path(__file__).resolve().parent;O=R/'build/program-import-dev6';sha=lambda b:hashlib.sha256(b).hexdigest()
sys.path.insert(0,str(R.parent/'installer'))
from mpc_mod_installer import Package,AUTORUN_CHECK_SALT
def read(p):return json.loads(p.read_text())
def verify_sources(proof):
 assert proof['passed']
 for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
vm='--vm' in sys.argv
compiled=read(O/'compile-verification.json');verify_sources(compiled)
library=(O/'phone-input.so').read_bytes();assert sha(library)==compiled['library_sha256']
version=re.search(r'#define AM_MOD_VERSION "([^"]+)"',(R/'src/mod_build.h').read_text()).group(1)
assert version=='r30b-program-import-dev6'
for path in [O/'manifest-verification.json',R/'build/pgm-import/verification.json',R/'build/pgm-import/native-pad-import-eq-check.json',R/'build/piano-input/verification.json',R/'build/usb-drive/verification.json']:
 verify_sources(read(path))
base=R.parent.parent/'releases/r30b-midi-features-dev5/am-mod-r30b-midi-features-dev5.zip'
base_report=read(base.parent/'package-verification.json');assert base_report['passed'] and sha(base.read_bytes())==base_report['package_sha256']
with zipfile.ZipFile(base) as z:files={i.filename:z.read(i) for i in z.infolist() if not i.is_dir() and not i.filename.startswith('SOURCE/')}
old_launcher=files['autorun/mod-launcher.sh'];launcher=old_launcher
assert launcher.count(sha(files['autorun/phone-input.so']).encode())==1
launcher=launcher.replace(sha(files['autorun/phone-input.so']).encode(),sha(library).encode());files['autorun/phone-input.so']=library
usb=read(R/'build/usb-drive/verification.json');helper=(R/'build/usb-drive/usb-drive-screen').read_bytes();assert sha(helper)==usb['screen_sha256']
assert launcher.count(sha(files['autorun/usb-drive-screen']).encode())==1
launcher=launcher.replace(sha(files['autorun/usb-drive-screen']).encode(),sha(helper).encode());files['autorun/usb-drive-screen']=helper
files['autorun/mod-launcher.sh']=launcher
entry=files['autorun/autorun.sh'];assert entry.count(sha(old_launcher).encode())==1
files['autorun/autorun.sh']=entry.replace(sha(old_launcher).encode(),sha(launcher).encode()).replace(b'r30b-midi-features-dev5',version.encode())
manifest=json.loads(files['autorun/autorun.json']);manifest['value0']['scripts'][0]['message']='Am MOD '+version
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode();files=add_bootloader_entry(files)
for n in ['autorun.sh','mod-launcher.sh']:subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+n],check=True)
files['README.ru.md']=(
 '# Am MOD '+version+'\n\n'+('Только для приватной виртуальной проверки.\n\n' if vm else '')+
 'PGM / Drum XPM: Sample Select → Shift+B2 → Programs. DATA: файл; B3: проверка / импорт; K1 или Pad Bank: начальный банк.\n'
 'Один Undo / Redo: Shift− / Shift+. Параметры, ограничения и проверки: PROGRAM_IMPORT_RU.md.\n\n'
 'Piano Roll: Shift+Seq → Events / Steps → Knob FX. K1: строки, K2: сетка, K3: время. DATA/Fader: время/строки; с Shift — зум.\n'
 'Seq Record + Play записывает пэды, ноты и чопы. MIDI-файлы: MIDI_FILES_RU.md.\n'
 'Sequence mutes: Mute+B3; снять все: Mute+B2. Seq Audition: Shift+B1 в браузере, K1: Orig Note файла.\n\n'
 'SampleBoot / HS Boot: BOOTLOADER_COMPAT_RU.md. Для сохранения меню используйте подпапку mods/AM-Mod или os/AM-Mod внутри autorun загрузчика.\n'
).encode()
for n in ['PROGRAM_IMPORT_RU.md','BOOTLOADER_COMPAT_RU.md']:files[n]=(R/n).read_bytes()
reports=[]
if not vm:
 proof=read(O/'vm/verification.json');verify_sources(proof);assert proof['library_sha256']==sha(library) and proof['native_named_save_and_reload']
 reports.append('program-import-verification.json');files['SOURCE/verification/program-import-verification.json']=json.dumps(proof,indent=2).encode()
 boot=read(R/'build/bootloader-compat/vm/sampleboot-verification.json');assert boot['passed'] and boot['library_sha256']==sha(library)
 assert boot['entry_fragment_sha256']==sha((R/'src/bootloader_entry.inc.sh').read_bytes());reports.append('sampleboot-verification.json')
 files['SOURCE/verification/sampleboot-verification.json']=json.dumps(boot,indent=2).encode()
 for p in sorted((R/'src').iterdir()):
  if p.is_file():files['SOURCE/src/'+p.name]=p.read_bytes()
 for p in sorted(R.glob('*.py')):files['SOURCE/'+p.name]=p.read_bytes()
 for p in sorted(R.glob('check*')):
  if p.is_file() and p.suffix in ('.c','.cc','.S','.h'):files['SOURCE/'+p.name]=p.read_bytes()
 for p in sorted((R/'lab').iterdir()):
  if p.is_file() and p.suffix in ('.py','.c'):files['SOURCE/lab/'+p.name]=p.read_bytes()
 for name,path in [('compile',O/'compile-verification.json'),('xpm',O/'manifest-verification.json'),('pgm',R/'build/pgm-import/verification.json'),('native-import',R/'build/pgm-import/native-pad-import-eq-check.json'),('piano-input',R/'build/piano-input/verification.json'),('usb-drive',R/'build/usb-drive/verification.json'),('boot-entry',R/'build/bootloader-compat/entry-verification.json')]:files['SOURCE/verification/'+name+'.json']=path.read_bytes()
 # User-supplied kit WAVs and third-party bootloader sources stay in the lab.
 for p in (O/'vm').glob('*.json'):files['SOURCE/verification/program-vm/'+p.name]=p.read_bytes()
target=O/('VM-ONLY-dev6-card.zip' if vm else 'am-mod-'+version+'.zip')
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
 for n,data in sorted(files.items()):z.writestr(n,data)
pkg=Package.from_file(target);assert pkg.revision==version and len(pkg.files)==84
report=dict(passed=True,revision=version,release_ready=not vm,package_sha256=pkg.sha256,base_sha256=base_report['package_sha256'],library_sha256=sha(library),vm_reports=reports,payload_sha256={n:sha(b) for n,b in pkg.files.items()},hardware_tested=False,card_written=False)
(O/('vm-package-verification.json' if vm else 'package-verification.json')).write_text(json.dumps(report,indent=2)+'\n')
print(target,pkg.sha256,flush=True)
