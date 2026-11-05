"""Render recorded preview ops (R/T lines) to 320x240 PNG, scaled x2."""
import sys
from PIL import Image,ImageDraw,ImageFont
FONT='/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'
def render(ops_path,png_path,scale=2):
    im=Image.new('RGB',(320*scale,240*scale),'black');d=ImageDraw.Draw(im)
    for line in open(ops_path,encoding='utf-8',errors='replace'):
        p=line.rstrip('\n').split(' ',8)
        if p[0]=='R':
            x,y,w,h=map(int,p[1:5]);c=int(p[5],16)
            if w>0 and h>0:d.rectangle([x*scale,y*scale,(x+w)*scale-1,(y+h)*scale-1],fill=((c>>16)&255,(c>>8)&255,c&255))
        elif p[0]=='T' and len(p)>=8:
            x,y,w,h,al=map(int,p[1:6]);size=float(p[6]);c=int(p[7],16);t=p[8] if len(p)>8 else ''
            if not t:continue
            f=ImageFont.truetype(FONT,max(1,int(size*0.82*scale)))
            tw=d.textlength(t,font=f)
            if tw>w*scale:
                # squeeze to 75%, then ellipsis
                if tw*0.75>w*scale:
                    while t and d.textlength(t+'...',font=f)*0.75>w*scale:t=t[:-1]
                    t+='...';tw=d.textlength(t,font=f)
                sx=min(1,w*scale/tw)
            else:sx=1
            asc,desc=f.getmetrics();th=asc+desc
            ty=y*scale+(h*scale-th)/2
            if al&8:ty=y*scale
            if al&16:ty=(y+h)*scale-th
            img=Image.new('RGBA',(int(tw)+4,th+4),(0,0,0,0));ImageDraw.Draw(img).text((0,0),t,font=f,fill=((c>>16)&255,(c>>8)&255,c&255,255))
            if sx<1:img=img.resize((max(1,int(img.width*sx)),img.height),Image.LANCZOS)
            vw=img.width-4*sx
            tx=x*scale if al&1 else (x+w)*scale-vw if al&2 else x*scale+(w*scale-vw)/2
            im.paste(img,(int(tx),int(ty)),img)
    im.save(png_path)
if __name__=='__main__':render(sys.argv[1],sys.argv[2])
