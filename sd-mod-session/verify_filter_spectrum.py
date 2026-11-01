"""Production selective waveform hook, pinned ABI, normal and UBSan ARM."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/filter-spectrum';O.mkdir(exist_ok=True)
names=['src/filter_spectrum.cc','src/filter_spectrum.h','src/music_ui.c','src/music_ui.h','src/phone_native.c','check_filter_spectrum.cc','verify_filter_spectrum.py']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
hashes={n:sha(R/n) for n in names}
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 target=O/name
 subprocess.run(['aarch64-linux-gnu-g++','-std=c++11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'check_filter_spectrum.cc','-pthread','-o',str(target)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target)],cwd=R,capture_output=True,text=True,timeout=30)
 (O/(name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
assert hashes=={n:sha(R/n) for n in names}
(O/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,sources_sha256=hashes,hardware_tested=False,scope='production selective hook and native ABI; full painter and sample-body receivers'),indent=2)+'\n')
