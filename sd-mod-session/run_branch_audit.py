from pathlib import Path
import json,os,subprocess,sys,time
R=Path(__file__).resolve().parent;O=R/'build/branch-audit';O.mkdir(exist_ok=True)
groups={
 'audio':[['verify_modern_fx.py'],['verify_compressor.py'],['verify_audio_clicks.py'],['verify_mixer_control.py'],['verify_mixer_fx_policy.py']],
 'features':[['verify_midi_tools.py'],['verify_midi_event.py'],['verify_midi_file.py'],['verify_midi_archive.py'],['verify_program_pgm.py'],['verify_program_manifest.py'],['verify_program_import_native.py','--eq']],
 'device':[['verify_usb.py'],['verify_usb_drive.py'],['verify_screen_mirror.py'],['verify_pad_lighting.py'],['verify_mixer_step1.py'],['verify_pad_colour_rgb.py']]}
group=sys.argv[1];results=[];env=dict(os.environ,GIT_DIR='/mnt/c/Users/Алексей/Desktop/akai mpc sample/.git/worktrees/am-mod-illustrated-fx-relink',GIT_WORK_TREE=str(R.parent))
for command in groups[group]:
 name=Path(command[0]).stem;start=time.monotonic()
 with (O/(name+'.log')).open('w') as log:
  p=subprocess.run([sys.executable,*command],cwd=R,env=env,stdout=log,stderr=subprocess.STDOUT)
 results.append(dict(command=command,passed=p.returncode==0,seconds=round(time.monotonic()-start,1)))
 (O/(group+'.json')).write_text(json.dumps(dict(passed=all(r['passed'] for r in results),checks=results),indent=2)+'\n')
 print(group,name,p.returncode,flush=True)
sys.exit(0 if all(r['passed'] for r in results) else 1)
