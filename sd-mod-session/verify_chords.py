"""Reproducible ARM fixtures for Chords; no hardware or live audio claim."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent
O=R/'build/chords';O.mkdir(exist_ok=True)
stock=R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(stock)=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
sources=['src/chords.c','src/chords.h','src/chords_ui.c','src/chords_ui.h',
 'src/poly_record.c','src/poly_record.h','src/phone_native.c','src/music_ui.c',
 'src/music_ui.h','src/note_native.c','src/note_native.h','src/music_settings.c',
 'src/music_settings.h','check_chords.c','check_chords_arp.c','check_chords_native.c',
 'check_chords_ui.c','check_chords_entry.c','check_chords_poly.c','src/workflow_ui.c','src/workflow_ui.h',
 'src/resample_ui.h','src/resample_simple_ui.cc','check_poly_record.c','check_fx_native.c','verify_chords.py']
hashes={s:sha(R/s) for s in sources}
cases=[('model',['check_chords.c','src/chords.c']),
       ('arp',['check_chords_arp.c','src/chords.c']),
       ('native',['check_chords_native.c','src/chords.c']),
       ('poly',['check_chords_poly.c']),
       ('ui',['check_chords_ui.c','src/chords.c','src/chords_ui.c','src/music_settings.c']),
       ('entry',['check_chords_entry.c','src/chords.c','src/chords_ui.c','src/music_settings.c'])]
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 for case,inputs in cases:
  target=O/(name+'-'+case)
  subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-std=c11','-Wall','-Wextra','-Werror',
   '-mbranch-protection=standard','-DMPC_RESAMPLE_TRIAL=1','-ffunction-sections','-fdata-sections',
   '-Wl,--gc-sections',*flags,*inputs,'-pthread','-ldl','-lm','-o',str(target)],cwd=R,check=True)
  command=['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target)]
  if case not in ('model','entry','arp'):command.append(str(stock))
  if case=='ui':command.append(str(O/'ui-preview.svg'))
  p=subprocess.run(command,cwd=R,text=True,capture_output=True,timeout=45)
  (O/(name+'-'+case+'.log')).write_text(p.stdout+p.stderr)
  print(p.stdout+p.stderr,end='',flush=True);p.check_returncode()
  assert 'PASS ' in p.stdout and 'runtime error:' not in p.stderr
assert hashes=={s:sha(R/s) for s in sources}
(O/'verification.json').write_text(json.dumps(dict(passed=True,ubsan=True,
 sources_sha256=hashes,stock_sha256=sha(stock),hardware_tested=False,
 full_vm_verified=False,card_written=False,
 scope='Native ARM physical MIDI down/release; pitch-specific Poly finalization; actual observer layout reproduces r24b crash; native unsigned Play parameter validation/queue captured. Model and production UI; no main/audio execution in these fixtures.'),indent=2)+'\n')
