"""Render actual isolated UI drawing calls; font substitution only."""
from pathlib import Path
import subprocess,sys
from PIL import Image
R=Path(__file__).resolve().parent;O=R/'build/pattern-tools'
trace=(O/'ui-draw.txt').read_text().splitlines();frames=[]
for line in trace:
    if line=='R 0 0 320 240 ff000000':frames.append([])
    if frames:frames[-1].append(line)
names=['parts-preview','pattern-edit','extract-preview','empty-pattern']
images=[]
for i,lines in enumerate(frames):
    name=names[i] if i<len(names) else 'frame-'+str(i)
    source=O/(name+'.txt');target=O/(name+'.png');source.write_text('\n'.join(lines)+'\n')
    subprocess.run([sys.executable,str(R/'render_piano_roll.py'),str(source),str(target)],check=True)
    with Image.open(target) as image:images.append(image.copy())
assert len(images)>=4
images[0].save(O/'workflow.gif',save_all=True,append_images=images[1:],duration=2200,loop=0,disposal=2)
print(O/'workflow.gif')
