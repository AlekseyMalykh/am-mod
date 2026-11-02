"""Build the owned Project menu payload; preserve frozen baseline byte-for-byte."""
from pathlib import Path
import hashlib,subprocess,struct
from phone_build import PROJECT_REQUEST_VA
R=Path(__file__).resolve().parent;O=R/'build/pad-lighting';O.mkdir(exist_ok=True)
def branch(a,b):return struct.pack('<I',0x14000000|(((b-a)//4)&0x3ffffff))
arrays={}
for name,flags in [('before',[]),('after',['-DPAD_LIGHTING_MENU=1'])]:
 obj=O/(name+'.o');elf=O/(name+'.elf');blob=O/(name+'.bin')
 subprocess.run(['aarch64-linux-gnu-gcc','-DNOTE_UI',*flags,'-c','src/project.S','-o',str(obj)],cwd=R,check=True)
 subprocess.run(['aarch64-linux-gnu-ld','--defsym=music_project_bridge='+hex(PROJECT_REQUEST_VA),'-T','src/project.ld',str(obj),'-o',str(elf)],cwd=R,check=True)
 subprocess.run(['aarch64-linux-gnu-objcopy','-O','binary','--only-section=.text',str(elf),str(blob)],cwd=R,check=True)
 syms={v[2]:int(v[0],16) for l in subprocess.check_output(['aarch64-linux-gnu-nm',str(elf)],text=True).splitlines() if len(v:=l.split())==3}
 data=blob.read_bytes();assert len(data)<0x400
 if name=='before':assert hashlib.sha256(data).hexdigest()=='1b43d37abec41cebf440ac882ed1b6506baba7ad1dd21577511ac8876acfcf0b'
 arrays['project_'+name]=data
 arrays['dispatch_'+name]=branch(0x1643908,syms['project_dispatch'])
arrays['rows_before']=struct.pack('<I',0xd28000c1);arrays['rows_after']=struct.pack('<I',0xd28000c1) # six visible rows, seven model items: native viewport scrolls
text='/* Generated solely from owned project.S; cold-start RAM patch. */\n'
for name,data in arrays.items():text+='static const unsigned char lighting_'+name+'[]={'+','.join(hex(b) for b in data)+'};\n'
(R/'build/pad_lighting_project.h').write_text(text)
print('Project lighting payload: frozen6-row baseline verified; new7-row payload generated')
