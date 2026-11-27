"""Package a verified release payload in the standard Am MOD layout (autorun/ + docs + SOURCE).
Never installs, writes a card or publishes. Output: <out>/am-mod-<version>.zip, SHA256SUMS.txt,
package-verification.json. Mod journals move to the mod folder's Logs/ subfolder."""
from pathlib import Path,PurePosixPath
import argparse,hashlib,io,json,re,subprocess,sys,tempfile,zipfile
from usb_drive_boot_package import transform as usb_boot_transform
R=Path(__file__).resolve().parent;ROOT=R.parent
sys.path.insert(0,str(ROOT/'installer'))
from mpc_mod_installer import Package,AUTORUN_CHECK_SALT
sha=lambda b:hashlib.sha256(b).hexdigest()
LOGS=('MPC_MOD_ENTRY.txt','MPC_MOD_CONSOLE.txt','MPC_MOD_EVENTS.txt','MPC_CLONE_TRIAL.txt','MPC_CLONE_ALREADY_ATTEMPTED.txt')

def once(text,old,new):
    assert text.count(old)==1,(old,text.count(old));return text.replace(old,new)

def logs_entry(text):
    """autorun.sh: journals in $DIR/Logs; earlier copies beside the mod files are removed (known names only)."""
    old='ENTRY="$DIR/MPC_MOD_ENTRY.txt"\nCONSOLE="$DIR/MPC_MOD_CONSOLE.txt"\n'
    new=('LOGS="$DIR/Logs"\n[ ! -L "$LOGS" ] || exit 1\nmkdir -p "$LOGS" || exit 1\n'
         '# r31a: journals live in Logs/. Remove the copies earlier releases kept beside the mod files.\n'
         'for am_old_log in '+' '.join(LOGS)+'; do\n'
         '    if [ -f "$DIR/$am_old_log" ] && [ ! -L "$DIR/$am_old_log" ]; then rm -f "$DIR/$am_old_log"; fi\n'
         'done\n'
         'ENTRY="$LOGS/MPC_MOD_ENTRY.txt"\nCONSOLE="$LOGS/MPC_MOD_CONSOLE.txt"\n')
    return once(text,old,new)

def logs_launcher(text):
    text=once(text,'[ ! -L "$event_dir/MPC_MOD_EVENTS.txt" ] || return 1','[ ! -L "$event_dir/Logs" ] && [ ! -L "$event_dir/Logs/MPC_MOD_EVENTS.txt" ] || return 1')
    text=once(text,'printf \'%s\\n\' "$1" >> "$event_dir/MPC_MOD_EVENTS.txt" || return 1','printf \'%s\\n\' "$1" >> "$event_dir/Logs/MPC_MOD_EVENTS.txt" || return 1')
    text=once(text,'status_sd=${status_report%/autorun/MPC_CLONE_TRIAL.txt}','status_sd=${status_report%/autorun/Logs/MPC_CLONE_TRIAL.txt}')
    text=once(text,'    } > "$DIR/MPC_CLONE_ALREADY_ATTEMPTED.txt"','    } > "$DIR/Logs/MPC_CLONE_ALREADY_ATTEMPTED.txt"')
    text=once(text,'if ! mkdir "$R"; then','{ [ ! -L "$DIR/Logs" ] && mkdir -p "$DIR/Logs"; } || exit 1\nif ! mkdir "$R"; then')
    text=once(text,'printf \'%s\\n\' "$DIR/MPC_CLONE_TRIAL.txt" > "$R/report-path"','printf \'%s\\n\' "$DIR/Logs/MPC_CLONE_TRIAL.txt" > "$R/report-path"')
    return text

def main():
    p=argparse.ArgumentParser();p.add_argument('--base',type=Path,required=True);p.add_argument('--version',required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    V=a.version;O=R/f'build/release-{V}'
    proof=json.loads((O/'compile-verification.json').read_text());baseproof=json.loads((O/'base-verification.json').read_text())
    assert proof['passed'] and baseproof['passed'] and baseproof['version']==V
    assert sha(a.base.read_bytes())==baseproof['base_sha256']=='c7883a402d387753d77f8770fae8306bf8358ae3a134b2bdb7ae9b10e02736ea'
    library=(O/'phone-input.so').read_bytes();assert sha(library)==proof['library_sha256'] and V.encode() in library
    splash=(O/'boot-splash').read_bytes();assert sha(splash)==baseproof['splash_sha256']
    for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
    with zipfile.ZipFile(a.base) as z:
        original={i.filename[8:]:z.read(i) for i in z.infolist() if not i.is_dir() and i.filename.startswith('autorun/') and i.filename.count('/')==1}
    files=dict(original)
    launcher=files['mod-launcher.sh'].decode()
    # Replace payload files; every pinned hash in the controller follows.
    replaced={'phone-input.so':library,'boot-splash':splash,'usb-drive-boot.sh':(R/'src/usb_drive_boot.sh').read_bytes()}
    for n,b in replaced.items():
        old=sha(files[n]);count=launcher.count(old);assert count>=1,n
        launcher=launcher.replace(old,sha(b));files[n]=b
    subprocess.run(['sh','-n'],input=files['usb-drive-boot.sh'],check=True)
    launcher=usb_boot_transform(launcher,sha(files['usb-drive.sh']),sha(files['usb-drive-boot.sh']))
    launcher=logs_launcher(launcher)
    files['mod-launcher.sh']=launcher.encode()
    entry=files['autorun.sh'].decode()
    entry=once(entry,sha(original['mod-launcher.sh']),sha(files['mod-launcher.sh']))
    entry=once(entry,'ENTRY_BEGIN revision=r30d-beta4','ENTRY_BEGIN revision='+V)
    entry=logs_entry(entry)
    files['autorun.sh']=entry.encode()
    manifest=json.loads(files['autorun.json']);manifest['value0']['scripts'][0]['message']='AM Mod '+V
    files['autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
    files['autorun.check']=sha(files['autorun.json']+files['autorun.sh']+AUTORUN_CHECK_SALT).encode()
    assert files['sampleboot.inplace']==b'' and files['sampleboot.txt']==b'Am MOD\n'
    for n in ('autorun.sh','mod-launcher.sh'):subprocess.run(['sh','-n','/dev/stdin'],input=files[n],check=True)
    text=files['autorun.sh'].decode()+files['mod-launcher.sh'].decode()
    for n in LOGS:assert '"$DIR/'+n+'"' not in text.replace('"$DIR/$am_old_log"',''),n
    flat=io.BytesIO()
    with zipfile.ZipFile(flat,'w',zipfile.ZIP_DEFLATED) as z:
        for n,b in files.items():z.writestr('autorun/'+n,b)
    validated=Package(flat.getvalue(),'release.zip');assert validated.revision==V,validated.revision
    checks={}
    for directory in ('power-button','power-native','about-power','synth-live-native','arrangement-native-plan','pattern-tools','arrangement-ui','arrangement-separate',
                      'song-repeats-native','rear-input-route','rear-mix-input-fx','input-config-ui','mixer-step1','usb-drive-boot','usb-drive-sampleboot'):
        data=json.loads((R/'build'/directory/'verification.json').read_text());assert data['passed'],directory
        for n,h in data['sources_sha256'].items():assert sha((R/n).read_bytes())==h,(directory,n)
        checks[directory]=data
    artifacts={'autorun/'+n:b for n,b in files.items()}
    for n in ('README.md','README.ru.md','README.es.md','CHANGELOG.md','LICENSE','NOTICE.md'):artifacts[n]=(ROOT/n).read_bytes()
    # Documentation: the READMEs and every docs/ file they link to, followed recursively.
    queue=[ROOT/'README.md',ROOT/'README.ru.md',ROOT/'README.es.md'];seen=set()
    while queue:
        doc=queue.pop()
        for link in re.findall(r'(?:\]\(|src=")([^)"#\s]+)',doc.read_text(encoding='utf-8')):
            if '://' in link or link.startswith('mailto:'):continue
            target=(doc.parent/link).resolve()
            if not target.is_relative_to((ROOT/'docs').resolve()) or not target.is_file() or target in seen:continue
            assert target.suffix.lower() in ('.md','.png','.gif','.svg','.jpg'),link
            seen.add(target);artifacts[target.relative_to(ROOT.resolve()).as_posix()]=target.read_bytes()
            if target.suffix.lower()=='.md':queue.append(target)
    source={}
    for path in sorted((R/'src').rglob('*')):
        if path.is_file():source[path.relative_to(R).as_posix()]=path.read_bytes()
    for pattern in ('build*.py','render*.py','check*.c','check*.cc','check*.S','check*.inc','verify*.py','package_release.py','usb_drive_boot_package.py'):
        for path in R.glob(pattern):source[path.name]=path.read_bytes()
    for n in ('build/r23a/build-info.json','build/phone_hooks.h','build/pad_lighting_project.h'):source[n]=(R/n).read_bytes()
    source['compile-verification.json']=(O/'compile-verification.json').read_bytes();source['base-verification.json']=(O/'base-verification.json').read_bytes()
    for n,data in checks.items():source['verification/'+n+'.json']=(json.dumps(data,indent=2)+'\n').encode()
    for n in ('installer/mpc_mod_installer.py','installer/test_installer.py'):artifacts['SOURCE/'+n]=(ROOT/n).read_bytes()
    artifacts.update({'SOURCE/sd-mod-session/'+n:b for n,b in source.items()})
    for n in artifacts:
        path=PurePosixPath(n);assert not path.is_absolute() and '..' not in path.parts and '\\' not in n
    a.out.mkdir(parents=True,exist_ok=True);target=a.out/f'am-mod-{V}.zip'
    with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
        for n,b in sorted(artifacts.items()):
            info=zipfile.ZipInfo(n);info.compress_type=zipfile.ZIP_DEFLATED
            info.external_attr=(0o100755 if n.endswith('.sh') or n.rsplit('/',1)[-1] in ('apply-session','stage-data','temp-workspace','boot-splash') else 0o100644)<<16
            z.writestr(info,b)
    data=target.read_bytes();released=Package(data,target.name);assert released.revision==V
    with zipfile.ZipFile(target) as z:assert z.testzip() is None and len(z.namelist())==len(set(z.namelist()))
    result=dict(passed=True,revision=V,package_sha256=sha(data),library_sha256=sha(library),splash_sha256=sha(splash),
        payload_sha256={n:sha(b) for n,b in files.items()},changed_payload=sorted(n for n in files if files[n]!=original.get(n)),
        logs_folder='Logs/',checks=sorted(checks),source_files=len(source),hardware_tested=False,card_written=False,published=False)
    (a.out/'package-verification.json').write_text(json.dumps(result,indent=2)+'\n')
    (a.out/'SHA256SUMS.txt').write_text(result['package_sha256']+'  '+target.name+'\n')
    print(target);print(json.dumps({k:v for k,v in result.items() if k!='payload_sha256'},indent=2))
if __name__=='__main__':main()
