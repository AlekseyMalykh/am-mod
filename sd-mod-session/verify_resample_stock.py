"""Pinned native export proof. Does not package or install a mod."""
from pathlib import Path
import hashlib,json,subprocess
root=Path(__file__).resolve().parent
stock=root.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample'
assert hashlib.sha256(stock.read_bytes()).hexdigest()=='b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
out=root/'build/resample-stock';out.mkdir(exist_ok=True)
sources=['src/pad_palette.h','src/pad_workflow_colour.h','src/resample_stock.cc','src/resample_stock.h','src/resample_stock_backend.cc','src/resample_stock_backend.h','src/resample_simple_ui.cc','src/resample_destination.c','src/resample_destination.h','check_resample_simple_ui.c','check_resample_stock.c','check_resample_stock_callback.cc','check_resample_stock_queue.cc','check_resample_stock_backend.c','check_resample_stock_sample.S','check_resample_trigger.c','check_resample_route.c','check_fx_native.c','verify_resample_stock.py']
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={s:digest(root/s) for s in sources}
for s in ['src/resample_file_probe.h','check_resample_file.cc']:before[s]=digest(root/s);sources.append(s)
for name,flags in [('normal',[]),('ubsan',['-fsanitize=undefined','-static-libubsan','-fno-sanitize-recover=all'])]:
 target=out/name
 file_check=out/(name+'-file')
 subprocess.run(['aarch64-linux-gnu-g++','-std=c++11','-O1','-Wall','-Wextra','-Werror',*flags,'check_resample_file.cc','-o',str(file_check)],cwd=root,check=True)
 subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(file_check)],cwd=root,check=True)
 obj=out/(name+'.o')
 helper=out/(name+'-callback.o')
 subprocess.run(['aarch64-linux-gnu-g++','-O1','-g','-std=c++11','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'-c','check_resample_stock_callback.cc','-o',str(helper)],cwd=root,check=True)
 subprocess.run(['aarch64-linux-gnu-g++','-O1','-g','-std=c++11','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'-c','src/resample_stock.cc','-o',str(obj)],cwd=root,check=True)
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'check_resample_stock.c',str(obj),str(helper),'-lstdc++','-pthread','-ldl','-lm','-o',str(target)],cwd=root,check=True)
 result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],cwd=root,capture_output=True,text=True,timeout=30)
 (out/(name+'.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='');result.check_returncode()
 trigger=out/(name+'-trigger')
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'check_resample_trigger.c',str(obj),'-lstdc++','-pthread','-ldl','-lm','-o',str(trigger)],cwd=root,check=True)
 result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(trigger),str(stock)],cwd=root,capture_output=True,text=True,timeout=30)
 (out/(name+'-trigger.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='');result.check_returncode()
 route=out/(name+'-route')
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'check_resample_route.c',str(obj),'-lstdc++','-pthread','-ldl','-lm','-o',str(route)],cwd=root,check=True)
 result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(route),str(stock)],cwd=root,capture_output=True,text=True,timeout=30)
 (out/(name+'-route.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='');result.check_returncode()
 backend=out/(name+'-backend.o');queue=out/(name+'-queue.o')
 for source,dest in [('src/resample_stock_backend.cc',backend),('check_resample_stock_queue.cc',queue)]:
  subprocess.run(['aarch64-linux-gnu-g++','-O1','-g','-std=c++11','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-DMPC_PAD_EQ=1',*flags,'-c',source,'-o',str(dest)],cwd=root,check=True)
 target=out/(name+'-backend')
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-DMPC_PAD_EQ=1',*flags,'check_resample_stock_backend.c','check_resample_stock_sample.S','src/pad_source_abi.S',str(obj),str(helper),str(backend),str(queue),'-lstdc++','-pthread','-ldl','-lm','-o',str(target)],cwd=root,check=True)
 result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],cwd=root,capture_output=True,text=True,timeout=30)
 (out/(name+'-backend.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='');result.check_returncode()
 ui=out/(name+'-ui.o')
 subprocess.run(['aarch64-linux-gnu-g++','-O1','-g','-std=c++11','-Wall','-Wextra','-Werror','-mbranch-protection=standard',*flags,'-c','src/resample_simple_ui.cc','-o',str(ui)],cwd=root,check=True)
 target=out/(name+'-ui')
 subprocess.run(['aarch64-linux-gnu-gcc','-O1','-g','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-DMPC_PAD_EQ=1',*flags,'check_resample_simple_ui.c','check_resample_stock_sample.S','src/pad_source_abi.S','src/resample_destination.c',str(obj),str(helper),str(backend),str(queue),str(ui),'-lstdc++','-pthread','-ldl','-lm','-o',str(target)],cwd=root,check=True)
 result=subprocess.run(['qemu-aarch64','-L','/home/user/mpc-rootfs-user-20260808',str(target),str(stock)],cwd=root,capture_output=True,text=True,timeout=30)
 (out/(name+'-ui.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='');result.check_returncode()
assert before=={s:digest(root/s) for s in sources}
(out/'verification.json').write_text(json.dumps({'passed':True,'ubsan':True,'hardware_tested':False,'host_connected':False,'audio_rendered':False,'card_written':False,'scope':'real native export wrapper ABI and single-pad settings before background task submission','sources_sha256':before},indent=2)+'\n')
