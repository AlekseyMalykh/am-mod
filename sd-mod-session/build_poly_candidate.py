"""Build a lab-only chord candidate; no package or physical SD writes."""
from pathlib import Path
import hashlib,json,subprocess,sys
R=Path(__file__).resolve().parent
sequence_only='--sequence-only' in sys.argv
O=R/('build/power-refresh' if '--power-refresh' in sys.argv else 'build/sequence-refresh' if '--sequence-refresh' in sys.argv else 'build/sequence-colours' if sequence_only else 'build/poly-record');O.mkdir(exist_ok=True)
sources=json.loads((R/'build/r22/verification.json').read_text())['linked_sources']+['src/poly_record.c']
if '--power-refresh' in sys.argv:sources+=['src/power_control.cc']
tracked=sorted(set(sources+['build_poly_candidate.py']+[
    p.as_posix() for p in Path('src').glob('*.h')
    if p.name not in ('resample_capture.h','resample_capture_native.h')]))
source_hashes={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in tracked}
common=['-O2','-g','-fPIC','-Wall','-Wextra','-Werror','-Ibuild','-DMPC_RESAMPLE_TRIAL=1','-DMPC_STOCK_RESAMPLE=1','-DMPC_EXPERIMENTAL_CHOP_EXPORT=1','-DMPC_PAD_EQ=1','-DMPC_FAST_RESAMPLE=1','-DMPC_SEQUENCE_COLOURS=1','-mbranch-protection=standard']
if not sequence_only:common+=['-DMPC_POLY_RECORD=1']
if '--power-refresh' in sys.argv:common+=['-DMPC_POWER_BUTTON=1']
objects=[]
for name in sources:
    obj=O/(Path(name).stem+'.o');objects.append(str(obj))
    cpp=name.endswith('.cc')
    subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11',*common,'-c',name,'-o',str(obj)],check=True)
library=O/'phone-input.so'
subprocess.run(['aarch64-linux-gnu-g++','-shared','-Wl,-z,noexecstack,-z,relro,-z,now','-Wl,--no-undefined',*objects,'-ldl','-pthread','-lm','-o',str(library)],check=True)
q=['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808']
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
p=subprocess.run([*q,str(R/'build/r22/bootstrap'),str(stock),str(library),str(R/'build/r22/bootstrap-patches.bin')],capture_output=True,text=True,timeout=45)
(O/'bootstrap.log').write_text(p.stdout+p.stderr);print(p.stdout+p.stderr);p.check_returncode()
assert ('PITCH_AWARE_OVERWRITE_INSTALLED' in p.stderr)==(not sequence_only)
assert 'PASS real full preload constructor' in p.stdout
print(hashlib.sha256(library.read_bytes()).hexdigest())
if '--power-refresh' in sys.argv:
    assert source_hashes=={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in tracked}
    (O/'compile-verification.json').write_text(json.dumps(dict(
        passed=True,library_sha256=hashlib.sha256(library.read_bytes()).hexdigest(),
        linked_sources=sources,sources_sha256=source_hashes,
        full_preload_constructor=True,native_main_executed=False,
        full_vm_verified=False,physical_card_written=False),indent=2)+'\n')
