"""Builds the r30d documentation images from real mixer_view painter traces (screens/*.png).
The synth parameter values and the live meter in the screens are fixtures: review artefacts, not hardware photos.
Usage: python build_images.py   (needs Pillow and DejaVu Sans)"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
H = Path(__file__).parent
F = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FB = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
if not Path(F).exists():
    F = "C:/Windows/Fonts/arial.ttf"; FB = "C:/Windows/Fonts/arialbd.ttf"
def font(s, bold=False): return ImageFont.truetype(FB if bold else F, s)
BG, FG, AMB, GREY, PANEL = "#15171a", "#f2f2f2", "#ffc247", "#9aa3ad", "#23272c"
T = {
 "en": dict(
  title="Am MOD synth: five modules, nine pages",
  sub="Hold Loop and press a pad (13-16 and 9), or use + / -. The DATA encoder flips the pages of a module. Shift shows the hidden parameter above each knob.",
  caps={"synth-osc":"Tone 1/2 - Oscillators: Wave, Osc2, Sub","synth-amp":"Envelope 1/2 - Amp ADSR, Velocity","synth-keys":"Keys - 16 Levels, as the Tune tab","synth-filter":"Filter + LFO - Filter, Resonance, Key track","synth-voices":"Voices - Unison, Detune, Spread, Glide","synth-controller":"Controller - Fader, Bend range, MIDI channel"},
  lbl_yes="Yes", lbl_no="No", flow_title="Exit to bootloader", flow=["Project > Mod Settings > About","Exit to bootloader  (B3 confirm, B1 cancel)","Project copy saved to SD: Projects/Mod-Exit-Last","Which boot menu is on the card?"],
  l1="D.S.B", l2="SampleBoot", l3="None",
  yes="D.S.B: start marked healthy, reboot into the D.S.B menu", sb="SampleBoot: reboot into the SampleBoot menu", no="No boot menu: stock firmware starts",
  foot="Pick another system in the menu - or leave the card in and keep working.", ov="Synth, Input ROUTE, Piano Roll area select, bootloader exit"),
 "ru": dict(
  title="Синтезатор Am MOD: пять модулей, девять страниц",
  sub="Удерживайте Loop и нажмите пэд (13-16 и 9) или используйте + / -. Энкодер DATA листает страницы модуля. Shift показывает скрытый параметр над каждой ручкой.",
  caps={"synth-osc":"Tone 1/2 - осцилляторы: Wave, Osc2, Sub","synth-amp":"Envelope 1/2 - ADSR усилителя, Velocity","synth-keys":"Keys - 16 Levels, как вкладка Tune","synth-filter":"Filter + LFO - Filter, Resonance, Key track","synth-voices":"Voices - Unison, Detune, Spread, Glide","synth-controller":"Controller - Fader, Bend range, MIDI-канал"},
  lbl_yes="Да", lbl_no="Нет", flow_title="Выход в загрузчик", flow=["Project > Mod Settings > About","Exit to bootloader  (B3 - подтвердить, B1 - отмена)","Копия проекта на SD: Projects/Mod-Exit-Last","Какое меню загрузки на карте?"],
  l1="D.S.B", l2="SampleBoot", l3="Нет",
  yes="D.S.B: запуск помечен успешным, перезагрузка в меню D.S.B", sb="SampleBoot: перезагрузка в меню SampleBoot", no="Меню загрузки нет: запускается штатная прошивка",
  foot="Выберите в меню другую систему - либо оставьте карту и продолжайте работу.", ov="Синт, Input ROUTE, выделение области в пианоролле, выход в загрузчик"),
 "es": dict(
  title="Sintetizador de Am MOD: cinco módulos, nueve páginas",
  sub="Mantén Loop y pulsa un pad (13-16 y 9), o usa + / -. El encoder DATA pasa las páginas de un módulo. Shift muestra el parámetro oculto sobre cada mando.",
  caps={"synth-osc":"Tone 1/2 - osciladores: Wave, Osc2, Sub","synth-amp":"Envelope 1/2 - ADSR del amplificador, Velocity","synth-keys":"Keys - 16 Levels, como la pestaña Tune","synth-filter":"Filter + LFO - Filter, Resonance, Key track","synth-voices":"Voices - Unison, Detune, Spread, Glide","synth-controller":"Controller - Fader, Bend range, canal MIDI"},
  lbl_yes="Sí", lbl_no="No", flow_title="Salir al cargador de arranque", flow=["Project > Mod Settings > About","Exit to bootloader  (B3 confirma, B1 cancela)","Copia del proyecto en la SD: Projects/Mod-Exit-Last","¿Qué menú de arranque hay en la tarjeta?"],
  l1="D.S.B", l2="SampleBoot", l3="Ninguno",
  yes="D.S.B: arranque marcado como correcto, reinicio al menú de D.S.B", sb="SampleBoot: reinicio al menú de SampleBoot", no="Sin menú de arranque: arranca el firmware original",
  foot="Elige otro sistema en el menú, o deja la tarjeta y sigue trabajando.", ov="Sintetizador, Input ROUTE, selección de área en Piano Roll, salida al cargador"),
}
ORDER = ["synth-osc", "synth-amp", "synth-keys", "synth-filter", "synth-voices", "synth-controller"]
def wrap(d, text, f, width):
    lines, cur = [], ""
    for w in text.split():
        t = (cur + " " + w).strip()
        if d.textlength(t, font=f) <= width: cur = t
        else: lines.append(cur); cur = w
    return lines + [cur]
def modules(lang):
    t = T[lang]; sc = 1.6; w, h = int(320*sc), int(240*sc); gap, top = 24, 150
    W = 3*w + 4*gap; Hh = top + 2*(h+46) + gap*2
    im = Image.new("RGB", (W, Hh), BG); d = ImageDraw.Draw(im)
    d.text((gap, 24), t["title"], font=font(36, True), fill=AMB)
    y = 78
    for line in wrap(d, t["sub"], font(20), W-2*gap)[:2]:
        d.text((gap, y), line, font=font(20), fill=GREY); y += 28
    for i, n in enumerate(ORDER):
        x = gap + (i % 3)*(w+gap); yy = top + (i//3)*(h+46+gap)
        s = Image.open(H/"screens"/(n+".png")).resize((w, h), Image.NEAREST)
        d.rectangle((x-3, yy-3, x+w+2, yy+h+2), outline="#3a4048", width=2); im.paste(s, (x, yy))
        d.text((x, yy+h+8), t["caps"][n], font=font(19), fill=FG)
    im.save(H/f"synth-modules-{lang}.png")
def box(d, xy, text, f, fill=PANEL, outline="#3a4048", ink=FG):
    d.rounded_rectangle(xy, 14, fill=fill, outline=outline, width=2)
    x0, y0, x1, y1 = xy; lines = []
    for part in text.split("\n"): lines += wrap(d, part, f, x1-x0-36)
    th = len(lines)*f.size*1.35; yy = (y0+y1)/2 - th/2
    for l in lines:
        d.text(((x0+x1)/2 - d.textlength(l, font=f)/2, yy), l, font=f, fill=ink); yy += f.size*1.35
def arrow(d, p, q, col=AMB):
    d.line((*p, *q), fill=col, width=4)
    import math
    a = math.atan2(q[1]-p[1], q[0]-p[0]); L = 14
    d.polygon([q, (q[0]-L*math.cos(a-.45), q[1]-L*math.sin(a-.45)), (q[0]-L*math.cos(a+.45), q[1]-L*math.sin(a+.45))], fill=col)
def flow(lang):
    t = T[lang]; W, Hh = 1500, 700
    im = Image.new("RGB", (W, Hh), BG); d = ImageDraw.Draw(im)
    d.text((40, 28), t["flow_title"], font=font(38, True), fill=AMB)
    f = font(23); bw, bh, y, gap = 440, 120, 110, 70
    xs = [40 + i*(bw+gap) for i in range(3)]
    for i in range(3): box(d, (xs[i], y, xs[i]+bw, y+bh), t["flow"][i], f)
    for i in range(2): arrow(d, (xs[i]+bw+6, y+bh//2), (xs[i+1]-6, y+bh//2))
    qx0 = (W-bw)//2; qy0 = 300
    box(d, (qx0, qy0, qx0+bw, qy0+90), t["flow"][3], f, outline=AMB)
    d.line((xs[2]+bw//2, y+bh+4, xs[2]+bw//2, 262), fill=AMB, width=4); d.line((xs[2]+bw//2, 262, qx0+bw//2, 262), fill=AMB, width=4); arrow(d, (qx0+bw//2, 262), (qx0+bw//2, qy0-6))
    bw3, y2, bus = 440, 480, 440
    xs3 = [40, 530, 1020]
    cols = ["#6bd36b", "#6bd36b", "#c9c9c9"]
    texts = [t["yes"], t["sb"], t["no"]]
    labels = [t["l1"], t["l2"], t["l3"]]
    qc = qx0+bw//2
    d.line((qc, qy0+90+4, qc, bus), fill=AMB, width=4)
    d.line((xs3[0]+bw3//2, bus, xs3[2]+bw3//2, bus), fill=AMB, width=4)
    for x3, col, tx, lb in zip(xs3, cols, texts, labels):
        box(d, (x3, y2, x3+bw3, y2+130), tx, f, outline=col)
        arrow(d, (x3+bw3//2, bus), (x3+bw3//2, y2-6), col)
        d.text((x3+bw3//2+14, bus+8), lb, font=font(22, True), fill=col)
    d.text((40, 644), t["foot"], font=font(21), fill=GREY)
    im.save(H/f"exit-bootloader-{lang}.png")
def overview(lang):
    t = T[lang]; sc = 2.0; w, h = int(320*sc), int(240*sc); gap = 20
    names = ["synth-osc", "synth-keys", "synth-filter", "synth-voices"]
    W = 2*w + 3*gap; Hh = 110 + 2*h + 3*gap
    im = Image.new("RGB", (W, Hh), BG); d = ImageDraw.Draw(im)
    d.text((gap, 22), "Am MOD r30d", font=font(40, True), fill=AMB)
    d.text((gap, 72), t["ov"], font=font(22), fill=GREY)
    for i, n in enumerate(names):
        x = gap + (i % 2)*(w+gap); yy = 110 + (i//2)*(h+gap)
        d.rectangle((x-3, yy-3, x+w+2, yy+h+2), outline="#3a4048", width=2); im.paste(Image.open(H/"screens"/(n+".png")).resize((w, h), Image.NEAREST), (x, yy))
    im.save(H/f"overview-{lang}.png")
for lang in T: modules(lang); flow(lang); overview(lang)
