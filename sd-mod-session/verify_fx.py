"""Offline FX implementation checks; never build/deploy a device package."""
from hashlib import sha256
from pathlib import Path
import json
import subprocess

R=Path(__file__).resolve().parent
ELF=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
ROOT=Path('/home/user/mpc-rootfs-user-20260808')
assert sha256(ELF.read_bytes()).hexdigest()=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
checks=[]
targets={
    'fx':['check_fx.c','src/fx_eq.c','src/fx_chain.c','-lm','-pthread'],
    'fx-editor':['check_fx_eq_editor.c','src/fx_eq_editor.c','src/fx_eq.c','-lm'],
    'fx-editor-ubsan':['check_fx_eq_editor.c','src/fx_eq_editor.c','src/fx_eq.c','-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all','-lm'],
    'fx-native':['check_fx_native.c','-ldl','-lm'],
    'fx-catalog-native':['inspect_fx_catalog_native.c','-ldl','-lm'],
}
for name,sources in targets.items():
    output=R/'build'/('check-'+name)
    subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror',*sources,'-o',str(output)],cwd=R,check=True)
    command=['qemu-aarch64','-L',str(ROOT),str(output)]
    if name in ('fx-native','fx-catalog-native'):command.append(str(ELF))
    run=subprocess.run(command,cwd=R,text=True,capture_output=True)
    (R/'build'/(name+'-verification.log')).write_text(run.stdout+run.stderr)
    run.check_returncode()
    assert 'PASS' in run.stdout
    checks.append({'name':name,'passed':True,'output':run.stdout.splitlines()[-1]})
result={'checks':checks,'device_package_changed':False,'hardware_tested':False,
        'native_fx_select_hook_installed':False,'resample_engine_connected':False,
        'notes':'Core DSP and exchange run as ARM; native legacy 2-band EQ audio executes; real 28-entry AC50 catalogue constructor executes until engine subscription boundary. Modern four-band EQ registration is static evidence, not an audio/routing test.'}
source_files={name for values in targets.values() for name in values if not name.startswith('-')}|{'src/fx_eq.h','src/fx_chain.h','src/fx_eq_editor.h','verify_fx.py'}
result['sources_sha256']={name:sha256((R/name).read_bytes()).hexdigest() for name in sorted(source_files)}
(R/'build/fx-verification.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
