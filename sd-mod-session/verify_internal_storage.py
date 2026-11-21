"""Pinned persistent-storage capability and path policy, host/ARM + UBSan."""
from pathlib import Path
import hashlib,json,subprocess,tempfile
R=Path(__file__).resolve().parent;O=R/'build/internal-storage';O.mkdir(exist_ok=True)
sources=['src/internal_storage.c','src/internal_storage.h','check_internal_storage.c',Path(__file__).name]
sha=lambda p:hashlib.sha256((R/p).read_bytes()).hexdigest();hashes={n:sha(n) for n in sources};checks=[]
for arch,cc,runner in [('host','gcc',[]),('arm','aarch64-linux-gnu-gcc',['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'])]:
 for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
  with tempfile.TemporaryDirectory(prefix='am-internal-',dir='/dev/shm') as tmp:
   binary=O/(arch+'-'+mode)
   definitions=[f'-DAM_INTERNAL_META="{tmp}/meta"',f'-DAM_INTERNAL_PROJECTS="{tmp}/Projects"',f'-DAM_INTERNAL_SAMPLES="{tmp}/Samples"']
   subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*definitions,'check_internal_storage.c','src/internal_storage.c','-o',str(binary)],cwd=R,check=True)
   p=subprocess.run([*runner,str(binary),tmp],cwd=R,text=True,capture_output=True,timeout=30)
   (O/(binary.name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode();checks.append(binary.name)
assert hashes=={n:sha(n) for n in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=hashes,hardware_tested=False),indent=2)+'\n')
