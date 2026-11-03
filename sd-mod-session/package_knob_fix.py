"""Package a verified knob/OptoCat candidate without accessing a card.
Run verify_knob_events.py and build_chords_candidate.py first.
Usage: python3 package_knob_fix.py ../am-mod-r26x.zip r26y
"""
from pathlib import Path
import hashlib,json,re,subprocess,sys,zipfile
R=Path(__file__).resolve().parent;O=R/'build/knob-events';sha=lambda b:hashlib.sha256(b).hexdigest()
if len(sys.argv) not in (2,3):sys.exit(__doc__)
revision=sys.argv[2] if len(sys.argv)==3 else 'r26y'
assert revision in ('r26f','r26g','r26h','r26i','r26j','r26k','r26l','r26m','r26n','r26o','r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y')
pad_colour_candidate=revision in ('r26l','r26m','r26n','r26o','r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y')
tests=json.loads((O/'verification.json').read_text());compiled=json.loads((R/'build/chords/compile-verification.json').read_text())
for proof in [tests,compiled]:
 assert proof['passed']
 for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
assert tests['baseline_reproduced'] and compiled['full_preload_constructor']
if revision in ('r26g','r26h','r26i','r26j','r26k','r26l','r26m','r26n','r26o','r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y'):assert tests['scaled_baseline_reproduced']
compressor=None
if revision in ('r26h','r26i','r26j','r26k','r26l','r26m','r26n','r26o','r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y'):
 compressor_revision=json.loads((R/'compressor_base.json').read_text())['revision'] if pad_colour_candidate else 'r26g'
 compressor=json.loads((R/'build/compressor'/compressor_revision/'verification.json').read_text())
 assert compressor['passed'] and compressor['ubsan'] and compressor['animation_verified']
 for n,h in compressor['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
 assert compiled['compressor_included'] and '-DMPC_COMPRESSOR=1' in compiled['compiler_flags']
 assert {'src/compressor_dsp.c','src/compressor_ui.c','src/compressor_native.c','src/compressor_animation.c','src/compressor_image.cc'}<=set(compiled['linked_sources'])
 if revision in ('r26i','r26j','r26k','r26l','r26m','r26n','r26o','r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y'):
  assert compiled['native_takeover_preserved'] and 'src/takeover_native.c' in compiled['linked_sources']
  assert 'src/takeover_endpoints.c' not in compiled['linked_sources']
  assert any('native knob input thread' in case['test'] and case['ubsan'] for case in tests['tests'])
 if revision in ('r26j','r26k','r26l','r26m','r26n','r26o','r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y'):
  assert compressor['shift_controls_verified'] and compressor['paw_timing_verified'] and compressor['native_gain_compared'] and compressor['maximum_gain_db']==6
 assert any('display' in case['test'] and case['ubsan'] for case in tests['tests'])
 if revision in ('r26m','r26n','r26o','r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y'):assert compressor['bypass_meters_hidden_verified'] and compressor['bypass_meter_baseline_reproduced'] and compressor['sleep_background_hidden_verified'] and compressor['sleep_background_baseline_reproduced'] and compressor['stereo_meters_verified']
if revision in ('r26n','r26o','r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y'):assert compressor['bypass_underline_verified']
if revision in ('r26o','r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y'):assert compressor['warm_verified'] and compressor['warm_native_audio_verified']
if revision in ('r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y'):assert all(compressor[k] for k in ('master_rebind_verified','master_rebind_baseline_reproduced','inflight_handoff_verified','shared_native_bypass_verified','open_active_or_duck_verified'))
restore=exit_copy=None
if revision in ('r26k','r26l','r26m','r26n','r26o','r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y'):
 restore=json.loads((R/'build/project-restore/verification.json').read_text())
 exit_copy=json.loads((R/'build/project-exit/verification.json').read_text())
 assert restore['passed'] and restore['ubsan'] and restore['modal_baseline_reproduced'] and exit_copy['passed']
 for proof in (restore,exit_copy):
  for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
colours=resample=None
if pad_colour_candidate:
 colours={}
 for folder in ('pad-colour-rgb','pad-colour-controls','pad-colour-workflows','pad-eq'):
  proof=json.loads((R/'build'/folder/'verification.json').read_text())
  assert proof['passed'] and proof['ubsan'],folder
  for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,(folder,n)
  colours[folder]=proof
if revision in ('r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y'):assert all(colours['pad-colour-rgb'][k] for k in ('sequence_pulses_verified','sequence_background_nonzero','sequence_other_brightness_stable','sequence_tempo_and_meter_verified'))
if revision in ('r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y'):
 assert all(compressor[k] for k in ('paw_actual_reduction_verified','paw_no_false_strikes_verified','paw_dsp_timing_verified'))
 assert colours['pad-colour-rgb']['sound_cursor_colour_verified']
if revision in ('r26s','r26t','r26u','r26v','r26w','r26x','r26y'):
 assert compressor['minimum_gain_db']==-6
 assert colours['pad-colour-rgb']['workflow_colours_verified'] and colours['pad-eq']['native_sidebars_preserved']
 resample=json.loads((R/'build/resample-stock/verification.json').read_text())
 assert resample['passed'] and resample['ubsan']
 for n,h in resample['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
if revision in ('r26t','r26u','r26v','r26w','r26x','r26y'):assert restore['batch_selection_pulses_verified'] and colours['pad-colour-rgb']['workflow_selection_pulses_verified']
if revision in ('r26w','r26x','r26y'):
 lighting=json.loads((R/'build/pad-lighting/verification.json').read_text())
 assert lighting['passed'] and lighting['ubsan'] and lighting['global_preference_verified'] and lighting['six_rows_without_mod_off_verified' if revision=='r26y' else 'seven_rows_mod_off_last_verified']
 for n,h in lighting['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
 assert compressor['note_repeat_buttons_verified']
if revision in ('r26x','r26y'):
 assert compressor['repeat_header_stable_verified']
 for mode in ('normal','ubsan'):
  log=(R/'build/pad-eq'/(mode+'-ui.log')).read_text()
  assert 'PASS Standard retains Lights mode/key/scale and Transpose16 controls' in log
recovery=spectrum=step_toggle=early=None
if revision=='r26y':
 for folder in ('project-recovery','filter-spectrum','step-toggle'):
  proof=json.loads((R/'build'/folder/'verification.json').read_text())
  assert proof['passed'] and proof['ubsan'],folder
  for n,h in proof['sources_sha256'].items():assert sha((R/n).read_bytes())==h,(folder,n)
  if folder=='project-recovery':recovery=proof
  elif folder=='filter-spectrum':spectrum=proof
  else:step_toggle=proof
 assert all(recovery[k] for k in ('shutdown_temp_handoff_verified','native_autosave_destination_preserved','display_basename_verified','async_serializer_origin_verified','project_display_settings_verified','project_optocat_settings_verified','failed_read_preserves_settings_verified','first_save_confirmation_verified'))
 assert '-DMPC_PROJECT_RECOVERY=1' in compiled['compiler_flags'] and '-DMPC_STARTUP_SPLASH=1' in compiled['compiler_flags']
 early=json.loads((R/'build/early-startup/verification.json').read_text())
 assert all(early[k] for k in ('passed','ubsan','first_load_abi_verified','one_load_only_verified','early_rollback_verified','splash_bounds_verified'))
 for n,h in early['sources_sha256'].items():assert sha((R/n).read_bytes())==h,n
 assert '-DMPC_EARLY_STARTUP=1' in compiled['compiler_flags']
lib=(R/'build/chords/phone-input.so').read_bytes();assert sha(lib)==compiled['library_sha256']
with zipfile.ZipFile(sys.argv[1]) as z:
 assert z.testzip() is None;old={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
files=dict(old);files['autorun/phone-input.so']=lib
def replace_once(text,old,new):
 assert text.count(old)==1,old;return text.replace(old,new,1)
launcher=old['autorun/mod-launcher.sh'].decode()
launcher=replace_once(launcher,'SAVE_IO_SHA='+sha(old['autorun/phone-input.so']),'SAVE_IO_SHA='+sha(lib))
if revision=='r26y':
 from early_startup import transform_launcher,transform_entry
 files['autorun/boot-splash']=(R/'build/early-startup/boot-splash').read_bytes()
 assert sha(files['autorun/boot-splash'])==early['splash_sha256']
 launcher=transform_launcher(launcher,early['splash_sha256'])
files['autorun/mod-launcher.sh']=launcher.encode()
entry=old['autorun/autorun.sh'].decode();assert '# sd-filesystems=exfat,vfat\n' in entry
if revision=='r26y':entry=transform_entry(entry)
entry=replace_once(entry,sha(old['autorun/mod-launcher.sh']),sha(files['autorun/mod-launcher.sh']))
old_revision=re.findall(r'revision=(r\d+[a-z]+)',entry);assert len(set(old_revision))==1
files['autorun/autorun.sh']=replace_once(entry,'revision='+old_revision[0],'revision='+revision).encode()
manifest=json.loads(old['autorun/autorun.json']);manifest['value0']['scripts'][0]['message']='AM Mod '+revision+(' - Mod Settings and project snapshots; experimental FAT32' if revision=='r26y' else ' - Repeat header and Standard Piano/Scale; experimental FAT32' if revision=='r26x' else ' - Standard/AM Mod lighting and Note Repeat fix; experimental FAT32' if revision=='r26w' else ' - clear blinking focus and selected-only targets; experimental FAT32' if revision=='r26v' else ' - reserved service colours and muted event flashes; experimental FAT32' if revision=='r26u' else ' - selected source white/hue pulses; experimental FAT32' if revision=='r26t' else ' - EQ side indicators and workflow colours; experimental FAT32' if revision=='r26s' else ' - GR-driven paw and Sound cursor; experimental FAT32' if revision=='r26r' else ' - tempo-synced Seq pulses; experimental FAT32' if revision=='r26q' else ' - shared compressor Bypass; experimental FAT32' if revision=='r26p' else ' - OptoCat Warm; experimental FAT32' if revision=='r26o' else ' - OptoCat Bypass and pad colours; experimental FAT32' if revision in ('r26m','r26n','r26o','r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y') else ' - pad colours and Seq lights; experimental FAT32' if revision=='r26l' else ' - project startup restore; experimental FAT32' if revision=='r26k' else ' - OptoCat controls; experimental FAT32' if revision=='r26j' else ' - takeover correction; experimental FAT32')
files['autorun/autorun.json']=(json.dumps(manifest,indent=2)+'\n').encode()
files['autorun/autorun.check']=sha(files['autorun/autorun.json']+files['autorun/autorun.sh']+b'f7bed1c9-7e90-4731-9194-c996666b4331').encode()
changed=sorted(n for n,b in files.items() if old.get(n)!=b)
assert set(changed)=={'autorun/phone-input.so','autorun/mod-launcher.sh','autorun/autorun.sh','autorun/autorun.json','autorun/autorun.check'}|({'autorun/boot-splash'} if revision=='r26y' else set()),changed
for name in ['README.md','README.ru.md','CHANGELOG.md','docs/RELEASE_NOTES-'+revision+'.md','docs/RELEASE_NOTES-'+revision+'.ru.md','docs/RELEASE_NOTES-r26f.md','docs/RELEASE_NOTES-r26f.ru.md']:
 files[name]=(R.parent/name).read_bytes()
if revision in ('r26h','r26i','r26j','r26k','r26l','r26m','r26n','r26o','r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y'):
 files['COMPRESSOR_TYPE_RU.md']=(R/'COMPRESSOR_TYPE_RU.md').read_bytes()
 files['sd-mod-session/TAKEOVER_SCALED_RU.md']=(R/'TAKEOVER_SCALED_RU.md').read_bytes()
if revision in ('r26k','r26l','r26m','r26n','r26o','r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y'):
 for name in ['CONTRIBUTING.md','docs/FEEDBACK.md','docs/FEEDBACK.ru.md','docs/KNOWN_ISSUES.md','docs/KNOWN_ISSUES.ru.md','docs/RELEASE_NOTES-r26i.md','docs/RELEASE_NOTES-r26i.ru.md','docs/RELEASE_NOTES-r26j.md','docs/RELEASE_NOTES-r26j.ru.md']:
  files[name]=(R.parent/name).read_bytes()
if pad_colour_candidate:
 for name in ('docs/TESTER_FEEDBACK_LEUSHA.ru.md','docs/RELEASE_NOTES-r26k.md','docs/RELEASE_NOTES-r26k.ru.md'):
  files[name]=(R.parent/name).read_bytes()
if revision in ('r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y'):
 for name in ('docs/FEATURES.md','docs/FEATURES.ru.md'):files[name]=(R.parent/name).read_bytes()
if revision in ('r26u','r26v','r26w','r26x','r26y'):files['docs/PAD_LIGHTING_MAP.ru.md']=(R.parent/'docs/PAD_LIGHTING_MAP.ru.md').read_bytes()
files['README_RU.md']=('# AM Mod '+revision+(' — OptoCat и цвета пэдов\n\n' if pad_colour_candidate else ' — проекты и OptoCat\n\n' if revision=='r26k' else ' — OptoCat\n\n' if revision=='r26j' else ' — кандидат исправления ручек\n\n')+
 ('Пять пользовательских цветов и отдельная служебная подсветка; OptoCat/Warm и исправление K сохранены.\n' if revision=='r26u' else 'Выбранные источники Copy/Batch/Chop/Mono/Resample мигают белым и своим цветом; остальные сохраняют оттенки. Исправлены указатели редактора Chop/Tune. Все изменения r26s сохранены. Нужна проба на MPC.\n' if revision=='r26t' else 'Исправлены шкалы EQ и цвета в Copy, браузере, Stereo→Mono, Resample и Extract; Gain OptoCat −6…+6 dB. Сохранены Warm, лапка по GR, Sound и Seq. FAT32 экспериментальный.\n' if revision=='r26s' else 'Лапка OptoCat следует красной GR-полоске, бантик — входному сигналу. В Steps при удержании Sound белый курсор чередуется с цветом пэда. Все исправления r26q сохранены; нужна проба на MPC.\n' if revision=='r26r' else 'Пэды Seq: выбранный при Stop — раз в такт, играющий — по долям, очередь — вдвое чаще; фон не гаснет, другие пэды не тускнеют от Play. Все исправления r26p и Warm сохранены. Нужна проба на MPC.\n' if revision=='r26q' else 'Общий Bypass выключает оба компрессора. Shift+Pad5 открывает работающий тип; если всё выключено — Duck. Исправлена повторная привязка после New Project; Warm сохранён. Нужна проба на MPC.\n' if revision=='r26p' else 'OptoCat: B1 Warm вкл/выкл, B2 Type, B3 Bypass. Сохранены все предыдущие исправления; нужна проба на MPC.\n' if revision=='r26o' else 'Финальный OptoCat объединён с исправлениями цветов, K и r26k. При Bypass/сне шкалы не показывают сигнал. Нужна проба на MPC.\n' if revision in ('r26m','r26n','r26o','r26p','r26q','r26r','r26s','r26t','r26u','r26v','r26w','r26x','r26y') else 'Цвета в Duck/OptoCat и Copy/Extract с Undo/Redo; играющая Seq горит постоянно. Сохранены K, OptoCat и r26k. Нужна проба на MPC.\n' if revision=='r26l' else 'Автозагрузка ждёт закрытия штатных диалогов; сохранены OptoCat r26j и исправление K. Нужна проба на MPC.\n' if revision=='r26k' else 'OptoCat: Shift+K1 Attack, Shift+K2 Release, Shift+K3 Knee; Gain не выше +6 dB. Нужна проба на MPC.\n' if revision=='r26j' else 'Штатный путь ввода K1–K3 и расчёт takeover; сохранён OptoCat. Нужна проба на MPC.\n' if revision=='r26i' else 'Коррекция крайних положений Scaled; сохранён OptoCat с анимацией. Нужна проба на MPC.\n')+
 ('r26v: мигающий белый курсор; пустые цели без выбора погашены; зелёный/красный только у выбранных назначений.\n' if revision=='r26v' else 'r26u: пять назначаемых цветов; красно-зелёные цели; серые вспышки Mute; белый курсор Sample/Chop.\n' if revision=='r26u' else '')+
 'Поддерживаются exFAT и экспериментальный FAT32; прошивка 1.3.0.12.\n'+
 'Скопируйте полную папку autorun в корень карты. Проекты и сэмплы можно оставить.\n'+
 'Подробности: README.ru.md и docs/RELEASE_NOTES-'+revision+'.ru.md. '+('Область проверок и оставшиеся ограничения указаны в релизе.\n' if revision=='r26s' else 'Кандидат не опубликован.\n')).encode()
if revision=='r26v':
 files['README_RU.md']=("# AM Mod r26v — проверочная сборка\n\n"
  "Текущий пэд Sample/Chop/Sound мигает: белый400 мс ↔ свой цвет400 мс. "
  "Свободные цели без выбора погашены; выбранные пустые зелёные, занятые красные.\n"
  "Пять цветов, серые вспышки Mute, ритм Seq, K, EQ и OptoCat/Warm сохранены.\n"
  "exFAT и экспериментальный FAT32; прошивка1.3.0.12.\n"
  "Копировать полную autorun в корень карты. Проекты/сэмплы сохраняются.\n"
  "Подробности: docs/RELEASE_NOTES-r26v.ru.md. Кандидат не опубликован.\n").encode()
if revision in ('r26w','r26x','r26y'):
 files['README_RU.md']=("# AM Mod r26w — проверочная сборка\n\n"
  "Project → Pad Lighting: Standard / AM Mod. B2 — Standard, B3 — AM Mod, K1/энкодер — выбор, B1 — назад.\n"
  "Общая настройка хранится на SD отдельно от проектов; назначенные цвета сохраняются.\n"
  "Project: семь пунктов в штатном окне на шесть строк, Mod Off последним.\n"
  "Исправлен B2 компрессора при Note Repeat; B1–B3 выбирают скорость.\n"
  "OptoCat/Warm, K, EQ, exFAT и экспериментальный FAT32 сохранены.\n"
  "Подробности и границы проверок: docs/RELEASE_NOTES-r26w.ru.md. Не опубликовано.\n").encode()
if revision=='r26x':
 for lang in ('','ru.'):
  name='docs/RELEASE_READINESS-r26x.'+lang+'md';files[name]=(R.parent/name).read_bytes()
 files['README_RU.md']=("# AM Mod r26x — проверочная сборка\n\n"
  "Исправлен путь скрытой перерисовки Note Repeat, мешавший заголовку Type.\n"
  "В Standard доступны Lights/Piano/Scale и музыкальные цвета 16 Levels, тоника/лад/Transpose16.\n"
  "Pad Colour/Seq Colour в Standard отключены; назначения сохранены.\n"
  "Все исправления r26t–w и OptoCat/Warm сохранены. Стабильный релиз пока не готов.\n"
  "Полная autorun в корень SD; прошивка1.3.0.12; exFAT и экспериментальный FAT32.\n"
  "Проверки: docs/RELEASE_READINESS-r26x.ru.md. Не опубликовано, нужна проба на MPC.\n").encode()
if revision=='r26y':
 for name in ('docs/MOD_SETTINGS.md','docs/MOD_SETTINGS.ru.md','docs/RELEASE_READINESS-r26y.md','docs/RELEASE_READINESS-r26y.ru.md','docs/BOOT_FLOW.ru.md','docs/STARTUP_AND_RECOVERY_PLAN.ru.md'):
  files[name]=(R.parent/name).read_bytes()
 files['README_RU.md']=("# AM Mod r26y — проверочная сборка\n\n"
  "Mod Settings: вкладки по кольцу B2/B3, энкодер — строки/значения.\n"
  "Restore recommended — последним в каждом разделе; B1 отменяет, B3 подтверждает.\n"
  "Настройки отображения и OptoCat/Warm сохраняются с XPJ.\n"
  "Ранняя заставка Am MOD; SD-проект в первой штатной загрузке, без поздней повторной автозагрузки.\n"
  "Mod Off удалён; обычное выключение сохраняет копию. SEQ RECORD — WRITE, SAMPLE RECORD — TOGGLE.\n"
  "Перед испытанием сделайте бэкап проектов и сэмплов.\n"
  "Полная autorun в корень SD; прошивка 1.3.0.12; exFAT и экспериментальный FAT32.\n"
  "Инструкция: docs/MOD_SETTINGS.ru.md; проверки: docs/RELEASE_READINESS-r26y.ru.md.\n"
  "Локальная сборка, не опубликована; аппаратная проверка ещё нужна.\n").encode()
for n in ['autorun.sh','mod-launcher.sh']:
 subprocess.run(['sh','-n','/dev/stdin'],input=files['autorun/'+n],check=True)
out=R.parent/('am-mod-'+revision+'.zip');assert not out.exists(),out
with zipfile.ZipFile(out,'x',zipfile.ZIP_DEFLATED) as z:
 for n,b in sorted(files.items()):z.writestr(n,b)
with zipfile.ZipFile(out) as z:assert z.testzip() is None and all(z.read(n)==b for n,b in files.items())
report=dict(early_startup_checks=early,project_recovery_checks=recovery,filter_spectrum_checks=spectrum,step_toggle_checks=step_toggle,resample_checks=resample if pad_colour_candidate else None,pad_lighting_checks=lighting if revision in ('r26w','r26x','r26y') else None,passed=True,revision=revision,package_sha256=sha(out.read_bytes()),library_sha256=sha(lib),changed_payload=changed,payload_sha256={n:sha(b) for n,b in files.items() if n.startswith('autorun/')},input_checks=tests,compressor_checks=compressor,project_restore_checks=restore,project_exit_checks=exit_copy,pad_colour_checks=colours,compile_verification=compiled,full_preload_constructor=True,hardware_warp_tested=False,full_native_warp_ui_tested=False,published=False)
(O/'package-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(dict(package=out.name,sha256=sha(out.read_bytes()),library_sha256=sha(lib),verified=True,hardware_warp_tested=False),indent=2))
