"""Render design illustrations, not runtime telemetry. Requires Pillow.

Run from any directory; outputs are adjacent to this script. All sample gains
and meter peaks below are manually chosen layout fixtures. Native AM Mod must
read its project model and audio-owned bus telemetry instead.
"""

from pathlib import Path
import os

from PIL import Image, ImageDraw, ImageFont


OUT = Path(__file__).resolve().parent
BLACK, WHITE, GREY = "#000000", "#ffffff", "#aaaaaa"
DIM, LINE, AMBER, RED = "#505050", "#303030", "#ffc247", "#ff4040"
COLOURS = ["#32c780", "#39c1d6", "#dc92ff", "#ffc247",
           "#69a5ff", "#e79c6c", "#dc769a", "#a6c855"]
NAMES = ["Kick", "Snr", "Bass", "Hat", "Keys", "Vox", "Perc", "FX"]
GAINS = [-3, -9, -6, 0, -12, -4, -8, -2]
PANS = ["L100", "L50", "C", "R25", "R50", "R75", "R100", "L25"]
PEAKS = [(-15, -16), (-24, -21), (-10, -13), (-60, -60),
         (-20, -18), (-60, -60), (-28, -30), (-1, -3)]


def font(size):
    candidates = [Path(os.environ.get("WINDIR", "C:/Windows")) / "Fonts/consola.ttf",
                  Path("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf")]
    for path in candidates:
        if path.is_file():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default(size=size)


class Canvas:
    def __init__(self):
        self.image = Image.new("RGB", (320, 240), BLACK)
        self.draw = ImageDraw.Draw(self.image)

    def box(self, x, y, w, h, colour, outline=False):
        assert w > 0 and h > 0 and 0 <= x < x+w <= 320 and 0 <= y < y+h <= 240
        rect = (x, y, x+w-1, y+h-1)
        self.draw.rectangle(rect, outline=colour if outline else None,
                            fill=None if outline else colour)

    def text(self, value, x, y, width, size=11, colour=WHITE, centre=False):
        face = font(size)
        # Ellipsize by measured width, not by an assumed number of glyphs.
        original = value
        while value and self.draw.textlength(value, font=face) > width:
            value = value[:-1]
        if value != original and len(value) > 1:
            value = value[:-1] + "~"
        actual = self.draw.textlength(value, font=face)
        assert actual <= width and x >= 0 and x+width <= 320 and y >= 0 and y+size <= 240
        self.draw.text((x+(width-actual)/2 if centre else x, y), value,
                       font=face, fill=colour, anchor="lt")

    def save(self, name):
        self.image.save(OUT / name)


def tabs(c, selected=None):
    for index, name in enumerate(["MIX", "FX", "ROUTE"]):
        x, width = [(0, 106), (106, 107), (213, 107)][index]
        c.text(name, x, 9, width, 14, AMBER if index == selected else WHITE, True)
        if index == selected:
            c.box(x+8, 27, width-16, 1, AMBER)


def gain_y(gain, top, bottom):
    # Separate finite-gain mapping from meter dBFS mapping; unity is explicit.
    if gain is None:
        return bottom
    unity = top+12
    if gain >= 0:
        return round(unity-min(gain, 6)*2)
    return round(unity+min(-gain, 60)*(bottom-unity-3)/60)


def fader(c, x, top, bottom, gain, selected):
    unity = gain_y(0, top, bottom)
    c.box(x+9, top, 2, bottom-top+1, DIM)
    c.box(x+2, unity, 18, 1, GREY)
    c.text("0", x-3, unity-4, 7, 8, GREY)
    y = gain_y(gain, top, bottom)
    c.box(x+3, y-3, 15, 7, AMBER if selected else GREY)
    c.box(x+4, y, 13, 1, BLACK)


def meters(c, x, top, bottom, peaks, width=3):
    for channel, peak in enumerate(peaks):
        left = x+channel*(width+1)
        c.box(left, top, width, bottom-top+1, LINE)
        height = round((bottom-top+1)*max(0, min(1, (peak+60)/60)))
        if height:
            c.box(left, bottom-height+1, width, height, WHITE)
        c.box(left, top, width, 1, RED if peak >= 0 else DIM)


def overview():
    c = Canvas()
    tabs(c)
    c.text("MIXER dB 2 SEL", 4, 34, 150, 11)
    c.text("MASTER -1.0 dB", 164, 34, 152, 11, GREY)
    for i, (name, gain, peaks) in enumerate(zip(NAMES, GAINS, PEAKS)):
        x, chosen = i*40, i in (2, 4)
        c.box(x, 50, 1, 148, LINE)
        c.box(x+3, 50, 34, 2, COLOURS[i])
        if chosen:
            c.box(x+1, 49, 38, 148, COLOURS[i], outline=True)
        c.text(chr(ord("A")+i) + ("*" if i == 2 else ""), x+3, 54, 34, 11,
               WHITE, True)
        c.text(name, x+3, 68, 34, 10, WHITE, True)
        c.text(f"{gain:.1f}", x+3, 80, 34, 10, AMBER if chosen else WHITE, True)
        c.text(PANS[i], x+3, 92, 34, 8, WHITE, True)
        fader(c, x+4, 102, 177, gain, chosen)
        meters(c, x+29, 102, 177, peaks)
        for j, letter in enumerate("MSP"):
            lit = (letter == "M" and i == 5) or (letter == "P" and i == 7)
            c.text(letter, x+5+j*11, 184, 10, 9,
                   (RED if letter == "P" else AMBER) if lit else DIM)
    c.box(0, 198, 320, 1, LINE)
    footer(c, [("Pan C", "C"), ("Send C", "--"), ("Context", "--")])
    c.save("overview.png")


def footer(c, values, split=False):
    start, width = (88, 75) if split else (0, 106)
    for i, (label, value) in enumerate(values):
        x = start+i*width
        c.text(label, x+2, 203, width-4, 10, AMBER, not split)
        c.text(value, x+2, 219, width-4, 12, WHITE, not split)


def detail_base(tab, master=False, title=""):
    c = Canvas()
    tabs(c, tab)
    c.box(79, 34, 1, 164, LINE)
    c.box(0, 198, 320, 1, LINE)
    colour = AMBER if master else COLOURS[2]
    c.box(4, 34, 71, 2, colour)
    c.text("MASTER" if master else "C BASS", 4, 42, 71, 12, WHITE, True)
    c.text("-1.0 dB" if master else "-6.0 dB", 4, 62, 71, 12, AMBER, True)
    c.text("C", 4, 78, 71, 8, WHITE, True)
    fader(c, 14, 92, 175, -1 if master else -6, True)
    meters(c, 53, 92, 175, (-4, -6) if master else PEAKS[2], width=5)
    for j, letter in enumerate("MSP" if not master else "P"):
        c.text(letter, 15+j*19, 185, 16, 10, DIM)
    c.text(title, 88, 37, 224, 13, WHITE)
    return c


def row(c, y, label, value="", active=False):
    if active:
        c.box(86, y-3, 228, 23, LINE)
        c.box(86, y-3, 2, 23, AMBER)
    c.text(label, 92, y, 142, 12, AMBER if active else WHITE)
    c.text(value, 237, y, 74, 11, GREY)


def details():
    c = detail_base(0, title="CHANNEL MIX")
    row(c, 67, "Pan", "C", True)
    row(c, 93, "Send", "--")
    row(c, 119, "Mute", "Off")
    row(c, 145, "Solo", "Off")
    row(c, 171, "FX Bypass", "Off")
    footer(c, [("Pan", "C"), ("Send", "--"), ("Colour", "Violet")], split=True)
    c.save("mix.png")

    c = detail_base(1, title="FX CHAIN")
    row(c, 67, "1  EQ", "On", True)
    row(c, 93, "2  Compressor", "Bypass")
    row(c, 119, "3  Empty")
    row(c, 145, "4  Empty")
    row(c, 171, "5  Empty")
    footer(c, [("Slot", "1 / 5"), ("State", "On"), ("Action", "Edit")], split=True)
    c.save("fx.png")

    c = detail_base(2, title="ROUTE  ALL BANKS")
    row(c, 67, "A03 Bass", "Track C", True)
    row(c, 93, "B07 Sub", "Track C")
    row(c, 119, "C02 Layer", "Track C")
    c.text("3 sounds -> Track C", 88, 156, 224, 11, GREY)
    c.text("PAD FX: assign sounds", 88, 178, 224, 10, GREY)
    footer(c, [("Bank", "All"), ("Sound", "A03"), ("Output", "Track C")], split=True)
    c.save("route.png")

    c = detail_base(1, master=True, title="MASTER FX / EQ")
    c.text("KNOB FX   1/2   ON", 88, 61, 224, 11, AMBER)
    row(c, 89, "Frequency", "800 Hz", True)
    row(c, 115, "Gain", "+1.5 dB")
    row(c, 141, "Q", "0.71")
    c.text("Encoder: parameter page", 88, 178, 224, 10, GREY)
    footer(c, [("Freq", "800 Hz"), ("Gain", "+1.5 dB"), ("Q", "0.71")], split=True)
    c.save("master-fx.png")


if __name__ == "__main__":
    overview()
    details()
    print("Rendered 5 static layout illustrations at 320x240; no runtime PCM data.")
