"""ARM/QEMU regression for Pad Colour leaking into FX Select. No SD writes."""
from pathlib import Path
import hashlib,json,os,subprocess
R=Path(__file__).resolve().parent
runtime=Path(os.environ.get('MPC_RUNTIME_ROOT',str(R.parent))).resolve()
O=R/'build/pad-colour-rgb';O.mkdir(parents=True,exist_ok=True)
sources=['src/workflow_ui.h','src/pad_workflow_colour.h','check_workflow_colours.c','check_sequence_rgb.c','check_workflow_rgb.c','check_pad_colour_rgb.c','check_pad_colour_steps.c','src/phone_native.c','src/phone_input.c','src/music_settings.c','src/step_native.c','src/step_native.h','src/step_abi.S','src/pad_palette.h','src/compressor.h','src/compressor_ui.c','verify_pad_colour_rgb.py']
hashes={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in sources}
sources+=['src/music_settings.h','check_palette_receiver.h','check_workflow_rgb.c','src/knob_reset.c','src/knob_reset.h','src/sample_playing.h']
hashes={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in sources}
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 for case,inputs in [('workflow-colours',['check_workflow_colours.c']),('rgb',['check_pad_colour_rgb.c','src/phone_input.c','src/music_settings.c']),('sequence',['check_sequence_rgb.c','src/phone_input.c']),('steps',['check_pad_colour_steps.c','src/music_settings.c','src/step_abi.S'])]:
  target=O/(name+'-'+case)
  special=['-DMPC_KNOB_RESET=1','src/knob_reset.c'] if case=='rgb' else []
  subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-I'+str(runtime/'sd-mod-session/build'),
  '-mbranch-protection=standard','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*flags,
  *special,*inputs,'-pthread','-ldl','-lm','-o',str(target)],cwd=R,check=True)
  p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target)],cwd=R,text=True,capture_output=True,timeout=30)
  (O/(name+'-'+case+'.log')).write_text(p.stdout+p.stderr)
  print(p.stdout+p.stderr,end='',flush=True);p.check_returncode();assert 'PASS ' in p.stdout
assert hashes=={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,workflow_selection_pulses_verified=True,workflow_colours_verified=True,sound_cursor_colour_verified=True,sequence_pulses_verified=True,sequence_background_nonzero=True,sequence_other_brightness_stable=True,sequence_tempo_and_meter_verified=True,sources_sha256=hashes,hardware_tested=False),indent=2)+'\n')
