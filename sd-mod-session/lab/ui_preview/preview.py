"""Build host previews of mod screens from the real paint code.

The source file is copied with FN() redirected to shim.h recorders; unknown
native calls become no-ops. Output PNGs are layout previews only."""
from pathlib import Path
import subprocess,sys,os
sys.path.insert(0,str(Path(__file__).parent))
from render import render
H=Path(__file__).resolve().parent;R=H.parent.parent
OUT=Path(os.environ.get('UI_PREVIEW_OUT',str(H/'out')));OUT.mkdir(exist_ok=True)
W=Path('/tmp/ui_preview');W.mkdir(exist_ok=True)
flags=(H/'flags.txt').read_text().split()
def build(name,source,scenario,extra=()):
    text=(R/source).read_text()
    text=text.replace('#define FN(o,t) ((t)(base+(o)))','#define FN(o,t) ((t)preview_fn((uintptr_t)(o)))')
    text=text.replace('#define FN(o,T) ((T)(image+(o)))','#define FN(o,T) ((T)preview_fn((uintptr_t)(o)))')
    text=text.replace('#define FN(a,t) ((t)(base+(a)))','#define FN(a,t) ((t)preview_fn((uintptr_t)(a)))')
    import re
    text=re.sub(r'#define FN\((\w+),\s*(\w+)\)\s*\(\(\2\)\(\w+\+\(\1\)\)\)',lambda m:'#define FN(%s,%s) ((%s)preview_fn((uintptr_t)(%s)))'%(m[1],m[2],m[2],m[1]),text)
    assert 'preview_fn' in text,source
    (W/(name+'_src.c')).write_text('#include "%s/shim.h"\n#line 1 "%s"\n'%(H,source)+text)
    (W/(name+'.c')).write_text('#include "%s_src.c"\n'%name+(H/scenario).read_text())
    exe=W/name
    subprocess.run(['gcc','-std=gnu11','-D_GNU_SOURCE','-w','-O0','-g','-I',str(R/'src'),'-I',str(R/'build'),*[f for f in flags if f.startswith('-D')],
        str(W/(name+'.c')),*[str(R/e) for e in extra],'-o',str(exe),'-lm','-lpthread','-Wl,--unresolved-symbols=ignore-all'],check=True)
    subprocess.run([str(exe)],cwd=W,check=True)
    pngs=[]
    for ops in sorted(W.glob(name+'-*.ops')):
        png=OUT/(ops.stem+'.png');render(str(ops),str(png));pngs.append(png.name)
    print(name,pngs)
if __name__=='__main__':
    build('settings','src/music_ui.c','scenario_settings.c',['src/music_settings.c'])
    build('rear','src/rear_ui.c','scenario_rear.c')
    build('search','src/workflow_ui.c','scenario_search.c',['src/phone_input.c'])
