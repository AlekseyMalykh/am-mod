"""Run fixed-path Internal syncfs helper against disposable real ext4 folders."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
R=Path(__file__).resolve().parent;O=R/'build/internal-flush';O.mkdir(exist_ok=True)
assert os.geteuid()==0,'Run this isolated filesystem verifier as root'
checks=[]
with tempfile.TemporaryDirectory(prefix='am-internal-flush-') as data, tempfile.TemporaryDirectory(prefix='am-internal-meta-',dir='/dev/shm') as ram:
 root=Path(data);meta=Path(ram)/'storage-meta';meta.mkdir()
 projects=root/'Projects';samples=root/'Samples';projects.mkdir();samples.mkdir()
 a=projects.stat();b=samples.stat()
 marker=f'AM_INTERNAL1\n{a.st_dev}:{a.st_ino}\n{b.st_dev}:{b.st_ino}\n'
 metadata=meta/'internal-storage';metadata.write_text(marker)
 definitions=[f'-DRAM_ROOT="{ram}"',f'-DAM_STAGE_INTERNAL_PROJECTS="{projects}"',f'-DAM_STAGE_INTERNAL_SAMPLES="{samples}"']
 for arch,cc,runner in [('host','gcc',[]),('arm','aarch64-linux-gnu-gcc',['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'])]:
  binary=O/arch
  subprocess.run([cc,'-std=c11','-O2','-g','-Wall','-Wextra','-Werror',*definitions,'src/stage_data.c','-o',str(binary)],cwd=R,check=True)
  def run(mode='--flush-internal'):
   return subprocess.run([*runner,str(binary),mode],cwd=R,text=True,capture_output=True,timeout=20)
  p=run();assert p.returncode==0 and 'INTERNAL_USER_STORAGE_FLUSHED' in p.stdout,p.stderr
  metadata.write_text('AM_INTERNAL1\n0:0\n0:0\n');assert run().returncode!=0
  metadata.write_text(marker);metadata.unlink();metadata.symlink_to(root/'absent');assert run().returncode!=0;metadata.unlink()
  metadata.write_text(marker);assert run('--flush-anywhere').returncode!=0
  checks.append(arch+': real syncfs succeeds only for pinned ext4 user folders; stale/redirected metadata and unknown mode rejected')
report=dict(passed=True,checks=checks,sources_sha256={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in ['src/stage_data.c',Path(__file__).name]},hardware_tested=False)
(O/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
