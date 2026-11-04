"""Render actual mixer_view.c painter traces; PCM in the harness is a fixture.
Usage: python render_painter.py path/to/build/mixer-step1 [output-directory]
The PNGs are review artifacts, not hardware screenshots.
"""
from pathlib import Path
import sys
from PIL import Image, ImageDraw, ImageFont
source = Path(sys.argv[1])
destination = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(__file__).parent
destination.mkdir(parents=True, exist_ok=True)
fonts = [Path("C:/Windows/Fonts/consola.ttf"), Path("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf")]
font_path = next(p for p in fonts if p.exists())
for path in source.glob("*.trace"):
    image = Image.new("RGB", (320, 240), "black")
    draw = ImageDraw.Draw(image)
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split(" ", 8)
        kind, x, y, width, height = parts[:5]
        x, y, width, height = map(int, (x, y, width, height))
        if kind == "R":
            draw.rectangle((x, y, x+width-1, y+height-1), fill="#" + parts[5][-6:])
        else:
            size, colour, centre, value = parts[5:]
            font = ImageFont.truetype(str(font_path), int(size))
            layer = Image.new("RGBA", (width, height))
            pen = ImageDraw.Draw(layer)
            length = pen.textlength(value, font=font)
            offset = (width-length)/2 if int(centre) else 0
            pen.text((offset, (height-int(size))/2-1), value, font=font, fill="#"+colour[-6:])
            image.paste(layer, (x, y), layer)
    image.save(destination / ("painter-" + path.stem + ".png"))
