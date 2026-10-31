"""Render the prototype's actual320x240 drawing trace for visual review."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
R=Path(__file__).resolve().parent
OUT=R/'work/resample';OUT.mkdir(parents=True,exist_ok=True)
font=Path('C:/Windows/Fonts/consola.ttf')
assert font.exists()
for suffix,label in [('', 'chooser'),('.sample','sample'),('.ready','ready'),('.fx','fx'),('.rendering','rendering'),('.confirm','confirm'),('.error','error')]:
 p=R/'build/resample-flow'/('choice-draw.txt'+suffix)
 image=Image.new('RGB',(320,240),'black');draw=ImageDraw.Draw(image)
 for line in p.read_text(encoding='utf-8').splitlines():
  if line.startswith('R '):
   _,x,y,w,h,c=line.split();x,y,w,h=map(int,(x,y,w,h));colour='#'+c[-6:]
   if w and h:draw.rectangle((x,y,x+w-1,y+h-1),fill=colour)
  elif line.startswith('T '):
   _,x,y,w,h,size,c,centre,body=line.split(' ',8);x,y,w,h,size,centre=map(int,(x,y,w,h,size,centre))
   f=ImageFont.truetype(str(font),size)
   while body and f.getlength(body)>w:
    body=body[:-2]+'…' if not body.endswith('…') else body[:-2]+'…'
   tile=Image.new('RGB',(w,h),'black');d=ImageDraw.Draw(tile)
   bbox=d.textbbox((0,0),body,font=f);px=(w-f.getlength(body))/2 if centre else 0;py=(h-(bbox[3]-bbox[1]))/2-bbox[1]
   d.text((px,py),body,font=f,fill='#'+c[-6:]);image.paste(tile,(x,y))
 image.resize((960,720),Image.Resampling.NEAREST).save(OUT/('resample-'+label+'.png'))
print(str(OUT/'resample-chooser.png'))
