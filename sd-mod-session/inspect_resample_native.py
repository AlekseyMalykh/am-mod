"""Read-only RTTI/callsite map for the pinned MPC Sample1.3.0.12 ELF."""
from pathlib import Path
from hashlib import sha256
import struct,json,array,subprocess
R=Path(__file__).resolve().parent
data=(R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample').read_bytes()
assert sha256(data).hexdigest()=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
phoff=struct.unpack_from('<Q',data,32)[0];phsize,phcount=struct.unpack_from('<HH',data,54)
segments=[struct.unpack_from('<IIQQQQQQ',data,phoff+i*phsize) for i in range(phcount)]
def offset(address):
 for kind,flags,file,start,physical,size,memory,align in segments:
  if kind==1 and start<=address<start+size:return file+address-start
 raise ValueError(hex(address))
def u64(address):return struct.unpack_from('<Q',data,offset(address))[0]
def string(address):
 pos=offset(address);return data[pos:data.index(0,pos)].decode('utf-8','replace')
tables={}
for address in [0x6dbd868,0x6dbd458,0x6dbd5f0,0x6dbd2f0,0x6d972e0,0x6e414e0]:
 rtti=u64(address-8);name=string(u64(rtti+8))
 tables[hex(address)]={'type':subprocess.check_output(['c++filt','-t',name],text=True).strip(),'entries':[hex(u64(address+i*8)) for i in range(12)]}
targets={0x2942150,0x2943b40,0x293c1a0,0x292e310,0x2944be0,0x2931440,0x2930880,0x292ed50,0x1d94720,0x2943820,0x209b1f0,0x1df36a0}
refs={hex(target):[] for target in targets}
for kind,flags,file,start,physical,size,memory,align in segments:
 if kind!=1 or not flags&1:continue
 words=array.array('I',data[file:file+size//4*4])
 for i,word in enumerate(words):
  if word&0xfc000000!=0x94000000:continue
  imm=word&0x3ffffff;imm=imm-(1<<26) if imm&(1<<25) else imm
  target=start+i*4+imm*4
  if target in targets:refs[hex(target)].append(hex(start+i*4))
report={'stock_sha256':sha256(data).hexdigest(),'vtables':tables,'bl_callsites':refs,'static_audit_only':True}
out=R/'build/resample/native-map.json';out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
