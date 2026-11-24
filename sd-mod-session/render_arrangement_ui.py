"""Render production Song Arrange draw calls from the QEMU fixture.
Layout is unchanged; render_piano_roll.py substitutes the native font.
"""
from pathlib import Path
import subprocess,sys
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parent;O=R/'build/arrangement-ui'
frames=[]
for line in (O/'normal-draw.txt').read_text().splitlines():
    if line=='R 0 0 320 240 ff000000':frames.append([])
    if frames:frames[-1].append(line)
names=['empty-arrange','inserted-pattern','cursor-before-chop','chop-at-cursor','joined-again','copied-to-track','zoom-and-scroll']
captions=[
    'Пустая аранжировка',
    'Вставка: блок 8 четвертей, исходный паттерн 8 четвертей',
    'Курсор после 3 четвертей; длина блока ещё 8',
    'Chop: левая часть 3, правая 5. Общая длина осталась 8',
    'Join: две части снова склеены в один блок длиной 8',
    'Copy / Paste: копия уже склеенного блока длиной 8',
    'Zoom / Scroll: меняется масштаб окна, длины блоков прежние',
]
assert len(frames)==len(names)
assert 'CHOP_LENGTHS source=7680 left=2880 right=4800 right_offset=2880' in (O/'normal.log').read_text()
font_path=Path('C:/Windows/Fonts/segoeui.ttf')
if not font_path.exists():font_path=Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
font=ImageFont.truetype(str(font_path),23)
images=[]
for name,lines,caption in zip(names,frames,captions):
    source=O/(name+'.txt');target=O/(name+'.png');source.write_text('\n'.join(lines)+'\n')
    subprocess.run([sys.executable,str(R/'render_piano_roll.py'),str(source),str(target)],check=True)
    with Image.open(target) as image:
        frame=Image.new('RGB',(image.width,image.height+105),'#151515');frame.paste(image,(0,0))
        draw=ImageDraw.Draw(frame);draw.text((18,image.height+12),caption,font=font,fill='#ffc247')
        draw.text((18,image.height+52),'QEMU UI fixture / подпись вне экрана устройства / 1 четверть = 960 ticks',font=font,fill='#bbbbbb')
        images.append(frame)
for name in ['workflow-v2.gif','workflow.gif']:
    images[0].save(O/name,save_all=True,append_images=images[1:],duration=[1800,2600,2400,3600,2800,3000,2400],loop=0,disposal=2)
print(O/'workflow-v2.gif')
