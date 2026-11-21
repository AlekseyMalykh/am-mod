"""Package the MIDI candidate over an exact verified r30b. No physical card writes."""
from pathlib import Path
import hashlib,json,os,re,subprocess,sys,zipfile
from package_r28 import SALT
R=Path(__file__).resolve().parent;VERSION=os.environ.get('AM_MOD_CANDIDATE_VERSION','r30b-midi3');O=R/'build'/VERSION;O.mkdir(parents=True,exist_ok=True)
assert re.fullmatch(r'r[0-9a-z-]+',VERSION)
sha=lambda b:hashlib.sha256(b).hexdigest()
base=R/'build/r30b/am-mod-r30b.zip'
base_proof=json.loads((base.parent/'package-verification.json').read_text())
assert base_proof['passed'] and sha(base.read_bytes())==base_proof['package_sha256']
compile_proof=json.loads((R/'build/mixer-r27r/compile-verification.json').read_text())
proof=json.loads((R/'build/midi-tools/verification.json').read_text())
archive_proof=json.loads((R/'build/midi-archive/verification.json').read_text())
assert compile_proof['passed'] and proof['passed'] and archive_proof['passed'] and '-DMPC_MIDI_TOOLS=1' in compile_proof['compiler_flags']
assert '#define AM_MOD_VERSION "'+VERSION+'"' in (R/'src/mod_build.h').read_text()
for report in (compile_proof,proof,archive_proof):
 for name,digest in report['sources_sha256'].items():assert sha((R/name).read_bytes())==digest,name
tuner_proof=None
if '-DMPC_SAMPLE_LIVE_TUNER=1' in compile_proof['compiler_flags']:
 tuner_proof=json.loads((R/'build/sample-live-tuner/verification.json').read_text())
 assert tuner_proof['passed']
 for name,digest in tuner_proof['sources_sha256'].items():assert sha((R/name).read_bytes())==digest,name
library=(R/'build/mixer-r27r/phone-input.so').read_bytes();assert sha(library)==compile_proof['library_sha256']
with zipfile.ZipFile(base) as z:
 assert z.testzip() is None
 infos=z.infolist();files={i.filename:z.read(i) for i in infos}
original=dict(files)
launcher=files['autorun/mod-launcher.sh'].decode();old=sha(files['autorun/phone-input.so']);assert launcher.count(old)==1
# Reuse the established report export before SD unmount, so MIDI trials can
# actually be diagnosed from a card shared to the PC without powering off.
start='    # Every SD namespace owned by Am MOD must go away before an unmount.\n'
source=(R/'src/usb_drive_launcher.inc.sh').read_text()
diagnostic=source[source.index('    # Capture MIDI subscriptions'):source.index(start)]
assert launcher.count(start)==1;launcher=launcher.replace(start,diagnostic+start)
point='    "$R/stage-data" --flush-sd >> "$R/events.txt" 2>&1 || return 1\n'
assert launcher.count(point)==1;launcher=launcher.replace(point,point+'    export_report || true\n')
point='"$R/app/native-crash.txt"; do'
assert launcher.count(point)==1;launcher=launcher.replace(point,'"$R/app/native-crash.txt" "$R/app/midi-transport.txt"; do')

files['autorun/phone-input.so']=library;files['autorun/mod-launcher.sh']=launcher.replace(old,sha(library)).encode()
entry=files['autorun/autorun.sh'].decode();old=sha(original['autorun/mod-launcher.sh']);assert entry.count(old)==1
entry=entry.replace(old,sha(files['autorun/mod-launcher.sh']));assert 'revision=r30b' in entry
entry=entry.replace('revision=r30b','revision='+VERSION);files['autorun/autorun.sh']=entry.encode()
manifest=json.loads(files['autorun/autorun.json']);manifest['value0']['scripts'][0]['message']='Am MOD '+VERSION
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+SALT).encode()
assert len(files['autorun/autorun.check'])==64
for name in set(compile_proof['sources_sha256'])|set(proof['sources_sha256'])|set(archive_proof['sources_sha256'])|{'src/midi_tools.h','src/midi_ui.h','check_mixer_phone_routes.c','package_midi_candidate.py','midi_pc_probe.ps1','src/usb_drive_launcher.inc.sh'}:
 files['SOURCE/midi-candidate/'+name]=(R/name).read_bytes()
files['SOURCE/midi-candidate/core-verification.json']=(R/'build/midi-tools/verification.json').read_bytes()
files['SOURCE/midi-candidate/compile-verification.json']=(R/'build/mixer-r27r/compile-verification.json').read_bytes()
files['SOURCE/midi-candidate/archive-verification.json']=(R/'build/midi-archive/verification.json').read_bytes()
if tuner_proof:
 for name in tuner_proof['sources_sha256']:files['SOURCE/midi-candidate/'+name]=(R/name).read_bytes()
 files['SOURCE/midi-candidate/tuner-verification.json']=(R/'build/sample-live-tuner/verification.json').read_bytes()
 vm_report=R/'build/sample-live-tuner/vm-t2/verification.json'
 if vm_report.exists():
  vm=json.loads(vm_report.read_text())
  assert vm['passed'] and vm['library_sha256']==sha(library)
  files['SOURCE/midi-candidate/tuner-vm-verification.json']=vm_report.read_bytes()
  for name,digest in vm['evidence_sha256'].items():
   evidence=(vm_report.parent/name).read_bytes();assert sha(evidence)==digest
   files['SOURCE/midi-candidate/tuner-vm/'+name]=evidence
 # One continuous sample with three pitch regions, including detuned notes
 # so both note/Hz and the cent needle visibly change during one playback.
 import io,math,struct,wave
 audio=bytearray();rate=44100
 for frequency in (220.,445.,326.):
  for frame in range(rate*2):
   fade=min(1.,frame/441.,(rate*2-1-frame)/441.)
   audio+=struct.pack('<h',round(12000*fade*math.sin(2*math.pi*frequency*frame/rate)))
 test=io.BytesIO()
 with wave.open(test,'wb') as wav:
  wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(rate);wav.writeframes(audio)
 files['TEST_AUDIO/TUNER_STEPS.wav']=test.getvalue()
 (O/'TUNER_STEPS.wav').write_bytes(test.getvalue())
files['midi_pc_probe.ps1']=(R/'midi_pc_probe.ps1').read_bytes()
guide=(R/os.environ.get('AM_MOD_CANDIDATE_GUIDE','MIDI_TOOLS_TRIAL_RU.md')).read_bytes();files['MIDI_TOOLS_TRIAL_RU.md']=guide
files['README.ru.md']=guide
for name in ('autorun.sh','mod-launcher.sh'):subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+name],check=True)
target=O/('am-mod-'+VERSION+'.zip')
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
 for info in infos:z.writestr(info,files[info.filename])
 for name in sorted(files.keys()-original.keys()):z.writestr(name,files[name])
with zipfile.ZipFile(target) as z:assert z.testzip() is None
sys.path.insert(0,str(R.parent/'installer'));from mpc_mod_installer import Package
assert Package.from_file(target).revision==VERSION
digest=sha(target.read_bytes());(O/'SHA256SUMS.txt').write_text(digest+'  '+target.name+'\n')
(O/'package-verification.json').write_text(json.dumps(dict(passed=True,revision=VERSION,package_sha256=digest,base_sha256=sha(base.read_bytes()),library_sha256=sha(library),payload_sha256={k:sha(v) for k,v in files.items() if k.startswith('autorun/')},hardware_tested=False,card_written=False),indent=2)+'\n')
print(target,digest)
