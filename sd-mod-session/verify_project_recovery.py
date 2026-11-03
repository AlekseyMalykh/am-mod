"""Recovery origin archive, native identity setter and first-Save routing checks."""
from pathlib import Path
import subprocess,json,hashlib
R=Path(__file__).resolve().parent;O=R/'build/project-recovery';O.mkdir(exist_ok=True)
sources=['src/project_recovery.c','src/project_recovery.h','src/music_archive.c','src/music_settings.c','src/music_settings.h','src/compressor_native.c','src/compressor_dsp.c','src/compressor.h','src/project_exit.cc','src/phone_native.c','src/workflow_ui.c','src/music_ui.c','check_project_recovery.c','verify_project_recovery.py']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={n:sha(R/n) for n in sources};stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 target=O/name
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-std=c11','-Wall','-Wextra','-Werror','-DMPC_PROJECT_RECOVERY=1','-DMPC_COMPRESSOR=1','-mbranch-protection=standard',*flags,'check_project_recovery.c','src/music_archive.c','src/music_settings.c','src/compressor_native.c','src/compressor_dsp.c','-pthread','-ldl','-lm','-Wl,--no-as-needed','-lstdc++','-o',str(target)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=30)
 (O/(name+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
assert before=={n:sha(R/n) for n in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,origin_archive_verified=True,async_serializer_origin_verified=True,shutdown_temp_handoff_verified=True,native_autosave_destination_preserved=True,display_basename_verified=True,project_display_settings_verified=True,project_optocat_settings_verified=True,failed_read_preserves_settings_verified=True,native_identity_property_verified=True,recovery_data_directory_preserved=True,first_save_confirmation_verified=True,sources_sha256=before,hardware_tested=False,full_native_main_tested=False),indent=2)+'\n')
