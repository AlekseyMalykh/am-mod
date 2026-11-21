"""Package the verified candidate with its regression evidence. No SD writes."""
from pathlib import Path
import hashlib,json,os,runpy,sys,zipfile
R=Path(__file__).resolve().parent
version='r30b-palette-rear-fix'
reports=['build/global-palette/verification.json','build/rear-mix-input-fx/verification.json','build/mixer-control/verification.json','build/pgm-import/verification.json']
extra={}
for name in reports:
 proof=json.loads((R/name).read_text());assert proof['passed'],name
 for source,digest in proof['sources_sha256'].items():
  content=(R/source).read_bytes();assert hashlib.sha256(content).hexdigest()==digest,source
  extra['SOURCE/rear-palette-fix/'+source]=content
 extra['SOURCE/rear-palette-fix/'+Path(name).parent.name+'-verification.json']=(R/name).read_bytes()
os.environ['AM_MOD_CANDIDATE_VERSION']=version
os.environ['AM_MOD_CANDIDATE_GUIDE']='REAR_PALETTE_FIXES_RU.md'
runpy.run_path(str(R/'package_midi_candidate.py'),run_name='__main__')
O=R/'build'/version;target=O/('am-mod-'+version+'.zip')
with zipfile.ZipFile(target,'a',zipfile.ZIP_DEFLATED) as z:
 for name,content in extra.items():z.writestr(name,content)
 z.writestr('REAR_PALETTE_FIXES_RU.md',(R/'REAR_PALETTE_FIXES_RU.md').read_bytes())
 z.writestr('SOURCE/rear-palette-fix/package_rear_palette_candidate.py',Path(__file__).read_bytes())
with zipfile.ZipFile(target) as z:assert z.testzip() is None
sys.path.insert(0,str(R.parent/'installer'))
from mpc_mod_installer import Package
assert Package.from_file(target).revision==version
digest=hashlib.sha256(target.read_bytes()).hexdigest()
(O/'SHA256SUMS.txt').write_text(digest+'  '+target.name+'\n')
p=O/'package-verification.json';proof=json.loads(p.read_text());proof['package_sha256']=digest
proof['regression_reports']=reports;p.write_text(json.dumps(proof,indent=2)+'\n')
print('Verified candidate with regression evidence:',target,digest)
