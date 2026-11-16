"""Package r30b over the pinned r30a release, never write a physical card."""
from pathlib import Path
import argparse, hashlib, json, posixpath, re, subprocess, sys, zipfile
from package_r28 import verified_asset, SALT
ROOT=Path(__file__).resolve().parent
VERSION='r30b'
BASE_SHA='1e3c26a15f0a082a0847ab4dcc4ed29bbed11d81167ea7fc1cafe17e9868131d'
def sha(b):return hashlib.sha256(b).hexdigest()

def add_usb(launcher,files_sha,mode_sha,names_sha='0'*64):
 def replace(old,new):
  nonlocal launcher
  assert launcher.count(old)==1,(old,launcher.count(old))
  launcher=launcher.replace(old,new)
 stage='storage_host_check || fail SD_SAVE_DIRECTORY_NOT_PINNED\n'
 replace(stage,stage+f'''# Verified host helpers are staged into RAM before any application switch.
for usb_name in usb-files usb-mode.sh; do
    case "$usb_name" in usb-files) usb_hash={files_sha} ;; usb-mode.sh) usb_hash={mode_sha} ;; esac
    [ "$(hash "$DIR/$usb_name")" = "$usb_hash" ] || fail USB_HELPER_HASH_MISMATCH
    cp "$DIR/$usb_name" "$R/$usb_name" || fail USB_HELPER_COPY_FAILED
    chmod 500 "$R/$usb_name"
    [ "$(hash "$R/$usb_name")" = "$usb_hash" ] || fail USB_HELPER_COPY_HASH_MISMATCH
done
# The kernel module is loaded only from verified RAM, never from SD or eMMC.
[ "$(hash "$DIR/am_usb_names.ko")" = "{names_sha}" ] || fail USB_NAMES_MODULE_HASH
cp "$DIR/am_usb_names.ko" "$R/am_usb_names.ko" || fail USB_NAMES_MODULE_COPY
chmod 400 "$R/am_usb_names.ko"
[ "$(hash "$R/am_usb_names.ko")" = "{names_sha}" ] || fail USB_NAMES_MODULE_COPY_HASH
''')
 before='    checkpoint BEFORE_CLONE_START || fail SD_JOURNAL_CHECKPOINT_FAILED\n'
 replace(before,'''    # Configure USB only after stock stopped, before the clone opens ALSA.
    "$R/usb-mode.sh" prepare "$(cat "$R/storage-meta/save-path")" || fail USB_AUDIO_PREPARE_FAILED
    if [ -f "$R/app/usb-audio-18.active" ]; then
        # The recreated gadget may receive a different ALSA card index. Add
        # only current audio character devices before systemd starts clone.
        udevadm settle --timeout=5 || fail USB_AUDIO_UDEV_TIMEOUT
        mkdir -p "/run/systemd/system/$U-clone.service.d"
        usb_drop="/run/systemd/system/$U-clone.service.d/usb-audio.conf"
        printf '[Service]\\n' > "$usb_drop"
        for usb_node in /dev/snd/*; do
            [ -c "$usb_node" ] || continue
            printf 'DeviceAllow=%s rw\\n' "$usb_node" >> "$usb_drop"
        done
        systemctl daemon-reload
    fi
'''+before)
 start='    systemctl start "$U-clone.service"\n'
 replace(start,start+'''    systemd-run --quiet --unit="$U-usb-files" \\
        -p "BindsTo=$U-clone.service" -p "After=$U-clone.service" \\
        -p NoNewPrivileges=yes -p ProtectSystem=strict -p ProtectHome=yes \\
        -p DevicePolicy=closed -p 'CapabilityBoundingSet=' \\
        -p 'RestrictAddressFamilies=AF_INET AF_UNIX' \\
        -p "ReadWritePaths=$(cat "$R/storage-meta/save-path")/Samples" \\
        -p Nice=15 -p MemoryMax=16M -p TasksMax=2 \\
        "$R/usb-files" "$(cat "$R/storage-meta/save-path")" "$R/app/usb-files.on" || log USB_FILES_START_FAILED
''')
 recover='    data_layer_stop || fail DATA_LAYER_STILL_MOUNTED\n'
 replace(recover,recover+'''    # Restore stereo before starting a stock audio owner, including crashes.
    if [ -x "$R/usb-mode.sh" ]; then "$R/usb-mode.sh" restore || fail USB_AUDIO_RESTORE_FAILED; fi
''')
 # Stop the helper before the recovery flush. Never let it keep writing after
 # the mod has stopped or while shutdown is flushing the SD.
 stop='    systemctl stop "$U-clone.service" || true\n'
 replace(stop,'''    systemctl stop "$U-usb-files.service" 2>/dev/null || true
'''+stop)
 return launcher

def add_usb_drive(launcher,script_sha,screen_sha):
 def replace(old,new):
  nonlocal launcher
  assert launcher.count(old)==1,(old,launcher.count(old))
  launcher=launcher.replace(old,new)
 stage='storage_host_check || fail SD_SAVE_DIRECTORY_NOT_PINNED\n'
 replace(stage,stage+f'''for drive_name in usb-drive.sh usb-drive-screen; do
    case "$drive_name" in usb-drive.sh) drive_sha={script_sha} ;; usb-drive-screen) drive_sha={screen_sha} ;; esac
    [ "$(hash "$DIR/$drive_name")" = "$drive_sha" ] || fail USB_DRIVE_HELPER_HASH
    cp "$DIR/$drive_name" "$R/$drive_name" || fail USB_DRIVE_HELPER_COPY
    chmod 500 "$R/$drive_name"
    [ "$(hash "$R/$drive_name")" = "$drive_sha" ] || fail USB_DRIVE_HELPER_COPY_HASH
done
''')
 start='    systemctl start "$U-clone.service"\n'
 a=launcher.index(start)+len(start);b=launcher.index('    n=0\n',a)
 aux=launcher[a:b]
 assert 'systemd-run' in aux and 'screen-mirror' in aux
 # An inactive transient unit can still be registered after stopping clone.
 aux=aux.replace('|| log USB_FILES_START_FAILED','|| systemctl start "$U-usb-files.service" || log USB_FILES_START_FAILED')
 aux=aux.replace('|| log SCREEN_MIRROR_START_FAILED','|| systemctl start "$U-screen-mirror.service" || log SCREEN_MIRROR_START_FAILED')
 launcher=launcher[:a]+'    usb_start_aux\n'+launcher[b:]
 functions=(ROOT/'src/usb_drive_launcher.inc.sh').read_text()
 functions+='\nusb_start_aux() {\n'+aux+'}\n'
 replace('case "${1:-main}" in\n',functions+'\ncase "${1:-main}" in\n')
 loop='    while systemctl is-active --quiet "$U-clone.service"; do\n'
 replace(loop,loop+'''        if [ -f "$R/app/usb-drive.request" ] && [ ! -L "$R/app/usb-drive.request" ] &&
           [ "$(stat -c %s "$R/app/usb-drive.request")" = 1 ] && [ "$(cat "$R/app/usb-drive.request")" = 1 ]; then
            usb_drive_cycle || fail USB_DRIVE_RETURN_FAILED
            verified=0;n=0;picker_seen=;picker_observed_serial=;picker_busy=0
            continue
        fi
''')
 guard='  guard)\n    exit_mode=guard\n'
 replace(guard,guard+'''    # The clone is intentionally stopped while its host supervisor owns SD.
    if [ -f "$R/usb-drive-active" ] && systemctl is-active --quiet "$U-supervisor.service"; then exit 0; fi
''')
 stop='    [ "$(prop "$U-clone.service" MainPID)" = 0 ] || fail CLONE_STILL_RUNNING\n'
 replace(stop,stop+'''    # Stop/close Mass Storage before recovery reads or writes the card.
    if [ -f "$R/usb-drive-active" ]; then
        "$R/usb-drive.sh" restore || fail USB_DRIVE_RECOVERY_FAILED
        rm -f "$R/usb-drive-active"
    fi
''')
 return launcher

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('base',type=Path);args=parser.parse_args()
 assert sha(args.base.read_bytes())==BASE_SHA,'Unexpected r30a base'
 assert f'#define AM_MOD_VERSION "{VERSION}"' in (ROOT/'src/mod_build.h').read_text()
 fx_ui=json.loads((ROOT/'build/fx-layout/verification.json').read_text());assert fx_ui['passed']
 amp_audio=json.loads((ROOT/'build/modern-fx/verification.json').read_text());assert amp_audio['passed']
 comp_revision=json.loads((ROOT/'compressor_base.json').read_text())['revision']
 compressor_ui=json.loads((ROOT/'build/compressor'/comp_revision/'verification.json').read_text());assert compressor_ui['passed']
 pad_rgb=json.loads((ROOT/'build/pad-colour-rgb/verification.json').read_text());assert pad_rgb['passed']
 overlay=json.loads((ROOT/'build/sample-overlay/verification.json').read_text());assert overlay['passed']
 takeover=json.loads((ROOT/'build/mixer-control/verification.json').read_text());assert takeover['passed']
 input_config=json.loads((ROOT/'build/input-config-ui/verification.json').read_text());assert input_config['passed']
 input_monitor=json.loads((ROOT/'build/rear-mix-input-fx/verification.json').read_text());assert input_monitor['passed']
 for proof in (fx_ui,amp_audio,compressor_ui,pad_rgb,overlay,takeover,input_config,input_monitor):
  for source,digest in proof['sources_sha256'].items():assert sha((ROOT/source).read_bytes())==digest,source
 report=json.loads((ROOT/'build/usb/verification.json').read_text());assert report['passed']
 for source,digest in report['sources_sha256'].items():assert sha((ROOT/source).read_bytes())==digest,source
 native=json.loads((ROOT/'build/usb-native/verification.json').read_text());assert native['passed']
 for source,digest in native['sources_sha256'].items():assert sha((ROOT/source).read_bytes())==digest,source
 drive_proof=json.loads((ROOT/'build/usb-drive/verification.json').read_text());assert drive_proof['passed']
 for source,digest in drive_proof['sources_sha256'].items():assert sha((ROOT/source).read_bytes())==digest,source
 mirror_proof=json.loads((ROOT/'build/mirror/verification.json').read_text());assert mirror_proof['passed']
 for source,digest in mirror_proof['sources_sha256'].items():assert sha((ROOT/source).read_bytes())==digest,source
 names_dir=ROOT/'build/usb-names'
 names_compile=json.loads((names_dir/'compile-verification.json').read_text());assert names_compile['compiled']
 names_vm=json.loads((names_dir/'vm-verification.json').read_text());assert names_vm['passed'] and names_vm['exact_firmware_kernel']
 for proof in [names_compile,names_vm]:
  for source,digest in proof['sources_sha256'].items():assert sha((ROOT/source).read_bytes())==digest,source
 names_module=(names_dir/'am_usb_names.ko').read_bytes()
 assert sha(names_module)==names_compile['module_sha256']==names_vm['module_sha256']
 library=verified_asset(ROOT/'build/mixer-r27r','compile-verification.json','phone-input.so','library_sha256')
 with zipfile.ZipFile(args.base) as z:
  assert z.testzip() is None;infos=z.infolist();files={i.filename:z.read(i) for i in infos}
 original=dict(files);launcher=files['autorun/mod-launcher.sh'].decode()
 assert launcher.count(sha(files['autorun/phone-input.so']))==1
 launcher=launcher.replace(sha(files['autorun/phone-input.so']),sha(library));files['autorun/phone-input.so']=library
 helper=ROOT/'build/usb/usb-files'
 subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O2','-static','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','src/usb_files.c','-o',str(helper)],cwd=ROOT,check=True)
 files['autorun/usb-files']=helper.read_bytes();files['autorun/usb-mode.sh']=(ROOT/'src/usb_mode.sh').read_bytes()
 files['autorun/am_usb_names.ko']=names_module
 launcher=add_usb(launcher,sha(files['autorun/usb-files']),sha(files['autorun/usb-mode.sh']),sha(names_module))
 # Corresponding GPL module source, exact kernel config, and reproducible
 # transformation/build instructions travel with the release archive.
 for name in ['f_uac2_named.c','u_uac2.h','u_audio.h','uac_common.h','usb_names_registry.inc','Makefile']:
  files['SOURCE/usb-audio-names/'+name]=(names_dir/name).read_bytes()
 files['SOURCE/usb-audio-names/build_usb_names.py']=(ROOT/'build_usb_names.py').read_bytes()
 files['SOURCE/usb-audio-names/kernel.config']=(ROOT/'build/mpc-kernel.config').read_bytes()
 files['SOURCE/usb-audio-names/compile-verification.json']=(names_dir/'compile-verification.json').read_bytes()
 files['SOURCE/usb-audio-names/vm-verification.json']=(names_dir/'vm-verification.json').read_bytes()
 files['SOURCE/usb-audio-names/LICENSE']=Path('/home/user/am-mod-kernel-sources/kernel-source/LICENSES/preferred/GPL-2.0').read_bytes()
 drive=ROOT/'build/usb/usb-drive-screen'
 subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O2','-static','-Wall','-Wextra','-Werror','src/usb_drive_screen.c','-o',str(drive)],cwd=ROOT,check=True)
 files['autorun/usb-drive-screen']=drive.read_bytes();files['autorun/usb-drive.sh']=(ROOT/'src/usb_drive.sh').read_bytes()
 launcher=add_usb_drive(launcher,sha(files['autorun/usb-drive.sh']),sha(files['autorun/usb-drive-screen']))
 splash=ROOT/'build/usb/boot-splash'
 subprocess.run(['aarch64-linux-gnu-gcc','-O2','-static','-Wall','-Wextra','-Werror','src/early_drm_splash.c','-o',str(splash)],cwd=ROOT,check=True)
 old=sha(files['autorun/boot-splash']);files['autorun/boot-splash']=splash.read_bytes();assert old in launcher
 launcher=launcher.replace(old,sha(files['autorun/boot-splash']));files['autorun/mod-launcher.sh']=launcher.encode()
 entry=files['autorun/autorun.sh'].decode();assert entry.count(sha(original['autorun/mod-launcher.sh']))==1
 assert entry.count('revision=r30a')==1
 files['autorun/autorun.sh']=entry.replace(sha(original['autorun/mod-launcher.sh']),sha(files['autorun/mod-launcher.sh'])).replace('revision=r30a',f'revision={VERSION}').encode()
 manifest=json.loads(files['autorun/autorun.json']);manifest['value0']['scripts'][0]['message']=f'Am MOD {VERSION}'
 files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
 files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+SALT).encode()
 for suffix in ('','.ru'):
  notes=(ROOT.parent/'docs'/f'RELEASE_NOTES-{VERSION}{suffix}.md').read_text()
  # The release README is at ZIP root, while its guides are under docs/.
  notes=re.sub(r'\]\((?!https?://|#)([^)]+)\)',r'](docs/\1)',notes)
  files[f'README{suffix}.md']=notes.encode()
 for name in ['USER_GUIDE.md','USER_GUIDE.ru.md','USB_GUIDE.md','USB_GUIDE.ru.md',
              'FX_GALLERY.md','FX_KNOB_LAYOUT.ru.md','AMP_VOICES.ru.md','BUILD.md',
              'RELEASE_NOTES-r30b.md','RELEASE_NOTES-r30b.ru.md']:
  files['docs/'+name]=(ROOT.parent/'docs'/name).read_bytes()
 for name in ['tape-delay','tape','vinyl','shimmer-reverb','delay','diff-delay',
              'spring-reverb','phaser','flanger','optocat','amp-models','bass-amp']:
  files['docs/media/fx/'+name+'.gif']=(ROOT.parent/'docs/media/fx'/(name+'.gif')).read_bytes()
 files['NOTICE.md']=(ROOT.parent/'NOTICE.md').read_bytes()
 for name in ['FX_KNOB_LAYOUT.ru.md','AMP_VOICES.ru.md']:
  files[name]=(ROOT.parent/'docs'/name).read_bytes()
 # Keep bundled guides usable offline; reference other repository documents
 # by the release tag instead of leaving broken relative links in the ZIP.
 for name,data in list(files.items()):
  if name.endswith('.md') and (name.startswith('docs/') or name.startswith('README') or name=='NOTICE.md'):
   def link(match):
    target=match.group(1)
    if re.match(r'\w+://|#',target):return match.group(0)
    path,sep,fragment=target.partition('#')
    resolved=posixpath.normpath(posixpath.join(posixpath.dirname(name),path))
    if resolved in files:return match.group(0)
    return '](https://github.com/AlekseyMalykh/am-mod/blob/'+VERSION+'/'+resolved+(sep+fragment if sep else '')+')'
   files[name]=re.sub(r'\]\(([^)]+)\)',link,data.decode()).encode()
 for name in ['autorun.sh','mod-launcher.sh','usb-mode.sh','usb-drive.sh']:subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+name],check=True)
 out=ROOT/'build'/VERSION;out.mkdir(exist_ok=True)
 target=out/f'am-mod-{VERSION}.zip'
 with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
  for info in infos:z.writestr(info,files[info.filename])
  for name in sorted(files.keys()-original.keys()):
   info=zipfile.ZipInfo(name);info.create_system=3;info.external_attr=0o100755<<16;info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,files[name])
 with zipfile.ZipFile(target) as z:assert z.testzip() is None
 sys.path.insert(0,str(ROOT.parent/'installer'));from mpc_mod_installer import Package
 assert Package.from_file(target).revision==VERSION
 digest=sha(target.read_bytes());(out/'SHA256SUMS.txt').write_text(f'{digest}  {target.name}\n')
 proof=dict(passed=True,revision=VERSION,base_sha256=BASE_SHA,package_sha256=digest,library_sha256=sha(library),
            payload_sha256={k:sha(v) for k,v in files.items() if k.startswith('autorun/')},
            usb_files=True,usb_drive=True,usb_audio_channels=18,default_usb_audio_channels=2,installer_parser=True,
            usb_audio_channel_names=names_compile['channels'],named_usb_descriptors_vm_verified=True,
            fx_ui_layout_verified=True,amp_voices=7,amp_native_audio_verified=True,
            stock_firmware_icons_verified=True,compressor_data_controls_verified=True,compressor_pad_rgb_verified=True,
            input_config_mode_filter_verified=True,input_monitor_verified=True,routing_stop_passthrough_verified=True,
            hardware_tested=False,card_written=False)
 (out/'package-verification.json').write_text(json.dumps(proof,indent=2)+'\n')
 print(target,digest)
if __name__=='__main__':main()
