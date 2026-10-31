from pathlib import Path
import json,hashlib,subprocess,zipfile
R=Path(__file__).resolve().parent;O=R/'build/power-audit';P=O/'autorun'
sha=lambda b:hashlib.sha256(b).hexdigest()
proof=json.loads((O/'verification.json').read_text());assert proof['passed']
for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h
base=json.loads((R/'build/r21a/build-info.json').read_text());archive=R.parent/base['zip'];assert sha(archive.read_bytes())==base['zip_sha256']=='795e3e6f8cbd19105108a4b78d5f90098beca39f26b3f507e83de42db49fab89'
with zipfile.ZipFile(archive) as z:files={n:z.read(n) for n in z.namelist()}
old=dict(files)
audit=(R/'src/power_audit.sh').read_bytes();files['autorun/power-audit.sh']=audit
entry=files['autorun/autorun.sh'].decode().replace('revision=r21a-mono-amber','revision=r21b-power-audit')
anchor='/bin/sh "$DIR/mod-launcher.sh"';assert entry.count(anchor)==1
entry=entry.replace(anchor,'''# Optional one-shot inventory: bounded and never changes charger settings.
if [ ! -L "$DIR/power-audit.sh" ] && [ "$(sha256sum "$DIR/power-audit.sh" | cut -d ' ' -f 1)" = 'AUDIT_SHA' ]; then
    timeout 20 /bin/sh "$DIR/power-audit.sh" || entry_line POWER_AUDIT_INCOMPLETE
else
    entry_line POWER_AUDIT_HASH_REJECTED
fi
'''.replace('AUDIT_SHA',sha(audit))+anchor)
files['autorun/autorun.sh']=entry.encode()
manifest=json.loads(files['autorun/autorun.json']);s=manifest['value0']['scripts'][0];s['message']='Start r21b? Same r21a mod plus one read-only power report. Charger settings are unchanged. Insert only after normal boot.';s['yesButtonText']='Start r21b'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
assert all(files[n]==b for n,b in old.items() if n.startswith('autorun/') and n not in ['autorun/autorun.sh','autorun/autorun.json','autorun/autorun.check'])
files['README_RU.md']=(R/'POWER_AUDIT_RU.md').read_bytes();files['POWER_AUDIT_RU.md']=files['README_RU.md']
P.mkdir(exist_ok=True)
for n,b in files.items():
 if n.startswith('autorun/'):(O/n).write_bytes(b)
for n in ['autorun.sh','power-audit.sh']:subprocess.run(['sh','-n',str(P/n)],check=True)
target=R.parent/'mpc-sample-r21b-power-audit.zip'
if not target.exists():
 with zipfile.ZipFile(target,'x',zipfile.ZIP_DEFLATED) as z:
  for n,b in sorted(files.items()):z.writestr(n,b)
with zipfile.ZipFile(target) as z:assert z.testzip() is None and set(z.namelist())==set(files) and all(z.read(n)==b for n,b in files.items())
info=dict(base);info.update(revision='r21b-power-audit',zip=target.name,zip_sha256=sha(target.read_bytes()),source_commit=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'rev-parse','HEAD'],cwd=R,text=True).strip(),payload_sha256={n[8:]:sha(b) for n,b in files.items() if n.startswith('autorun/')},power_modes_implemented=False,power_audit_read_only=True)
(O/'build-info.json').write_text(json.dumps(info,indent=2)+'\n');print(json.dumps({'revision':info['revision'],'zip_sha256':info['zip_sha256'],'unchanged_library':info['library_sha256']},indent=2))
