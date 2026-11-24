"""Package a verified local candidate beside stable SampleBoot modules.
Never installs a bootloader, writes a card, or publishes a release.
"""
from pathlib import Path,PurePosixPath
import argparse,hashlib,io,json,re,subprocess,sys,tempfile,zipfile
R=Path(__file__).resolve().parent
sys.path.insert(0,str(R.parent/'installer'))
from mpc_mod_installer import Package,AUTORUN_CHECK_SALT
sha=lambda b:hashlib.sha256(b).hexdigest()
VERSION='r30d-arrange-test1'
PREFIX='autorun/mods/AM-Mod-Arrange-Test1/'
README='''Am MOD Arrange Test 1 — отдельный мод для SampleBoot

Установка: распакуйте этот архив в корень карты с уже установленным SampleBoot.
Появится папка autorun/mods/AM-Mod-Arrange-Test1/. Файлы загрузчика и стабильный
AM-Mod архив не заменяет. Обычный установщик Am MOD для этого архива не нужен.
В меню SampleBoot выберите Am MOD Arrange Test 1.

Кнопка питания: B1 Cancel, B2 Bootloader, B3 Power off. B2/B3 сначала сохраняют
копию проекта Projects/Mod-Exit-Last.xpj. Ошибка сохранения оставляет мод включённым.
В About остаются версия и ссылка на исходники.

Для тестирования используйте копию проекта: Projects общая для двух модов;
стабильная версия при пересохранении может потерять новые расширения Arrange.

Arrange: Song → B1 Export → Horizontal Arrange, при остановленном транспорте.
B2 Actions: дорожки/блоки, Copy/Paste, Move, длина, Join, Mute/Solo, Edit Pattern.
B3 фиксирует выбранный блок, DATA двигает курсор, CHOP разрезает в его позиции.
Shift+B3 открывает паттерн в существующем Piano Roll, B1 возвращает в Arrange.
Изменения паттерна действуют на все блоки с тем же Pattern ID.

Эта сборка позволяет проверить редактор и переходы на устройстве. Play/Record
в Arrange ещё заблокированы: производственный запуск параллельных паттернов
будет следующим этапом. Семь событий партий 2/4/8 и Synth PCM проверены через
настоящий Engine в QEMU с тестовым транспортом, а не через экран Arrange.

Проверены конструктор полной библиотеки и аппаратные маршруты ввода в QEMU.
Полный графический запуск, сохранение XPJ через приложение и физическое
устройство этой проверкой не подтверждаются. На карту агент ничего не записывал.
Для возврата к стабильной версии: питание → B2 → выбрать прежний Am MOD.
'''

def main():
 p=argparse.ArgumentParser();p.add_argument('--base',type=Path,required=True);a=p.parse_args()
 O=R/'build/arrange-sampleboot-test1';proof=json.loads((O/'compile-verification.json').read_text())
 baseproof=json.loads((O/'base-verification.json').read_text());assert proof['passed'] and baseproof['passed']
 assert sha(a.base.read_bytes())==baseproof['base_sha256']=='c7883a402d387753d77f8770fae8306bf8358ae3a134b2bdb7ae9b10e02736ea'
 library=(O/'phone-input.so').read_bytes();assert sha(library)==proof['library_sha256']
 assert b'r30d-arrange-test1' in library
 for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
 with zipfile.ZipFile(a.base) as z:
  original={i.filename[8:]:z.read(i) for i in z.infolist() if not i.is_dir() and i.filename.startswith('autorun/') and i.filename.count('/')==1}
 files=dict(original);oldlib=sha(files['phone-input.so']);files['phone-input.so']=library
 launcher=files['mod-launcher.sh'];needle=('SAVE_IO_SHA='+oldlib).encode();assert launcher.count(needle)==1
 files['mod-launcher.sh']=launcher.replace(needle,('SAVE_IO_SHA='+sha(library)).encode())
 entry=files['autorun.sh'];oldcontroller=sha(original['mod-launcher.sh']).encode();assert entry.count(oldcontroller)==1
 entry=entry.replace(oldcontroller,sha(files['mod-launcher.sh']).encode())
 assert entry.count(b'ENTRY_BEGIN revision=r30d-beta4')==1
 files['autorun.sh']=entry.replace(b'ENTRY_BEGIN revision=r30d-beta4',('ENTRY_BEGIN revision='+VERSION).encode())
 manifest=json.loads(files['autorun.json']);manifest['value0']['scripts'][0]['message']='Am MOD Arrange Test 1'
 files['autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
 files['autorun.check']=sha(files['autorun.json']+files['autorun.sh']+AUTORUN_CHECK_SALT).encode()
 files['sampleboot.txt']=b'Am MOD Arrange Test 1\n'
 assert files['sampleboot.inplace']==b'' and b'BOOTLOADER_START_CLEAN_STOCK' in entry
 for n in ('autorun.sh','mod-launcher.sh'):subprocess.run(['sh','-n','/dev/stdin'],input=files[n],check=True)
 # Validate the complete module with the existing installer parser, in memory.
 flat=io.BytesIO()
 with zipfile.ZipFile(flat,'w',zipfile.ZIP_DEFLATED) as z:
  for n,b in files.items():z.writestr('autorun/'+n,b)
 validated=Package(flat.getvalue(),'candidate.zip');assert validated.revision==VERSION
 checks={}
 for directory in ('power-button','power-native','about-power','synth-live-native','arrangement-native-plan','pattern-tools','arrangement-ui'):
  path=R/'build'/directory/'verification.json';data=json.loads(path.read_text());assert data['passed'],directory
  for n,h in data['sources_sha256'].items():assert sha((R/n).read_bytes())==h,(directory,n)
  checks[directory]=data
 source={}
 # Include current source plus frozen link manifest/header and verification.
 for path in sorted((R/'src').rglob('*')):
  if path.is_file():source[path.relative_to(R).as_posix()]=path.read_bytes()
 for pattern in ('build*.py','render*.py','check*.c','check*.cc','check*.S','check*.inc','verify*.py','*RU.md'):
  for path in R.glob(pattern):source[path.name]=path.read_bytes()
 for n in ('package_arrange_sampleboot.py','check_fx_native.c','build/r23a/build-info.json','build/phone_hooks.h','build/pad_lighting_project.h'):
  source[n]=(R/n).read_bytes()
 source['compile-verification.json']=(O/'compile-verification.json').read_bytes()
 source['base-verification.json']=(O/'base-verification.json').read_bytes()
 for n,data in checks.items():source['verification/'+n+'.json']=(json.dumps(data,indent=2)+'\n').encode()
 artifacts={PREFIX+n:b for n,b in files.items()}
 artifacts['README-Arrange-Test1.txt']=README.encode('utf-8')
 for name in ('LICENSE','NOTICE.md'):artifacts['SOURCE-Arrange-Test1/'+name]=(R.parent/name).read_bytes()
 artifacts.update({'SOURCE-Arrange-Test1/'+n:b for n,b in source.items()})
 for n in artifacts:
  path=PurePosixPath(n);assert not path.is_absolute() and '..' not in path.parts and '\\' not in n
  if n.startswith('autorun/'):assert n.startswith(PREFIX)
 target=O/('am-mod-'+VERSION+'-sampleboot.zip')
 with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
  for n,b in sorted(artifacts.items()):
   info=zipfile.ZipInfo(n);info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=(0o100755 if n.endswith('.sh') or n.rsplit('/',1)[-1] in ('apply-session','stage-data','temp-workspace','boot-splash') else 0o100644)<<16
   z.writestr(info,b)
 # Extract to an isolated workspace fixture with a stable module and bootloader.
 with tempfile.TemporaryDirectory(prefix='coexist-',dir=O) as d:
  root=Path(d);sentinels={'autorun/autorun.sh':b'bootloader root','autorun/config.json':b'bootloader config'}
  sentinels.update({'autorun/mods/AM-Mod/'+n:b for n,b in original.items()})
  for n,b in sentinels.items():path=root/n;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b)
  with zipfile.ZipFile(target) as z:
   assert z.testzip() is None and len(z.namelist())==len(set(z.namelist()))
   z.extractall(root)
  for n,b in sentinels.items():assert (root/n).read_bytes()==b,n
  for n,b in files.items():assert (root/PREFIX/n).read_bytes()==b,n
 result=dict(passed=True,version=VERSION,module_path=PREFIX,package_sha256=sha(target.read_bytes()),library_sha256=sha(library),stable_payload_unchanged=True,bootloader_root_unchanged=True,coexistence_fixture=True,
  changed_payload=[n for n in files if files[n]!=original.get(n)],source_files=len(source),hardware_tested=False,card_written=False,full_vm_boot=False,arrangement_ui_playback=False)
 target.with_suffix('.verification.json').write_text(json.dumps(result,indent=2)+'\n')
 target.with_suffix('.sha256').write_text(result['package_sha256']+'  '+target.name+'\n')
 print(target);print(json.dumps(result,indent=2))
if __name__=='__main__':main()
