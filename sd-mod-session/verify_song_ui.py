"""Song UI over the pinned ELF in QEMU (normal + UBSan); no device."""
from pathlib import Path
import hashlib,json,os,subprocess
R=Path(__file__).resolve().parent
runtime=Path(os.environ.get('MPC_RUNTIME_ROOT',str(R.parent))).resolve()
stock=runtime/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
root=os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')
out=R/'build/song-ui';out.mkdir(parents=True,exist_ok=True)
sources=['check_song_ui.c','src/song_ui.c','src/song_ui.h','check_fx_native.c','verify_song_ui.py']
hashes={n:sha(R/n) for n in sources}
checks=[]
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    target=out/name
    subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',
                    *flags,'check_song_ui.c','-ldl','-lm','-pthread','-o',str(target)],cwd=R,check=True)
    p=subprocess.run(['qemu-aarch64','-L',root,str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=60)
    (out/(name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True)
    p.check_returncode();assert 'PASS Song UI' in p.stdout;checks.append(dict(name=name,passed=True))
assert hashes=={n:sha(R/n) for n in sources}
(out/'verification.json').write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=hashes,hardware_tested=False),indent=2)+'\n')
