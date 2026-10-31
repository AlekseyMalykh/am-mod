"""Package verified r20 over the frozen r16d storage/recovery payload.
Only clone library and the bounded recovery crash snapshot change.
"""
from pathlib import Path
import hashlib,json,zipfile,subprocess
R=Path(__file__).resolve().parent;O=R/'build/r20d';P=O/'autorun'
sha=lambda b:hashlib.sha256(b).hexdigest()
base=json.loads((R/'build-info.json').read_text());archive=R.parent/base['zip']
assert base['revision']=='r16d-sample-input-preview' and sha(archive.read_bytes())==base['zip_sha256']=='8bbdcf239235fec7592b8f4614a9b3ca6b79260f406b0d0148aaff1e4aa19b8d'
report=json.loads((O/'verification.json').read_text());assert report['passed'] and report['hardware_trial_candidate']
source_commit=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'rev-parse','HEAD'],cwd=R,text=True).strip()
for name,digest in report['sources_sha256'].items():assert sha((R/name).read_bytes())==digest,name
library=(O/'phone-input.so').read_bytes();assert sha(library)==report['library_sha256']
with zipfile.ZipFile(archive) as z:files={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
old=dict(files)
for name,digest in base['payload_sha256'].items():assert sha(files['autorun/'+name])==digest
files['autorun/phone-input.so']=library
controller=files['autorun/mod-launcher.sh'].decode();template=(R/'src/autorun.template.sh').read_text()
assert controller.count('SAVE_IO_SHA='+sha(old['autorun/phone-input.so']))==1
controller=controller.replace('SAVE_IO_SHA='+sha(old['autorun/phone-input.so']),'SAVE_IO_SHA='+sha(library))
crash='native_crash_snapshot() {'+template.split('native_crash_snapshot() {',1)[1].split('\nexport_report()',1)[0]
assert 'native_crash_snapshot()' not in controller and controller.count('export_report() {')==1
controller=controller.replace('export_report() {',crash+'\nexport_report() {')
anchor='"$R/app/save-storage.txt"; do';assert controller.count(anchor)==1
controller=controller.replace(anchor,'"$R/app/save-storage.txt" "$R/app/native-crash.txt"; do')
anchor='    [ "$(prop "$U-clone.service" MainPID)" = 0 ] || fail CLONE_STILL_RUNNING\n';assert controller.count(anchor)==1
controller=controller.replace(anchor,anchor+'    native_crash_snapshot\n')
files['autorun/mod-launcher.sh']=controller.encode()
entry=files['autorun/autorun.sh'].decode();assert entry.count(sha(old['autorun/mod-launcher.sh']))==1
entry=entry.replace(sha(old['autorun/mod-launcher.sh']),sha(files['autorun/mod-launcher.sh'])).replace('revision=r16d-sample-input-preview','revision=r20d-resample-output')
files['autorun/autorun.sh']=entry.encode()
manifest=json.loads(files['autorun/autorun.json']);script=manifest['value0']['scripts'][0]
script['message']='Start r20d? Sample trial: native audio output enabled; device audio still needs verification. Use one short sample and a free destination. Insert this SD only after the normal interface has loaded.'
script['yesButtonText']='Start r20d';files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
allowed={'autorun/'+s for s in ['phone-input.so','mod-launcher.sh','autorun.sh','autorun.json','autorun.check']}
assert {n for n in old if n.startswith('autorun/') and old[n]!=files[n]}==allowed
files['README_RU.md']=(R/'R20D_RU.md').read_bytes()
for n in ['R20D_RU.md','PAD_RESAMPLE_DESIGN_RU.md','PARAMETRIC_EQ_DESIGN_RU.md','POWER_CONFIG_RESEARCH_RU.md']:files[n]=(R/n).read_bytes()
P.mkdir(exist_ok=True)
for n,b in files.items():
 if n.startswith('autorun/'):path=O/n;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b)
for n in ['autorun.sh','mod-launcher.sh']:subprocess.run(['sh','-n',str(P/n)],check=True)
zip_path=R.parent/'mpc-sample-r20d-resample-output.zip'
if not zip_path.exists():
 with zipfile.ZipFile(zip_path,'x',zipfile.ZIP_DEFLATED) as z:
  for n,b in sorted(files.items()):z.writestr(n,b)
# An interrupted metadata write may be resumed, but a frozen archive is never overwritten.
with zipfile.ZipFile(zip_path) as z:assert z.testzip() is None and set(z.namelist())==set(files) and all(z.read(n)==b for n,b in files.items()),'Frozen release differs'
info=dict(base);info.update(revision=report['revision'],zip=zip_path.name,zip_sha256=sha(zip_path.read_bytes()),hardware_tested=False,limited_device_trial_ready=True,payload_sha256={n[8:]:sha(b) for n,b in files.items() if n.startswith('autorun/')},library_sha256=sha(library),resample_backend='private single-note feed in native fast Seq loop for Sample; native Seq unchanged; validated WAV import/assignment/Undo',eq='one point per pad; native post-instrument render; K1 frequency K2 gain Shift K2 Q K3 type',source_commit=source_commit)
(O/'build-info.json').write_text(json.dumps(info,indent=2)+'\n');print(json.dumps({'revision':info['revision'],'zip':str(zip_path),'payload_files':len(info['payload_sha256']),'library_sha256':sha(library)},indent=2))
