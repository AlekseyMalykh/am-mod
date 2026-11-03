"""Package the display-only r26z fix over immutable r26y; keep the library exact."""
from pathlib import Path
import hashlib,json,zipfile,subprocess
from early_startup import transform_launcher
R=Path(__file__).resolve().parent;O=R/'build/early-startup';sha=lambda b:hashlib.sha256(b).hexdigest()
base=R.parent/'am-mod-r26y.zip';assert sha(base.read_bytes())=='a2b2117223c16838eb79f53ad223f6891b0e99ac13070ea7d05612d340305161'
proof=json.loads((O/'verification.json').read_text());assert proof['passed'] and proof['ubsan']
for name,h in proof['sources_sha256'].items():assert sha((R/name).read_bytes())==h,name
with zipfile.ZipFile(base) as z:assert z.testzip() is None;old={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
files=dict(old);helper=(O/'boot-splash').read_bytes();assert sha(helper)==proof['splash_sha256']
assert sha(old['autorun/phone-input.so'])=='c72f838700b93668736078f9ea757e491e84bdbe67bf6fcc85f2d655e5210ca8'
with zipfile.ZipFile(R.parent/'am-mod-r26x.zip') as z:
 frozen=z.read('autorun/mod-launcher.sh').decode();previous_lib=z.read('autorun/phone-input.so')
assert frozen.count('SAVE_IO_SHA='+sha(previous_lib))==1
launcher=transform_launcher(frozen.replace('SAVE_IO_SHA='+sha(previous_lib),'SAVE_IO_SHA='+sha(old['autorun/phone-input.so'])),sha(helper)).encode()
files['autorun/boot-splash']=helper;files['autorun/mod-launcher.sh']=launcher
entry=old['autorun/autorun.sh'].decode();assert entry.count(sha(old['autorun/mod-launcher.sh']))==1 and entry.count('revision=r26y')==1
files['autorun/autorun.sh']=entry.replace(sha(old['autorun/mod-launcher.sh']),sha(launcher)).replace('revision=r26y','revision=r26z').encode()
manifest=json.loads(old['autorun/autorun.json']);manifest['value0']['scripts'][0]['message']='AM Mod r26z - framebuffer reset and early handoff trial; experimental FAT32'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
changed=sorted(n for n in files if n.startswith('autorun/') and files[n]!=old[n])
assert set(changed)=={'autorun/boot-splash','autorun/mod-launcher.sh','autorun/autorun.sh','autorun/autorun.json','autorun/autorun.check'}
for name in ('README.md','README.ru.md','CHANGELOG.md','docs/BOOT_FLOW.ru.md','docs/KNOWN_ISSUES.md','docs/KNOWN_ISSUES.ru.md','docs/RELEASE_NOTES-r26z.md','docs/RELEASE_NOTES-r26z.ru.md'):
 files[name]=(R.parent/name).read_bytes()
files['README_RU.md']=('# AM Mod r26z — проверочная сборка\n\nПолный сброс framebuffer перед Am MOD, ранний обработчик возврата через setsid.\n'
 'Библиотека и все функции r26y сохранены. Одна загрузка SD подтверждена журналом MPC; видимость заставки требует новой пробы.\n'
 'Сделайте бэкап сэмплов и проектов. Полная autorun в корень SD; exFAT и экспериментальный FAT32.\n'
 'Сценарий: docs/RELEASE_NOTES-r26z.ru.md. Не опубликовано.\n').encode()
for name in ('autorun.sh','mod-launcher.sh'):subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+name],check=True)
out=R.parent/'am-mod-r26z.zip';assert not out.exists(),out
with zipfile.ZipFile(out,'x',zipfile.ZIP_DEFLATED) as z:
 for name,data in sorted(files.items()):z.writestr(name,data)
with zipfile.ZipFile(out) as z:assert z.testzip() is None and z.read('autorun/phone-input.so')==old['autorun/phone-input.so']
report=dict(passed=True,revision='r26z',package_sha256=sha(out.read_bytes()),library_sha256=sha(files['autorun/phone-input.so']),library_unchanged=True,splash_sha256=sha(helper),changed_payload=changed,early_startup_checks=proof,payload_sha256={n:sha(b) for n,b in files.items() if n.startswith('autorun/')},physical_splash_visibility_confirmed=False,published=False)
(O/'package-verification-r26z.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k not in ('early_startup_checks','payload_sha256')},indent=2))
