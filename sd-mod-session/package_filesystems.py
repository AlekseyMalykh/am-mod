"""Build an exFAT/FAT32 candidate over a verified release without changing
the app, library or fixed native patches. Only storage helpers/guards change.
Usage: python3 package_filesystems.py PREVIOUS.zip r26e
"""
from pathlib import Path
import hashlib,json,re,subprocess,sys,zipfile
R=Path(__file__).resolve().parent;O=R/'build/filesystems';O.mkdir(parents=True,exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
def function(text,name):
 start=text.index(name+'() {');return text[start:text.index('\n}\n',start)+3]
def replace_once(text,old,new):
 assert text.count(old)==1,old
 return text.replace(old,new,1)
if len(sys.argv)!=3:sys.exit(__doc__)
previous=Path(sys.argv[1]);revision=sys.argv[2];assert re.fullmatch(r'r\d+[a-z]+',revision)
with zipfile.ZipFile(previous) as z:
 assert z.testzip() is None;old={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
files=dict(old);sources=['src/stage_data.c','src/autorun.template.sh','src/entry.template.sh','src/power_audit.sh','src/collect_startup.sh','package_filesystems.py','../installer/mpc_mod_installer.py','../README.md','../README.ru.md','../CHANGELOG.md','../docs/RELEASE_NOTES-r26e.md','../docs/RELEASE_NOTES-r26e.ru.md']
hashes={n:sha((R/n).read_bytes()) for n in sources}
helper=O/'stage-data'
subprocess.run(['aarch64-linux-gnu-gcc','-O2','-Wall','-Wextra','-Werror','-mbranch-protection=standard','-Wl,-z,noexecstack','src/stage_data.c','-o',str(helper)],cwd=R,check=True)
files['autorun/stage-data']=helper.read_bytes()
controller=old['autorun/mod-launcher.sh'].decode();template=(R/'src/autorun.template.sh').read_text()
for name in ['storage_host_check','sandbox_check']:
 controller=replace_once(controller,function(controller,name),function(template,name))
old_check='awk -v p="$SD" \'$2==p && $3=="exfat" && $4 ~ /(^|,)rw(,|$)/ {ok=1} END {exit !ok}\' /proc/mounts || early_fail SD_NOT_RW_EXFAT'
new_check='\n'.join(s for s in template.splitlines() if s.startswith(('SD_FS=$(', 'case "$SD_FS:')))
assert new_check.count('\n')==1
controller=replace_once(controller,old_check,new_check)
anchor='stat -c \'%d:%i\' "$DIR" > "$R/storage-meta/journal-identity"'
controller=replace_once(controller,anchor,anchor+'\nprintf \'%s\\n\' "$SD_FS" > "$R/storage-meta/sd-filesystem"')
controller=replace_once(controller,'STAGE_SHA='+sha(old['autorun/stage-data']),'STAGE_SHA='+sha(files['autorun/stage-data']))
files['autorun/mod-launcher.sh']=controller.encode()
entry=old['autorun/autorun.sh'].decode()
entry=replace_once(entry,'#!/bin/sh\n','#!/bin/sh\n# sd-filesystems=exfat,vfat\n')
entry=replace_once(entry,'$2==p && $3=="exfat" &&','$2==p && ($3=="exfat" || $3=="vfat") &&')
entry=replace_once(entry,sha(old['autorun/mod-launcher.sh']),sha(files['autorun/mod-launcher.sh']))
audit=(R/'src/power_audit.sh').read_bytes();files['autorun/power-audit.sh']=audit
entry=replace_once(entry,sha(old['autorun/power-audit.sh']),sha(audit))
revisions=re.findall(r'revision=(r\d+[a-z]+(?:-[a-z0-9-]+)?)',entry);assert len(set(revisions))==1
entry=entry.replace('revision='+revisions[0],'revision='+revision)
files['autorun/autorun.sh']=entry.encode()
manifest=json.loads(old['autorun/autorun.json']);manifest['value0']['scripts'][0]['message']='AM Mod '+revision+' - exFAT and FAT32'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
changed={n for n,b in files.items() if old.get(n)!=b}
assert changed=={'autorun/stage-data','autorun/mod-launcher.sh','autorun/power-audit.sh','autorun/autorun.sh','autorun/autorun.json','autorun/autorun.check'},changed
assert files['autorun/phone-input.so']==old['autorun/phone-input.so']
for n in ['autorun.sh','mod-launcher.sh','power-audit.sh']:
 subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+n],check=True)
assert controller.index('storage-meta/sd-filesystem')<controller.index('checkpoint JOURNAL_HELPER_READY')
for name in ['README.md','README.ru.md','CHANGELOG.md','docs/RELEASE_NOTES-r26e.md','docs/RELEASE_NOTES-r26e.ru.md']:
 files[name]=(R.parent/name).read_bytes()
files['README_RU.md']=('# AM Mod '+revision+'\n\nПодробная инструкция: [README.ru.md](README.ru.md).\n\n'+
 'Поддерживается exFAT; FAT32 доступен в экспериментальном режиме. Скопируйте полную папку autorun в корень карты.\n'+
 'Проекты и сэмплы можно оставить; переформатирование FAT32 не требуется.\n'+
 'Имя тома: MPC или MPCSAMPLE, без пробелов и кириллицы.\n'+
 'Сборка-кандидат: физическая проверка FAT32 на MPC пока не выполнена.\n').encode()
out=R.parent/('am-mod-'+revision+'.zip');assert not out.exists(),out
with zipfile.ZipFile(out,'x',zipfile.ZIP_DEFLATED) as z:
 for n,b in sorted(files.items()):z.writestr(n,b)
with zipfile.ZipFile(out) as z:assert z.testzip() is None and all(z.read(n)==b for n,b in files.items())
assert hashes=={n:sha((R/n).read_bytes()) for n in sources}
report=dict(passed=True,revision=revision,package=out.name,package_sha256=sha(out.read_bytes()),sources_sha256=hashes,payload_sha256={n:sha(b) for n,b in files.items() if n.startswith('autorun/')},library_unchanged=True,library_sha256=sha(files['autorun/phone-input.so']),helper_sha256=sha(files['autorun/stage-data']),changed=sorted(changed),hardware_tested=False)
(O/'package-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
