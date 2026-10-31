"""Bounded native splash drawing and timing checks; no framebuffer writes."""
from pathlib import Path
import hashlib,json,subprocess,struct,zlib,tempfile,time
R=Path(__file__).resolve().parent;O=R/'build/mod-splash';O.mkdir(exist_ok=True)
sources=['src/mod_splash.c','src/mod_splash.h','src/boot_splash.c','check_mod_splash.c','verify_mod_splash.py']
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={s:digest(R/s) for s in sources}
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 exe=O/name
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'check_mod_splash.c','src/mod_splash.c','-o',str(exe)],cwd=R,check=True)
 p=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(exe),str(O/(name+'.ppm'))],cwd=R,capture_output=True,text=True,check=True,timeout=30)
 print(p.stdout,end='')
helper=O/'boot-splash'
subprocess.run(['aarch64-linux-gnu-gcc','-O2','-Wall','-Wextra','-Werror','-mbranch-protection=standard','src/boot_splash.c','src/mod_splash.c','-o',str(helper)],cwd=R,check=True)
for ms,expected in [(0,2),(1800,3),(3150,6),(6340,7)]:
 path=O/f'frame-{ms}.png'
 subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(helper),'--frame',str(ms),str(path)],cwd=R,check=True)
 data=path.read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n';pos=8;chunks={}
 while pos<len(data):
  n=struct.unpack_from('>I',data,pos)[0];tag=data[pos+4:pos+8];body=data[pos+8:pos+8+n]
  assert zlib.crc32(tag+body)==struct.unpack_from('>I',data,pos+8+n)[0]
  chunks[tag]=body;pos+=12+n
 assert struct.unpack_from('>II',chunks[b'IHDR'])==(320,240)
 raw=zlib.decompress(chunks[b'IDAT']);assert len(raw)==961*240
 colours={raw[y*961+1+x*3:y*961+4+x*3] for y in range(240) for x in range(320)}
 assert len(colours)==expected
with tempfile.TemporaryDirectory(prefix='mpc-splash-ctl-') as folder:
 folder=Path(folder);ctl=folder/'ctl';log=folder/'calls';exe=folder/'helper'
 ctl.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "'+str(log)+'"\n');ctl.chmod(0o700)
 subprocess.run(['gcc','-O1','-Wall','-Wextra','-Werror','-DBOOT_SPLASH_CTL="'+str(ctl)+'"','src/boot_splash.c','src/mod_splash.c','-o',str(exe)],cwd=R,check=True)
 subprocess.run([str(exe),'--release'],check=True,timeout=3);assert log.read_text().strip()=='drop-display'
 ctl.write_text('#!/bin/sh\nwhile :; do :; done\n');start=time.monotonic()
 p=subprocess.run([str(exe),'--release'],timeout=3);assert p.returncode==1 and time.monotonic()-start<2
print('PASS actual helper PNG CRC/zlib/pixels, native display release receiver, bounded hung display command')
assert before=={s:digest(R/s) for s in sources}
(O/'verification.json').write_text(json.dumps({'passed':True,'hardware_tested':False,'sources_sha256':before},indent=2)+'\n')
