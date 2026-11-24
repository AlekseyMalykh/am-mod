"""Render real QEMU menu draw calls, with the existing font substitute."""
from pathlib import Path
import subprocess,sys
from PIL import Image
R=Path(__file__).resolve().parent;O=R/'build/arrangement-ui'
frames=[]
for line in (O/'normal-draw.txt-menus.txt').read_text().splitlines():
 if line=='R 0 0 320 240 ff000000':frames.append([])
 if frames:frames[-1].append(line)
assert len(frames)==3
images=[]
for name,lines in zip(('actions-top','actions-bottom','pattern-picker'),frames):
 source=O/(name+'.txt');target=O/(name+'.png');source.write_text('\n'.join(lines)+'\n')
 subprocess.run([sys.executable,str(R/'render_piano_roll.py'),str(source),str(target)],check=True)
 with Image.open(target) as im:images.append(im.copy())
images[0].save(O/'menus-scrollbars.gif',save_all=True,append_images=images[1:],duration=[2400,3000,2200],loop=0,disposal=2)
print(O/'menus-scrollbars.gif')
