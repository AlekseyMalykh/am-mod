"""Isolated ARM native Repeats regression; never builds the app/package."""
from pathlib import Path
import argparse, hashlib, json, os, subprocess
p=argparse.ArgumentParser();p.add_argument('--stock',required=True);a=p.parse_args()
R=Path(__file__).resolve().parent;stock=Path(a.stock).resolve()
assert hashlib.sha256(stock.read_bytes()).hexdigest()=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
out=R/'build/song-repeats-native';out.mkdir(parents=True,exist_ok=True)
tracked=['src/song_repeats.inc','check_song_repeats_native.c','check_fx_native.c','verify_song_repeats_native.py']
hashes={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in tracked}
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    target=out/name
    subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'check_song_repeats_native.c','-ldl','-lm','-pthread','-o',str(target)],cwd=R,check=True)
    result=subprocess.run(['qemu-aarch64','-L',os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808'),str(target),str(stock)],cwd=R,text=True,capture_output=True,timeout=30)
    (out/(name+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True)
    result.check_returncode();assert 'PASS native Repeats' in result.stdout
assert hashes=={n:hashlib.sha256((R/n).read_bytes()).hexdigest() for n in tracked}
(out/'verification.json').write_text(json.dumps(dict(passed=True,variants=['normal','ubsan'],sources_sha256=hashes,native_builder=True,native_audio_cache=True,native_sequence_chain_observer=True,native_command_apply_undo=True,native_end_of_sequence_chooser=True,native_end_handler='0x2120590',dsp_voice_activation_sinks=True,queue_storage_fixture=True,executor_fixture=True,controller_storage_fixture=True,transport_tested=False,hardware_tested=False,application_built=False),indent=2)+'\n')
