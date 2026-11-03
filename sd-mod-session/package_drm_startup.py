"""r27a: replace only the r26z display helper/launcher, retain exact app bytes."""
from pathlib import Path
import hashlib,json,subprocess,zipfile
from early_drm_startup import transform
R=Path(__file__).resolve().parent;O=R/'build/early-drm';sha=lambda b:hashlib.sha256(b).hexdigest()
base=R.parent/'am-mod-r26z.zip';assert sha(base.read_bytes())=='c7fbd705140ff9d619b7a4e1e1dd8a0580ae70545f73375ffadb7218fc5a13f2'
proof=json.loads((O/'verification.json').read_text());assert proof['passed'] and proof['ubsan'] and proof['master_release_verified']
for name,h in proof['sources_sha256'].items():assert sha((R/name).read_bytes())==h,name
with zipfile.ZipFile(base) as z:assert z.testzip() is None;old={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
files=dict(old);helper=(O/'boot-splash').read_bytes();assert sha(helper)==proof['splash_sha256']
assert sha(old['autorun/phone-input.so'])=='c72f838700b93668736078f9ea757e491e84bdbe67bf6fcc85f2d655e5210ca8'
launcher=transform(old['autorun/mod-launcher.sh'].decode(),sha(old['autorun/boot-splash']),sha(helper)).encode()
files['autorun/boot-splash']=helper;files['autorun/mod-launcher.sh']=launcher
entry=old['autorun/autorun.sh'].decode();assert entry.count('revision=r26z')==1 and entry.count(sha(old['autorun/mod-launcher.sh']))==1
files['autorun/autorun.sh']=entry.replace('revision=r26z','revision=r27a').replace(sha(old['autorun/mod-launcher.sh']),sha(launcher)).encode()
manifest=json.loads(old['autorun/autorun.json']);manifest['value0']['scripts'][0]['message']='AM Mod r27a - DRM splash handoff trial; experimental FAT32'
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
changed=sorted(n for n in files if n.startswith('autorun/') and files[n]!=old[n]);assert set(changed)=={'autorun/boot-splash','autorun/mod-launcher.sh','autorun/autorun.sh','autorun/autorun.json','autorun/autorun.check'}
for name in ('README.md','README.ru.md','CHANGELOG.md','docs/BOOT_FLOW.ru.md','docs/KNOWN_ISSUES.md','docs/KNOWN_ISSUES.ru.md','docs/RELEASE_NOTES-r27a.md','docs/RELEASE_NOTES-r27a.ru.md'):
 files[name]=(R.parent/name).read_bytes()
files['README_RU.md']=('# AM Mod r27a — проверка заставки через DRM\n\nСобственный буфер явно назначается экрану; после готовности управление получает MPC.\n'
 'Показ и передача проверены в VM; физическая видимость требует пробы. Библиотека приложения и все функции r26y/r26z не менялись.\n'
 'Сделайте бэкап сэмплов и проектов. Полная autorun в корень SD; exFAT и экспериментальный FAT32.\n'
 'Сценарий: docs/RELEASE_NOTES-r27a.ru.md. Не опубликовано.\n').encode()
for name in ('autorun.sh','mod-launcher.sh'):subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+name],check=True)
out=R.parent/'am-mod-r27a.zip';assert not out.exists(),out
with zipfile.ZipFile(out,'x',zipfile.ZIP_DEFLATED) as z:
 for name,data in sorted(files.items()):z.writestr(name,data)
with zipfile.ZipFile(out) as z:assert z.testzip() is None and z.read('autorun/phone-input.so')==old['autorun/phone-input.so']
report=dict(passed=True,revision='r27a',package_sha256=sha(out.read_bytes()),library_sha256=sha(files['autorun/phone-input.so']),library_unchanged=True,splash_sha256=sha(helper),changed_payload=changed,drm_checks=proof,payload_sha256={n:sha(b) for n,b in files.items() if n.startswith('autorun/')},physical_splash_visibility_confirmed=False,published=False)
(O/'package-verification-r27a.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k not in ('drm_checks','payload_sha256')},indent=2))
