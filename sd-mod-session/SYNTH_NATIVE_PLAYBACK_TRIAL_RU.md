# Привязка Synth к проекту и native playback: изолированный прототип

Продолжение `4d4ca42` в ветке `codex/pad-midi-recording`. Установочный пакет и полный preload не собирались по указанию пользователя. Результаты предыдущей полной VM-проверки записи относятся к прежней библиотеке; они не подтверждают автоматически этот этап.

## Что добавлено в исходниках

`synth_binding.c/.h` хранит явное назначение **Program identity + carrier → Synth** и 36 параметров инструмента. Точная строка identity берётся из native Program `+0xdc8`, как в существующем `pad_eq_native.c`. Runtime pointer, выбранный на экране пэд, MIDI-канал и шина A–H не заменяют identity. Пока поддержан один Synth и пустой A16; пользовательского выбора carrier здесь нет. При несовпадении identity или занятом sample carrier нота Synth не запускается. Rename/replacement Program требует проверки устойчивости ключа в полной программе; автоматический перенос назначения не предполагается.

`synth_archive.cc` добавляет необязательное строковое поле `amSynthV1` в существующий native архив. Payload `AMSY1` содержит только ключ Program, carrier и параметры; ноты по-прежнему являются native tag-3 событиями и не дублируются. Сжатие, открытие файлов и commit XPJ выполняет прежний код. Legacy-проект без поля сохраняется без него, пока пользователь явно не включит MIDI trial. Неизвестный/неподдержанный строковый payload сохраняется байт в байт и запрещает перезапись через arm; duplicate/non-string extension считается ошибкой. Публикация нового состояния происходит после успешного внешнего `music_read`; ошибки сохраняют предыдущее состояние этого расширения. Это не обещание общей транзакции всех подсистем проекта.

`synth_pad_record.inc` сохраняет binding при включении trial, применяет загруженный preset и переносит изменения параметров в project-owned snapshot на UI tick. Выключение клавиатурной записи не удаляет назначение инструмента. Load другого проекта с тем же адресом Program всё равно разоружает ввод благодаря отдельному generation. Legacy load не заменяет глобальный Synth preset. Проверка native dirty/autosave при редактировании только параметров ещё нужна; явный Save использует snapshot.

`phone_native.c` оборачивает существующие Save/Load/Read, не заменяя Mixer/music archive, и сбрасывает owned-голоса на New/Load. Все эти изменения включаются только с `MPC_PAD_MIDI_RECORDING_TRIAL`.

## Воспроизведение: отдельный, пока выключенный флаг

Дополнительный `MPC_SYNTH_NATIVE_PLAYBACK_TRIAL` требует pad trial и `MPC_MIDI_EVENTS`. В build script это `AM_MOD_SYNTH_NATIVE_PLAYBACK_TRIAL=1`; по умолчанию флаг выключен. Полная библиотека с ним пока не собиралась.

`synth_accept_native.cc` перехватывает точный virtual call штатного dispatcher: `0x1e1e350`, ожидаемая инструкция `blr x6`. Штатный Track строит 44-байтовую команду, затем Track и Program выполняют собственные channel/note/velocity filters. Только прошедшая команда попадает в tap. Сам tap не требует открытия внешнего MIDI-порта. Исходный virtual backend всегда вызывается с прежними аргументами — существующая MIDI-маршрутизация не подавляется.

Исследованный общий filter `0x1e5e280` не вычисляет probability: он проверяет channel, разрешённые ноты и диапазон velocity. Adapter не перебирает исходные события, не делает собственных probability решений и не размножает ratchets. Но фактическое прохождение probability/Note Repeat через весь Engine всё ещё требует полной VM-проверки.

`synth_live.inc` встраивает токен `synth_sequence` в существующий `live_note`; второй event pool, clock или таймер длительности не создаётся. Используется окончательная velocity команды и native Tune относительно MIDI60. Slice, другие choices и изменение source note на этой границе пока дают отказ Synth. Live key input пропускается, чтобы не удваивать уже существующий worker Synth.

Off приходит через существующий exact due-heap release. Для одинаковых source/tick/duration/Tune выбирается самый старый occurrence ID, а не первый адрес массива. Раннее aggregate release сохраняет tombstone до соответствующего scheduled Off: новое одинаковое событие не занимает его место. При полном буфере Off отменяет owned-поколение вне очереди; native MIDI releases продолжаются. При заполненном live pool новые события отклоняются существующим bounded механизмом.

`synth_route.c` публикует с UI только атомарный value snapshot ключа, carrier, source generation и serial. Native event receiver делает ограниченное число попыток чтения, без архивного mutex, heap allocation или ожидания UI. Смена параметров не меняет инструментальную identity. После постановки On повторно проверяется, не произошли ли Stop/Load/rebind во время копирования команды.

Физический Stop передаёт отдельный serial и немедленную отмену owned-голосов на UI tick. Закрытие Synth Keys прекращает клавиатурный ввод, но не считается Stop секвенции. Смена binding сохраняет старые tombstones; настоящая смена проекта/Stop очищает их на music executor. **Требуется full-VM подтверждение**, что native Stop отменяет соответствующие due entries, а также аудит остальных transport Stop/Seek/конца песни. Эти предпосылки нельзя считать доказанными тестом с fixture транспортом.

Native offset до virtual backend сохраняется в frames: в тесте значение 96 проходит неизменным. Synth DSP пока применяет очередь на границе своего buffer. Нет доказательства sample-accurate фазы общего аудиографа; offset не выдаётся за реализованное смещение внутри DSP buffer.

## Проверки и их пределы

`verify_synth_archive.py`: host, host ASan/UBSan, AArch64 QEMU, AArch64 UBSan. Проверены bounded codec, все параметры, legacy/future/corrupt payload, staged read commit/rollback, исключения, stale UI edits, конкурентные snapshot и публикация route, UI arm/disarm и восстановление preset. Native archive frames и writer — **fixtures**, реального XPJ-файла здесь нет.

`verify_synth_live_native.py`: normal/UBSan на pinned штатном ELF SHA256 `b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508`. Исполняются настоящие Track event/command builder, оба filters, перехваченный native virtual call и native due heap. Проверяются:

- передача исходному backend и неизменность source event;
- высота MIDI67 в настоящем Synth DSP из Tune +7;
- перекрывающиеся одинаковые ноты и независимые scheduled releases;
- Track mute, оба channel filters, занятый carrier, неверный Program key, отсутствие дублирования клавиатурного ввода;
- ранний Off, поздний scheduled Off и новый identical occurrence;
- потеря binding, Stop с заполненным pool, новое поколение проекта;
- заполненная command queue и owned-only аварийное освобождение.

Project generation, instrument lookup/sample presence, property notification, optional Program RTTI extension, virtual backend sink и aggregate-release engine service являются **fixtures**. Это не запущенная полная программа, не физические пэды и не проверка аппаратной производительности. Логи и hashes — `build/synth-live-native/verification.json`, `build/synth-archive/verification.json`.

Регрессионные проверки отдельно: `verify_synth_queue.py`, `verify_piano_synth_input.py`, `verify_midi_event.py`. Syntax-only проверяется реальное включение phone/workflow/mixer/MIDI источников с обоими trial flags; она не заменяет full link/load.

## Следующий обязательный этап

После разрешения на полную тестовую сборку: запустить isolated system QEMU, записать партию физическим pad protocol, воспроизвести при закрытом Synth UI, изменить ноты в существующем Piano Roll, сохранить native XPJ, выполнить New/Load и проверить preset/binding/ноты. Отдельно проверить UI и native Stop, Seek, конец Song, loop, probability, Note Repeat, simultaneous live input, sample tails, export/mixdown и аудиофазу. До этого сценарий A не объявляется завершённым и сборка на устройство не предлагается.

Horizontal Arrangement, музыкальные Tracks, параллельные clips, automation lanes и CHOP на курсоре остаются следующим этапом архитектуры; этот прототип их не включает.
