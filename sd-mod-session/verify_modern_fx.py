"""Pinned native AIR/MPC DSP and the production adapter, entirely in QEMU."""
from concurrent.futures import ThreadPoolExecutor
from hashlib import sha256
from pathlib import Path
import json,os,re,subprocess,sys
R=Path(__file__).resolve().parent
san='--ubsan' in sys.argv
out=R/('build/modern-fx-ubsan' if san else 'build/modern-fx');out.mkdir(exist_ok=True)
flags=['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'] if san else []
runtime=Path(os.environ.get('MPC_RUNTIME_ROOT',str(R.parent))).resolve()
stock=runtime/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
assert sha256(stock.read_bytes()).hexdigest()=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
sources=['check_modern_fx.cc','src/fx_native.cc','src/fx_eq.c','src/fx_catalog.c','src/fx_chain.c', 'src/fx_own.c', 'src/compressor_dsp.c']
tracked=sources+['check_modern_fx.c','check_fx_metadata.h','check_fx_native.c','src/fx_native.h','src/fx_chain.h','src/fx_catalog.h','src/fx_eq.h','verify_modern_fx.py']
before={s:sha256((R/s).read_bytes()).hexdigest() for s in tracked}
objects=[]
for source in sources:
    obj=out/(Path(source).stem+'.o');objects.append(str(obj))
    cpp=source.endswith('.cc')
    subprocess.run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-c','-O1','-g','-Wall','-Wextra','-Werror',*(['-std=c++11'] if cpp else []),*flags,source,'-o',str(obj)],cwd=R,check=True)
target=out/'native-fx'
subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',*flags,'check_modern_fx.c',*objects,'-lstdc++','-ldl','-lm','-o',str(target)],cwd=R,check=True)
ids=re.findall(r'\{0x([0-9a-f]+),', (R/'src/fx_catalog.c').read_text())
assert len(ids)==25 and len(set(ids))==25
def run(ident):
    r=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock),ident],cwd=R,capture_output=True,text=True,timeout=45)
    (out/(ident+'.log')).write_text(r.stdout+r.stderr)
    r.check_returncode();assert 'PASS production native FX backend' in r.stdout
    return {'id':ident,'passed':True,'audio':next(line for line in r.stdout.splitlines() if 'real audio energy=' in line)}
with ThreadPoolExecutor(max_workers=3) as pool:checks=list(pool.map(run,ids))
assert before=={s:sha256((R/s).read_bytes()).hexdigest() for s in tracked},'Sources changed during verification'
report={'passed':True,'ubsan':san,'checks':checks,'hardware_tested':False,'card_written':False,'fixture':'native metadata and DSP sample-rate table initialization, empty preset-directory service; real native AIR/MPC constructors, complete parameters, prepare/process/destruction and production adapter; five-stage audio/tail/reorder proof; no GUI or audio devices','sources_sha256':before}
(out/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS all 25 actual native AIR/MPC effects and production FX backend'+(' under UBSan' if san else ''))
