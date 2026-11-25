# Stage 4e — подготовка общего потока к native-назначению

Добавлен `arrangement_native_plan.h/.cc` и мост общего Store. Это проверенный
шаг подготовки к playback, а не уже звучащая аранжировка. Play/Record в Arrange
по-прежнему блокируются. Приложение/preload/пакет не собирались.

## Что выяснено в существующем коде

`batch_events.cc` и `pattern_native.cc` получают native Clip через
`2957ae0(sequence, native_track, 0)`. Последний аргумент — выбор default map,
не команда создания. Program берётся из native Track (`+0xbb8`); карта
pad→note находится у Program (`+0x12a8`), note→pad — `+0x14a8`.
Следовательно, музыкальная строка Arrange не становится native Track или
Mixer A–H автоматически. Несколько партий одного Program можно объединить
в один поток native Clip; разные Program требуют отдельного backend routing.

Существующий Synth/MIDI тракт уже оборачивает `Engine::event` (`0x2119d00`),
heap drain (`0x2119550`) и cancellation native Track (`0x211e610`), см.
`midi_native.c`, `synth_live.inc`, `synth_accept_native.*`. Их текущие scopes
заимствуют pointers только на время штатного вызова. Они не являются API
для безопасного удержания нового Arrange Track между callback.

`step_set_bars` поддерживает штатную команду 1…128 bars, но это не основание
считать длину любой native Sequence равной `128 * 4 * 960`. Размер может
отличаться. Preflight требует точного совпадения tick length назначения.
Получение этих данных из native объектов и владение ими ещё предстоит
подключить на подтверждённом owner.

## Подготовленный результат

`NativeDestination` — только значения: project epoch, routing revision,
destination revision, Program key, native tick length, снимок транспорта и
две карты пэдов. Destination revision должен меняться при замене native
назначения, даже если его Program key и длина совпадают. Это контракт будущего
owner, не уже подключённый publisher native-состояния.

`prepare_native_plan` принимает готовый `PreparedArrangement` всех слышимых
партий. Проверяет до allocation результата:

- Ненулевые версии, ограничения 32768 событий/1024 целей и границы времени.
- Совпадение проекта, известный остановленный транспорт без записи.
- Точную длину назначения и один соответствующий Program для первого backend.
- Используемые physical pad → current native note → тот же physical pad.
- Только inline tag 3, корректный payload, provenance и отсортированные onsets.

Сохранённый source note является происхождением записи, не вечной картой
инструментов. Если карту Program изменили, native note переназначается по
сохранённому physical pad. Tune/Chop, MIDI channel bits, velocity, mute,
probability и repeats остаются прежними. Шина микшера не используется как
правило назначения. Ни chance, ни Note Repeat здесь не исполняются повторно:
это задача штатного движка.

Длинные gates сохраняются за границами блока/песни. Нулевая native duration
также сохраняется: модель допускает instant/one-shot значения, и подмена их
на искусственную длительность изменила бы старые события. Отрицательные gates
и переполнение времени отклоняются. Это сохранение событий, не доказательство
корректного DSP playback конкретного сэмпла или seek внутри его аудио.

Результат содержит собственные события и музыкальное происхождение каждого
onset: track/clip/pattern/event/cycle, physical pad и записанный source note.
Failure/exception сохраняет прежний output. Нет нового clock, audio callback,
native объектов, команд изменения Sequence или записи в Project.

`NativeArrangementPlan::matches` — проверка перед допуском: версии источника,
все значения карты и остановленного назначения должны совпадать. Это не
проверка активного playback session и не lifetime lease native объектов.
Равные версии не защищают raw pointer от освобождения.

## Store

`pt_prepare_native_plan` использует существующий compiler и epoch guard,
готовит значения вне metadata mutex, затем повторно проверяет актуальность
Store перед выдачей. `pt_native_plan_current` проверяет общие версии под
metadata mutex и не обращается к native памяти. Изменение блоков, Library,
New/Load и новая карта назначения делают старую подготовку неактуальной.
Формат AMP1/AAR1 не менялся. Модуль включён только в прежний opt-in trial.

## Проверки

`verify_arrangement_native_plan.py`: host, ASan/UBSan, ARM QEMU и ARM UBSan.
Проверены параллельные 2/4/8 тактов, актуальная карта пэдов, payload/длинные
gates/нулевой gate, смешанные Program, точная длина, playing/recording, stale
epochs/revisions/maps, ошибочные tag/target/order, дубли, лимит и две точки
allocation failure с сохранением всего output.

Packed getter в этом отдельном наборе — fixture seam, без выдуманного native
кодека. Отдельный `check_arrangement_native_choices.cc` вызывается ARM UI
suite: настоящие native factory/getter/setter создают Tune=-12, Chop=7,
chance=61 и repeat=4; новый plan меняет native note 36→48, сохраняя остальной
payload. Program/Track/audio для этого теста packed choices не создаются.

В `verify_synth_live_native.py` добавлен отдельный проход: compiler объединяет
три партии 2/4/8 тактов в семь событий, план направляет их в один fixture
native Track. События проходят настоящие `Engine::event`, stock Track/filters,
due heap и Am MOD Synth DSP. Проверены три одновременных On, ненулевой конечный
PCM, перекрывающиеся ноты одного пэда, хвост за концом аранжировки и native
Track cancellation. После cancellation release envelope дренируется отдельно;
снятие нот не означает мгновенное исчезновение естественного DSP-хвоста.

Этот тест выявил контракт низкоуровневого `0x21185a0`: ему требуется точная
граница ближайшего heap event. Вызов с более поздним UI/seek tick может
зациклиться на `0x21185f4…0x21185fc`. Fixture теперь обрабатывает существующие
границы native heap по порядку с ограничением числа шагов и watchdog.
Production adapter обязан использовать штатный scheduler, а не подставлять
UI tick в low-level drain. Это не исправление движка: исправлен тестовый
драйвер, нарушавший контракт; production новых вызовов drain не получил.

Project/Program metadata, музыкальное время и доставка On в этом сценарии —
fixtures. Подтверждена обработка скомпилированных событий настоящим Engine/DSP,
но не frame-accurate clock из Arrange. Тест не воспроизводит sample audio и
не открывает Play на экране. Один native Track здесь принадлежит сценарию;
его полная отмена не доказывает безопасный Stop рядом с чужими активными
голосами. Музыкальные строки по-прежнему не являются native Tracks.

`verify_pattern_tools.py` проверяет Store bridge, отказ running destination,
stale после cut/join/reset и прежние Piano Roll/архивные сценарии.
Дополнительно plan теперь получает runtime `store_receipt`: любое изменение
Store, включая Undo/Redo с возвратом к тому же объекту и revisions, требует
новой подготовки. Receipt проверяется и после компиляции перед публикацией,
и в `pt_native_plan_current`. Это защита устаревшего запроса Start, не
удержание Project/Program; XPJ и библиотечный формат не меняются.
`verify_arrangement_ui.py` сохраняет регрессию Arrange и Sequential/Export.
Proof/logs/hashes: `build/arrangement-native-plan/verification.json`,
`build/pattern-tools/verification.json`, `build/arrangement-ui/verification.json`.
Native Engine/DSP proof: `build/synth-live-native/verification.json`.

## Следующая граница подключения

### Проверка штатного входа секвенсора

Сценарий 2/4/8 тактов теперь подаёт семь событий через настоящий
`0x211a230(engine, time, frame_offset, track, event)`. Этот вход дополнительно
проходит native политику Track и далее вызывает общий `0x2119d00` с source=2,
mask=`0x7ffffffe`. Проверены прежние три одновременные ноты, Synth PCM,
перекрытия, хвосты и отмена, в том числе с установленными fast-render и phone
gate. В тесте разрешающий preview receiver отделён от сценария, в котором
этот же receiver специально запрещает событие.

Position conversion `0x2142f00`, record-state query `0x2133800` и notification
`0x21560f0` в этом проходе заменяются fixture-службами; после прохода исходные
16 байт каждого entry восстанавливаются в частной ELF image. Исходный файл
MPC Sample не изменяется. Подача On по времени всё ещё fixture, Play в Arrange
не подключён. Это следующий подтверждённый участок штатного тракта, а не
проверка реального транспорта или удержания объектов.

При исследовании также уточнены точки подключения: `0x2114c00` заполняет
position snapshot и сам не является драйвером событий. Путь
`0x2125960 → 0x2124ad0 → 0x211bf70` обслуживает очереди отложенного ввода и
может обращаться к recorder; подменять его готовыми нотами Arrange без
доказанного контракта нельзя. Ни один из этих новых адресов не подключён
production-хуком этого этапа.

Нужен native sink на штатном executor с реальным удержанием назначения,
а не только счётчиком epoch. Он должен создавать собственный playback
материал, проверять currency перед публикацией, отменять частичную подготовку
и не перезаписывать пользовательскую Sequence. Production admission прежнего
`SourceDomain` остаётся закрытым; текущий plan не открывает его в обход
непроверенных writers.

До включения Play проверить реальный контракт native scheduling: tick/frame
offset, порядок одинаковых onsets в native tree, исчерпание очереди, Start/Stop,
очистку heap/голосов и удерживаемые ноты при New/Load/Program change. Первая
публикация — на остановленном транспорте. Live edits и mute/solo во время
playback потребуют отдельного протокола voice ownership. Изолированные тесты
этого этапа не подтверждают полный sampler/FX тракт, синхронность audio callback,
export/mixdown или физическое устройство.
