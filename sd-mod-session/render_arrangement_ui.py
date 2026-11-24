"""Render production Song Arrange draw calls from the QEMU fixture.
Layout is unchanged; render_piano_roll.py substitutes the native font.
"""
from pathlib import Path
import subprocess,sys
from PIL import Image
R=Path(__file__).resolve().parent;O=R/'build/arrangement-ui'
frames=[]
for line in (O/'normal-draw.txt').read_text().splitlines():
    if line=='R 0 0 320 240 ff000000':frames.append([])
    if frames:frames[-1].append(line)
names=['empty-arrange','inserted-pattern','chop-at-cursor','copied-to-track','zoom-and-scroll']
assert len(frames)==len(names)
images=[]
for name,lines in zip(names,frames):
    source=O/(name+'.txt');target=O/(name+'.png');source.write_text('\n'.join(lines)+'\n')
    subprocess.run([sys.executable,str(R/'render_piano_roll.py'),str(source),str(target)],check=True)
    with Image.open(target) as image:images.append(image.copy())
images[0].save(O/'workflow.gif',save_all=True,append_images=images[1:],duration=2000,loop=0,disposal=2)
print(O/'workflow.gif')
