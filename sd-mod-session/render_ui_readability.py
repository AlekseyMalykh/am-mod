"""Render real GUI-fixture draw traces; approximate font, no device screenshot."""
from pathlib import Path
import json
import re
from PIL import Image, ImageDraw, ImageFont

R=Path(__file__).resolve().parent
OUT=R/'work/ui-readability'
SCREENS={'steps':'r15-step-draw.txt','mapping':'r15-workflow-draw.txt.batch',
         'query':'r15-workflow-draw.txt','results':'r15-workflow-draw.txt.results',
         'chops':'r15-workflow-draw.txt.chops'}

def parse(path):
    commands=[]
    for line in path.read_text(encoding='utf-8').splitlines():
        if line.startswith('R '):
            _,x,y,w,h,colour=line.split()
            commands.append(dict(kind='R',x=int(x),y=int(y),w=int(w),h=int(h),colour=colour))
        elif line.startswith('T '):
            _,x,y,w,h,colour,size,caption=line.split(' ',7)
            align=0x24
            if caption.startswith('@'):
                value,caption=caption.split(' ',1);align=int(value[1:],16)
            commands.append(dict(kind='T',x=int(x),y=int(y),w=int(w),h=int(h),colour=colour,size=float(size),text=caption,align=align))
    return commands

def render(commands,scale=3):
    picture=Image.new('RGB',(320*scale,240*scale),'black')
    draw=ImageDraw.Draw(picture);draw.rectangle((0,0,320*scale,3*scale-1),fill='#66708a')
    fontpath=next(p for p in [Path('C:/Windows/Fonts/consola.ttf'),Path('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf')] if p.exists())
    for c in commands:
        x,y,w,h=[c[k]*scale for k in ('x','y','w','h')];colour='#'+c['colour'][2:]
        if c['kind']=='R':
            if w and h:draw.rectangle((x,y,x+w-1,y+h-1),fill=colour)
            continue
        font=ImageFont.truetype(str(fontpath),round(c['size']*scale))
        width=max(1,int(draw.textlength(c['text'],font=font)+1));glyph=Image.new('RGBA',(width,h),(0,0,0,0))
        ImageDraw.Draw(glyph).text((0,h/2),c['text'],font=font,fill=colour,anchor='lm')
        factor=min(1,max(.75,w/width))
        if factor<1:glyph=glyph.resize((round(width*factor),h),Image.Resampling.LANCZOS)
        left=0 if c['align']&1 else w-glyph.width if c['align']&2 else (w-glyph.width)//2
        clipped=Image.new('RGBA',(w,h),(0,0,0,0));clipped.alpha_composite(glyph,(left,0))
        picture.paste(clipped,(x,y),clipped)
    return picture

def main():
    data={}
    for name,file in SCREENS.items():
        data[name]={'before':parse(OUT/file),'after':parse(R/'build'/file)}
        for c in data[name]['after']:
            assert c['x']>=0 and c['y']>=5 and c['x']+c['w']<=320 and c['y']+c['h']<=240,(name,c)
            if c['kind']=='T':assert c['size']>=11,(name,c)
        before=render(data[name]['before'],2);after=render(data[name]['after'],2)
        pair=Image.new('RGB',(1300,480),'#202020');pair.paste(before,(0,0));pair.paste(after,(660,0));pair.save(OUT/(name+'-comparison.png'))
    assert any(c.get('text','').startswith('Step ') and c.get('size',0)>=21 for c in data['steps']['after'])
    assert any(c.get('text')=='Snare.wav' and c.get('size')==18 for c in data['results']['after'])
    assert any(c.get('text')=='SOURCE' for c in data['mapping']['after'])
    (OUT/'draw-data.json').write_text(json.dumps(data,ensure_ascii=False),encoding='utf-8')
    fragment=OUT/'mpc-readable-workflows.html'
    source=fragment.read_text(encoding='utf-8')
    serialized=json.dumps(data,ensure_ascii=False).replace('</','<\\/')
    if '__MPC_DRAW_DATA__' in source:source=source.replace('__MPC_DRAW_DATA__',serialized)
    else:source=re.sub(r'const traces = .*?;\n',lambda _: 'const traces = '+serialized+';\n',source,count=1)
    assert len(source.encode())<1_000_000 and '<!doctype' not in source.lower()
    fragment.write_text(source,encoding='utf-8')
    print(json.dumps({'screens':len(data),'native_bounds_checked':True,'hardware_tested':False,'fragment':str(fragment)},ensure_ascii=False))

if __name__=='__main__':main()
