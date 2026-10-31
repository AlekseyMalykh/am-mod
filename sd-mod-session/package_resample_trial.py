"""Package the verified experimental library over the frozen r16d launcher.
Never invokes build.py, alters production autorun, or writes a memory card.
Run in WSL after both verification variants pass for these exact sources.
"""
from pathlib import Path,PurePosixPath
from hashlib import sha256
import json,re,subprocess,zipfile,sys
R=Path(__file__).resolve().parent
fx_fix='--r18a' in sys.argv
graphic_fx=fx_fix or '--r18' in sys.argv
selection_fix=graphic_fx or '--r17d' in sys.argv
exit_fix=selection_fix or '--r17c' in sys.argv
lights=exit_fix or '--r17b' in sys.argv
update=lights or '--r17a' in sys.argv
revision='r18a-fx-columns-context' if fx_fix else 'r18-graphic-eq-native-fx' if graphic_fx else 'r17d-resample-selection-input' if selection_fix else 'r17c-levels-exit' if exit_fix else 'r17b-picker-lights' if lights else 'r17a-browser-keyboard-steps' if update else 'r17-resample-trial'
OUT=R/('build/r18a-trial' if fx_fix else 'build/r18-trial' if graphic_fx else 'build/r17d-trial' if selection_fix else 'build/r17c-trial' if exit_fix else 'build/r17b-trial' if lights else 'build/r17a-trial' if update else 'build/resample-trial');OUT.mkdir(exist_ok=True)
sha=lambda b:sha256(b).hexdigest()
info=json.loads((R/'build-info.json').read_text())
assert info['revision']=='r16d-sample-input-preview'
basezip=R.parent/info['zip'];assert sha(basezip.read_bytes())==info['zip_sha256']=='8bbdcf239235fec7592b8f4614a9b3ca6b79260f406b0d0148aaff1e4aa19b8d'
protected={p:sha(p.read_bytes()) for p in [R/'build-info.json',basezip,*sorted(p for p in (R/'autorun').rglob('*') if p.is_file())]}
reports=[]
if update:
 for folder in ['r17a','r17a-ubsan']:
  checks=json.loads((R/'build'/folder/'verification.json').read_text());assert checks['passed']
  for name,digest in checks['sources_sha256'].items():assert sha((R/name).read_bytes())==digest,name
for folder in ['resample-device','resample-device-ubsan']:
 report=json.loads((R/'build'/folder/'verification.json').read_text());reports.append(report)
 assert report['passed'] and report['production_unchanged'] and report['limited_device_trial_ready']
 for name,digest in report['sources_sha256'].items():assert sha((R/name).read_bytes())==digest,name
if graphic_fx:
 core=json.loads((R/'build/fx-verification.json').read_text());assert all(check['passed'] for check in core['checks'])
 for name,digest in core['sources_sha256'].items():assert sha((R/name).read_bytes())==digest,name
 for folder in ['modern-fx','modern-fx-ubsan']:
  checks=json.loads((R/'build'/folder/'verification.json').read_text());assert checks['passed']
  for name,digest in checks['sources_sha256'].items():assert sha((R/name).read_bytes())==digest,name
library=(R/'build/resample-device/phone-input-resample.so').read_bytes();assert sha(library)==reports[0]['library_sha256']
with zipfile.ZipFile(basezip) as archive:
 assert archive.testzip() is None
 files={name:archive.read(name) for name in archive.namelist() if not name.endswith('/')}
for name in files:
 p=PurePosixPath(name);assert not p.is_absolute() and '..' not in p.parts
for name,digest in info['payload_sha256'].items():assert sha(files['autorun/'+name])==digest
old={n:b for n,b in files.items() if n.startswith('autorun/')}
files['autorun/phone-input.so']=library
controller=files['autorun/mod-launcher.sh'].decode()
assert controller.count('SAVE_IO_SHA='+sha(old['autorun/phone-input.so']))==1
controller=controller.replace('SAVE_IO_SHA='+sha(old['autorun/phone-input.so']),'SAVE_IO_SHA='+sha(library))
files['autorun/mod-launcher.sh']=controller.encode()
entry=files['autorun/autorun.sh'].decode()
assert entry.count(sha(old['autorun/mod-launcher.sh']))==1
entry=entry.replace(sha(old['autorun/mod-launcher.sh']),sha(files['autorun/mod-launcher.sh'])).replace('revision=r16d-sample-input-preview','revision='+revision)
files['autorun/autorun.sh']=entry.encode()
manifest=json.loads(files['autorun/autorun.json'])
script=manifest['value0']['scripts'][0]
script['message']='Start r17 resample trial? Sample: Shift+pad11 > Sample. First test: short sample > Render > To Pad, free destination > Apply > Undo. Close Resample with Cancel before Project > Mod Off. Insert after stock loads.'
script['yesButtonText']='Start r17'
if update:
 script['message']='Start r17a? Browser folder reveal and Storage search, pad typing cursor/Backspace/Delete, Steps Shift+K3 Other Sounds and encoder cursor toggle. Resample entry: Sample Shift+pad11. Insert after stock loads.'
 script['yesButtonText']='Start r17a'
if lights:
 script['message']='Start r17b? Lights colours in Tune event editor from Events and Steps. Original/selected note markers preserved. Resample capture errors now report the exact failed check. Insert after stock loads.'
 script['yesButtonText']='Start r17b'
if exit_fix:
 script['message']='Start r17c? Live 16 Levels state: leaving Tune or Chop restores source-pad playback lights. Includes r17b Lights colours in event editors. Insert only after stock loads.'
 script['yesButtonText']='Start r17c'
if selection_fix:
 script['message']='Start r17d? Resample uses native selected pad. Naming owns Shift and navigation; Pads +/- move cursor. Lights Scale is last. Insert after stock loads. First resample trial: short source, empty FX, free destination.'
 script['yesButtonText']='Start r17d'
if graphic_fx:
 script['message']='Start r18? Resample: Sample Shift+pad11 > Sample > FX. Graphic EQ with addable points and stock AIR/MPC effects, max5. First trial: short source, free destination, Render/Apply/Undo. Insert after stock loads.'
 script['yesButtonText']='Start r18'
if fx_fix:
 script['message']='Start r18a? FX parameters in K1/K2/K3 columns; Shift+B2/B3 move in chain and native editor. Context capture fix. Original/FX audition and normal Sample/FX Select EQ still pending. Insert after stock loads.'
 script['yesButtonText']='Start r18a'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
files['README_RU.md']=(R/('R18A_RU.md' if fx_fix else 'R18_RU.md' if graphic_fx else 'R17D_RU.md' if selection_fix else 'R17C_RU.md' if exit_fix else 'R17B_RU.md' if lights else 'R17A_RU.md' if update else 'R17_RU.md')).read_bytes()
files['R17_RU.md']=(R/'R17_RU.md').read_bytes()
if update:files['R17A_RU.md']=(R/'R17A_RU.md').read_bytes()
if lights:files['R17B_RU.md']=(R/'R17B_RU.md').read_bytes()
if exit_fix:files['R17C_RU.md']=(R/'R17C_RU.md').read_bytes()
if selection_fix:files['R17D_RU.md']=(R/'R17D_RU.md').read_bytes()
if graphic_fx:files['R18_RU.md']=(R/'R18_RU.md').read_bytes()
if fx_fix:files['R18A_RU.md']=(R/'R18A_RU.md').read_bytes()
files['RESAMPLE_UI_RU.md']=(R/'RESAMPLE_UI_RU.md').read_bytes()
allowed={'autorun/'+s for s in ['phone-input.so','mod-launcher.sh','autorun.sh','autorun.json','autorun.check']}
assert {name for name in old if old[name]!=files[name]}==allowed
# All ARM patches, RAM patch helper, mount/storage and recovery policy remain
# identical to frozen r16d. Only the new library and its integrity chain differ.
payload=OUT/'autorun';payload.mkdir(exist_ok=True)
for name,body in files.items():
 if name.startswith('autorun/'):
  path=OUT/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(body)
qemu=['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808']
for name in ['autorun.sh','mod-launcher.sh']:
 subprocess.run([*qemu,'/home/user/mpc-rootfs-user-20260808/usr/bin/busybox.nosuid','sh','-n',str(payload/name)],check=True)
assert re.search(r'SAVE_IO_SHA=([0-9a-f]{64})',controller)[1]==sha(library)
archive=R.parent/('mpc-sample-r18a-fx-columns-context.zip' if fx_fix else 'mpc-sample-r18-graphic-eq-native-fx.zip' if graphic_fx else 'mpc-sample-r17d-resample-selection-input.zip' if selection_fix else 'mpc-sample-r17c-levels-exit.zip' if exit_fix else 'mpc-sample-r17b-picker-lights.zip' if lights else 'mpc-sample-r17a-browser-keyboard-steps.zip' if update else 'mpc-sample-resample-r17-trial.zip')
if graphic_fx:assert not archive.exists(),'Frozen trial archive already exists'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
 for name,body in sorted(files.items()):
  zi=zipfile.ZipInfo(name,date_time=(2026,10,26,0,0,0));zi.compress_type=zipfile.ZIP_DEFLATED;zi.external_attr=0o100644<<16;z.writestr(zi,body)
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None
 assert all(z.read(name)==body for name,body in files.items())
assert protected=={p:sha(p.read_bytes()) for p in protected}
trial=dict(info)
trial.update(revision=revision,zip=archive.name,zip_sha256=sha(archive.read_bytes()),hardware_tested=False,limited_device_trial_ready=True,production_base_revision=info['revision'],source_commit=subprocess.check_output(['git','-c','safe.directory='+str(R.parent),'rev-parse','HEAD'],cwd=R,text=True).strip(),payload_sha256={name[8:]:sha(body) for name,body in files.items() if name.startswith('autorun/')})
if update:trial['r17a']={'browser':'real native root setter plus asynchronous result-row selection; K3 query Storage, result Preview Volume','phone':'Pads encoder cursor; display-only caret; insert, Backspace, Shift Delete','steps':'Shift K3 Other Sounds; encoder click toggles edit cursor; playhead unchanged','resample_entry':'ownership rather than broad Chop-observer activity'}
if lights:trial['r17b']={'picker_lights':'Tune editor uses own source Lights, project/local tonic and scale; exact event semitone does not reapply Transpose16; original/chosen markers override colours; no Sample published snapshot change','capture_diagnostics':'exact owner capture stage on screen and in log; not a confirmed hardware fix'}
if exit_fix:trial['r17c']={'playback_lights':'UI Timer samples live properties through protected native layer lifetime, independently of refresh callbacks; off/invalid reference clears cached state; Tune/Chop exit clears lingering note flashes, outer Sample uses source pads','hardware_verified':False}
if selection_fix:trial['r17d']={'resample_selection':'CurrentThings5f8 selected pad -> native Program getter; inverse and parent checks retained; unrelated60 never treated as Instrument; selected index rechecked at end of capture','naming':'exclusive modal FIFO routing; Pads +/- encoder pad1/pad3 share cursor, Shift+pad16 Delete','lights_order':'Monochrome Piano Tonic Degrees Scale; stored enum IDs unchanged','hardware_verified':False}
if graphic_fx:trial['r18']={'graphic_eq':'eight addable/removable points, actual response curve, independent stage state and band/global bypass','native_fx':'25 real AIR/MPC processors; max5 mixed stages; full32 parameter storage; selected stage edit/replace/remove/reorder','excluded':'Vintage global bank callback; ACVX mismatched sample rate fails closed','hardware_verified':False}
if fx_fix:trial['r18a']={'parameters':'three knob-aligned columns, pages retained','move':'Shift+B2/B3 in chain and native parameter editor; bounded occupied/first-empty selection and visible boundary feedback','context':'PluginManager6da1f90; native registry/playhead path tested with fixture services; actual device registry availability unverified','pending':['Original/FX audition','individual Filter third EQ tab and audio','native FX Select Graphic EQ and audio']}
trial['phone_input']=dict(info['phone_input'],library_sha256=sha(library))
trial['startup_logging']=dict(info['startup_logging'],controller_sha256=sha(files['autorun/mod-launcher.sh']))
trial['resample']={'scope':'raw Trim PCM plus up to five mixed graphic EQ and stock native FX; no automatic source Tune/envelope/filter bake or live mix comparison' if graphic_fx else 'raw Trim PCM plus up to five EQ stages; no native voice/FX bake or live mix comparison','shortcut':'Shift+pad11 from Sample','first_trial':'short unprocessed source, free destination, play, Undo/Redo, Cancel before Mod Off','installed_firmware_written':False,'hardware_audio_and_Save_reopen_verified':False,'normal_exit':'UI Cancel -> owner acknowledgement -> runtime destruction -> existing Project Mod Off process exit','forced_context_exit':'stop queue repost, native tagged cancellation, orphan retained until process exit; no dlclose guarantee'}
(OUT/'build-info.json').write_text(json.dumps(trial,indent=2)+'\n')
print(json.dumps({key:trial[key] for key in ['revision','zip','zip_sha256','hardware_tested','limited_device_trial_ready']},indent=2))
