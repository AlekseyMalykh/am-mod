"""Same-Sound changing-pitch regression. No device or SD writes."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/sample-live-tuner';O.mkdir(parents=True,exist_ok=True)
proof=O/'verification.json';proof.unlink(missing_ok=True)
sources=['src/sample_live_tuner.cc','src/sample_live_tuner.h','check_sample_live_tuner.cc','check_sample_live_tuner_threaded.cc','verify_sample_live_tuner.py']
sha=lambda name:hashlib.sha256((R/name).read_bytes()).hexdigest()
before={name:sha(name) for name in sources};checks=[]
for architecture,cc,prefix in [('host','g++',[]),('arm64','aarch64-linux-gnu-g++',['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'])]:
    for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
        target=O/(architecture+'-'+mode)
        subprocess.run([cc,'-std=c++11','-O2','-g','-Wall','-Wextra','-Werror',*flags,'check_sample_live_tuner.cc','-pthread','-lm','-o',str(target)],cwd=R,check=True)
        p=subprocess.run([*prefix,str(target)],cwd=R,capture_output=True,text=True,timeout=45)
        (O/(architecture+'-'+mode+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
        # Compile the actual threaded production module as well as the deterministic fixture.
        subprocess.run([cc,'-std=c++11','-O2','-Wall','-Wextra','-Werror','-fPIC','-c','src/sample_live_tuner.cc','-o',str(O/(architecture+'-production.o'))],cwd=R,check=True)
        checks.append(architecture+'-'+mode)
    target=O/(architecture+'-threaded')
    subprocess.run([cc,'-std=c++11','-O2','-g','-Wall','-Wextra','-Werror','check_sample_live_tuner_threaded.cc','src/sample_live_tuner.cc','-pthread','-lm','-o',str(target)],cwd=R,check=True)
    p=subprocess.run([*prefix,str(target)],cwd=R,capture_output=True,text=True,timeout=15)
    (O/(architecture+'-threaded.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode();checks.append(architecture+'-threaded')
assert before=={name:sha(name) for name in sources}
proof.write_text(json.dumps(dict(passed=True,sources_sha256=before,checks=checks,hardware_tested=False,native_pcm_tap_hardware_tested=False),indent=2)+'\n')
