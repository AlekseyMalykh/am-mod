"""Render the mixer effect screens as animated GIFs for the release notes.
usage: render_fx_gif.py <rfv binary> <catalog index> <out.gif> [param=value ...]
Needs the host harness built from render_fx_view.c (see build_fx_gifs.sh)."""
import os,subprocess,sys,tempfile
from PIL import Image,ImageDraw,ImageFont
F='/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'
def frame(path):
    im=Image.new('RGB',(320,240),'black');d=ImageDraw.Draw(im)
    for line in open(path,encoding='utf-8'):
        if line.startswith('R '):
            _,x,y,w,h,c=line.split();x,y,w,h=map(int,(x,y,w,h));d.rectangle((x,y,x+w-1,y+h-1),fill='#'+c[-6:])
        elif line.startswith('T '):
            _,x,y,w,h,size,c,centre,body=line.rstrip('\n').split(' ',8);x,y,w,h,size,centre=map(int,(x,y,w,h,size,centre))
            f=ImageFont.truetype(F,size)
            while body and f.getlength(body)>w:body=body[:-1]
            tile=Image.new('RGB',(w,h),'black');td=ImageDraw.Draw(tile)
            bb=td.textbbox((0,0),body,font=f);px=(w-f.getlength(body))/2 if centre else 0;py=(h-(bb[3]-bb[1]))/2-bb[1]
            td.text((px,py),body,font=f,fill='#'+c[-6:]);im.paste(tile,(x,y))
    return im.resize((640,480),Image.Resampling.NEAREST)
def main():
    binary,index,out=sys.argv[1:4];frames=int(os.environ.get('FRAMES','48'))
    with tempfile.TemporaryDirectory() as t:
        env=dict(os.environ,ANIM=str(frames))
        subprocess.run([binary,index,'1',os.path.join(t,'f')]+sys.argv[4:],check=True,env=env)
        imgs=[frame(os.path.join(t,'f_%03d.txt'%i)) for i in range(frames)]
    pal=imgs[0].quantize(colors=64,method=Image.Quantize.MEDIANCUT)
    q=[i.quantize(palette=pal,dither=Image.Dither.NONE) for i in imgs]
    q[0].save(out,save_all=True,append_images=q[1:],duration=80,loop=0,optimize=True,disposal=1)
main()
