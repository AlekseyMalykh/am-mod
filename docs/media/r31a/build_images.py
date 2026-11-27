"""Builds the r31a documentation images from production painter traces (screens/*.png).
Screens come from QEMU fixtures of the real Arrange and Mixer painters (effect pages: frames of the illustrated FX GIFs in docs/media/fx) with a substituted font:
review artefacts, not hardware photos. Usage: python build_images.py (Pillow + DejaVu Sans)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
H = Path(__file__).parent
F = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"; FB = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
if not Path(F).exists(): F = "C:/Windows/Fonts/arial.ttf"; FB = "C:/Windows/Fonts/arialbd.ttf"
def font(s, bold=False): return ImageFont.truetype(FB if bold else F, s)
BG, FG, AMB, GREY = "#15171a", "#f2f2f2", "#ffc247", "#9aa3ad"
T = {
 "en": dict(
  arr_title="Arrange (beta): a song from sequences and patterns",
  arr_sub="Song > Shift+B1. Rows are free lanes; Seq blocks stay linked to their sequence, patterns are your own variations.",
  arr={"lane-playing":"Lanes, block colours, playhead and bar grid","multi-move":"Hold Note Repeat + DATA / fader: move a selection","actions-block":"Shift+B3 Actions, grouped Add / Block / Track / Song","rename":"Rename a pattern or track: pads type letters, K1 switches input"},
  mix_title="Mixer: eight tracks, Input, Master and four FX slots each",
  mix_sub="Knob FX opens the Mixer. Pads 1-8 = tracks A-H, 9-12 = FX slots, 13 = Synth, 15 = Input, 16 = Master.",
  mix={"overview":"Overview: faders, meters, S / M / fx","chain":"FX chain of a track (pads 9-12)","routing-sends":"Route: sends between tracks","input-rear-left":"Input ROUTE: source, monitor, record","fx-tape-delay":"Tape Delay","fx-reverb":"Reverb","fx-sidechain":"Sidechain Compressor","fx-amp-models":"Amp Sim voices"}),
 "ru": dict(
  arr_title="Arrange (бета): песня из секвенций и паттернов",
  arr_sub="Song > Shift+B1. Строки — свободные дорожки; Seq-блоки связаны со своей секвенцией, паттерны — ваши вариации.",
  arr={"lane-playing":"Дорожки, цвета блоков, курсор воспроизведения и сетка тактов","multi-move":"Note Repeat + DATA / фейдер: перенос выделения","actions-block":"Shift+B3 Actions: группы Add / Block / Track / Song","rename":"Переименование паттерна или дорожки: буквы с пэдов, K1 — способ ввода"},
  mix_title="Микшер: восемь треков, Input, Master и по четыре слота FX",
  mix_sub="Knob FX открывает микшер. Пэды 1-8 — треки A-H, 9-12 — слоты FX, 13 — синт, 15 — Input, 16 — Master.",
  mix={"overview":"Обзор: фейдеры, индикаторы, S / M / fx","chain":"Цепочка FX трека (пэды 9-12)","routing-sends":"Route: посылы между треками","input-rear-left":"Input ROUTE: источник, мониторинг, запись","fx-tape-delay":"Tape Delay","fx-reverb":"Reverb","fx-sidechain":"Сайдчейн-компрессор","fx-amp-models":"Голоса Amp Sim"}),
 "es": dict(
  arr_title="Arrange (beta): una canción con secuencias y patrones",
  arr_sub="Song > Shift+B1. Las filas son pistas libres; los bloques Seq siguen enlazados a su secuencia, los patrones son tus variaciones.",
  arr={"lane-playing":"Pistas, colores de bloque, cursor de reproducción y rejilla de compases","multi-move":"Mantén Note Repeat + DATA / fader: mover una selección","actions-block":"Shift+B3 Actions: grupos Add / Block / Track / Song","rename":"Renombrar patrón o pista: letras con los pads, K1 cambia la entrada"},
  mix_title="Mezclador: ocho pistas, Input, Master y cuatro ranuras FX cada una",
  mix_sub="Knob FX abre el mezclador. Pads 1-8 = pistas A-H, 9-12 = ranuras FX, 13 = sintetizador, 15 = Input, 16 = Master.",
  mix={"overview":"Vista general: faders, medidores, S / M / fx","chain":"Cadena FX de una pista (pads 9-12)","routing-sends":"Route: envíos entre pistas","input-rear-left":"Input ROUTE: fuente, monitor, grabación","fx-tape-delay":"Tape Delay","fx-reverb":"Reverb","fx-sidechain":"Compresor sidechain","fx-amp-models":"Voces de Amp Sim"}),
}
def wrap(d, text, f, width):
    lines, cur = [], ""
    for w in text.split():
        t = (cur + " " + w).strip()
        if d.textlength(t, font=f) <= width: cur = t
        else: lines.append(cur); cur = w
    return lines + [cur]
def sheet(title, sub, items, prefix, cols, out, sc=1.6):
    w, h = int(320*sc), int(240*sc); gap, top = 24, 150; rows = (len(items)+cols-1)//cols
    W = cols*w + (cols+1)*gap; Hh = top + rows*(h+50+gap)
    im = Image.new("RGB", (W, Hh), BG); d = ImageDraw.Draw(im)
    d.text((gap, 24), title, font=font(34, True), fill=AMB); y = 76
    for line in wrap(d, sub, font(20), W-2*gap)[:2]: d.text((gap, y), line, font=font(20), fill=GREY); y += 28
    for i, (name, cap) in enumerate(items):
        x = gap + (i % cols)*(w+gap); yy = top + (i//cols)*(h+50+gap)
        s = Image.open(H/"screens"/(prefix+name+".png")).resize((w, h), Image.LANCZOS)
        d.rectangle((x-3, yy-3, x+w+2, yy+h+2), outline="#3a4048", width=2); im.paste(s, (x, yy))
        for k, line in enumerate(wrap(d, cap, font(19), w)[:2]): d.text((x, yy+h+8+k*24), line, font=font(19), fill=FG)
    im.save(H/out, optimize=True)
for lang, t in T.items():
    sheet(t["arr_title"], t["arr_sub"], list(t["arr"].items()), "arrange-", 2, f"arrange-{lang}.png")
    sheet(t["mix_title"], t["mix_sub"], list(t["mix"].items()), "mixer-", 4, f"mixer-{lang}.png", sc=1.1)
print("ok")
