"""Host integration + target compile checks; does not access MPC or an SD card."""
from pathlib import Path
import hashlib, json, os, subprocess
R=Path(__file__).resolve().parent
O=R/'build/usb';O.mkdir(parents=True,exist_ok=True)
tracked=['src/usb_audio.c','src/usb_audio.h','src/usb_audio_alsa.c','src/usb_files.c','src/usb_mode.sh',
         'check_usb_audio.c','check_usb_alsa.c','check_usb_alsa_fake.c','check_usb_files.py','check_usb_mode.py','verify_usb.py']
hashes={s:hashlib.sha256((R/s).read_bytes()).hexdigest() for s in tracked}
def run(*args,**kwargs):
 print('+',' '.join(map(str,args)),flush=True)
 return subprocess.run(list(map(str,args)),cwd=R,check=True,**kwargs)
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation']
san=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
run('gcc',*flags,*san,'check_usb_audio.c','src/usb_audio.c','-lm','-pthread','-o',O/'check-audio')
run(O/'check-audio')
run('gcc',*flags,*san,'-DUSB_FILES_ADDRESS="127.0.0.1"','-DUSB_FILES_PORT=18082','src/usb_files.c','-o',R/'build/usb-files-host')
run('python3','check_usb_files.py')
active=O/'active'
run('gcc',*flags,'-shared','-fPIC','check_usb_alsa_fake.c','-o',O/'libfake-alsa.so')
run('gcc',*flags,'-shared','-fPIC',f'-DMPC_USB_ACTIVE_PATH="{active}"','src/usb_audio_alsa.c','src/usb_audio.c','-ldl','-pthread','-lm','-o',O/'libusb-audio.so')
run('gcc',*flags,'check_usb_alsa.c',f'-L{O}',f'-Wl,-rpath,{O}','-lfake-alsa','-Wl,--no-as-needed','-lusb-audio','-o',O/'check-alsa')
run(O/'check-alsa',active,env={**os.environ,'LD_PRELOAD':str(O/'libusb-audio.so')})
run('sh','-n','src/usb_mode.sh')
run('python3','check_usb_mode.py')
for source in ['usb_audio.c','usb_audio_alsa.c']:
 run('aarch64-linux-gnu-gcc',*flags,'-fPIC','-mbranch-protection=standard','-c','src/'+source,'-o',O/(source+'.o'))
run('aarch64-linux-gnu-gcc',*flags,'-static','src/usb_files.c','-o',O/'usb-files')
assert hashes=={s:hashlib.sha256((R/s).read_bytes()).hexdigest() for s in tracked}
(O/'verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=hashes,host_audio_sanitizers=True,alsa_interposition=True,usb_files_integration=True,aarch64_compile=True,hardware_tested=False,card_written=False),indent=2)+'\n')
print('PASS USB host integration and AArch64 compile; hardware test pending')
