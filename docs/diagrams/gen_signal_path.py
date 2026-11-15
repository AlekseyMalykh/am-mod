#!/usr/bin/env python3
"""Sound-path diagrams for Am MOD README: Stock and Am MOD, RU and EN (SVG)."""
import sys, html

FONT = "DejaVu Sans, Arial, Helvetica, sans-serif"
CLS = {  # fill, stroke
    "src":  ("#eceff3", "#7b8794"),
    "proc": ("#e3f0fb", "#3b82c4"),
    "fx":   ("#fff3d6", "#c98a0b"),
    "bus":  ("#e4f5e8", "#2f8f4e"),
    "comp": ("#fde8e8", "#c0392b"),
    "file": ("#efe7fa", "#7c4dbd"),
    "out":  ("#2d3540", "#1b2027"),
}

def esc(s): return html.escape(s, quote=False)

class Svg:
    def __init__(s, w, h):
        s.w, s.h, s.p = w, h, []
    def add(s, x): s.p.append(x)
    def text(s, x, y, t, size=14, bold=False, fill="#1f2933", anchor="start", italic=False):
        s.add(f'<text x="{x}" y="{y}" font-size="{size}" font-weight="{"700" if bold else "400"}" '
              f'fill="{fill}" text-anchor="{anchor}"{" font-style=\"italic\"" if italic else ""}>{esc(t)}</text>')
    def box(s, x, y, w, h, lines, cls, dashed=False):
        f, st = CLS[cls]
        dash = ' stroke-dasharray="6 4"' if dashed else ""
        s.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="9" fill="{f}" stroke="{st}" stroke-width="2"{dash}/>')
        col = "#ffffff" if cls == "out" else "#1f2933"
        n = len(lines)
        lh = 18
        top = y + h / 2 - (n - 1) * lh / 2 + 5
        for i, ln in enumerate(lines):
            s.text(x + w / 2, top + i * lh, ln, 15 if i == 0 else 12.5, i == 0, col, "middle")
    def arrow(s, pts, dashed=False, color="#52606d"):
        d = " ".join(f"{px},{py}" for px, py in pts)
        dash = ' stroke-dasharray="6 4"' if dashed else ""
        s.add(f'<polyline points="{d}" fill="none" stroke="{color}" stroke-width="2.2"{dash} marker-end="url(#ah)"/>')
    def section(s, x, y, t):
        s.text(x, y, t, 17, True, "#102a43")
        s.add(f'<line x1="{x}" y1="{y+7}" x2="{s.w-40}" y2="{y+7}" stroke="#bcccdc" stroke-width="1.5"/>')
    def render(s):
        head = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{s.w}" height="{s.h}" viewBox="0 0 {s.w} {s.h}" '
                f'font-family="{FONT}"><defs><marker id="ah" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" '
                f'markerHeight="8" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="#52606d"/></marker></defs>'
                f'<rect width="100%" height="100%" fill="#ffffff"/>')
        return head + "".join(s.p) + "</svg>"

def hline(s, x1, x2, y, **k): s.arrow([(x1, y), (x2, y)], **k)

# ---------------------------------------------------------------- texts
T = {
 "ru": {
  "mon_stock": ["Мониторинг → Выходы"],
  "stock_title": "Режим Stock: путь звука",
  "mod_title": "Режим Am MOD: путь звука",
  "stock_sub": "Штатная маршрутизация и штатный FX Selection. Микшер и цепочка Input FX обходятся.",
  "mod_sub": "Микшер A–H и Master заменяет штатный FX Selection. Штатные эффекты FX Select в этом режиме не применяются.",
  "s_play": "1. Воспроизведение", "s_rec": "2. Запись входа", "s_res": "3. Ресемплинг",
  "pads": ["Пэды /", "секвенсор"], "smp": ["Окно Sample", "Trim · Tune · Filter", "Envelope · Level · Pan"],
  "fxsel": ["FX Select", "выбранные пэды:", "LP/BP Filter, Bus Comp,", "Limiter, Pumper …"],
  "nofx": ["Остальные пэды", "без эффекта"],
  "main": ["Главная шина", "Program main"], "comp": ["Компрессор"],
  "outv": ["Master volume", "→ Выходы"],
  "in": ["Вход", "Mic / USB / Rear"],
  "srec": ["Sample Record", "штатная запись", "Source · Rec Length · Monitor"],
  "newpad": ["Новый сэмпл", "на пэде (WAV)"],
  "trig": ["Shift + Pad 11", "экран Sample → сэмпл", "экран секвенции → секвенция"],
  "fxsel2": ["FX Select", "выбранные пэды"],
  "wav": ["WAV", "(рендер)"], "tpad": ["Целевой", "пэд"],
  "route": ["Назначение", "Pad FX: трек A–H", "или Direct"],
  "track": ["Трек A–H", "Track FX · Fader · Pan", "Mute / Solo · Send"],
  "direct": ["Direct: мимо треков"],
  "master": ["Master", "Master FX"],
  "trk_r": ["Трек пэда", "(если пэд на треке)"], "mfx_r": ["Master FX"],
  "mixch": ["Канал Input", "Level · Pan · Mute", "Rear: Rear L/R · Split · Mix"],
  "infx": ["Input FX", "цепочка из 4 слотов", "общая для Mic / USB / Rear"],
  "point": ["Точка записи", "Input · Track A–H · Master", "Wet: эффекты до точки · Dry: без них"],
  "recpad": ["Запись на пэд", "новый сэмпл (WAV)"],
  "mon": ["Мониторинг: Off / Auto / On", "слышит сигнал той же точки"],
  "tuner": ["Тюнер", "нота и центы входа"],
  "notes_stock": [
    "• Режим выбирается в Project → Mod Settings → Quick → FX mode; применяется после перезапуска.",
    "• Эффекты, назначенные в FX Select, сохраняются в проекте и не влияют на звук режима Am MOD.",
    "• Ресемплинг включает окно Sample, FX Select и компрессор, то есть результат звучит так же, как воспроизведение.",
    "• Запись и ресемплинг в Stock работают штатно: функции микшера и Input FX не используются."],
  "notes_mod": [
    "• Режим выбирается в Project → Mod Settings → Quick → FX mode; применяется после перезапуска.",
    "• Пэд назначается на трек A–H (Pad FX в микшере) или на Direct: Direct идёт мимо треков сразу в Master.",
    "• Input — отдельный канал микшера (пэд 15 в микшере): Level, Pan, Mute и цепочка Input FX. Mic, USB и Rear используют один канал.",
    "• Запись берёт сигнал в выбранной точке (Input ROUTE → Record via): Input, Track A–H или Master. Wet пишет эффекты до этой точки, Dry — без эффектов.",
    "• Мониторинг слышит сигнал из той же точки. Тюнер читает вход (Input ROUTE → Tuner, Sample Record B3).",
    "• Ресемплинг сэмпла и секвенции идёт тем же путём, что воспроизведение: Sample → трек → Master → компрессор; фейдеры влияют на уровень.",
    "• Записанный сэмпл при воспроизведении снова проходит свой трек: для первой проверки используйте Direct."],
 },
 "en": {
  "mon_stock": ["Monitoring → Outputs"],
  "stock_title": "Stock mode: signal path",
  "mod_title": "Am MOD mode: signal path",
  "stock_sub": "Stock routing and stock FX Selection. The Mixer and Input FX chain are bypassed.",
  "mod_sub": "The A–H / Master Mixer replaces stock FX Selection. Stock FX Select effects are not applied in this mode.",
  "s_play": "1. Playback", "s_rec": "2. Input recording", "s_res": "3. Resampling",
  "pads": ["Pads /", "sequencer"], "smp": ["Sample window", "Trim · Tune · Filter", "Envelope · Level · Pan"],
  "fxsel": ["FX Select", "selected pads:", "LP/BP Filter, Bus Comp,", "Limiter, Pumper …"],
  "nofx": ["All other pads", "no effect"],
  "main": ["Main bus", "Program main"], "comp": ["Compressor"],
  "outv": ["Master volume", "→ Outputs"],
  "in": ["Input", "Mic / USB / Rear"],
  "srec": ["Sample Record", "stock recording", "Source · Rec Length · Monitor"],
  "newpad": ["New sample", "on the pad (WAV)"],
  "trig": ["Shift + Pad 11", "Sample screen → sample", "Sequence screen → sequence"],
  "fxsel2": ["FX Select", "selected pads"],
  "wav": ["WAV", "(render)"], "tpad": ["Target", "pad"],
  "route": ["Assignment", "Pad FX: track A–H", "or Direct"],
  "track": ["Track A–H", "Track FX · Fader · Pan", "Mute / Solo · Send"],
  "direct": ["Direct: bypasses tracks"],
  "master": ["Master", "Master FX"],
  "trk_r": ["Pad's track", "(if the pad is on a track)"], "mfx_r": ["Master FX"],
  "mixch": ["Input channel", "Level · Pan · Mute", "Rear: Rear L/R · Split · Mix"],
  "infx": ["Input FX", "4-slot chain", "shared by Mic / USB / Rear"],
  "point": ["Record point", "Input · Track A–H · Master", "Wet: effects up to the point · Dry: none"],
  "recpad": ["Record to pad", "new sample (WAV)"],
  "mon": ["Monitor: Off / Auto / On", "hears the same point"],
  "tuner": ["Tuner", "pitch and cents of the input"],
  "notes_stock": [
    "• The mode is chosen in Project → Mod Settings → Quick → FX mode and applies after a restart.",
    "• Effects assigned in FX Select are saved in the project and do not affect the sound in Am MOD mode.",
    "• Resampling includes the Sample window, FX Select and the compressor, so the result sounds like playback.",
    "• Recording and resampling work as in stock: the Mixer and Input FX are not used."],
  "notes_mod": [
    "• The mode is chosen in Project → Mod Settings → Quick → FX mode and applies after a restart.",
    "• A pad is assigned to a track A–H (Pad FX in the Mixer) or to Direct: Direct bypasses the tracks and goes straight to Master.",
    "• Input is its own Mixer channel (pad 15 in the Mixer): Level, Pan, Mute and the Input FX chain. Mic, USB and Rear share it.",
    "• Recording takes the signal at the chosen point (Input ROUTE → Record via): Input, Track A–H or Master. Wet records the effects up to that point, Dry records none.",
    "• Monitoring hears the same point. The tuner reads the input (Input ROUTE → Tuner, Sample Record B3).",
    "• Sample and sequence resampling follow the playback path: Sample → track → Master → compressor; the faders affect the level.",
    "• A recorded sample passes through its pad's track again on playback: use Direct for a first check."],
 },
}

def stock(lang):
    t = T[lang]; s = Svg(1500, 940)
    s.text(40, 48, t["stock_title"], 26, True, "#102a43"); s.text(40, 76, t["stock_sub"], 14, False, "#52606d")
    s.section(40, 118, t["s_play"])
    cy = 215
    s.box(40, cy-50, 150, 100, t["pads"], "src")
    s.box(230, cy-50, 230, 100, t["smp"], "proc")
    s.box(520, 132, 250, 90, t["fxsel"], "fx")
    s.box(520, 245, 250, 56, t["nofx"], "src", dashed=True)
    s.box(830, cy-50, 170, 100, t["main"], "bus")
    s.box(1040, cy-50, 150, 100, t["comp"], "comp")
    s.box(1230, cy-50, 230, 100, t["outv"], "out")
    hline(s, 190, 230, cy)
    s.arrow([(460, cy), (490, cy), (490, 177), (520, 177)]); s.arrow([(460, cy), (490, cy), (490, 273), (520, 273)])
    s.arrow([(770, 177), (800, 177), (800, cy), (830, cy)]); s.arrow([(770, 273), (800, 273), (800, cy), (830, cy)])
    hline(s, 1000, 1040, cy); hline(s, 1190, 1230, cy)
    s.section(40, 352, t["s_rec"])
    ry = 420
    s.box(40, ry-50, 190, 100, t["in"], "src")
    s.box(290, ry-50, 300, 100, t["srec"], "proc")
    s.box(650, ry-50, 220, 100, t["newpad"], "file")
    s.box(290, 505, 300, 46, t["mon_stock"], "out", dashed=True)
    hline(s, 230, 290, ry); hline(s, 590, 650, ry)
    s.arrow([(135, ry+50), (135, 528), (290, 528)], dashed=True)
    s.section(40, 600, t["s_res"])
    qy = 690
    s.box(40, qy-55, 260, 110, t["trig"], "src")
    s.box(340, qy-55, 220, 110, t["smp"], "proc")
    s.box(600, qy-55, 220, 110, t["fxsel2"], "fx")
    s.box(860, qy-55, 150, 110, t["comp"], "comp")
    s.box(1050, qy-55, 150, 110, t["wav"], "file")
    s.box(1240, qy-55, 150, 110, t["tpad"], "file")
    for a, b in [(300, 340), (560, 600), (820, 860), (1010, 1050), (1200, 1240)]: hline(s, a, b, qy)
    for i, ln in enumerate(t["notes_stock"]): s.text(40, 800 + i * 26, ln, 14, False, "#334e68")
    return s.render()

def mod(lang):
    t = T[lang]; s = Svg(1500, 1070)
    s.text(40, 48, t["mod_title"], 26, True, "#102a43"); s.text(40, 76, t["mod_sub"], 14, False, "#52606d")
    s.section(40, 118, t["s_play"])
    cy = 215
    s.box(40, cy-50, 140, 100, t["pads"], "src")
    s.box(215, cy-50, 200, 100, t["smp"], "proc")
    s.box(450, cy-50, 200, 100, t["route"], "fx")
    s.box(700, 132, 260, 90, t["track"], "proc")
    s.box(700, 245, 260, 56, t["direct"], "src", dashed=True)
    s.box(1010, cy-50, 230, 100, t["master"] + [], "bus") if False else None
    s.box(1000, cy-50, 130, 100, t["master"], "bus")
    s.box(1165, cy-50, 120, 100, t["comp"], "comp")
    s.box(1320, cy-50, 140, 100, t["outv"], "out")
    hline(s, 180, 215, cy); hline(s, 415, 450, cy)
    s.arrow([(650, cy), (675, cy), (675, 177), (700, 177)]); s.arrow([(650, cy), (675, cy), (675, 273), (700, 273)])
    s.arrow([(960, 177), (980, 177), (980, cy), (1000, cy)]); s.arrow([(960, 273), (980, 273), (980, cy), (1000, cy)])
    hline(s, 1130, 1165, cy); hline(s, 1285, 1320, cy)
    s.section(40, 352, t["s_rec"])
    ry = 455
    s.box(40, ry-50, 150, 100, t["in"], "src")
    s.box(225, ry-50, 240, 100, t["mixch"], "proc")
    s.box(500, ry-50, 200, 100, t["infx"], "fx")
    s.box(735, ry-50, 300, 100, t["point"], "bus")
    s.box(1070, ry-50, 220, 100, t["recpad"], "file")
    s.box(735, 540, 300, 46, t["mon"], "out", dashed=True)
    s.box(225, 540, 240, 46, t["tuner"], "out", dashed=True)
    hline(s, 190, 225, ry); hline(s, 465, 500, ry); hline(s, 700, 735, ry); hline(s, 1035, 1070, ry)
    s.arrow([(885, ry+50), (885, 540)], dashed=True)
    s.arrow([(345, ry+50), (345, 540)], dashed=True)
    s.section(40, 645, t["s_res"])
    qy = 735
    s.box(40, qy-55, 230, 110, t["trig"], "src")
    s.box(300, qy-55, 190, 110, t["smp"], "proc")
    s.box(520, qy-55, 200, 110, t["trk_r"], "proc")
    s.box(750, qy-55, 170, 110, t["mfx_r"], "bus")
    s.box(950, qy-55, 150, 110, t["comp"], "comp")
    s.box(1130, qy-55, 150, 110, t["wav"], "file")
    s.box(1310, qy-55, 150, 110, t["tpad"], "file")
    for a, b in [(270, 300), (490, 520), (720, 750), (920, 950), (1100, 1130), (1280, 1310)]: hline(s, a, b, qy)
    for i, ln in enumerate(t["notes_mod"]): s.text(40, 850 + i * 26, ln, 14, False, "#334e68")
    return s.render()

if __name__ == "__main__":
    out = sys.argv[1]
    for lang in ("ru", "en"):
        open(f"{out}/signal-path-stock.{lang}.svg", "w", encoding="utf-8").write(stock(lang))
        open(f"{out}/signal-path-ammod.{lang}.svg", "w", encoding="utf-8").write(mod(lang))
    print("ok")
