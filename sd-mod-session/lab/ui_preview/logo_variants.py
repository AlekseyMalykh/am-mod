from PIL import Image,ImageDraw,ImageFont
G='/usr/share/fonts/truetype/google-fonts/'
BG=(9,12,16);WHITE=(244,240,232);AMBER=(255,194,71)
def lora(italic,weight,size):
    f=ImageFont.truetype(G+('Lora-Italic-Variable.ttf' if italic else 'Lora-Variable.ttf'),size)
    try:f.set_variation_by_axes([weight])
    except Exception:pass
    return f
def make(variant):
    im=Image.new('RGB',(320*4,240*4),BG);d=ImageDraw.Draw(im);S=4
    if variant=='a':  # Lora italic Am + Lora semibold MOD, shared baseline
        f1=lora(True,500,92*S//4*1);f2=lora(False,600,46*S//4*1)
        f1=lora(True,500,88);f2=lora(False,700,50)
        parts=[('Am',f1*1 if False else f1,WHITE,0),('MOD',f2,AMBER,26)]
    elif variant=='b':  # Poppins geometric
        f1=ImageFont.truetype(G+'Poppins-Medium.ttf',60);f2=ImageFont.truetype(G+'Poppins-Bold.ttf',60)
        parts=[('Am',f1,WHITE,0),('MOD',f2,AMBER,14)]
    else:  # Lora italic Am + Poppins bold MOD tracked
        f1=lora(True,500,96);f2=ImageFont.truetype(G+'Poppins-SemiBold.ttf' if False else G+'Poppins-Bold.ttf',40)
        parts=[('Am',f1,WHITE,0),('M O D',f2,AMBER,22)]
    # scale fonts x S
    scaled=[]
    for t,f,c,gap in parts:
        f=f.font_variant(size=f.size*S);scaled.append((t,f,c,gap*S))
    widths=[d.textlength(t,font=f) for t,f,c,g in scaled];total=sum(widths)+sum(g for *_,g in scaled)
    x=(im.width-total)/2;base=128*S
    for (t,f,c,gap),w in zip(scaled,widths):
        x+=gap;d.text((x,base),t,font=f,fill=c,anchor='ls');x+=w
    small=im.resize((320,240),Image.LANCZOS);return small
for v in 'abc':make(v).save('lab/ui_preview/out/logo-%s.png'%v)
