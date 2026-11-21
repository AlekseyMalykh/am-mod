"""Host ownership transactions + real ARM startup selection. No physical SD."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile,zipfile
from package_usb_candidate import add_usb,add_usb_drive
R=Path(__file__).resolve().parent;O=R/'build/usb-drive';O.mkdir(parents=True,exist_ok=True)
sources=['src/usb_drive.sh','src/usb_drive_screen.c','src/usb_drive_launcher.inc.sh','src/startup_project.cc','src/project.S',
         'src/music_ui.c','src/music_ui.h','src/phone_native.c','check_usb_drive.py','check_startup_project.cc',
         'check_project.c','build_pad_lighting_menu.py','verify_pad_lighting.py','verify_usb_drive.py','package_usb_candidate.py',
         'src/early_drm_splash.c','src/splash_branding.h','src/branding_font_aa.h','src/mod_build.h',
         'src/usb_drive_input.h','check_usb_drive_screen.c']
sources+=['src/usb_drive_lights.h']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before={s:sha(R/s) for s in sources}
def run(*a,**kw):return subprocess.run(list(map(str,a)),cwd=R,check=True,**kw)
run('sh','-n','src/usb_drive.sh');run('sh','-n','src/usb_drive_launcher.inc.sh');run('python3','check_usb_drive.py')
base=Path(os.environ.get('AM_USB_BASE_ZIP','/mnt/c/Users/Алексей/Desktop/akai mpc sample/releases/r30a/am-mod-r30a.zip'))
with zipfile.ZipFile(base) as z:launcher=z.read('autorun/mod-launcher.sh').decode()
launcher=add_usb_drive(add_usb(launcher,'0'*64,'1'*64),'2'*64,'3'*64)
run('sh','-n','/dev/stdin',input=launcher,text=True)
(O/'generated-launcher.sh').write_text(launcher)
assert launcher.index('systemctl stop "$service.service"')<launcher.index('"$R/usb-drive.sh" prepare')
recovery=launcher[launcher.index('\n  recover)\n'):launcher.index('\n  supervise)\n')]
assert recovery.index('"$R/usb-drive.sh" restore')<recovery.index('"$R/stage-data" --flush-sd')
guard=launcher[launcher.index('  guard)\n'):launcher.index('  main) ;')]
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp);(root/'clone-verified').touch();(root/'usb-drive-active').touch()
 mock=root/'systemctl';mock.write_text('''#!/bin/sh
if [ "$1" = is-active ]; then case "$3" in *supervisor.service) [ -f "$R/supervisor-alive" ]; exit $? ;; *) exit 3 ;; esac; fi
printf '%s\\n' "$*" >> "$R/calls"
''');mock.chmod(0o755);(root/'supervisor-alive').touch()
 script='log() { :; }\ncase guard in\n'+guard+'esac\n'
 env={**os.environ,'PATH':str(root)+':'+os.environ['PATH'],'R':str(root),'U':'usb-fixture'}
 run('sh','-c',script,env=env);assert not (root/'calls').exists()
 (root/'supervisor-alive').unlink();run('sh','-c',script,env=env);assert 'usb-fixture-recover.service' in (root/'calls').read_text()
print('PASS generated launcher: helpers stop before SD handoff; recovery unexports before SD flush; guard protects live transfer and recovers a dead owner')
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
sysroot=os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')
test_media=os.environ.get('AM_USB_TEST_MEDIA','/media/am-mod-tests')
assert test_media.startswith('/media/') and os.access(test_media,os.W_OK),'Prepare an owned test directory under /media; set AM_USB_TEST_MEDIA'
for mode,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 exe=O/(mode+'-startup')
 run('aarch64-linux-gnu-g++','-O1','-g','-Wall','-Wextra','-Werror',f'-DSTARTUP_TEST_PARENT="{test_media}"',*flags,'check_startup_project.cc','-ldl','-o',exe)
 p=subprocess.run(['qemu-aarch64','-L',sysroot,str(exe),str(stock)],cwd=R,capture_output=True,text=True,timeout=45)
 (O/(mode+'-startup.log')).write_text(p.stdout+p.stderr);print(p.stdout+p.stderr,end='');p.check_returncode()
 assert 'PASS USB resume loads the exact older saved copy once' in p.stdout
run('aarch64-linux-gnu-gcc','-std=c11','-O2','-static','-Wall','-Wextra','-Werror','src/usb_drive_screen.c','-o',O/'usb-drive-screen')
for label,flags in [('normal',[]),('san',['-fsanitize=address,undefined','-fno-sanitize-recover=all'])]:
 run('gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,'check_usb_drive_screen.c','-o',O/(label+'-screen'))
 run(O/(label+'-screen'))
preview=O/'screen-preview.c'
preview.write_text('''#define USB_DRIVE_SCREEN_NO_MAIN 1
#include "src/usb_drive_screen.c"
int main(int argc,char**argv){
 if(argc!=2&&argc!=3)return 2;
 uint32_t pixels[320*240];struct screen s={.fd=-1,.map=pixels,.pitch=1280};draw(&s,argc==3?argv[2]:"sharing");
 FILE*f=fopen(argv[1],"wb");if(!f)return 2;fprintf(f,"P6\\n320 240\\n255\\n");
 for(unsigned i=0;i<320*240;++i){unsigned char c[3]={(unsigned char)(pixels[i]>>16),(unsigned char)(pixels[i]>>8),(unsigned char)pixels[i]};fwrite(c,1,3,f);}return fclose(f)!=0;
}
''')
run('aarch64-linux-gnu-gcc','-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-I.',preview,'-o',O/'screen-preview')
run('qemu-aarch64','-L',sysroot,O/'screen-preview',O/'screen.ppm')
run('qemu-aarch64','-L',sysroot,O/'screen-preview',O/'screen-confirm.ppm','confirm')
assert before=={s:sha(R/s) for s in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,sources_sha256=before,simulated_ownership_transactions=True,
 exact_resume_arm=True,ubsan=True,display_compiled_and_rendered=True,screen_sha256=sha(O/'usb-drive-screen'),hardware_tested=False,card_written=False),indent=2)+'\n')
