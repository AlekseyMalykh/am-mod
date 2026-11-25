"""Package a verified local candidate beside stable SampleBoot modules.
Never installs a bootloader, writes a card, or publishes a release.
"""
from pathlib import Path,PurePosixPath
import argparse,hashlib,io,json,re,subprocess,sys,tempfile,zipfile
from usb_drive_boot_package import transform as usb_boot_transform
R=Path(__file__).resolve().parent
sys.path.insert(0,str(R.parent/'installer'))
from mpc_mod_installer import Package,AUTORUN_CHECK_SALT
sha=lambda b:hashlib.sha256(b).hexdigest()
README='''Am MOD Arrange Test {number} — отдельный мод для SampleBoot

Установка: распакуйте этот архив в корень карты с уже установленным SampleBoot.
Появится папка autorun/mods/AM-Mod-Arrange-Test{number}/. Файлы загрузчика и стабильный
AM-Mod архив не заменяет. Обычный установщик Am MOD для этого архива не нужен.
В меню SampleBoot выберите Am MOD Arrange Test {number}.

Кнопка питания: B1 Cancel, B2 Bootloader, B3 Power off. B2/B3 сначала сохраняют
копию проекта Projects/Mod-Exit-Last.xpj. Ошибка сохранения оставляет мод включённым.
В About остаются версия и ссылка на исходники.

Rear Input: Input settings → выбрать Rear → B2 Routing.
K1: общий Monitor Off/Auto/On; B2/B3: независимые Mon L/Mon R.
K2/K3: Pan L/R; Shift+K2/K3: Level L/R. Pad Bank + K2/K3 сбрасывает параметр.
Также Mixer → Input → ROUTE содержит отдельные строки уровня, панорамы и
мониторинга Rear L и Rear R, с прокруткой DATA. Общий Monitor сохраняет выбор каналов.
Из Sample Record убран Shift+B2 Routing; B2 по-прежнему включает To Seq.
Pan/Level в Stereo/Left/Right/Split относятся к мониторингу; режим L+R Mix
также записывает настроенную стереосмесь. Monitor Off не выключает запись.

Исправлены Repeats обычного Sequential Song Mode: native список учитывает
повторы и обновляется при изменении Repeats/Undo. Проверено в ARM QEMU;
слышимое воспроизведение на устройстве ещё не проверялось.

SD Card Access: исправлено освобождение и восстановление привязки SampleBoot
(autorun/mods/<имя>). Сохранена поддержка D.S.B (autorun/os/<имя>) и обычного
запуска без загрузчика. Проверены реальные exFAT loop-образы и откат ошибок;
USB-передача с физического устройства требует проверки.

Для тестирования используйте копию проекта: Projects общая для двух модов;
стабильная версия при пересохранении может потерять новые расширения Arrange.

При остановленном транспорте: Song → Shift+B1 → Arrange. На временной шкале
Shift+B1 возвращает обычный Song. В Piano Roll прежнее Shift+B1 сохранено.
DATA двигает курсор, нажатие DATA выделяет блок, фейдер выбирает дорожку.
B3: New на пустом месте, Open на блоке, Edit при выделении, Apply в preview.
B1: Back/Cancel. B2 Range задаёт длину нового блока. Shift+B3 открывает Actions:
Add track, Insert pattern, Copy/Paste, Move, Join, Mute/Solo, Create variation.
CHOP разрезает блок на курсоре. Shift -/+ — Undo/Redo.
В открытом из Arrange Piano Roll B3 New вставляет событие в пустую ячейку.
Для отделения строк: удерживать B2, выбрать пэды, затем B3 Separate, не
отпуская B2. В Tune/Chop можно отделять отдельные высоты/срезы одного пэда.
Copy selection в Actions копирует выбранное. Отделение создаёт самостоятельный
паттерн/дорожку одной операцией Undo; другие экземпляры исходника сохраняются.
Обычное редактирование общего Pattern ID действует на все его блоки.
Пустой проект создаёт draft с неназначенным инструментом. Назначение
инструмента и воспроизведение/запись Arrange пока не подключены.

Эта сборка позволяет проверить редактор и переходы на устройстве. Play/Record
в Arrange ещё заблокированы: производственный запуск параллельных паттернов
будет следующим этапом. Семь событий партий 2/4/8 и Synth PCM проверены через
настоящий Engine в QEMU с тестовым транспортом, а не через экран Arrange.

Проверены конструктор полной библиотеки и аппаратные маршруты ввода в QEMU.
Полный графический запуск, сохранение XPJ через приложение и физическое
устройство этой проверкой не подтверждаются. Этот архив сам ничего не устанавливает.
Для возврата к стабильной версии: питание → B2 → выбрать прежний Am MOD.
'''

def main():
 p=argparse.ArgumentParser();p.add_argument('--base',type=Path,required=True);p.add_argument('--test-number',type=int,default=2);a=p.parse_args()
 assert 1<=a.test_number<=99
 VERSION=f'r30d-arrange-test{a.test_number}';PREFIX=f'autorun/mods/AM-Mod-Arrange-Test{a.test_number}/'
 source_prefix=f'SOURCE-Arrange-Test{a.test_number}/';menu_name=f'Am MOD Arrange Test {a.test_number}'
 O=R/f'build/arrange-sampleboot-test{a.test_number}';proof=json.loads((O/'compile-verification.json').read_text())
 baseproof=json.loads((O/'base-verification.json').read_text());assert proof['passed'] and baseproof['passed']
 assert sha(a.base.read_bytes())==baseproof['base_sha256']=='c7883a402d387753d77f8770fae8306bf8358ae3a134b2bdb7ae9b10e02736ea'
 library=(O/'phone-input.so').read_bytes();assert sha(library)==proof['library_sha256']
 assert VERSION.encode() in library and baseproof['version']==VERSION
 for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
 with zipfile.ZipFile(a.base) as z:
  original={i.filename[8:]:z.read(i) for i in z.infolist() if not i.is_dir() and i.filename.startswith('autorun/') and i.filename.count('/')==1}
 files=dict(original);oldlib=sha(files['phone-input.so']);files['phone-input.so']=library
 launcher=files['mod-launcher.sh'];needle=('SAVE_IO_SHA='+oldlib).encode();assert launcher.count(needle)==1
 files['mod-launcher.sh']=launcher.replace(needle,('SAVE_IO_SHA='+sha(library)).encode())
 # Keep host SD handoff compatible with SampleBoot's mods/<name> overmount.
 files['usb-drive-boot.sh']=(R/'src/usb_drive_boot.sh').read_bytes()
 files['mod-launcher.sh']=usb_boot_transform(files['mod-launcher.sh'].decode(),
     sha(files['usb-drive.sh']),sha(files['usb-drive-boot.sh'])).encode()
 subprocess.run(['sh','-n'],input=files['usb-drive-boot.sh'],check=True)
 entry=files['autorun.sh'];oldcontroller=sha(original['mod-launcher.sh']).encode();assert entry.count(oldcontroller)==1
 entry=entry.replace(oldcontroller,sha(files['mod-launcher.sh']).encode())
 assert entry.count(b'ENTRY_BEGIN revision=r30d-beta4')==1
 files['autorun.sh']=entry.replace(b'ENTRY_BEGIN revision=r30d-beta4',('ENTRY_BEGIN revision='+VERSION).encode())
 manifest=json.loads(files['autorun.json']);manifest['value0']['scripts'][0]['message']=menu_name
 files['autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
 files['autorun.check']=sha(files['autorun.json']+files['autorun.sh']+AUTORUN_CHECK_SALT).encode()
 files['sampleboot.txt']=(menu_name+'\n').encode()
 assert files['sampleboot.inplace']==b'' and b'BOOTLOADER_START_CLEAN_STOCK' in entry
 for n in ('autorun.sh','mod-launcher.sh'):subprocess.run(['sh','-n','/dev/stdin'],input=files[n],check=True)
 # Validate the complete module with the existing installer parser, in memory.
 flat=io.BytesIO()
 with zipfile.ZipFile(flat,'w',zipfile.ZIP_DEFLATED) as z:
  for n,b in files.items():z.writestr('autorun/'+n,b)
 validated=Package(flat.getvalue(),'candidate.zip');assert validated.revision==VERSION
 checks={}
 for directory in ('power-button','power-native','about-power','synth-live-native','arrangement-native-plan','pattern-tools','arrangement-ui','arrangement-separate',
                   'song-repeats-native','rear-input-route','rear-mix-input-fx','input-config-ui','mixer-step1','usb-drive-boot','usb-drive-sampleboot'):
  path=R/'build'/directory/'verification.json';data=json.loads(path.read_text());assert data['passed'],directory
  for n,h in data['sources_sha256'].items():assert sha((R/n).read_bytes())==h,(directory,n)
  checks[directory]=data
 for directory in ('usb-drive-boot','usb-drive-sampleboot'):
  data=json.loads((R/'build'/directory/'shared-verification.json').read_text());assert data['passed'] and data['shared_mounts']
  for n,h in data['sources_sha256'].items():assert sha((R/n).read_bytes())==h,(directory,n)
  checks[directory+'-shared']=data
 source={}
 # Include current source plus frozen link manifest/header and verification.
 for path in sorted((R/'src').rglob('*')):
  if path.is_file():source[path.relative_to(R).as_posix()]=path.read_bytes()
 for pattern in ('build*.py','render*.py','check*.c','check*.cc','check*.S','check*.inc','verify*.py','*RU.md'):
  for path in R.glob(pattern):source[path.name]=path.read_bytes()
 for n in ('package_arrange_sampleboot.py','usb_drive_boot_package.py','check_fx_native.c','build/r23a/build-info.json','build/phone_hooks.h','build/pad_lighting_project.h'):
  source[n]=(R/n).read_bytes()
 source['compile-verification.json']=(O/'compile-verification.json').read_bytes()
 source['base-verification.json']=(O/'base-verification.json').read_bytes()
 for n,data in checks.items():source['verification/'+n+'.json']=(json.dumps(data,indent=2)+'\n').encode()
 artifacts={PREFIX+n:b for n,b in files.items()}
 artifacts[f'README-Arrange-Test{a.test_number}.txt']=README.format(number=a.test_number).encode('utf-8')
 for name in ('LICENSE','NOTICE.md'):artifacts[source_prefix+name]=(R.parent/name).read_bytes()
 artifacts.update({source_prefix+n:b for n,b in source.items()})
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
