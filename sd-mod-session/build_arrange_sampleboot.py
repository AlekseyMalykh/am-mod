"""Build an isolated Arrange candidate from the pinned r30d-beta4 envelope.
Local files/QEMU only. No card, device, installation or release upload.
"""
from pathlib import Path
import argparse,hashlib,json,os,struct,subprocess,zipfile
p=argparse.ArgumentParser();p.add_argument('--base',type=Path,required=True);p.add_argument('--stock',type=Path,required=True);p.add_argument('--test-number',type=int,default=2);a=p.parse_args()
assert 1<=a.test_number<=99
version=f'r30d-arrange-test{a.test_number}'
R=Path(__file__).resolve().parent;O=R/f'build/arrange-sampleboot-test{a.test_number}';O.mkdir(parents=True,exist_ok=True)
for name in ('compile-verification.json','base-verification.json'):(O/name).unlink(missing_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
assert sha(a.base.read_bytes())=='c7883a402d387753d77f8770fae8306bf8358ae3a134b2bdb7ae9b10e02736ea','Use exact r30d-beta4 release'
assert sha(a.stock.read_bytes())=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
B=O/'bootstrap';B.mkdir(exist_ok=True)
manifest=json.loads((R/'build/r23a/build-info.json').read_text())
patches=[]
with zipfile.ZipFile(a.base) as z:
    assert z.testzip() is None
    assert any(n.endswith('/phone_hooks.h') and z.read(n)==(R/'build/phone_hooks.h').read_bytes() for n in z.namelist()),'Native hook layout differs from release'
    with (B/'bootstrap-patches.bin').open('wb') as f:
        for item in manifest['patches']:
            data=z.read('autorun/'+item['file']);offset=int(item['offset'],16)
            assert 0<len(data)<=65536 and offset+len(data)<=0x7500000
            f.write(struct.pack('<QQ',offset,len(data))+data)
            patches.append(dict(file=item['file'],offset=item['offset'],size=len(data),sha256=sha(data)))
subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-rdynamic','check_resample_bootstrap.c','-ldl','-lm','-o',str(B/'bootstrap')],cwd=R,check=True)
env=dict(os.environ,AM_MOD_BUILD_NAME=O.name,AM_MOD_VERSION_OVERRIDE=version,AM_MOD_BOOTSTRAP_DIR=str(B),MPC_STOCK_APP=str(a.stock.resolve()),
    AM_MOD_PATTERN_TOOLS_TRIAL='1',AM_MOD_PAD_MIDI_RECORDING_TRIAL='1',AM_MOD_SYNTH_NATIVE_PLAYBACK_TRIAL='1',AM_MOD_MIDI_EVENT_CONTEXT='1',
    AM_MOD_PAD_MIDI_VM_PROBE='0',AM_MOD_PATTERN_SOURCE_TRIAL='0',AM_MOD_PGM_NATIVE_PROBE='0',AM_MOD_RECORD_DIAGNOSTICS='0',AM_MOD_RECORD_CHAIN_BYPASS='0',AM_MOD_RECORD_KEEP_RECOVERY_PATH='0')
subprocess.run(['python3','build_mixer_r27r.py'],cwd=R,env=env,check=True)
compiled=json.loads((O/'compile-verification.json').read_text());assert compiled['passed']
assert version.encode() in (O/'phone-input.so').read_bytes()
(O/'base-verification.json').write_text(json.dumps(dict(passed=True,base_sha256=sha(a.base.read_bytes()),stock_sha256=sha(a.stock.read_bytes()),patches=patches,
    bootstrap_sha256=sha((B/'bootstrap').read_bytes()),bundle_sha256=sha((B/'bootstrap-patches.bin').read_bytes()),version=version,card_written=False,hardware_tested=False),indent=2)+'\n')
print('PASS Arrange test candidate compile/constructor/routes',compiled['library_sha256'],flush=True)
