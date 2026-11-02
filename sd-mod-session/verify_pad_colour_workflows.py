"""Pinned ARM colour metadata, Copy/Extract rollback and native mute scans.
No physical card writes. PCM, GUI and file persistence remain fixture boundaries.
"""
from pathlib import Path
import hashlib,json,subprocess,zipfile
R=Path(__file__).resolve().parent
O=R/'build/pad-colour-workflows';O.mkdir(parents=True,exist_ok=True)
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
with zipfile.ZipFile(R.parent/'am-mod-r26k.zip') as z:bridge=z.read('autorun/phone-code.bin')
assert hashlib.sha256(bridge).hexdigest()=='4912f9dcc38212ff1f37d051a8133d493f784305d97663d0d5333dc466e2e0d7'
(O/'phone-bridges.bin').write_bytes(bridge)
sources=['src/pad_palette.h','verify_pad_colour_workflows.py','check_pad_colour_history.c','check_batch_eq.c',
 'check_batch_native.c','check_chop_native.c','check_steps.c','check_steps_support.cc',
 'check_steps_support.S','check_chop_support.S','src/step_native.c','src/step_abi.S',
 'src/music_settings.c','src/music_settings.h','src/pad_eq.c','src/fx_eq.c',
 'src/pad_eq_history.cc','src/pad_eq_history.h','src/batch_copy.cc','src/batch_events.cc',
 'src/step_mute.cc','src/step_exceptions.cc','build/phone_hooks.h']
before={n:sha(R/n) for n in sources}
results=[]
for variant,extra in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 common=['-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-DMPC_PAD_EQ=1',
         '-DSTEP_BRIDGE_PATH="'+str(O/'phone-bridges.bin')+'"',
         '-mbranch-protection=standard','-ffunction-sections','-fdata-sections',*extra]
 objects=[]
 for source in ['check_steps_support.cc','src/step_mute.cc','src/step_exceptions.cc','src/batch_copy.cc','src/batch_events.cc','src/pad_eq_history.cc']:
  obj=O/(variant+'-'+Path(source).stem+'.o');objects.append(str(obj))
  subprocess.run(['aarch64-linux-gnu-g++','-std=c++11',*common,'-c',source,'-o',str(obj)],cwd=R,check=True)
 for case,cfile,asm in [('history','check_pad_colour_history.c',[]),('batch','check_batch_eq.c',[]),
                        ('extract','check_chop_native.c',['check_chop_support.S']),('mute','check_steps.c',[])]:
  target=O/(variant+'-'+case)
  subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*common,'-Wl,--gc-sections',cfile,
    'check_steps_support.S','src/step_abi.S',*asm,'src/pad_eq.c','src/fx_eq.c',*objects,
    '-lstdc++','-pthread','-ldl','-lm','-o',str(target)],cwd=R,check=True)
  args=[str(O/(variant+'-mute-draw.txt'))] if case=='mute' else []
  p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock),*args],
                   cwd=R,capture_output=True,text=True,timeout=60)
  (O/(variant+'-'+case+'.log')).write_text(p.stdout+p.stderr)
  print('\n'.join(s for s in (p.stdout+p.stderr).splitlines() if s.startswith(('PASS','FAIL','CHOP_FAULT','FAULT'))),flush=True)
  if p.returncode:raise RuntimeError((variant,case,p.returncode,p.stdout[-2000:],p.stderr[-3000:]))
  assert 'PASS ' in p.stdout
  results.append(dict(variant=variant,case=case,passed=True))
assert before=={n:sha(R/n) for n in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,results=results,
 sources_sha256=before,stock_sha256=sha(stock),hardware_tested=False,card_written=False),indent=2)+'\n')
