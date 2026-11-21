"""Disposable VM snapshot for the Internal storage candidate; never a physical card."""
from pathlib import Path
import argparse,json,os,socket,subprocess,sys,zipfile
R=Path(__file__).resolve().parent.parent
VERSION=sys.argv[2] if len(sys.argv)>2 else 'r30c-internal-dev1'
assert VERSION.startswith('r30c-') and all(c.isalnum() or c=='-' for c in VERSION)
LAB=Path('/opt/mpc-sample-lab');RUN=LAB/'out/run'/VERSION;BASE=LAB/'out/run/piano-roll-dev'
sys.path.insert(0,str(LAB));import lab
mode=sys.argv[1]
if mode=='stage':
 assert os.geteuid()==0 and not lab.port_open(lab.PORTS['qmp'])
 assert RUN.resolve()==RUN and not RUN.is_symlink()
 new=not RUN.exists();RUN.mkdir(exist_ok=True)
 if new:
  for name in ['emmc.qcow2','sdcard.img']:
   source=BASE/name;assert source.is_file() and not source.is_symlink()
   subprocess.run(['cp','--reflink=auto','--sparse=always',str(source),str(RUN/name)],check=True)
  (RUN/'internal-test-snapshot.json').write_text(json.dumps(dict(source=str(BASE),purpose='private Internal storage VM only'))+'\n')
 assert (RUN/'internal-test-snapshot.json').is_file()
 mount=RUN/'card-stage';mount.mkdir(exist_ok=True)
 assert not mount.is_symlink() and not os.path.ismount(mount) and not any(mount.iterdir())
 card=RUN/'sdcard.img';assert card.resolve()==card
 table=json.loads(subprocess.check_output(['sfdisk','--json',str(card)],text=True))['partitiontable']
 assert table['sectorsize']==512 and len(table['partitions'])==1
 partition=table['partitions'][0];assert partition['start']==2048
 subprocess.run(['mount','-t','exfat','-o',f"loop,offset={partition['start']*512},sizelimit={partition['size']*512}",str(card),str(mount)],check=True)
 try:
  autorun=mount/'autorun';backup=mount/'autorun-before-internal-dev1'
  if autorun.exists():
   assert autorun.resolve().parent==mount and not autorun.is_symlink()
   if not backup.exists():autorun.rename(backup)
  autorun.mkdir(exist_ok=True)
  package=R/'build'/VERSION/('am-mod-'+VERSION+'.zip')
  with zipfile.ZipFile(package) as z:
   for n in z.namelist():
    if not n.startswith('autorun/') or n.endswith('/'):continue
    relative=Path(n);assert len(relative.parts)==2 and relative.name not in ('.','..')
    target=autorun/relative.name;assert not target.is_symlink();target.write_bytes(z.read(n));assert target.read_bytes()==z.read(n)
  if VERSION.startswith('r30c-mic-probe-'):
   preferences=mount/'MPC-Sample/Projects';assert preferences.is_dir() and not preferences.is_symlink()
   (preferences/'.am-mod-fx-mode').write_text('AM_FXMODE '+('0' if VERSION.endswith('-dry') else '1')+'\n')
  os.sync()
 finally:subprocess.run(['umount',str(mount)],check=True)
 owner=R.stat();os.chown(RUN,owner.st_uid,owner.st_gid);os.chown(mount,owner.st_uid,owner.st_gid)
 for name in ['emmc.qcow2','sdcard.img','internal-test-snapshot.json']:os.chown(RUN/name,owner.st_uid,owner.st_gid)
 print('Staged disposable Internal VM snapshot; original VM and physical card untouched.',flush=True)
elif mode=='run':
 assert (RUN/'internal-test-snapshot.json').is_file()
 lab.RUN=str(RUN);lab.SD_IMG=str(RUN/'sdcard.img')
 qemu=LAB/'qemu/bin/qemu-system-aarch64';assert qemu.is_file()
 original_tool=lab.tool
 lab.tool=lambda name:str(qemu) if name=='qemu-system-aarch64' else original_tool(name)
 lab.cmd_run(argparse.Namespace(headless=True,no_panel=False,accel='tcg',extra=''))
else:raise SystemExit('stage | run')
