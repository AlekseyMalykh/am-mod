"""Build the destination-only stock resampler and single-point EQ host.
No SD writes. Old r18 PCM/FX-chain code is deliberately absent from this link.
"""
from pathlib import Path
import subprocess,json,hashlib,tempfile
R=Path(__file__).resolve().parent;O=R/'build/r20g';O.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
c=['src/phone_input.c','src/phone_events.c','src/phone_native.c','src/note_native.c','src/note_abi.S','src/music_settings.c','src/music_archive.c','src/music_ui.c','src/music_lighting.c','src/step_native.c','src/step_abi.S','src/workflow_ui.c','src/workflow_abi.S','src/browser_tree.c','src/resample_destination.c','src/pad_source_abi.S','src/fx_eq.c','src/pad_eq.c','src/pad_eq_native.c','src/resample_fast_plan.c','src/resample_fault.c']
cpp=['src/step_exceptions.cc','src/step_mute.cc','src/batch_copy.cc','src/batch_events.cc','src/resample_queue.cc','src/resample_stock.cc','src/resample_stock_backend.cc','src/resample_simple_ui.cc','src/pad_eq_history.cc','src/resample_fast_native.cc']
sources=sorted(set(c+cpp+[p.as_posix() for p in Path('src').glob('*.h') if p.name not in ('resample_capture.h','resample_capture_native.h')]+['verify_r20g.py','check_batch_eq.c','check_batch_native.c','check_resample_rgb.c','check_workflow_rgb.c','check_resample_bootstrap.c']))
before={s:sha(R/s) for s in sources}
common=['-O2','-g','-fPIC','-Wall','-Wextra','-Werror','-Ibuild','-DMPC_RESAMPLE_TRIAL=1','-DMPC_STOCK_RESAMPLE=1','-DMPC_EXPERIMENTAL_CHOP_EXPORT=1','-DMPC_PAD_EQ=1','-DMPC_FAST_RESAMPLE=1','-mbranch-protection=standard']
objects=[]
for source in cpp:
 obj=O/(Path(source).stem+'.o');objects.append(str(obj))
 subprocess.run(['aarch64-linux-gnu-g++','-std=c++11',*common,'-c',source,'-o',str(obj)],cwd=R,check=True)
library=O/'phone-input.so'
subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*common,'-Wcast-function-type','-shared','-Wl,-z,noexecstack,-z,relro,-z,now','-Wl,--no-undefined',*c,*objects,'-lstdc++','-ldl','-pthread','-lm','-o',str(library)],cwd=R,check=True)
from verify_callbacks import verify
callbacks=verify(library)
q=['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808']
support=O/'check-support.o'
subprocess.run(['aarch64-linux-gnu-g++','-std=c++11',*common,'-c','check_steps_support.cc','-o',str(support)],cwd=R,check=True)
batch=O/'batch-eq'
subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*common,'check_batch_eq.c','check_steps_support.S','src/step_abi.S','src/pad_eq.c','src/fx_eq.c',str(support),str(O/'step_mute.o'),str(O/'batch_copy.o'),str(O/'batch_events.o'),str(O/'step_exceptions.o'),'-lstdc++','-ldl','-pthread','-lm','-o',str(batch)],cwd=R,check=True)
result=subprocess.run([*q,str(batch),str(stock)],cwd=R,capture_output=True,text=True,timeout=30)
(O/'batch-eq.log').write_text(result.stdout+result.stderr);result.check_returncode()
rgb=O/'resample-rgb'
subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*common,'-ffunction-sections','-fdata-sections','-Wl,--gc-sections','check_resample_rgb.c','src/phone_input.c','-ldl','-pthread','-lm','-o',str(rgb)],cwd=R,check=True)
result=subprocess.run([*q,str(rgb)],cwd=R,capture_output=True,text=True,timeout=30)
(O/'resample-rgb.log').write_text(result.stdout+result.stderr);result.check_returncode()
# Test the actual constructor with every immutable r20 clone patch applied.
import struct,zipfile
frozen=R.parent/'mpc-sample-r20-private-sample-seq.zip'
assert sha(frozen)=='b7e1431082718c3327b1b0f4000a0a6b73cb2fb2901450bf59d5753a16af5ee4'
manifest=json.loads((R/'build/r20/build-info.json').read_text())
bundle=O/'bootstrap-patches.bin'
with zipfile.ZipFile(frozen) as archive,bundle.open('wb') as output:
 for patch in manifest['patches']:
  body=archive.read('autorun/'+patch['file']);assert hashlib.sha256(body).hexdigest()==patch['sha256']
  output.write(struct.pack('<QQ',int(patch['offset'],16),len(body))+body)
boot=O/'bootstrap'
subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-rdynamic','check_resample_bootstrap.c','-ldl','-lm','-o',str(boot)],cwd=R,check=True)
p=subprocess.run([*q,str(boot),str(stock),str(library),str(bundle)],cwd=R,capture_output=True,text=True,timeout=30)
(O/'bootstrap.log').write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='');p.check_returncode();assert 'PASS real full preload constructor' in p.stdout
with tempfile.TemporaryDirectory(prefix='mpc-r19-') as folder:
 path=Path(folder)/'phone-input.so';path.write_bytes(library.read_bytes())
 p=subprocess.run([*q,'-E','LD_DEBUG=libs','-E','LD_PRELOAD='+str(path),'/home/user/mpc-rootfs-user-20260808/usr/bin/busybox.nosuid','true'],cwd=R,capture_output=True,text=True,timeout=30)
 (O/'load.log').write_text(p.stdout+p.stderr);p.check_returncode();assert 'calling init: '+str(path) in p.stderr and 'cannot be preloaded' not in p.stderr
for report in ['build/resample-fast/verification.json','build/resample-stock/verification.json','build/pad-eq/verification.json','build/r17a/verification.json','build/r17a-ubsan/verification.json']:
 check=json.loads((R/report).read_text());assert check['passed'],report
 for s,digest in check['sources_sha256'].items():assert sha(R/s)==digest,(report,s)
assert before=={s:sha(R/s) for s in sources}
report={'revision':'r20g-rendered-file','passed':True,'hardware_tested':False,'card_written':False,'hardware_trial_candidate':True,'callbacks':callbacks,'library_sha256':sha(library),'linked_sources':c+cpp,'excluded':['old MemorySample resample renderer','separate FX chain','live output recorder prototype'],'scope':'private one-note Seq-render path for Sample; stock Seq and per-pad EQ retained; r20f comparison identifies Sample track WAV vs requested WAV mismatch; retain validated native output through cleanup and atomically finalize requested import path; hardware trial pending','sources_sha256':before}
(O/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='sources_sha256'},indent=2))
