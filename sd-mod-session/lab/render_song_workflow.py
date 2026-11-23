"""Render proposed UX traces to PNG/GIF. No firmware artwork or device capture.

The traces are produced by preview_song_workflow.c under AArch64/QEMU.
Like docs/media/mixer/render_painter.py this uses a substitute console font.
"""
from pathlib import Path
import argparse
import json
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser()
parser.add_argument('traces', type=Path)
args = parser.parse_args()
dest = args.traces / 'previews'
dest.mkdir(parents=True, exist_ok=True)
fonts = [Path('C:/Windows/Fonts/consola.ttf'), Path('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf')]
font_path = next(p for p in fonts if p.exists())
font = lambda size: ImageFont.truetype(str(font_path), size)
captions = {
    'mixed-arrange': ('Смешанная аранжировка', 'Нотные паттерны и два вокальных сэмпла на пэдах.'),
    'pattern': ('Вход в паттерн', 'Song → выбранный блок → существующий Piano Roll.'),
    'selection': ('Выделение области', 'Удержание DATA: время энкодером, строки фейдером.'),
    'edit-preview': ('Редактирование нот', 'Edit: DATA — время, фейдер — высота. B3 — Apply.'),
    'edited-pattern': ('Изменённый паттерн', 'Ноты изменены. Возврат в Song сохраняет контекст блока.'),
    'record-auto': ('Запись без готового блока', 'New Take / Auto: блок растёт по записи, Stop завершает дубль.'),
    'record-loop': ('Запись в фиксированный цикл', 'Loop: запись и Overdub внутри заданной длины.'),
    'record-punch': ('Запись выбранного участка', 'Punch: запись только внутри выделенного диапазона.'),
    'record-arrange': ('Запись прямо в Arrange', 'Выбранная партия записывается; остальные продолжают играть.'),
    'record-pattern': ('Запись внутри паттерна', 'Один материал для живой записи и ручного редактирования.'),
    'insert-menu': ('Ручное добавление события', 'Существующее Actions → Insert; сетка задаёт положение.'),
    'inserted-note': ('Событие добавлено вручную', 'Добавленную ноту можно двигать, растягивать и менять velocity.'),
    'render-review': ('Render to Pad', 'Выбор источника, диапазона, обработки, хвоста и свободного пэда.'),
    'frozen-arrange': ('Аудиокопия на временной шкале', 'Waveform вместо миниатюры нот. Исходник сохранён для Unfreeze.'),
    'vocal-pad': ('Вокал остаётся на пэде', 'Sample / Trim / Chop подготавливают звук; Song хранит его запуск.'),
    'empty-pattern': ('Пустой паттерн заданной длины', 'Записать с пэдов, вставить события из другой Sequence или расставить вручную.'),
    'automation': ('Автоматизация паттерна', 'Предлагаемая lane параметра. Переключение через Actions, без вытеснения Range.'),
}

def wrapped(draw, text, x, y, width, size, colour):
    f = font(size)
    line = ''
    for word in text.split():
        candidate = (line + ' ' + word).strip()
        if line and draw.textlength(candidate, font=f) > width:
            draw.text((x,y),line,font=f,fill=colour)
            y += size + 9
            line = word
        else:
            line = candidate
    draw.text((x,y),line,font=f,fill=colour)

frames = {}
commands = {}
for path in sorted(args.traces.glob('*.trace')):
    if path.stem not in captions:
        continue
    image = Image.new('RGB',(320,240),'black')
    draw = ImageDraw.Draw(image)
    packed = []
    for line in path.read_text().splitlines():
        a = line.split(' ',8)
        kind = a[0]
        x,y,w,h = map(int,a[1:5])
        assert x >= 0 and y >= 0 and x+w <= 320 and y+h <= 240
        if kind == 'R':
            draw.rectangle((x,y,x+w-1,y+h-1),fill='#'+a[5][-6:])
            packed.append([kind,x,y,w,h,a[5]])
        else:
            size=int(a[5]); ink='#'+a[6][-6:]; centre=int(a[7]); value=a[8]
            layer=Image.new('RGBA',(w,h));pen=ImageDraw.Draw(layer);f=font(size)
            length=pen.textlength(value,font=f)
            offset=(w-length)/2 if centre else 0
            pen.text((offset,(h-size)/2-1),value,font=f,fill=ink)
            image.paste(layer,(x,y),layer)
            packed.append([kind,x,y,w,h,size,a[6],centre,value])
    image.save(dest/(path.stem+'-native.png'))
    frame=Image.new('RGB',(1008,900),'#080b0e')
    pen=ImageDraw.Draw(frame)
    pen.text((24,15),'Am MOD / ПРОЕКТИРУЕМЫЙ WORKFLOW',font=font(21),fill='#9ba5b0')
    pen.text((24,49),captions[path.stem][0],font=font(29),fill='#ffc247')
    frame.paste(image.resize((960,720),Image.Resampling.NEAREST),(24,100))
    wrapped(pen,captions[path.stem][1],24,838,960,20,'#ffffff')
    frame.save(dest/(path.stem+'.png'))
    frames[path.stem]=frame
    commands[path.stem]={'title':captions[path.stem][0],'detail':captions[path.stem][1],'commands':packed}

movies = {
    'mixed-pattern-render': ['mixed-arrange','pattern','selection','edit-preview','edited-pattern','render-review','frozen-arrange'],
    'record-and-insert': ['mixed-arrange','record-auto','record-loop','record-punch','record-arrange','record-pattern','insert-menu','inserted-note'],
}
for name, shots in movies.items():
    images=[frames[s].quantize(colors=128) for s in shots]
    images[0].save(dest/(name+'.gif'),save_all=True,append_images=images[1:],duration=2400,loop=0,disposal=2)
(dest/'scenes.json').write_text(json.dumps(commands,ensure_ascii=False),encoding='utf-8')
(dest/'manifest.json').write_text(json.dumps(dict(concept_only=True,source='lab/preview_song_workflow.c',font=str(font_path),native_size=[320,240],scenes=list(frames),animations=movies),ensure_ascii=False,indent=2),encoding='utf-8')
print(f'Wrote {len(frames)} scene PNGs and {len(movies)} GIFs to {dest}')
