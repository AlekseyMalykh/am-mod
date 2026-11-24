"""Production draw calls of the existing Piano Roll in its value-only host.
This is an isolated editor fixture, not a full app Song screen recording.
"""
from pathlib import Path
import subprocess,sys
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parent;O=R/'build/pattern-tools'
frames=[]
for line in (O/'ui-draw.txt-host').read_text().splitlines():
    if line=='R 0 0 320 240 ff000000':frames.append([])
    if frames:frames[-1].append(line)
names=['host-pad','host-tune','host-edit-preview','host-inserted']
captions=['Один Piano Roll: паттерн во внешнем host',
          'Существующий слой Tune: выбор и редактирование нот',
          'Preview: сдвиг времени, высоты и длительности',
          'После Apply: ручная вставка через Actions > Insert']
assert len(frames)==len(names)
font_path=Path('C:/Windows/Fonts/segoeui.ttf')
if not font_path.exists():font_path=Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
font=ImageFont.truetype(str(font_path),23);images=[]
for name,lines,caption in zip(names,frames,captions):
    source=O/(name+'.txt');target=O/(name+'.png');source.write_text('\n'.join(lines)+'\n')
    subprocess.run([sys.executable,str(R/'render_piano_roll.py'),str(source),str(target)],check=True)
    with Image.open(target) as image:
        frame=Image.new('RGB',(image.width,image.height+105),'#151515');frame.paste(image,(0,0));draw=ImageDraw.Draw(frame)
        draw.text((18,image.height+12),caption,font=font,fill='#ffc247')
        draw.text((18,image.height+52),'QEMU editor fixture / шрифт подменён / воспроизведение не подключено',font=font,fill='#bbbbbb')
        images.append(frame)
images[0].save(O/'pattern-host-v1.gif',save_all=True,append_images=images[1:],duration=[2300,2500,3200,2800],loop=0,disposal=2)
print(O/'pattern-host-v1.gif')
