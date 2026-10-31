"""Build a lab payload over frozen r22c. No physical SD access or release claim."""
from pathlib import Path
import hashlib,json,zipfile,subprocess
R=Path(__file__).resolve().parent
O=R/'build/power-refresh';A=O/'autorun';A.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
archive=R.parent/'mpc-sample-r22c-seq-lights.zip'
assert sha(archive.read_bytes())=='9b3168a70cc826b40372eb1aa8f88fb0d319586dc379465329fbf67d81f7999c'
with zipfile.ZipFile(archive) as z:old={n:z.read(n) for n in z.namelist()}
files=dict(old);lib=(O/'phone-input.so').read_bytes();files['autorun/phone-input.so']=lib
compiled=json.loads((O/'compile-verification.json').read_text())
power=json.loads((R/'build/power-button/verification.json').read_text())
assert compiled['passed'] and compiled['full_preload_constructor'] and compiled['library_sha256']==sha(lib)
assert power['passed']
for report in (compiled,power):
    for n,h in report['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
launcher=old['autorun/mod-launcher.sh'].decode();template=(R/'src/autorun.template.sh').read_text()
def once(before,after):
    global launcher
    assert launcher.count(before)==1,repr(before[:100])
    launcher=launcher.replace(before,after)
once('SAVE_IO_SHA='+sha(old['autorun/phone-input.so']),'SAVE_IO_SHA='+sha(lib))
old_flush='        "$R/stage-data" --flush-sd >> "$R/events.txt" 2>&1 || log SD_SAVE_FLUSH_FAILED'
new_flush=template[template.index('        if ! "$R/stage-data" --flush-sd'):template.index('\n    fi\n    data_layer_stop',template.index('        if ! "$R/stage-data" --flush-sd'))]
once(old_flush,new_flush)
start=template.index('    # The clone can request this only after')
end=template.index('    if [ -f "$R/switch-intent" ]; then',start)
once('    data_layer_stop || fail DATA_LAYER_STILL_MOUNTED\n','    data_layer_stop || fail DATA_LAYER_STILL_MOUNTED\n'+template[start:end])
start=template.index('        if [ -f "$R/app/power-off.request" ]')
end=template.index('        if [ -f "$R/app/mod-off.request" ]',start)
anchor='        storage_host_check || { log SD_SAVE_IDENTITY_LOST; break; }\n'
once(anchor,anchor+template[start:end])
once('DevicePolicy=closed','BindReadOnlyPaths=/run/udev/data:/run/udev/data\nDevicePolicy=closed')
files['autorun/mod-launcher.sh']=launcher.encode()
entry=old['autorun/autorun.sh'].decode();assert entry.count(sha(old['autorun/mod-launcher.sh']))==1
entry=entry.replace(sha(old['autorun/mod-launcher.sh']),sha(files['autorun/mod-launcher.sh'])).replace('revision=r22c-seq-lights','revision=r23-power-off')
files['autorun/autorun.sh']=entry.encode()
manifest=json.loads(old['autorun/autorun.json']);script=manifest['value0']['scripts'][0]
script['message']='Start r23: Power button saves a separate SD project copy before shutdown; Seq and workflow lights updated.'
script['yesButtonText']='Start r23'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
for n,b in files.items():
    if n.startswith('autorun/'):
        p=O/n;p.parent.mkdir(exist_ok=True);p.write_bytes(b)
for n in ['autorun.sh','mod-launcher.sh']:subprocess.run(['sh','-n',str(A/n)],check=True)
(O/'candidate-payload.json').write_text(json.dumps(dict(revision='r23-power-off',library_sha256=sha(lib),payload_sha256={n[8:]:sha(b) for n,b in files.items() if n.startswith('autorun/')},full_vm_verified=False,hardware_tested=False,physical_card_written=False),indent=2)+'\n')
print('Lab payload only:',sha(lib))
