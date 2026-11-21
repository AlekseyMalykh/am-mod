"""Pinned native CopyPadCommand proof; no device or SD writes."""
from pathlib import Path
import hashlib,json,subprocess,sys
R=Path(__file__).resolve().parent;O=R/'build/pgm-import';O.mkdir(exist_ok=True)
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
assert hashlib.sha256(stock.read_bytes()).hexdigest()=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
flags=['-O2','-g','-fPIC','-Wall','-Wextra','-Werror','-DMPC_PGM_NATIVE_PROBE=1','-mbranch-protection=standard','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-Ibuild']
eq='--eq' in sys.argv[1:]
if eq:flags.append('-DMPC_PAD_EQ=1')
objects=[]
for source in ['check_steps_support.cc','src/batch_copy.cc','src/batch_events.cc','src/step_mute.cc','src/step_exceptions.cc']:
    target=O/('import-check-'+Path(source).stem+'.o');objects.append(str(target))
    subprocess.run(['aarch64-linux-gnu-g++','-std=c++11',*flags,'-c',source,'-o',str(target)],cwd=R,check=True)
binary=O/('check-program-import-native-eq' if eq else 'check-program-import-native')
subprocess.run(['aarch64-linux-gnu-gcc','-std=c11',*flags,'-Wno-unused-variable','check_program_import_native.c','check_steps_support.S',
                'src/step_abi.S',*(['src/pad_eq.c','src/fx_eq.c'] if eq else []),*objects,'-lstdc++','-ldl','-pthread','-lm','-o',str(binary)],cwd=R,check=True)
result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(binary),str(stock)],cwd=R,capture_output=True,text=True,timeout=60)
label='native-pad-import-eq-check' if eq else 'native-pad-import-check'
(O/(label+'.log')).write_text(result.stdout+result.stderr)
print(result.stdout);result.check_returncode()
(O/(label+'.json')).write_text(json.dumps({'passed':True,'full_vm_tested':False,'hardware_tested':False,
 'native_commands_tested':True,'destination_eq_and_colours_preserved':eq,'log':label+'.log',
 'sources_sha256':{s:hashlib.sha256((R/s).read_bytes()).hexdigest() for s in ['src/batch_copy.cc','check_program_import_native.c','verify_program_import_native.py']}},indent=2)+'\n')
