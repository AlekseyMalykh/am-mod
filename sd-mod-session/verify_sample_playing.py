"""PlayingPadsLighting owner snapshot using the real firmware voice lookup."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/sample-playing';O.mkdir(exist_ok=True)
sources=['src/sample_playing.c','src/sample_playing.h','check_sample_playing.c','check_fx_native.c','verify_sample_playing.py']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before={n:sha(R/n) for n in sources}
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    exe=O/name
    subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'check_sample_playing.c','-ldl','-lm','-o',str(exe)],cwd=R,check=True)
    result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(exe),str(stock)],cwd=R,capture_output=True,text=True,timeout=30)
    (O/(name+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True);result.check_returncode()
    assert 'PASS actual native any-note voice ownership' in result.stdout
assert before=={n:sha(R/n) for n in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,actual_native_voice_lookup=True,hardware_tested=False,sources_sha256=before),indent=2)+'\n')
