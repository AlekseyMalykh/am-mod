# Native Engine, XPJ и аппаратный путь Synth Keys

Актуальное продолжение: [привязка Synth и native playback trial](SYNTH_NATIVE_PLAYBACK_TRIAL_RU.md). Это изолированные тесты нового исходного кода; приведённая здесь полная VM-библиотека остаётся прежним checkpoint.

Следующее изменение после проверки этого этапа: [ядро приёма событий Synth и владение голосами](SYNTH_SEQUENCE_CORE_RU.md). Проверенная здесь полная VM-библиотека относится к прежнему checkpoint; новый приёмник пока не включён в native playback.

Работа ведётся только в `codex/pad-midi-recording`. Это opt-in прототип, не release для физической карты.

## Подтверждённый цикл native Engine и XPJ

В полном системном QEMU запущен штатный ELF 1.3.0 с полной trial-библиотекой Am MOD. Через штатное меню New Project создан пустой проект. Native Seq Record / Play запускают запись; VM-only ввод отправляет две ноты через существующий `batch_piano_synth_input` и настоящий `Engine+0x211b090`, без подмены Engine, pending recorder или project commit.

Сохранённый штатным autosave XPJ содержит два native tag-3 события на пустом carrier A16: музыкальные высоты 60 и 67, velocity 54 и 112, положительные начала/длительности. Native Tune сериализуется штатными `modifierValue0` / `modifierActiveState0`. В этом прогоне времена 2880/6960, длительности 7270/7418 native ticks; первая сыгранная нота пересекает конец двухтактового loop. Длительность здесь зависит от медленного SSH-ввода стенда и не является измерением задержки аппаратных пэдов.

Неизменённый native autosave сохранён как именованный XPJ на отдельной виртуальной SD. Затем штатный New Project очищает музыкальные данные (в следующем autosave 0 нот), и штатный Load Project восстанавливает XPJ. Все поля двух событий после загрузки совпадают с записанными. Это проверка настоящего загрузчика, а не только JSON encode/decode. Прошивка, образы дисков и XPJ не добавляются в Git.

Доказательства в игнорируемом `build/pad-midi-vm`: `engine-verification.json`, `engine-before.xpj`, `engine-recorded.xpj`, `reload-cleared.xpj`, `engine-reloaded.xpj`, `engine-journal.log`. Отчёт содержит хеш именно проверенной библиотеки; он не распространяет результат автоматически на последующие сборки.

## Подключение пэдов: ограниченный trial

Добавлен путь **Mixer → Synth → 16 Levels → Shift + Seq Record**. Комбинация явно включает/выключает session-only MIDI trial. Обычные Seq Record / Play сохраняют штатное значение. Нужен пустой A16: прототип не заменяет его sample и не выбирает носитель по MIDI-каналу или аудиошине. Включение показывает `MIDI trial: empty A16`; занятый A16 даёт отказ.

Пэдовый пакет проходит production `phone_native` → существующую UI FIFO → `mixer_control_pad_velocity` → `workflow_synth_pad` → ту же native worker queue. В музыкальное событие передаются высота текущей раскладки и настоящая velocity из пакета. На Down сохраняется отдельный occurrence ID, на Up используется он же; последующая смена раскладки не меняет адрес отпускания. Legacy Synth On не отправляется вторично. Stop, выход из Synth/Keys и смена Program завершают пробные удержания; непрошедшие отпускания повторяются на UI tick. Worker повторно проверяет пустоту A16 и взаимно обратные pad/note tables до каждого On.

В `synth.c` только trial-сборка получает фиксированную MPSC очередь на 64 команды. Producer ограничен числом попыток, audio callback не ждёт незавершённый slot и читает не более 64 команд за buffer. Note On резервирует 16 мест под Off. Gate ownership записи остаётся на native music worker; старые external/UI producers используют безопасный общий транспорт. Адресация голосов самого Synth по pitch остаётся прежней: независимая идентичность одинаковых нот от разных источников ещё требует отдельной модели голосов.

## Что ещё не готово

- Этот ввод подключён к **Synth Keys в Mixer**, не ко всем экранам Synth и не к обычной Sample Record клавиатуре.
- Назначение carrier → Synth и preset пока не сохраняется. Сохранённые ноты переживают XPJ load, но автоматическое воспроизведение этих нот Synth после загрузки ещё не подключено. Нельзя считать пользовательский сценарий A полностью готовым.
- Нет пользовательского выбора carrier, сохранённой инструментальной дорожки, интеграции Note Repeat и Synth offline mixdown. Это следующие части реализации, а не доказанные возможности этого этапа.
- MIDI trial выключен в обычной сборке. На реальном MPC аппаратное поведение и производительность ещё не проверены.

## Воспроизводимость

Из `sd-mod-session` под WSL с AArch64 toolchain, user-QEMU sysroot и отдельно подготовленной full-system лабораторией `/opt/mpc-sample-lab`:

```sh
python3 verify_piano_synth_input.py --stock '/path/to/unmodified/MPC Sample'
python3 verify_synth_queue.py
AM_MOD_BUILD_NAME=pad-midi-pads AM_MOD_PAD_MIDI_RECORDING_TRIAL=1 \
AM_MOD_PAD_MIDI_VM_PROBE=1 AM_MOD_BOOTSTRAP_DIR='/path/to/verified/bootstrap-directory' \
MPC_STOCK_APP='/path/to/unmodified/MPC Sample' python3 build_mixer_r27r.py
```

Frozen build manifest `build/r23a/build-info.json`, bridge headers и bootstrap берутся из проверенного исходного build окружения, не из случайной сборки. `build_mixer_r27r.py` проверяет загрузку полного preload и существующие phone routes. `lab/pad_midi_vm.py` создаёт частные overlay/SD и проверяет их путь по QMP перед guest-доступом. Переменные `AM_MOD_VM_RUN_NAME` (префикс `pad-midi-recording`) и `AM_MOD_VM_BUILD_NAME` выбирают отдельный стенд/артефакты. `package` принимает локальный проверенный release envelope; `stage` проверяет актуальность source hashes и library hash до записи в VM. Лабораторные скрипты не предназначены для подключения к устройству.

`verify_synth_queue.py`: normal/UBSan, 4 producer × 20 000 команд, порядок внутри каждого producer и ровно одна доставка, полный buffer, резерв Off, незавершённый slot и wrap счётчиков. `verify_piano_synth_input.py`: прежние шесть normal/UBSan прогонов, включая общий Synth DSP, stale/reverse releases, смену Program и backpressure.

`lab/check_pad_midi_engine_vm.py` проверяет native запись диагностического ввода. `lab/check_synth_pads_vm.py` предназначен для production-пэдового пути через протокол control surface в QEMU; его отдельный `pads-verification.json` появляется только после успешного полного прогона. Эмуляция control surface не равна проверке физического устройства.

Полный пэдовый прогон выполнен: Pad 5/7 → production hooks → Synth Keys → native Engine → XPJ, velocity 73/101, высоты 52/54, начала 480/1200, длительности 1053/820 ticks (есть перекрытие). В момент проверки после обоих Up, ещё до Stop, worker сообщил `faults=0 held=0`. После штатного New Project autosave содержит 0 нот; штатный Load Project восстанавливает обе ноты без изменения полей. `lab/check_synth_pads_load_vm.py` сравнивает события и обновляет отдельный `build/pad-midi-pads/pads-verification.json`. Проверенная библиотека: `b20b44f7422d2a8fa4e6c0c5f791a8f6a709e4a35e33c759aea4d3bce1fc517d`. Снимок `synth-keys-armed.ppm` — настоящий экран QEMU, не макет.
