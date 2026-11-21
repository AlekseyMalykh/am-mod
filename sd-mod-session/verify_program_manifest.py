from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/program-import-dev6';O.mkdir(exist_ok=True)
sources=['src/program_manifest.cc','src/program_manifest.h','check_program_manifest.cc','verify_program_manifest.py']
sha=lambda n:hashlib.sha256((R/n).read_bytes()).hexdigest();before={n:sha(n) for n in sources};checks=[]
for arch,cc,prefix in [('host','g++',[]),('arm','aarch64-linux-gnu-g++',['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'])]:
 for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-fno-sanitize-recover=all','-static-libubsan'])]:
  output=O/('manifest-'+arch+'-'+mode)
  subprocess.run([cc,'-std=c++11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'check_program_manifest.cc','src/program_manifest.cc','-ldl','-o',str(output)],cwd=R,check=True)
  p=subprocess.run([*prefix,str(output)],cwd=R,check=True,capture_output=True,text=True,timeout=60)
  (O/(output.name+'.log')).write_text(p.stdout+p.stderr);print(arch,mode,p.stdout.strip(),flush=True);checks.append(output.name)
assert before=={n:sha(n) for n in sources}
(O/'manifest-verification.json').write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,hardware_tested=False),indent=2)+'\n')
