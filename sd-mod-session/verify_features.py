from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;O=R/'build/features-dev5';O.mkdir(exist_ok=True)
sources=['src/seq_mutes.c','src/seq_mutes.h','src/seq_audition.c','src/seq_audition.h','src/music_settings.c','src/music_settings.h','src/music_archive.c','src/workflow_ui.c','src/phone_native.c','src/step_native.c','check_seq_mutes.c','check_seq_audition.c','check_feature_routes.c','check_features_layout.c','check_mixer_phone_routes.c','verify_features.py']
sha=lambda p:hashlib.sha256((R/p).read_bytes()).hexdigest();before={p:sha(p) for p in sources};checks=[]
for arch,cc,runner in [('host','gcc',[]),('arm64','aarch64-linux-gnu-gcc',['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'])]:
 for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
  for name,files in [('mutes',['check_seq_mutes.c','src/seq_mutes.c','src/music_settings.c']),('audition',['check_seq_audition.c','src/seq_audition.c']),('routes',['check_feature_routes.c'])]:
   key='-'.join([name,arch,mode]);target=O/key
   subprocess.run([cc,'-std=c11','-D_POSIX_C_SOURCE=200809L','-Isrc','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',*flags,*files,'-Wl,--gc-sections','-pthread','-lm','-o',str(target)],cwd=R,check=True)
   p=subprocess.run([*runner,str(target)],cwd=R,capture_output=True,text=True,timeout=45);(O/(key+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode();checks.append(key)
subprocess.run(['gcc','-std=c11','-D_POSIX_C_SOURCE=200809L','check_features_layout.c','-pthread','-o',str(O/'layout')],cwd=R,check=True)
assert before=={p:sha(p) for p in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=before,checks=checks,hardware_tested=False,commits=['f4313b1548dc6d98f05cabd0a42ff338c317a5b3','8e9056ba9004b4921cef896cced9548e0f799836']),indent=2)+'\n')
