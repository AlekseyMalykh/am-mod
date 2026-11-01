"""Install the verified OptoCat branch onto the identified removable SD.
Back up every existing mod file/log and verify every music byte unchanged.
No formatting, partitioning, or project/sample writes.
"""
from pathlib import Path,PurePosixPath
from ctypes import wintypes
import argparse,ctypes,hashlib,json,os,shutil,stat,uuid,zipfile
R=Path(__file__).resolve().parent
baseline=json.loads((R/'compressor_base.json').read_text())
O=R/'build/compressor'/baseline['revision']
parser=argparse.ArgumentParser();parser.add_argument('--card',choices=['E'],required=True)
parser.add_argument('--from-version',choices=[baseline['revision']+'-optocat'])
parser.add_argument('--prepare-only',action='store_true');args=parser.parse_args()
CARD=Path(args.card+':/')
K=ctypes.WinDLL('kernel32',use_last_error=True)
K.GetVolumeInformationW.argtypes=[wintypes.LPCWSTR,wintypes.LPWSTR,wintypes.DWORD,ctypes.POINTER(wintypes.DWORD),ctypes.POINTER(wintypes.DWORD),ctypes.POINTER(wintypes.DWORD),wintypes.LPWSTR,wintypes.DWORD]
K.GetDriveTypeW.argtypes=[wintypes.LPCWSTR]
def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
 return h.hexdigest()
def identity():
 label=ctypes.create_unicode_buffer(128);fs=ctypes.create_unicode_buffer(32)
 serial=wintypes.DWORD();length=wintypes.DWORD();flags=wintypes.DWORD()
 assert K.GetVolumeInformationW(str(CARD),label,128,ctypes.byref(serial),ctypes.byref(length),ctypes.byref(flags),fs,32),ctypes.get_last_error()
 assert serial.value==0x30989DE5 and label.value=='' and fs.value.lower()=='exfat','Card identity changed'
 assert K.GetDriveTypeW(str(CARD))==2 and 125_000_000_000<shutil.disk_usage(CARD).total<126_000_000_000
def music():
 root=CARD/'MPC-Sample';assert root.is_dir() and not root.is_symlink()
 result={}
 for p in sorted(root.rglob('*')):
  assert not p.is_symlink(),p
  if p.is_file():result[p.relative_to(CARD).as_posix()]=digest(p)
 return result
def inventory():
 return {p.relative_to(CARD).as_posix():[p.stat().st_size,p.stat().st_mtime_ns]
         for p in sorted((CARD/'MPC-Sample').rglob('*')) if p.is_file()}
def replace(p,body):
 identity();assert p.parent.resolve().is_relative_to(CARD.resolve()) and not p.is_symlink()
 temp=p.with_name(p.name+'.optocat-'+uuid.uuid4().hex)
 with temp.open('xb') as f:f.write(body);f.flush();os.fsync(f.fileno())
 if p.exists():os.chmod(p,stat.S_IREAD|stat.S_IWRITE)
 os.replace(temp,p);assert digest(p)==hashlib.sha256(body).hexdigest()
identity()
previous=json.loads((CARD/'MPC_MOD_VERSION.json').read_text(encoding='utf-8-sig'))
if previous['revision']!=baseline['revision']:
 assert args.from_version==previous['revision'],'Refuse to replace a different version silently'
 prior=json.loads((O/'deployed-optocat-build-info.json').read_text())
 receipt=json.loads((O/'deployed-optocat-receipt.json').read_text())
 assert previous['revision']==prior['revision']==receipt['revision']
 assert previous['payload_sha256']==prior['payload_sha256'] and previous['library_sha256']==receipt['library_sha256']
 assert prior['zip_sha256']==receipt['zip_sha256']==previous['zip_sha256']
old=previous['payload_sha256']
for name,h in old.items():
 assert len(PurePosixPath(name).parts)==1 and name not in ('.','..')
 p=CARD/'autorun'/name;assert p.is_file() and not p.is_symlink() and digest(p)==h,name
snapshot=O/'card-preflight.json'
cached=json.loads(snapshot.read_text()) if snapshot.exists() and not args.prepare_only else None
current_inventory=inventory()
if cached and cached.get('serial')=='30989DE5' and cached.get('revision') in (baseline['revision'],previous['revision']) and cached.get('inventory')==current_inventory:
 before=cached['music_sha256'];print('Using unchanged preflight inventory; full music hashes will be verified after the write',flush=True)
else:
 print('Hashing samples/projects...',flush=True);before=music()
print('Identified E: 30989DE5,',previous['revision'],'; music files:',len(before),flush=True)
if args.prepare_only:
 O.mkdir(parents=True,exist_ok=True)
 (O/'card-preflight.json').write_text(json.dumps(dict(serial='30989DE5',revision=previous['revision'],music_sha256=before,inventory=current_inventory),indent=2)+'\n')
 raise SystemExit(0)
info=json.loads((O/'build-info.json').read_text())
assert info['revision']==baseline['revision']+'-optocat-s' and info['full_vm_verified']
assert info['live_verification']['audio_asserted'] and info['live_verification']['animation_verified']
assert info['live_verification']['untraced'],'Require the final normal VM run'
assert info['verification']['passed'] and info['verification']['ubsan'] and info['compile_verification']['full_preload_constructor']
for proof in (info['verification'],info['compile_verification'],info['input_verification']):
 for name,h in proof['sources_sha256'].items():assert digest(R/name)==h,name
archive=R.parent/info['zip'];assert digest(archive)==info['zip_sha256']
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None
 payload={name.removeprefix('autorun/'):z.read(name) for name in z.namelist() if name.startswith('autorun/')}
assert set(payload)==set(old)==set(info['payload_sha256'])
assert all(hashlib.sha256(body).hexdigest()==info['payload_sha256'][name] for name,body in payload.items())
backup=R/'backups'/('before-optocat-'+uuid.uuid4().hex[:12]);backup.mkdir(parents=True,exist_ok=False)
shutil.copytree(CARD/'autorun',backup/'autorun')
shutil.copy2(CARD/'MPC_MOD_VERSION.json',backup/'MPC_MOD_VERSION.json')
for p in CARD.iterdir():
 if p.is_file() and p.suffix.lower() in ('.md','.txt'):shutil.copy2(p,backup/p.name)
(backup/'music-before.json').write_text(json.dumps(before,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
for p in (CARD/'autorun').rglob('*'):
 assert not p.is_symlink()
 if p.is_file():assert digest(p)==digest(backup/'autorun'/p.relative_to(CARD/'autorun'))
changed=[name for name,body in payload.items() if hashlib.sha256(body).hexdigest()!=old[name]]
ordered=sorted(changed,key=lambda n:(n=='autorun.check',n=='autorun.json',n=='autorun.sh',n=='mod-launcher.sh',n))
print('Backup:',backup,'; updating:',ordered,flush=True)
try:
 for name in ordered:replace(CARD/'autorun'/name,payload[name])
 installed={k:info[k] for k in ('revision','base_revision','zip','zip_sha256','library_sha256','payload_sha256','full_vm_verified','hardware_tested')}
 installed['source_branch']='codex/compressor-type-r26d'
 replace(CARD/'MPC_MOD_VERSION.json',(json.dumps(installed,indent=2)+'\n').encode())
 identity();assert all(digest(CARD/'autorun'/name)==info['payload_sha256'][name] for name in payload)
 print('Verifying all music after update...',flush=True);assert music()==before
except BaseException:
 print('Update failed; restoring previous mod files from backup',flush=True)
 for name in ordered:
  source=backup/'autorun'/name
  if digest(CARD/'autorun'/name)!=digest(source):replace(CARD/'autorun'/name,source.read_bytes())
 replace(CARD/'MPC_MOD_VERSION.json',(backup/'MPC_MOD_VERSION.json').read_bytes())
 raise
receipt=dict(revision=info['revision'],drive='E:',serial='30989DE5',backup=str(backup),previous_revision=previous['revision'],
 updated_mod_files=ordered,music_files_verified_unchanged=len(before),music_bytes=sum((CARD/name).stat().st_size for name in before),
 library_sha256=info['library_sha256'],zip_sha256=info['zip_sha256'],full_vm_verified=True,hardware_tested=False,
 windows_safe_eject_still_required=True)
body=json.dumps(receipt,ensure_ascii=False,indent=2)+'\n'
(backup/'card-copy-receipt.json').write_text(body,encoding='utf-8')
(O/'card-copy-receipt.json').write_text(body,encoding='utf-8');print(body,flush=True)
