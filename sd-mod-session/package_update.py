"""Make the next SD package from the previous one and the freshly built library.

    python3 package_update.py PREVIOUS.zip NEW-REVISION ["Start message"]

Run build_chords_candidate.py and its checks first. Takes build/chords/phone-input.so
(it must match compile-verification.json and verification.json), swaps it into the
previous package, updates the launcher hash, the revision, the start message and
autorun.check, and writes ../am-mod-NEW-REVISION.zip next to the repository.
Only autorun/ files change. Never accesses an SD card."""
from pathlib import Path
import hashlib,json,re,subprocess,sys,zipfile
R=Path(__file__).resolve().parent;C=R/'build/chords'
sha=lambda b:hashlib.sha256(b).hexdigest()
if len(sys.argv) not in (3,4):sys.exit(__doc__)
base,new=Path(sys.argv[1]),sys.argv[2]
assert re.fullmatch(r'r\d+[a-z]*(-[a-z0-9-]+)?',new),'revision looks like r24w or r24v-eq-amber'
compile_proof=json.loads((C/'compile-verification.json').read_text())
tests=json.loads((C/'verification.json').read_text())
lib=(C/'phone-input.so').read_bytes()
for proof in (compile_proof,tests):
    assert proof['passed']
    for n,h in proof.get('sources_sha256',{}).items():assert sha((R/n).read_bytes())==h,'changed since the check: '+n
assert sha(lib)==compile_proof['library_sha256']
assert tests.get('library_sha256',compile_proof['library_sha256'])==compile_proof['library_sha256'],'checks are for another library'
with zipfile.ZipFile(base) as z:
    assert z.testzip() is None
    old={n:z.read(n) for n in z.namelist()}
files=dict(old);files['autorun/phone-input.so']=lib
launcher=old['autorun/mod-launcher.sh'].decode()
key='SAVE_IO_SHA='+sha(old['autorun/phone-input.so']);assert launcher.count(key)==1
files['autorun/mod-launcher.sh']=launcher.replace(key,'SAVE_IO_SHA='+sha(lib)).encode()
entry=old['autorun/autorun.sh'].decode()
revs=re.findall(r'revision=([a-z0-9-]+)',entry);assert len(set(revs))==1,revs
previous=sha(old['autorun/mod-launcher.sh']);assert entry.count(previous)==1
entry=entry.replace(previous,sha(files['autorun/mod-launcher.sh'])).replace('revision='+revs[0],'revision='+new)
files['autorun/autorun.sh']=entry.encode()
manifest=json.loads(old['autorun/autorun.json']);script=manifest['value0']['scripts'][0]
short=new.split('-')[0]
if len(sys.argv)==4:script['message']=sys.argv[3]
else:script['message']=re.sub(r'\br\d+[a-z]*\b',short,script.get('message',''))
script['yesButtonText']='Start AM Mod'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
changed=sorted(n for n,b in files.items() if old.get(n)!=b)
assert all(n.startswith('autorun/') for n in changed),changed
out=R.parent/('am-mod-'+new+'.zip')
assert not out.exists(),f'{out} exists'
with zipfile.ZipFile(out,'x',zipfile.ZIP_DEFLATED) as z:
    for n,b in sorted(files.items()):z.writestr(n,b)
with zipfile.ZipFile(out) as z:assert z.testzip() is None and all(z.read(n)==b for n,b in files.items())
for n in ('autorun.sh','mod-launcher.sh'):subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+n],check=True)
print(json.dumps(dict(previous=revs[0],revision=new,zip=out.name,zip_sha256=sha(out.read_bytes()),library_sha256=sha(lib),changed=changed),indent=2))
