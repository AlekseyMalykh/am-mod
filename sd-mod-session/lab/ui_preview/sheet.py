import sys
from PIL import Image
out=sys.argv[1];fs=sys.argv[2:];ims=[Image.open(f) for f in fs];W,H=ims[0].size;c=3;r=(len(ims)+c-1)//c
m=Image.new('RGB',(c*(W+8),r*(H+8)),'gray')
for i,im in enumerate(ims):m.paste(im,((i%c)*(W+8),(i//c)*(H+8)))
m.save(out)
