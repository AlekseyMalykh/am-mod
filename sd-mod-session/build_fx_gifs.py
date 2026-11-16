"""Rebuild documentation GIFs from the production C painter, without firmware.

Linux/WSL: Python + Pillow, gcc and DejaVu Sans. These animations use synthetic
meter/PCM data and a substitute font; they are not recordings from an MPC.
"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
R=Path(__file__).resolve().parent
out=R/'build/fx-gifs';out.mkdir(parents=True,exist_ok=True)
media=R.parent/'docs/media/fx';media.mkdir(parents=True,exist_ok=True)
sources=['render_fx_view.c','src/mixer_view.c','src/mixer_model.c','src/mixer_meters.c',
 'src/mixer_buses.c','src/fx_catalog.c','src/fx_eq.c','src/fx_chain.c','src/fx_own.c',
 'src/compressor_dsp.c','src/compressor_animation.c']
binary=out/'render-fx'
subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror',*sources,
 '-lm','-pthread','-o',str(binary)],cwd=R,check=True)
# Catalog indexes are resolved from a tiny executable, not duplicated tables.
catalog=out/'catalog.c'
catalog.write_text('#include "fx_catalog.h"\n#include <stdio.h>\nint main(void){'
 'for(unsigned i=0;i<fx_mixer_catalog_count();++i)printf("%u|%s\\n",i,fx_mixer_catalog_at(i)->name);}\n')
subprocess.run(['gcc','-std=c11','-O2','-Isrc',str(catalog),'src/fx_catalog.c',
 'src/fx_chain.c','src/fx_eq.c','src/fx_own.c','src/compressor_dsp.c','-lm',
 '-o',str(out/'catalog')],cwd=R,check=True)
indexes={name:int(i) for i,name in (line.split('|',1) for line in
 subprocess.check_output([str(out/'catalog')],text=True).splitlines())}
effects={'tape-delay':'Tape Delay','tape':'Tape','vinyl':'Vinyl',
 'shimmer-reverb':'Shimmer Reverb','delay':'Delay','diff-delay':'Diff Delay',
 'spring-reverb':'Spring Reverb','phaser':'Phaser','flanger':'Flanger',
 'optocat':'OptoCat Comp','amp-models':'Amp Sim','bass-amp':'Amp Sim'}
for name,effect in effects.items():
 env=dict(os.environ,FRAMES='40')
 if name=='amp-models':env.update(AMP_CYCLE='1',FRAMES='64')
 if name=='bass-amp':env.update(BASS_CYCLE='1',FRAMES='48')
 subprocess.run([sys.executable,'render_fx_gif.py',str(binary),str(indexes[effect]),
  str(media/(name+'.gif'))],cwd=R,env=env,check=True)
 print('Rendered',name,flush=True)
tracked=sources+['src/fx_visual.inc','src/fx_layout.inc','src/fx_amp_models.inc',
 'render_fx_gif.py','build_fx_gifs.py']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
(out/'verification.json').write_text(json.dumps(dict(
 passed=True,synthetic_signal=True,device_recording=False,
 sources_sha256={p:sha(R/p) for p in tracked},
 gifs_sha256={name+'.gif':sha(media/(name+'.gif')) for name in effects}),indent=2)+'\n')
