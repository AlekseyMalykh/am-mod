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

Для установления владельца теперь обёрнут общий native entry `0x2119d00(engine, offset, track, event, kind, tag)`. Небольшой C++ scope передаёт точные Engine и Track в существующий receiver; вложенный вызов/исключение восстанавливает предыдущий scope. Synth layer допускается только внутри известного entry и для его Track. Прямой вызов Track без этого происхождения продолжает выполнять прежний backend, но не угадывает владельца Synth. Живой Synth worker использует прежний независимый API.

Callsite `0x211a114` наблюдает постановку конкретной принятой ноты в штатную очередь. Только после успешного оригинального `0x212bc10` occurrence получает runtime-признак `native_due - native_duration`, а также identity очереди и исходный канал. Никакой указатель на stack event не сохраняется. Если уже принятый On не удаётся сопоставить с реально поставленным событием или постановка прерывается исключением, соответствующий Engine/Track освобождается и блокируется до успешной отмены либо New/Load. Captured MIDI Off также отправляются; новые MIDI On этого аварийного владельца подавляются, чтобы не создавать новые ноты без надёжного отпускания. Другой Track продолжает работать.

Off приходит через существующий exact due-heap release. Проверены три штатных пути: `0x21185a0`, `0x2118c20`, `0x2118ed0`. Их release callsites — `0x2118654`, `0x2118d04`, `0x2118fbc`. Первые два передают entry из x8; третий должен захватить entry из x2 **до** native загрузки Track в этот регистр на `0x2118fb8`. Все дополнительные hooks включаются только с playback trial. Учитываются Engine, Track, source channel, исходные tick/note/Tune/Slice и native-признак постановки. Неизменённая duration имеет приоритет над перенесённым отпусканием: одновременные унисоны разной длины завершаются независимо. Среди эквивалентных кандидатов выбирается самый старый occurrence ID. Раннее aggregate release сохраняет tombstone до соответствующего scheduled Off: новое одинаковое событие не занимает его место. При полном буфере Off отменяет owned-поколение вне очереди; native MIDI releases продолжаются. При заполненном live pool новые события отклоняются существующим bounded механизмом.

`synth_route.c` публикует с UI только атомарный value snapshot ключа, carrier, source generation и serial. Native event receiver делает ограниченное число попыток чтения, без архивного mutex, heap allocation или ожидания UI. Смена параметров не меняет инструментальную identity. После постановки On повторно проверяется, не произошли ли Stop/Load/rebind во время копирования команды.

Физический Stop передаёт отдельный serial и отмену owned-голосов непосредственно при получении запроса кнопки, с фактическим освобождением на следующем Synth audio buffer. Отложенный UI tick завершает клавиатурный ввод, но больше не может отменить ноты уже следующего Play. Этот production button hook пока проверен только syntax-only, а не полной VM. Закрытие Synth Keys не считается Stop секвенции.

Запрос Stop **не очищает** tombstones: он не доказывает удаления старых native due entries. До штатного подтверждения они занимают места пула, и при исчерпании новых нот Synth не принимается. Позднее scheduled Off старого события по-прежнему находит старый occurrence, не новый. Смена проекта сохраняет прежнюю отдельную generation policy.

Для подтверждения остановки обёрнута настоящая native функция `0x2119550`, вызываемая, в частности, из `0x211fa84` и `0x21205ec`. Она сама освобождает heap `engine + 0x6c8`. Внутри её цикла callsite `0x211969c` наблюдает текущий entry до штатного release gate и находит соответствующий Synth occurrence. Captured Synth/MIDI Off отправляются даже при отказе gate после изменения состояния Track. Оригинальный gate и остальной native drain исполняются. Только после успешного возврата и проверки пустоты native heap освобождаются захваченные слоты. Сопоставление включает Track и полный используемый fingerprint; очистка очереди другого Engine не отменяет наши ноты.

Drain scope живёт на стеке C++-адаптера: 256 occurrence IDs, около 2 КиБ, без allocation и mutex. На каждую entry, которую уже удаляет native цикл, выполняется скан существующего пула не более 256 элементов. Вложенные вызовы восстанавливают предыдущий thread-local scope; исключение восстанавливает scope без преждевременного удаления tombstones. Проверка ID перед удалением защищает от повторного использования слота. Три дополнительные relay и native drain trampoline используют четыре страницы по 4 КиБ при установке hooks; отдельного event pool нет.

Это исправляет прежнюю предпосылку checkpoint `ba66cd7`, где очистка по UI Stop происходила до подтверждения native heap. **Остаётся** full-VM аудит всего Stop/Play, Seek и конца Song; следующие изолированные проверки не заменяют его.

## Отмена Track и перенос отпускания

После checkpoint `c3c1aae` добавлена обёртка настоящего удаления очереди Track `0x211e610`. Единственный найденный прямой caller — `0x20eaea0` внутри управления текущим Track, перед последующими действиями над его состоянием/Program. В scope захватываются только occurrence IDs данного **Engine + native Track**. Оригинальная функция удаляет свои entries и восстанавливает heap; после её успешного возврата отправляются captured Synth/MIDI Off и освобождаются соответствующие слоты. Другие Track, тот же Track в другом Engine и legacy live Synth не сбрасываются.

Если удаление прервалось, даже уже после изменения heap, слоты не переиспользуются, а затронутый владелец временно блокируется. Повторная успешная native отмена снимает блокировку, в том числе когда первоначальная операция уже успела удалить все его entries. Небольшой связанный список активных scopes защищает вложенную отмену; проверка ограничена восемью scopes и далее даёт безопасный отказ. История музыкальных событий, native sequence и XPJ при этом не меняются. Runtime native Track здесь **не является** новой музыкальной дорожкой Horizontal Arrangement.

Отдельно выполнена штатная функция `0x211e7b0`, которая переносит выбранные будущие отпускания на границу. Она меняет due и duration на одинаковую величину; признак `due - duration` сохраняется. Поэтому голос Synth остаётся активным до реального native Off и узнаётся после переноса без дополнительного retrigger, all-notes-off или собственного таймера. Поддержана проверенная операция, а не произвольное изменение heap. Для ещё не сопоставленного entry сохраняется прежнее точное сравнение полного fingerprint; подставлять выбранный на UI Track вместо владельца нельзя.

В QEMU проверены остаточные native duration 120, 0 и −10 ticks, а также одинаковые исходные ноты разных loop origins, одновременно перенесённые на одну границу, и разные source channels. Нулевая/отрицательная duration здесь относится только к изменённому **runtime release entry**; исходная музыкальная нота остаётся с прежней положительной длительностью. Это не исправление и не воспроизведение сообщения о коротких записанных Sample Note On / 16 Levels в конце секвенции: recorder и сохранённые данные этим изменением не переписываются.

Дополнительные расходы этого этапа ограничены метаданными в существующих 256 `live_note`, небольшим event scope и примерно 2 КиБ на stack scope отмены. Нет нового event pool, audio mutex, обхода исходных секвенций в render callback или отдельной транспортной шкалы. Два prologue trampoline и дополнительный relay используют ещё три страницы по 4 КиБ при установке trial hooks. Владелец live state предполагается тем же native music executor; проверка реальных потоков вызова cancel/dispatch/drain остаётся частью full-system этапа.

Native offset до virtual backend сохраняется в frames: в тесте значение 96 проходит неизменным. Synth DSP пока применяет очередь на границе своего buffer. Нет доказательства sample-accurate фазы общего аудиографа; offset не выдаётся за реализованное смещение внутри DSP buffer.

## Проверки и их пределы

`verify_synth_archive.py`: host, host ASan/UBSan, AArch64 QEMU, AArch64 UBSan. Проверены bounded codec, все параметры, legacy/future/corrupt payload, staged read commit/rollback, исключения, stale UI edits, конкурентные snapshot и публикация route, UI arm/disarm и восстановление preset. Native archive frames и writer — **fixtures**, реального XPJ-файла здесь нет.

`verify_synth_live_native.py`: normal/UBSan на pinned штатном ELF SHA256 `b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508`. Исполняются настоящий общий Engine entry и его постановка в очередь, Track event/command builder, оба filters, перехваченный native virtual call, native due heap, отмена Track и перенос отпусканий. Проверяются:

- передача исходному backend и неизменность source event;
- высота MIDI67 в настоящем Synth DSP из Tune +7;
- перекрывающиеся одинаковые ноты и независимые scheduled releases;
- Track mute, оба channel filters, занятый carrier, неверный Program key, отсутствие дублирования клавиатурного ввода;
- ранний Off, поздний scheduled Off и новый identical occurrence;
- потеря binding, Stop request с оставшейся старой due entry, новое поколение проекта;
- все три настоящих due paths: один Off освобождает один из двух identical occurrences;
- настоящая native очистка heap, восстановление после заполнения всех 256 слотов, изоляция другого Engine;
- разрешение/отказ release gate, исключение посреди drain и вложенные scopes без преждевременного удаления;
- совместные Synth + настоящий native MIDI backend: парные On/Off с исходными occurrence IDs при drain;
- scoped native отмена Track: сохранение другого Track и того же Track в другом Engine, повторное использование освобождённого слота;
- ошибка отмены уже после удаления entries, ошибка после On до постановки Off, несовпадение поставленного события: освобождение, отказ новых нот и восстановление успешной отменой;
- MIDI Off затронутого Track без снятия чужой ноты той же высоты и без новых orphan On в аварийном состоянии;
- native перенос due/duration: положительный, нулевой и отрицательный остаток, loop origins, source channels, унисоны с разными исходными длительностями;
- заполненная command queue и owned-only аварийное освобождение.

Project generation, instrument lookup/sample presence, property/position notifications, optional Program RTTI extension, instrument virtual backend sink, aggregate-release engine service, gate и конечный drain service являются **fixtures**. Record mode и clock — подготовленные модели; random generator возвращает фиксированное значение, поэтому проверка общей probability/Note Repeat интеграции не заявляется. Отдельные новые сценарии исполняют настоящий общий Engine entry; большинство прежних сценариев использует явный fixture Engine scope вокруг настоящего Track вызова. Полный drain, три due loops, удаление Track и перенос due/duration исполняются из штатного ELF. В дополнительном MIDI-сценарии gate разрешает штатный Track Off, выполняется настоящий generic instrument Off, а sink вызывает настоящие native On/Off MIDI backend; конечная queue insertion и необязательный bookkeeping наблюдаются fixtures, без отправки на внешнее устройство. Проверяется отсутствие дублирующих Off. Исключения моделируются отдельными C++ fixtures вне ELF frames без зарегистрированных unwind tables, включая throw после возврата настоящего удаления Track. Это не запущенная полная программа, не физические пэды и не проверка аппаратной производительности. Логи и hashes — `build/synth-live-native/verification.json`, `build/synth-archive/verification.json`.

Регрессионные проверки отдельно: `verify_synth_queue.py`, `verify_piano_synth_input.py`, `verify_midi_event.py`. Syntax-only проверяется реальное включение phone/workflow/mixer/MIDI источников с обоими trial flags; она не заменяет full link/load.

## Следующий обязательный этап

### Совместная живая игра и воспроизведение: изолированный DSP-стенд

`check_piano_synth_coexist.inc` расширяет `verify_piano_synth_input.py`.
Через настоящую native ThreadQueue выполняется live Synth worker, затем
настоящий Synth DSP получает одновременно live gate и owned occurrence
той же высоты. Owned On здесь подан напрямую как модель уже звучащей ноты
секвенции; его native scheduling проверяет отдельный playback verifier.

Проверены оба порядка отпускания: live Off сохраняет owned voice, owned Off
сохраняет live gate. Восемь повторных worker handoffs одного пэда и stale Off
не снимают owned voice. Очистка клавиатурного ввода через worker Stop снимает
live gate, сохраняя owned playback; это **не** полный Stop транспорта, который
должен отменить обе группы через отдельный transport hook.

Восемь различных live высот одновременно занимают реальные восемь голосов
Synth. Перемешанный порядок отпусканий последовательно уменьшает и число
DSP voices, и worker held count до нуля. Fault counter не растёт. Это проверка
существующего лимита, а не обещание 16-голосной полифонии синтезатора.

Native Engine On и recorder Off в этом стенде остаются fixtures. Отсутствие
конфликта DSP voices не доказывает полный overdub, quantization, XPJ Save/Load,
физический Stop или совместный render на устройстве. Источники и результаты
normal/UBSan записывает `build/piano-synth-input/verification.json`.

### Проверка повторов без новой сборки приложения (2026-11-24)

`check_synth_repeat.inc`, включённый в `verify_synth_live_native.py`, исполняет
настоящие native expansion, pending-On delivery `0x2124c80` и due-Off loop.
Событие с тремя ударами начинается на tick 7640: On приходят на 7640, 7720,
7800, отдельные Off — на 7719, 7799, 7879. Проверены ровно три вызова backend,
освобождение каждой Synth occurrence, пустые обе очереди и неизменный fault
counter. Normal и UBSan прошли. Это воспроизведение ratchets сохранённого
события; recorder interception, clock и bookkeeping здесь fixtures. Удержание
физического Note Repeat и его запись этим тестом не подменяются.

На уже существующей VM-библиотеке `b20b44f7422d2a8fa4e6c0c5f791a8f6a709e4a35e33c759aea4d3bce1fc517d`
отдельно выполнен surface overdub обычного Sample Note On. Запуск PLAY,
затем SEQ RECORD сохранил все 7 прежних событий и добавил восьмое, длиной
500 ticks. Следующий take с удержанием NOTE REPEAT сохранил все 8 событий
и добавил 6 ударов на 6240…7440 с шагом 240 и длиной 180 ticks.
Сравнивается multiset полных событий XPJ, а не только их число.
Повторяемый helper — `lab/check_overdub_preservation_vm.py`; нужны непустая
частная VM-секвенция, явно выбранный режим пэдов и уникальная velocity.
В 16 Levels / Tune запись Note Repeat на соседней высоте сохранила все
23 прежних события и добавила 7 ударов, в том числе через loop. Take на
той же высоте и tick 0 заменил прежнюю длинную ноту новым коротким ударом;
его строгая проверка сохранности закономерно не прошла и сохранена отдельно.
Подробности и ограничения — в `NOTE_ON_BOUNDARY_INVESTIGATION_RU.md`.
Отчёты — `build/pad-midi-pads/overdub-followup-*-verification.json`.
Это исходный input/recorder прежней библиотеки; новая Synth-интеграция
полностью запущенным приложением здесь не проверена. Новая библиотека,
пакет для установки и сборка на устройство не создавались.

После разрешения на полную тестовую сборку: запустить isolated system QEMU, записать партию физическим pad protocol, воспроизвести при закрытом Synth UI, изменить ноты в существующем Piano Roll, сохранить native XPJ, выполнить New/Load и проверить preset/binding/ноты. Отдельно проверить UI и native Stop, Seek, конец Song, loop, probability, Note Repeat, simultaneous live input, sample tails, export/mixdown и аудиофазу. До этого сценарий A не объявляется завершённым и сборка на устройство не предлагается.

Horizontal Arrangement, музыкальные Tracks, параллельные clips, automation lanes и CHOP на курсоре остаются следующим этапом архитектуры; этот прототип их не включает.
