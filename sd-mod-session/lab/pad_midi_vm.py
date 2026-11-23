"""Disposable full-system lab for native Engine/XPJ checks, never a device.

Run under WSL. Each command checks the private SD image before guest access.
The release envelope supplies its original bootstrap; only the trial .so is
replaced, with launcher checksums updated. No firmware is distributed here.
"""
from pathlib import Path
import argparse, hashlib, io, json, os, sys, tarfile, zipfile
from types import SimpleNamespace

R = Path(__file__).resolve().parent.parent
O = R/'build'/os.environ.get('AM_MOD_VM_BUILD_NAME','pad-midi-vm')
assert O.parent == R/'build'
run_name=os.environ.get('AM_MOD_VM_RUN_NAME','pad-midi-recording')
assert run_name.startswith('pad-midi-recording') and '/' not in run_name and '..' not in run_name
PRIVATE = Path('/opt/mpc-sample-lab/out/run')/run_name
os.environ['PATH'] = '/opt/mpc-sample-lab/qemu/bin:'+os.environ['PATH']
sys.path.insert(0, '/opt/mpc-sample-lab')
import lab
lab.RUN = str(PRIVATE)
lab.SD_IMG = str(PRIVATE/'sdcard.img')
lab.SDCARD = str(PRIVATE/'card-source')
lab.SHARE = str(PRIVATE/'share')
sha = lambda b: hashlib.sha256(b).hexdigest()

def guard():
    assert str(PRIVATE/'sdcard.img') in str(lab.qmp('query-block')), 'Not our private VM'

def package(base):
    proof = json.loads((O/'compile-verification.json').read_text())
    assert proof['passed'] and '-DMPC_PAD_MIDI_VM_PROBE=1' in proof['compiler_flags']
    for name, digest in proof['sources_sha256'].items():
        assert sha((R/name).read_bytes()) == digest, name
    library = (O/'phone-input.so').read_bytes()
    assert sha(library) == proof['library_sha256']
    with zipfile.ZipFile(base) as z:
        files = {n:z.read(n) for n in z.namelist() if n.startswith('autorun/') and not n.endswith('/')}
    launcher = files['autorun/mod-launcher.sh']
    old = sha(files['autorun/phone-input.so']).encode()
    assert launcher.count(old) == 1
    files['autorun/phone-input.so'] = library
    files['autorun/mod-launcher.sh'] = launcher.replace(old, sha(library).encode())
    entry = files['autorun/autorun.sh']
    assert entry.count(sha(launcher).encode()) == 1
    files['autorun/autorun.sh'] = entry.replace(sha(launcher).encode(), sha(files['autorun/mod-launcher.sh']).encode())
    manifest = json.loads(files['autorun/autorun.json'])
    manifest['value0']['scripts'][0]['message'] = 'VM ONLY native pad MIDI recording trial'
    files['autorun/autorun.json'] = (json.dumps(manifest, indent=2)+'\n').encode()
    sys.path.insert(0,str(R))
    from package_r28 import SALT
    files['autorun/autorun.check'] = sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+SALT).encode()
    with zipfile.ZipFile(O/'VM-ONLY-pad-midi.zip','w',zipfile.ZIP_DEFLATED) as z:
        for n,b in files.items(): z.writestr(n,b)
    (O/'envelope.json').write_text(json.dumps({'base_sha256':sha(Path(base).read_bytes()),'library_sha256':sha(library)},indent=2)+'\n')
    print('Packaged VM-only trial', flush=True)

def stage():
    guard()
    assert lab.guest('systemctl show mpc-clone-trial-v1-clone.service -p MainPID --value').strip() == b'0'
    # vvfat's default label contains a space; keep the verified launcher path
    # inside this VM without changing the lab's shared card or mount policy.
    lab.guest('mountpoint -q /media/MPCSAMPLE || { mkdir -p /media/MPCSAMPLE && mount --bind /media/MPC\\ SAMPLE /media/MPCSAMPLE; }',timeout=20)
    with zipfile.ZipFile(O/'VM-ONLY-pad-midi.zip') as z:
        files = {n:z.read(n) for n in z.namelist()}
    assert all(len(Path(n).parts)==2 and '..' not in n for n in files)
    proof=json.loads((O/'compile-verification.json').read_text())
    assert proof['passed'] and sha(files['autorun/phone-input.so'])==proof['library_sha256']
    for name,digest in proof['sources_sha256'].items():assert sha((R/name).read_bytes())==digest,name
    payload=io.BytesIO()
    with tarfile.open(fileobj=payload,mode='w:gz') as tar:
        for name,data in files.items():
            info=tarfile.TarInfo(name);info.size=len(data);info.mode=0o755
            tar.addfile(info,io.BytesIO(data))
    lab.guest('mkdir -p /media/MPCSAMPLE/autorun && tar -xzf - -C /media/MPCSAMPLE && sync',data=payload.getvalue(),timeout=180)
    assert lab.guest('sha256sum /media/MPCSAMPLE/autorun/phone-input.so').split()[0].decode() == sha(files['autorun/phone-input.so'])
    print('Verified private-card library; starting native SD launcher',flush=True)
    print(lab.guest('/bin/sh /media/MPCSAMPLE/autorun/autorun.sh',timeout=180).decode(),flush=True)

p=argparse.ArgumentParser();p.add_argument('command',choices=['boot','package','stage','status','request','capture','shot','guest'])
p.add_argument('value',nargs='?');a=p.parse_args()
if a.command=='boot':
    assert not PRIVATE.is_symlink()
    for port in lab.PORTS.values(): assert not lab.port_open(port), 'Lab port already in use'
    Path(lab.SDCARD,'MPC-Sample/Projects').mkdir(parents=True,exist_ok=True)
    Path(lab.SDCARD,'VM-ONLY.txt').write_text('Disposable pad MIDI native Engine lab. No user projects.\n')
    lab.cmd_run(SimpleNamespace(headless=True,accel='tcg',extra='',no_panel=False))
elif a.command=='package':package(a.value)
elif a.command=='stage':stage()
elif a.command=='guest':
    guard();print(lab.guest(a.value,timeout=45).decode())
elif a.command=='status':
    guard();print(lab.guest('systemctl show ac50.service mpc-clone-trial-v1-clone.service -p ActiveState -p MainPID; tail -25 /run/mpc-clone-trial-v1/events.txt',timeout=20).decode())
elif a.command=='request':
    guard();assert a.value and '\n' not in a.value
    pid=int(lab.guest('systemctl show mpc-clone-trial-v1-clone.service -p MainPID --value',timeout=20));assert pid>1
    path='/proc/'+str(pid)+'/root/run/am-pad-midi-vm'
    lab.guest('cat > '+path+'.tmp && mv '+path+'.tmp '+path+'.request',data=(a.value+'\n').encode(),timeout=20)
elif a.command=='capture':
    import gzip
    guard();name=a.value or 'recorded'
    assert name.replace('-','').isalnum()
    pid=int(lab.guest('systemctl show mpc-clone-trial-v1-clone.service -p MainPID --value',timeout=20))
    assert pid>1
    data=lab.guest('cat /proc/'+str(pid)+'/root/data/TempProjects/Temp-Project.xpj',timeout=30)
    plain=gzip.decompress(data); project=json.loads(plain[plain.index(b'{'):])
    assert isinstance(project.get('data'),dict)
    (O/(name+'.xpj')).write_bytes(data)
    (O/(name+'.json')).write_text(json.dumps(project,indent=2)+'\n')
    print(name,len(data),sha(data))
elif a.command=='shot':
    guard();target=PRIVATE/'screen.ppm'
    lab.qmp('screendump',filename=str(target))
    import shutil
    shutil.copyfile(target,O/'native-screen.ppm')
    print(O/'native-screen.ppm')
