"""Render actual experimental EQ painter trace; approximate desktop font."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parent
trace=R/'build/resample-device/eq-draw.txt'
out=R/'work/resample';out.mkdir(parents=True,exist_ok=True)
image=Image.new('RGB',(320,240),'black');draw=ImageDraw.Draw(image)
font=Path('C:/Windows/Fonts/consola.ttf')
for line in trace.read_text().splitlines():
 if line.startswith('R '):
  _,x,y,w,h,c=line.split();x,y,w,h=map(int,(x,y,w,h))
  draw.rectangle((x,y,x+w-1,y+h-1),fill='#'+c[-6:])
 elif line.startswith('T '):
  _,x,y,w,h,size,c,centre,body=line.split(' ',8);x,y,w,h,size,centre=map(int,(x,y,w,h,size,centre))
  f=ImageFont.truetype(str(font),size)
  while body and f.getlength(body)>w:body=body[:-2]+'…'
  tile=Image.new('RGB',(w,h),'black');d=ImageDraw.Draw(tile);bbox=d.textbbox((0,0),body,font=f)
  d.text(((w-f.getlength(body))/2 if centre else 0,(h-bbox[3]+bbox[1])/2-bbox[1]),body,font=f,fill='#'+c[-6:]);image.paste(tile,(x,y))
path=out/'resample-device-eq.png';image.resize((960,720),Image.Resampling.NEAREST).save(path)
print(path)
