"""Render recorded production drawing calls; substitutes fonts, not UI layout."""
from pathlib import Path
import re,sys
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parent;O=R/'build/piano-roll'
scale=3;im=Image.new('RGB',(320*scale,240*scale),'black');draw=ImageDraw.Draw(im)
font_path=Path('C:/Windows/Fonts/segoeui.ttf')
if not font_path.exists():font_path=Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
source=Path(sys.argv[1]) if len(sys.argv)>1 else O/'draw-normal.txt'
for line in source.read_text().splitlines():
 if line.startswith('R '):
  _,x,y,w,h,ink=line.split();x,y,w,h=map(int,(x,y,w,h))
  if w and h:draw.rectangle((x*scale,y*scale,(x+w)*scale-1,(y+h)*scale-1),fill='#'+ink[2:])
 elif line.startswith('T '):
  match=re.fullmatch(r'T (-?\d+) (-?\d+) (\d+) (\d+) ([0-9a-f]+) ([0-9.]+) @([0-9a-f]+) (.*)',line)
  assert match,line
  x,y,w,h=map(int,match.group(1,2,3,4));ink,size,flags,text=match.group(5,6,7,8);size=max(1,int(float(size)*scale))
  font=ImageFont.truetype(str(font_path),size)
  while draw.textlength(text,font=font)>w*scale and size>6:
   size-=1;font=ImageFont.truetype(str(font_path),size)
  width=draw.textlength(text,font=font);xx=x*scale
  if int(flags,16)&4:xx+=(w*scale-width)/2
  elif int(flags,16)&2:xx+=w*scale-width
  bounds=draw.textbbox((0,0),text,font=font);yy=y*scale+(h*scale-(bounds[3]-bounds[1]))/2-bounds[1]
  draw.text((xx,yy),text,font=font,fill='#'+ink[2:])
target=Path(sys.argv[2]) if len(sys.argv)>2 else O/'piano-roll-preview.png';im.save(target);print(target)
