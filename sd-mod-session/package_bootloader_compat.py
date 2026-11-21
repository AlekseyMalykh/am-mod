"""Add the documented SampleBoot/HS Boot contract to a verified AM package.
No code/library changes, SD writes, bootloader installation or publication.
"""
from pathlib import Path
import hashlib,json,subprocess,sys,zipfile
R=Path(__file__).resolve().parent
sys.path.insert(0,str(R.parent/'installer'))
from mpc_mod_installer import Package,AUTORUN_CHECK_SALT
sha=lambda b:hashlib.sha256(b).hexdigest()
def add_bootloader_entry(files):
 result=dict(files);entry=result['autorun/autorun.sh']
 if b'BOOTLOADER_START_CLEAN_STOCK' not in entry:
  needle=b'entry_line "CONTROLLER_HASH_VERIFIED sha256=$actual" || exit 1\n'
  assert entry.count(needle)==1,'Unsupported AM entry: no unique controller verification'
  fragment=(R/'src/bootloader_entry.inc.sh').read_text(encoding='utf-8').encode()
  entry=entry.replace(needle,needle+b'\n'+fragment+b'\n')
 result['autorun/autorun.sh']=entry
 result['autorun/sampleboot.inplace']=b''
 result['autorun/autorun.check']=sha(result['autorun/autorun.json']+entry+AUTORUN_CHECK_SALT).encode()
 return result
def package(source,target):
 original=Package.from_file(source)
 with zipfile.ZipFile(source) as z:files={i.filename:z.read(i) for i in z.infolist() if not i.is_dir()}
 updated=add_bootloader_entry(files)
 assert add_bootloader_entry(updated)==updated
 for n,data in files.items():
  if n not in ('autorun/autorun.sh','autorun/autorun.check'):assert updated[n]==data,n
 subprocess.run(['sh','-n','/dev/stdin'],input=updated['autorun/autorun.sh'],check=True)
 updated['BOOTLOADER_README.txt']=(
  'AM Mod: SampleBoot / HS Boot compatibility entry\n\n'
  'Unzip separately. Copy the CONTENTS of its autorun folder to:\n'
  '  SampleBoot: <card>/autorun/mods/AM-Mod/\n'
  '  HS Boot (D.S.B): <card>/autorun/os/AM-Mod/\n'
  'Keep the sampleboot.inplace file with the other AM Mod files.\n'
  'Use the bootloader menu to start AM-Mod.\n'
  'The regular AM installer replaces the root autorun folder; use the folder\n'
  'layout above to keep your existing boot menu.\n\n'
  'This changes the entry script and adds the in-place marker. The AM library\n'
  'and verified controller are unchanged. Clean stock is started only after\n'
  'firmware/service checks; a bootstrap lock prevents recursive SD autorun.\n'
  'Host checks and virtual-MPC checks are reported separately; physical\n'
  'compatibility still needs confirmation from the affected unit.\n'
 ).encode()
 for name in ['src/bootloader_entry.inc.sh','package_bootloader_compat.py','verify_bootloader_entry.py']:
  updated['SOURCE/'+name]=(R/name).read_bytes()
 target.parent.mkdir(parents=True,exist_ok=True)
 with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
  for n,data in sorted(updated.items()):z.writestr(n,data)
 result=Package.from_file(target);assert result.revision==original.revision
 assert result.files['phone-input.so']==original.files['phone-input.so']
 proof=dict(passed=True,revision=result.revision,base_sha256=original.sha256,package_sha256=result.sha256,library_sha256=sha(result.files['phone-input.so']),controller_unchanged=True,payload_files=len(result.files),hardware_tested=False,vm_tested=False,sources_sha256={n:sha((R/n).read_bytes()) for n in ['src/bootloader_entry.inc.sh','package_bootloader_compat.py']})
 target.with_suffix('.verification.json').write_text(json.dumps(proof,indent=2)+'\n')
 print(target,result.sha256,flush=True)
if __name__=='__main__':package(Path(sys.argv[1]),Path(sys.argv[2]))
