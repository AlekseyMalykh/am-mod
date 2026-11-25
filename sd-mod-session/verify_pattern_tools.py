"""Pattern library/editor and arrangement Store checks; isolated tests only."""
from pathlib import Path
import argparse,ast,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--stock',required=True);p.add_argument('--ui-only',action='store_true');a=p.parse_args()
R=Path(__file__).resolve().parent;O=R/'build/pattern-tools';O.mkdir(exist_ok=True)
stock=Path(a.stock);assert hashlib.sha256(stock.read_bytes()).hexdigest()=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
core=['src/pattern_model.cc','src/pattern_library.cc','src/pattern_codec.cc','src/pattern_tools.cc','src/arrangement.cc','src/arrangement_codec.cc','src/arrangement_native_plan.cc','src/midi_event.c','src/piano_roll.c']
tracked=core+['src/pattern_model.h','src/pattern_library.h','src/pattern_tools.h','src/pattern_roll.inc','src/piano_roll.h','src/piano_roll_native.inc','src/step_native.c','src/phone_native.c','build_mixer_r27r.py','check_pattern_tools.cc','check_arrangement_native_choices.cc','check_pattern_ui_native.c','check_piano_roll_native.c','check_steps.c','check_steps_support.cc','check_steps_support.S','src/step_abi.S','src/step_exceptions.cc','src/step_mute.cc','verify_pattern_tools.py']
tracked+=['src/arrangement.h','src/arrangement_native_plan.h','src/pattern_host.inc','src/midi_event.h','src/step_native.h','src/song_ui.h','src/phone_events.h','src/music_settings.c','src/music_settings.h','src/seq_mutes.c','src/seq_mutes.h','src/shift_badge.h','src/usb_drive_lights.h','src/ui_caption.h','build/phone_hooks.h']
tracked+=['src/ui_list_geometry.h']
tracked+=['check_pattern_rgb.c','check_pad_colour_rgb.c','src/phone_input.c','src/mixer_leds.h','src/mixer_control.h']
digest=lambda:{s:hashlib.sha256((R/s).read_bytes()).hexdigest() for s in tracked}
before=digest();proof=O/'verification.json';proof.unlink(missing_ok=True);checks=[]
sysroot=os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808')
def run(args,**kw):return subprocess.run(args,cwd=R,check=True,**kw)
def compile(source,name,prefix,flags):
    cpp=source.endswith('.cc');out=O/(name+'-'+Path(source).name+'.o')
    run([prefix+('g++' if cpp else 'gcc'),'-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-Ibuild','-c',source,'-o',str(out)])
    return str(out)
def execute(target,args,runner,label):
    result=subprocess.run([*runner,str(target),*args],cwd=R,text=True,capture_output=True,timeout=90)
    (O/(target.name+'.log')).write_text(result.stdout+result.stderr);print(target.name,result.stdout+result.stderr,flush=True);result.check_returncode();assert label in result.stdout
    if label=='PASS pattern tools:':assert 'PASS arrangement store:' in result.stdout and 'PASS direct creation Store:' in result.stdout
    if label=='PASS pattern UI:':assert 'PASS pattern host:' in result.stdout and 'PASS direct unassigned pattern:' in result.stdout and 'PASS row extraction host:' in result.stdout
    checks.append(target.name)
for name,prefix,flags,runner in ([] if a.ui_only else [('host','',[],[]),('host-asan','',['-fsanitize=address,undefined','-fno-sanitize-recover=all'],[]),('arm','aarch64-linux-gnu-',[],['qemu-aarch64','-L',sysroot]),('arm-ubsan','aarch64-linux-gnu-',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'],['qemu-aarch64','-L',sysroot])]):
    objects=[compile(s,name,prefix,flags) for s in core+['check_pattern_tools.cc']]
    target=O/name;run([prefix+'g++',*flags,*objects,'-pthread','-o',str(target)]);execute(target,[],runner,'PASS pattern tools:')
for name,extra in [('ui',[]),('ui-ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    flags=['-DMPC_PATTERN_TOOLS_TRIAL=1','-mbranch-protection=standard','-ffunction-sections','-fdata-sections',*extra]
    sources=core+['src/seq_mutes.c','check_steps_support.cc','src/step_exceptions.cc','src/step_mute.cc','check_pattern_ui_native.c','check_arrangement_native_choices.cc','check_steps_support.S','src/step_abi.S']
    objects=[compile(s,name,'aarch64-linux-gnu-',flags) for s in sources]
    target=O/name;run(['aarch64-linux-gnu-g++',*extra,*objects,'-Wl,--gc-sections','-ldl','-pthread','-lm','-o',str(target)])
    execute(target,[str(stock),str(O/(name+'-draw.txt'))],['qemu-aarch64','-L',sysroot],'PASS pattern UI:')
# Parse build flags as data: never import/execute a build script.
for name,extra in [('pattern-rgb',[]),('pattern-rgb-ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    target=O/name
    run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Ibuild','-mbranch-protection=standard','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*extra,'check_pattern_rgb.c','src/phone_input.c','src/music_settings.c','-pthread','-ldl','-lm','-o',str(target)])
    execute(target,[],['qemu-aarch64','-L',sysroot],'PASS pattern RGB:')
tree=ast.parse((R/'build_mixer_r27r.py').read_text());common=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='common' for t in n.targets))
for flags in [[],['-DMPC_PATTERN_TOOLS_TRIAL=1'],['-DMPC_PATTERN_TOOLS_TRIAL=1','-DMPC_PAD_MIDI_RECORDING_TRIAL=1','-DMPC_SYNTH_NATIVE_PLAYBACK_TRIAL=1','-DMPC_PATTERN_SOURCE_TRIAL=1']]:
    for source in ['src/step_native.c','src/phone_native.c']:run(['aarch64-linux-gnu-gcc','-std=c11',*common,'-DMPC_MIDI_EVENTS=1',*flags,'-c',source,'-o',str(O/('production-'+str(len(flags))+'-'+Path(source).stem+'.o'))])
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,application_built=False,hardware_tested=False,native_live_reader_enabled=False,arrangement_ui_checked_here=False,pattern_host_checked=True,arrangement_playback_connected=False,workflow='existing Piano Roll value snapshot -> parts/library -> existing Piano Roll -> native clipboard paste; shared arrangement Store; same editor in value-only external host with poisoned native pointers',archive='optional amPatternsV1 + amArrangementV1; native frame/writer fixtures; compressed XPJ/device roundtrip pending'),indent=2)+'\n')
