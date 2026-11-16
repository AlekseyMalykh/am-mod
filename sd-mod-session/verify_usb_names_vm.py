"""Boot the exact, unmodified MPC kernel on QEMU with a RAM-only USB fixture.

The test console and dummy controller are never included in an MPC package.
No emulated SD, physical USB passthrough, networking or host disks are attached.
"""
from pathlib import Path
import gzip,hashlib,json,os,struct,subprocess
from build_usb_names import ROOT,KERNEL_RELEASE,sha

O=ROOT/'build/usb-names/vm';O.mkdir(parents=True,exist_ok=True)
K=Path('/home/user/am-mod-kernel-sources/kernel-source')
function=K/'drivers/usb/gadget/udc/dummy_hcd.c'
# Stock dummy_hcd deliberately does not advertise ISO transfer support. This
# fixture tests EP0 descriptors and lifetimes only: enable ISO endpoint
# allocation, but never load host snd-usb-audio or start an ISO transfer.
dummy=function.read_text()
old='#define TYPE_BULK_OR_INT\t(USB_EP_CAPS_TYPE_BULK | USB_EP_CAPS_TYPE_INT)'
assert dummy.count(old)==1
dummy=dummy.replace(old,old[:-1]+' | USB_EP_CAPS_TYPE_ISO)')
(O/'dummy_hcd.c').write_text(dummy)
(O/'am_vm_console.c').write_bytes((ROOT/'src/usb_names_vm_console.c').read_bytes())
(O/'Makefile').write_text('obj-m += am_vm_console.o dummy_hcd.o\n')
subprocess.run(['make','-C',str(K),'ARCH=arm64','CROSS_COMPILE=aarch64-linux-gnu-',f'M={O}','KBUILD_MODPOST_WARN=1','modules'],check=True)

fit=ROOT.parent/'extracted_v1.3.0/partitions/part2/kernel.fit'
b=fit.read_bytes();h=struct.unpack_from('>10I',b);assert h[0]==0xd00dfeed
pos=h[2];strings=b[h[3]:h[3]+h[8]];stack=[];props={}
while True:
    token=struct.unpack_from('>I',b,pos)[0];pos+=4
    if token==1:
        end=b.index(0,pos);stack.append(b[pos:end].decode());pos=(end+4)&~3
    elif token==2:stack.pop()
    elif token==3:
        size,nameoff=struct.unpack_from('>II',b,pos);pos+=8
        name=strings[nameoff:strings.index(0,nameoff)].decode();value=b[pos:pos+size];pos=(pos+size+3)&~3
        if '/'.join(stack)=='/images/kernel':props[name]=value
    elif token==4:pass
    elif token==9:break
    else:raise ValueError(token)
assert props['compression']==b'none\0'
offset=struct.unpack('>I',props['data-position'])[0] if 'data-position' in props else ((h[1]+3)&~3)+struct.unpack('>I',props['data-offset'])[0]
image=b[offset:offset+struct.unpack('>I',props['data-size'])[0]]
assert len(image)>20_000_000 and struct.unpack_from('<I',image,56)[0]==0x644d5241
(O/'Image').write_bytes(image)

ram=O/'initramfs';ram.mkdir(exist_ok=True)
for name in ['dev','proc','sys']:(ram/name).mkdir(exist_ok=True)
subprocess.run(['aarch64-linux-gnu-gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-static',str(ROOT/'check_usb_names_vm.c'),'-o',str(ram/'init')],check=True)
for source in [O/'am_vm_console.ko',O/'dummy_hcd.ko',O.parent/'am_usb_names.ko']:(ram/source.name).write_bytes(source.read_bytes())
files=['.']+[str(p.relative_to(ram)) for p in sorted(ram.rglob('*'))]
cpio=subprocess.run(['cpio','--null','-o','--format=newc'],cwd=ram,input=('\0'.join(files)+'\0').encode(),stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True).stdout
(O/'initramfs.cpio.gz').write_bytes(gzip.compress(cpio,mtime=0))
command=['qemu-system-aarch64','-M','virt,gic-version=3','-cpu','cortex-a57','-m','1024','-smp','2',
         '-nographic','-nodefaults','-monitor','none','-serial','stdio','-no-reboot',
         '-kernel',str(O/'Image'),'-initrd',str(O/'initramfs.cpio.gz'),'-append','rdinit=/init panic=-1']
try:
    result=subprocess.run(command,capture_output=True,text=True,timeout=90)
    output=result.stdout+result.stderr
except subprocess.TimeoutExpired as exc:
    output=(exc.stdout or b'').decode(errors='replace')+(exc.stderr or b'').decode(errors='replace')
    (O/'qemu.log').write_text(output)
    raise
(O/'qemu.log').write_text(output);print(output)
assert result.returncode==0,result.returncode
assert 'AM_VM PASS USB_NAMES_VM' in output
assert output.count('PASS 18 actual USB string descriptors')==2
assert output.count('PASS stock stereo descriptors after module removal')==2
assert 'BUG:' not in output and 'Oops:' not in output and 'WARNING:' not in output
report=dict(passed=True,exact_firmware_kernel=True,kernel_image_sha256=hashlib.sha256(image).hexdigest(),
            kernel_release=KERNEL_RELEASE,module_sha256=sha(O.parent/'am_usb_names.ko'),
            actual_usb_descriptor_names_verified=True,repeated_load_unload=True,stock_restored=True,
            active_module_unload_refused=True,hardware_tested=False,
            ep0_only=True,iso_audio_streaming_tested=False,test_controller_iso_allocation_enabled=True,
            sources_sha256={n:sha(ROOT/n) for n in ['verify_usb_names_vm.py','check_usb_names_vm.c','src/usb_names_vm_console.c']})
(O.parent/'vm-verification.json').write_text(json.dumps(report,indent=2)+'\n')
