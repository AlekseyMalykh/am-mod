"""Pinned ARM tests; no physical card or device access."""
from pathlib import Path
import hashlib,json,os,subprocess
R=Path(__file__).resolve().parent
baseline=json.loads((R/'compressor_base.json').read_text())
O=R/'build/compressor'/baseline['revision'];O.mkdir(parents=True,exist_ok=True)
(O.parent/'art').mkdir(exist_ok=True)
runtime=Path(os.environ.get('MPC_RUNTIME_ROOT',str(R.parent))).resolve()
stock=runtime/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
tracked=['src/compressor.h','src/compressor_dsp.c','src/compressor_native.c','src/compressor_ui.c',
 'src/phone_native.c','check_compressor_dsp.c','check_compressor_factory.cc','check_modern_fx.cc',
 'check_modern_fx.c','check_fx_native.c','verify_compressor.py','src/compressor_animation.c',
 'src/compressor_image.cc','src/optocat_atlas.inc','check_compressor_animation.c','compressor_base.json']
before={s:sha(R/s) for s in tracked}
for name,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 common=['-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-ffunction-sections','-fdata-sections',*extra]
 subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*common,'check_compressor_dsp.c','src/compressor_dsp.c','-lm','-o',str(O/(name+'-dsp'))],cwd=R,check=True)
 subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*common,'check_compressor_animation.c','src/compressor_animation.c','-lm','-o',str(O/(name+'-animation'))],cwd=R,check=True)
 objects=[]
 for source in ['check_modern_fx.c','check_compressor_factory.cc','src/compressor_dsp.c','src/compressor_native.c','src/compressor_ui.c','src/compressor_animation.c','src/compressor_image.cc']:
  cpp=source.endswith('.cc');obj=O/(name+'-'+Path(source).stem+'.o');objects.append(str(obj))
  subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11',*common,'-c',source,'-o',str(obj)],cwd=R,check=True)
 target=O/(name+'-native')
 subprocess.run(['aarch64-linux-gnu-g++',*extra,'-Wl,--gc-sections',*objects,'-ldl','-lm','-o',str(target)],check=True)
 for case in ['dsp','animation','native']:
  command=['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(O/(name+'-'+case))]
  if case=='native':command.append(str(stock))
  p=subprocess.run(command,capture_output=True,text=True,timeout=45)
  (O/(name+'-'+case+'.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
  assert 'PASS ' in p.stdout and 'runtime error:' not in p.stderr
assert before=={s:sha(R/s) for s in tracked}
proof=dict(passed=True,ubsan=True,animation_verified=True,native_image_blit=True,sources_sha256=before,stock_sha256=sha(stock),
 hardware_tested=False,card_written=False,full_vm_verified=False,
 scope='Real pinned native Colour factory/AudioPlugin prepare/process, software Image factory and exact pixel blit; production adapter and Type UI with isolated Component/Graphics/service-owner fixtures; signal-driven animation compositor, stereo PCM, DSP and input lifecycle checks. Native app main not executed.')
(O/'verification.json').write_text(json.dumps(proof,indent=2)+'\n')
