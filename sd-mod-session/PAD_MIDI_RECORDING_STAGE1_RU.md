# Первый прототип представления нот с пэдов

Ветка `codex/pad-midi-recording`, отдельная рабочая копия от `am-mod/main` `a99d474`. Новые application hooks не устанавливаются, формат сохранения не меняется. Пробный ввод Synth пока не включён в release build.

`src/pad_midi_note.c/.h` создаёт native tag-3 событие с абсолютной музыкальной высотой, представленной через Tune относительно MIDI 60, реальной velocity и MIDI channel. Использует переданные штатные factory/choice helpers и существующий `midi_event` decoder. Нота источника — native Program note, а не номер физического пэда. Назначение Synth должно приходить от явного binding; канал или открытый экран не используются как признак инструмента.

`check_pad_midi_note_native.c` запускает под AArch64/QEMU настоящую фабрику событий и packed choices точного stock ELF. Проверены все 128 высот × 127 velocity, каналы, отказ без изменения результата, отбрасывание Chop/чужих модификаторов, завершение двух разных нот одного источника в обратном порядке и повторный устаревший Off. Native pending recorder считает длительности; конечный project commit заменён fixture. Создание pending notes также выполняется fixture, поэтому это не проверка полного Engine input.

Значения исходной и записанных нот подаются в реальный `synth.c`; проверяется ненулевой конечный PCM и завершение голоса после Off. Это не проверка общего live/offline audio routing и не доказательство точной временной доставки в Synth. В native fixture нет Sample/audio voice, но из этого нельзя заключить, что полноценный Engine принимает и сохраняет ноты пустого carrier без дополнительного исследования.

Проверка:

```sh
python3 sd-mod-session/verify_pad_midi_note.py --stock '/path/to/unmodified/MPC Sample'
```

Нужны AArch64 GCC, qemu-aarch64 и sysroot (можно передать `--sysroot`). SHA-256 stock ELF проверяется до запуска. Результаты normal/UBSan и hashes сохраняются в `sd-mod-session/build/pad-midi-note/verification.json`. Файлы прошивки не включаются в Git.

Следующий runtime этап — доказать полный путь native input/recorder для явного carrier, независимые releases и безопасную привязку/отвязку context, затем подключать физические Synth Keys. Пока обычные пэды, текущий Piano Roll, Sequential Song, Mixer и Save/Load используют прежний код.

## Пробный Synth input в очереди Piano Roll

Под `MPC_PAD_MIDI_RECORDING_TRIAL` добавлена `batch_piano_synth_input` в существующий `piano_input.inc`, без второго worker или собственного recorder. Она принимает подготовленное native событие и ненулевой occurrence ID, который ввод обязан захватить на Down и повторить на Up. В music worker проверяются Program, взаимные таблицы note/pad и пустота carrier; событие идёт через прежний `0x211b090`, а Synth получает ту же высоту и velocity. Это проверяемый session-only мост, не сохранённый instrument binding и не scheduler для записанной песни.

Одинаковая высота на нескольких клавишах использует общий gate до последнего отпускания. Повторный Down закрывает прежнюю ноту; старый/дублированный ID не может переписать новый Down или отпустить новый голос. Поздний обычный Piano key-up также не владеет пробным Synth голосом. Stop инвалидирует ещё не исполненные Ons. При изменении проекта безопасно освобождаются значения Synth без разыменования старых native identities; caller обязан завершить ввод до смены/закрытия context.

Добавленная `am_synth_try_note` сообщает acceptance и резервирует 16 команд для отпусканий. Если Synth не принимает On, recorder handoff не выполняется. Непринятый Off сохраняет владение для повторной отправки; UI получает счётчик отказов и snapshot held keys через `batch_piano_synth_faults/held`. После освобождения очереди caller повторяет release/Stop. Обычные `am_synth_note/cc` сохраняют прежний путь.

Пробный режим требует эксклюзивного единственного producer Synth — этого music worker. Его нельзя одновременно включать с независимыми прямыми UI/MIDI producers: текущая Synth queue является SPSC, а Note Off адресуется по pitch. До подключения аппаратных Keys нужно объединить владельцев ввода/вывода и обработку потерь очереди; один compile-флаг этого не решает. Автоматическое включение trial, новые пункты UI и запись на SD здесь отсутствуют.

`verify_piano_synth_input.py --stock ...` генерирует pinned bridge headers штатным генератором, останавливая его до сборки preload, затем запускает normal/UBSan в двух вариантах: обычный Piano input без флага, тот же Piano input с флагом и новый Synth fixture. Всего шесть QEMU запусков. Проверяются настоящие native ThreadQueues/lifetime и Synth DSP, captured pitch/velocity, обратные/stale releases, shared gates, Stop, смена проекта, занятый carrier, native/Synth backpressure и retry Off после заполнения CC-командами. Engine On и recorder release receivers в этих проверках заменены fixtures. Полное принятие/сохранение события живым native Engine и XPJ round-trip ещё не доказаны.

Протокол: `build/piano-synth-input/verification.json`, отдельные логи рядом. Первый native codec/recorder-finalization набор остаётся в `build/pad-midi-note/verification.json`. Успех этих тестов не означает готовности пользовательского сценария A на устройстве.

## Визуальный прототип

`lab/preview_song_workflow.c` исполняется под QEMU и генерирует 17 кадров в native сетке 320×240. Цвета, верхние B1–B3 и нижние K1–K3 продолжают существующие painters Mixer/MIDI/Piano Roll. Это отдельный UX painter с тестовыми waveform/нотами, не установленное приложение и не запись экрана MPC.

Он показывает смешанный Arrange, вход в Pattern, удержание DATA для Range, редактирование/Apply, запись в Arrange и Pattern, Auto/Loop/Punch, Insert, Empty Pattern, вокал на пэде, Auto lane и Render to Pad. Кадры записи и рендера иллюстрируют проектируемое поведение; реальные звукозапись и создание WAV в этом painter не выполняются.

Сборка и экспорт из `sd-mod-session`:

```sh
mkdir -p build/song-workflow
aarch64-linux-gnu-gcc -std=c11 -O1 -g -Wall -Wextra -Werror lab/preview_song_workflow.c -lm -o build/song-workflow/preview-native
qemu-aarch64 -L /path/to/sysroot build/song-workflow/preview-native build/song-workflow
python3 lab/render_song_workflow.py build/song-workflow
```

PNG и две GIF находятся в `build/song-workflow/previews`. Шрифт подменён Consolas/DejaVu Sans Mono; это не pixel-perfect воспроизведение native JUCE font. Нативный размер и увеличенные картинки сохраняются отдельно. Изменения происходят только в этой ветке; на SD ничего не записывалось.
