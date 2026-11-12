"""Render a render_fx_view trace to PNG (3x). usage: render_fx_view.py trace.txt out.png"""
import sys
from PIL import Image, ImageDraw, ImageFont
F='/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'
im=Image.new('RGB',(320,240),'black');d=ImageDraw.Draw(im)
for line in open(sys.argv[1],encoding='utf-8'):
    if line.startswith('R '):
        _,x,y,w,h,c=line.split();x,y,w,h=map(int,(x,y,w,h));d.rectangle((x,y,x+w-1,y+h-1),fill='#'+c[-6:])
    elif line.startswith('T '):
        _,x,y,w,h,size,c,centre,body=line.rstrip('\n').split(' ',8);x,y,w,h,size,centre=map(int,(x,y,w,h,size,centre))
        f=ImageFont.truetype(F,size)
        while body and f.getlength(body)>w:body=body[:-1]
        tile=Image.new('L',(w,h),0);td=ImageDraw.Draw(tile)
        bb=td.textbbox((0,0),body,font=f);px=(w-f.getlength(body))/2 if centre else 0;py=(h-(bb[3]-bb[1]))/2-bb[1]
        td.text((px,py),body,font=f,fill=255);im.paste(Image.new('RGB',(w,h),'#'+c[-6:]),(x,y),tile)
im.resize((960,720),Image.Resampling.NEAREST).save(sys.argv[2])
