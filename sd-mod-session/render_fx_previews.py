"""Render test painter traces for visual QA (substitute font, no native UI capture)."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import sys
D=Path(sys.argv[1] if len(sys.argv)>1 else Path(__file__).resolve().parent/'build/fx-layout')
font_path=next(p for p in [Path('C:/Windows/Fonts/arial.ttf'),Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')] if p.exists())
def render(path):
 im=Image.new('RGB',(320,240));d=ImageDraw.Draw(im)
 for line in path.read_text().splitlines():
  if line.startswith('R '):
   _,x,y,w,h,c=line.split();x,y,w,h=map(int,(x,y,w,h));d.rectangle((x,y,x+w-1,y+h-1),fill='#'+c[-6:])
  elif line.startswith('T '):
   a=line.split(' ',8)
   if len(a)<9:continue
   _,x,y,w,h,size,c,center,txt=a;x,y,w,h,size,center=map(int,(x,y,w,h,size,center));font=ImageFont.truetype(str(font_path),size)
   patch=Image.new('RGBA',(w,h));pd=ImageDraw.Draw(patch);b=pd.textbbox((0,0),txt,font=font)
   tx=(w-(b[2]-b[0]))/2 if center else 0
   pd.text((tx,(h-(b[3]-b[1]))/2-b[1]),txt,font=font,fill='#'+c[-6:]);im.paste(patch,(x,y),patch)
 return im
for path in D.glob('*.trace'):render(path).save(path.with_suffix('.png'))
def gallery(names,target,cols):
 canvas=Image.new('RGB',(320*cols,240*((len(names)+cols-1)//cols)))
 for i,name in enumerate(names):canvas.paste(render(D/(name+'.trace')),(i%cols*320,i//cols*240))
 canvas.resize((canvas.width*2,canvas.height*2),Image.Resampling.NEAREST).save(D/target)
gallery(['amp-voice-'+str(i) for i in range(1,5)],'amp-models.png',2)
gallery(['amp-voice-'+str(i) for i in range(5,8)],'bass-models.png',3)
gallery(['6146446c-0-0','61464444-0-0','61465370-0-0','61465073-0-0','6146466c-0-0','54456678-0-0'],'effect-details.png',2)
gallery(['gallery-54456678-0','gallery-41435658-0','gallery-56456678-0','gallery-54456678-1','gallery-41435658-1','gallery-56456678-1'],'analog-gallery.png',3)
gallery(['gallery-414d4f01-0','gallery-414d4f01-1'],'shimmer-gallery.png',2)
