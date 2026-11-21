"""Global palette, real controller handlers and codecs, host/ARM + UBSan."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent
O=R/'build/global-palette';O.mkdir(parents=True,exist_ok=True)
sources=['check_global_palette.c','check_palette_banks_ui.c','src/music_ui.c','src/music_settings.c','src/music_settings.h','src/mixer_model.c','src/mixer_model.h','src/phone_input.c']
sources+=['src/usb_drive_lights.h','src/music_ui.h']
hashes={s:hashlib.sha256((R/s).read_bytes()).hexdigest() for s in sources}
for architecture,cc,runner in [('host','gcc',[]),('arm','aarch64-linux-gnu-gcc',['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808'])]:
 for mode,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
  binary=O/(architecture+'-'+mode)
  subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',*extra,'check_global_palette.c','src/music_settings.c','src/mixer_model.c','-Wl,--gc-sections','-pthread','-o',str(binary)],cwd=R,check=True)
  result=subprocess.run([*runner,str(binary)],cwd=R,text=True,capture_output=True,timeout=45)
  (O/(binary.name+'.log')).write_text(result.stdout+result.stderr)
  print(architecture,mode,result.stdout+result.stderr,flush=True);result.check_returncode()
  ui=O/('ui-'+architecture+'-'+mode)
  subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-Ibuild',*extra,'check_palette_banks_ui.c','src/music_settings.c','src/phone_input.c','-Wl,--gc-sections','-pthread','-o',str(ui)],cwd=R,check=True)
  result=subprocess.run([*runner,str(ui)],cwd=R,text=True,capture_output=True,timeout=45)
  (O/(ui.name+'.log')).write_text(result.stdout+result.stderr)
  print(architecture,mode,result.stdout+result.stderr,flush=True);result.check_returncode()
assert hashes=={s:hashlib.sha256((R/s).read_bytes()).hexdigest() for s in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=hashes,hardware_tested=False),indent=2)+'\n')
