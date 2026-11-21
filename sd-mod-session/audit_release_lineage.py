"""Compare shipping r30e/dev6 build inputs to this final compiled snapshot."""
from pathlib import Path
import hashlib,json,zipfile,ast,subprocess
R=Path(__file__).resolve().parent;O=R/'build/branch-audit';O.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
current=json.loads((R/'build/mixer-r27r/compile-verification.json').read_text())
def compilation(archive):
 with zipfile.ZipFile(archive) as z:
  candidates=[]
  for n in z.namelist():
   if n.endswith('.json') and not n.startswith('autorun/'):
    p=json.loads(z.read(n));p=p.get('compile_verification',p)
    if p.get('linked_sources'):candidates.append((n,p))
  print(archive,[(n,len(p['linked_sources'])) for n,p in candidates])
  if not candidates:
   print([(n,list(json.loads(z.read(n)))[:12]) for n in z.namelist() if n.endswith('.json') and not n.startswith('autorun/')])
  assert candidates
  return candidates[-1][1]
repo='/mnt/c/Users/Алексей/Desktop/am-mod-branches/sample-swap-audition'
def git(path):return subprocess.check_output(['git','-C',repo,'show','8e9056b:sd-mod-session/'+path])
script=git('build_mixer_r27r.py').decode()
scope={'frozen':json.loads((R/'build/r23a/build-info.json').read_text())}
for node in ast.parse(script).body:
 if isinstance(node,ast.Assign) and isinstance(node.targets[0],ast.Name) and node.targets[0].id in ('sources','mixer','common'):
  exec(compile(ast.Module(body=[node],type_ignores=[]),'<r30e-inputs>','exec'),scope)
old=dict(linked_sources=scope['sources'],compiler_flags=scope['common'],sources_sha256={})
for path in set(current['sources_sha256'])|set(old['linked_sources']):
 try:old['sources_sha256'][path]=sha(git(path))
 except subprocess.CalledProcessError:pass
base=None
def comparison(before):
 a=set(before['linked_sources']);b=set(current['linked_sources'])
 common=set(before['sources_sha256'])&set(current['sources_sha256'])
 return dict(before_modules=len(a),after_modules=len(b),removed_modules=sorted(a-b),added_modules=sorted(b-a),
  missing_flags=sorted(set(before['compiler_flags'])-set(current['compiler_flags'])),
  changed_inputs=sorted(n for n in common if before['sources_sha256'][n]!=current['sources_sha256'][n]),
  unchanged_inputs=sorted(n for n in common if before['sources_sha256'][n]==current['sources_sha256'][n]),
  old_inputs_not_in_current_manifest=sorted(set(before['sources_sha256'])-set(current['sources_sha256'])))
report=dict(passed=True,r30e=comparison(old),baseline_commit='8e9056b',library_sha256=current['library_sha256'])
for name in ('r30e',):
 assert not report[name]['removed_modules'],report[name]
 assert not report[name]['missing_flags'],report[name]
for n,h in current['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
(O/'lineage.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
