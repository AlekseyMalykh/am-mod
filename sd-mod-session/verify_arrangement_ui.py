"""Production SongScreen Arrange overlay under ARM QEMU; no app/package build."""
from pathlib import Path
import argparse,ast,hashlib,json,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--stock',required=True);a=p.parse_args()
R=Path(__file__).resolve().parent;O=R/'build/arrangement-ui';O.mkdir(parents=True,exist_ok=True)
stock=Path(a.stock);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
core=['src/pattern_tools.cc','src/arrangement.cc','src/arrangement_codec.cc','src/pattern_model.cc','src/pattern_library.cc','src/pattern_codec.cc','src/piano_roll.c','src/midi_event.c']
tracked=core+['src/song_ui.c','src/song_ui.h','src/arrangement_ui.inc','src/pattern_tools.h','src/arrangement.h','src/pattern_model.h','src/pattern_library.h','src/piano_roll.h','src/step_native.h','src/midi_event.h','src/phone_events.h','src/music_settings.h','src/startup_project.h','src/mixer_stems.h','src/phone_native.c','build_mixer_r27r.py','build/phone_hooks.h','check_arrangement_ui.c','check_song_ui.c','check_fx_native.c','verify_arrangement_ui.py']
digest=lambda:{s:sha(R/s) for s in tracked};before=digest();proof=O/'verification.json';proof.unlink(missing_ok=True)
sysroot=os.environ.get('MPC_QEMU_SYSROOT','/home/user/mpc-rootfs-user-20260808');checks=[]
def run(args):subprocess.run(args,cwd=R,check=True)
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    objects=[]
    for source in core+['check_arrangement_ui.c']:
        cpp=source.endswith('.cc');obj=O/(name+'-'+Path(source).name+'.o');objects.append(str(obj))
        run(['aarch64-linux-gnu-g++' if cpp else 'aarch64-linux-gnu-gcc','-std=c++11' if cpp else '-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'-c',source,'-o',str(obj)])
    target=O/name;run(['aarch64-linux-gnu-g++',*flags,*objects,'-pthread','-ldl','-lm','-o',str(target)])
    result=subprocess.run(['qemu-aarch64','-L',sysroot,str(target),str(stock),str(O/(name+'-draw.txt'))],cwd=R,capture_output=True,text=True,timeout=90)
    (O/(name+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,flush=True);result.check_returncode();assert 'PASS Arrange UI:' in result.stdout;checks.append(name)
# Baseline Song hooks retain the existing suite with the trial flag absent.
for name,flags in [('baseline',[]),('baseline-ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
    target=O/name;run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'check_song_ui.c','-pthread','-ldl','-lm','-o',str(target)])
    result=subprocess.run(['qemu-aarch64','-L',sysroot,str(target),str(stock)],cwd=R,capture_output=True,text=True,timeout=90)
    (O/(name+'.log')).write_text(result.stdout+result.stderr);result.check_returncode();assert 'PASS Song UI:' in result.stdout;checks.append(name)
tree=ast.parse((R/'build_mixer_r27r.py').read_text());common=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='common' for t in n.targets))
for flags in [[],['-DMPC_PATTERN_TOOLS_TRIAL=1'],['-DMPC_PATTERN_TOOLS_TRIAL=1','-DMPC_MIDI_TOOLS=1']]:
    run(['aarch64-linux-gnu-gcc','-std=c11',*common,*flags,'-Ibuild','-fsyntax-only','src/song_ui.c'])
assert before==digest()
proof.write_text(json.dumps(dict(passed=True,checks=checks,sources_sha256=before,application_built=False,hardware_tested=False,arrangement_playback_connected=False,pattern_drill_down_connected=True,editor_handoff='Song fixture here; real editor with poisoned Events pointers in verify_pattern_tools.py',graphics='production layout; recorded native graphics fixtures, substituted font'),indent=2)+'\n')
