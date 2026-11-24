"""Separate Store/UI fixtures, real draw calls with substituted native font."""
from pathlib import Path
import subprocess,sys
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parent;O=R/'build/arrangement-ui'
font_path=Path('C:/Windows/Fonts/segoeui.ttf')
if not font_path.exists():font_path=Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
font=ImageFont.truetype(str(font_path),22)
def render(path,name,captions,scope):
 frames=[]
 for line in path.read_text().splitlines():
  if line=='R 0 0 320 240 ff000000':frames.append([])
  if frames:frames[-1].append(line)
 assert len(frames)==len(captions),(name,len(frames))
 images=[]
 for i,(lines,caption) in enumerate(zip(frames,captions)):
  trace=O/(name+'-'+str(i)+'.txt');target=O/(name+'-'+str(i)+'.png');trace.write_text('\n'.join(lines)+'\n')
  subprocess.run([sys.executable,str(R/'render_piano_roll.py'),str(trace),str(target)],check=True)
  with Image.open(target) as im:
   output=Image.new('RGB',(im.width,im.height+95),'#151515');output.paste(im,(0,0));draw=ImageDraw.Draw(output)
   draw.text((16,im.height+8),caption,font=font,fill='#ffc247');draw.text((16,im.height+46),scope+' / подпись вне экрана',font=font,fill='#bbbbbb');images.append(output)
 images[0].save(O/(name+'.gif'),save_all=True,append_images=images[1:],duration=2800,loop=0,disposal=2);print(O/(name+'.gif'))
render(O/'normal-draw.txt-separate.txt','separate-arrange',[
 'До отделения: один паттерн используется в нескольких блоках',
 'Separate: новая партия под исходной дорожкой; другие ссылки сохранены',
],'QEMU Song fixture; handoff/закрытие редактора — фикстура')
render(R/'build/pattern-tools/ui-draw.txt-separate','separate-rows',[
 'B2 + пэды: выбраны две строки; B3 Separate отделит их без меню',
 'Tune: выбран только +4 одного пэда; Copy selection создаёт партию',
],'QEMU existing Piano Roll; native Events pointers недоступны')
