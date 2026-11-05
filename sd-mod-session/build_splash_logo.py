"""Regenerate src/am_mod_splash.png and .inc: original "Am" lettering with a
"MOD" rebuilt at the same cap height and on the same baseline.

Source: src/am_mod_splash_original.png (the r27 logo). MOD uses Poppins Bold
(SIL Open Font License). Version/repository text is drawn at runtime."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parent
FONT='/usr/share/fonts/truetype/google-fonts/Poppins-Bold.ttf'
src=Image.open(R/'src/am_mod_splash_original.png').convert('RGB');bg=src.getpixel((0,0))
AM=(56,86,158,146)            # box around the original "Am" (x0,y0,x1,y1)
CAP_TOP,BASE=93,139           # top of "A" and baseline of "Am"
AMBER=(255,194,71);GAP=16;S=4
am=src.crop(AM)
f=ImageFont.truetype(FONT,100*S);probe=Image.new('L',(10,10));pd=ImageDraw.Draw(probe)
top=pd.textbbox((0,0),'MOD',font=f,anchor='ls')[1]   # cap height at size 100*S (negative)
size=round(100*S*(BASE-CAP_TOP)*S/(-top))
f=ImageFont.truetype(FONT,size)
box=pd.textbbox((0,0),'MOD',font=f,anchor='ls');mod_w=(box[2]-box[0])/S
am_left=62-AM[0];am_w=151-62+1
total=am_w+GAP+mod_w;start=round((320-total)/2)
im=Image.new('RGB',(320,240),bg)
im.paste(am,(start-am_left,AM[1]))
big=Image.new('RGB',(320*S,240*S),bg);d=ImageDraw.Draw(big)
d.text(((start+am_w+GAP)*S-box[0],BASE*S+S),'MOD',font=f,fill=AMBER,anchor='ls')
mod=big.resize((320,240),Image.LANCZOS)
x0=start+am_w+GAP-2;im.paste(mod.crop((x0,80,320,150)),(x0,80))
im.save(R/'src/am_mod_splash.png')
px=list(im.get_flattened_data() if hasattr(im,'get_flattened_data') else im.getdata());runs=[]
for r,g,b in px:
    v=0xff000000|(r<<16)|(g<<8)|b
    if runs and runs[-1][1]==v:runs[-1][0]+=1
    else:runs.append([1,v])
out=['/* Generated from am_mod_splash.png by build_splash_logo.py; 320 x 240 ARGB runs. */','static const struct { unsigned count, argb; } am_splash_runs[]={']
out+=['{%d,0x%08xu},'%(n,v) for n,v in runs];out.append('};')
(R/'src/am_mod_splash.inc').write_text('\n'.join(out)+'\n');print(len(runs),'runs; MOD size',size/S,'width',mod_w,'start',start)
