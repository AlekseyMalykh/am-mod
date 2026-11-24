"""Two separate native-graphics QEMU fixtures; fonts are substituted.
These GIFs are not a full application recording or an Engine playback proof.
"""
from pathlib import Path
import subprocess,sys
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parent;O=R/'build/arrangement-ui'
fontpath=Path('C:/Windows/Fonts/segoeui.ttf')
if not fontpath.exists():fontpath=Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
font=ImageFont.truetype(str(fontpath),22)
def render(source,name,captions,scope):
 frames=[]
 for line in source.read_text().splitlines():
  if line=='R 0 0 320 240 ff000000':frames.append([])
  if frames:frames[-1].append(line)
 assert len(frames)==len(captions),(name,len(frames))
 images=[]
 for i,(lines,caption) in enumerate(zip(frames,captions)):
  trace=O/(name+'-'+str(i)+'.txt');target=O/(name+'-'+str(i)+'.png');trace.write_text('\n'.join(lines)+'\n')
  subprocess.run([sys.executable,str(R/'render_piano_roll.py'),str(trace),str(target)],check=True)
  with Image.open(target) as im:
   picture=Image.new('RGB',(im.width,im.height+95),'#151515');picture.paste(im,(0,0));draw=ImageDraw.Draw(picture)
   draw.text((16,im.height+9),caption,font=font,fill='#ffc247');draw.text((16,im.height+47),scope+' / подпись вне экрана',font=font,fill='#bbbbbb');images.append(picture)
 images[0].save(O/(name+'.gif'),save_all=True,append_images=images[1:],duration=2300,loop=0,disposal=2)
 print(O/(name+'.gif'))
render(O/'normal-draw.txt-new.txt','direct-workflow',[
 'Пустой Arrange: B3 New без входа в Actions',
 'После New: создан паттерн и блок 4q; B3 Open возвращает к нотам',
 'DATA выделяет блок; B3 Edit: предварительное перемещение',
 'B3 Apply: положение и длина изменены одной операцией Undo',
 'B2 Range: выделенная область задаёт длину нового паттерна',
 'Новый паттерн 2q на той же дорожке',
],'QEMU Arrange fixture; редактор в этом тесте — handoff fixture')
render(R/'build/pattern-tools/ui-draw.txt-new-pattern','direct-notes',[
 'Новый пустой паттерн: инструмент пока не назначен',
 'B3 New: ручная вставка события пэда',
 '16 Levels → Tune: вторая нота; Undo / Redo проверены',
],'QEMU existing Piano Roll; native Events pointers недоступны')
