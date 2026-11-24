# Stage 3a — изолированное ядро Virtual Parts / Extract Pattern

Актуальное продолжение: `PATTERN_TOOLS_STAGE3_RU.md` — модель подключена
к opt-in библиотеке Parts / Patterns, существующему Piano Roll и XPJ hooks.
Ниже сохранён контракт исходного ядра Stage 3a и его отдельных проверок.

Статус исходного ядра: реализация модели и проверки готовы.
Сборка preload/установочного пакета не выполнялась. Это продолжение
`SEQUENCER_MIDI_ARRANGEMENT_DESIGN_RU.md`, а не новый sequencer/playback.

## Что реализовано

`src/pattern_model.h/.cc` использует существующий inline `step_event` tag 3
и его валидатор `am_midi_event_copy`. Сохраняются native modifiers, velocity,
duration, probability, repeats, mute и channel bits. UI selection очищается,
ABI padding нормализуется. Tag 4 и неизвестные variants отклоняются целиком:
их refcounted native pointers нельзя переносить как самостоятельные данные.
Canonical MIDI Note / InstrumentBinding ещё не введены этим модулем.

Snapshot владеет одним неизменяемым массивом событий и таблицей маршрутов.
Маршрут содержит Program key, pad 0…127 (банк определяется pad/16), native
source note и снимок bus A–H/Master Direct/Unknown. Native source note не
принимается за номер физического pad: будущий adapter обязан передать
подтверждённую взаимную Program mapping, как в существующих batch/input путях.
Неоднозначные дубли Program+pad или Program+source note отклоняются.

Идентичность события — пара Snapshot ID + ordinal 1…N. Два одинаковых
одновременных события получают разные ordinals. Snapshot ID и source key
передаёт вызывающий проектный слой; реестр устойчивых project identities
пока не реализован. Нельзя подставлять туда native tree pointer или изменяемый
индекс Sequence. `matches(key,revision)` позволяет будущему adapter отвергать
устаревшее представление; модуль сам не наблюдает native history.

Split by Pads группирует Program+pad. Split by Mixer группирует по сохранённой
шине, включая Master Direct, и отказывается при Unknown. Он не создаёт связи
«музыкальная партия = mixer bus»: каждая Part содержит явные Event IDs.
Manual membership может разделить события одного пэда на разные партии.
В пределах одного Split каждое событие должно принадлежать ровно одной Part;
невыделенный материал нужно явно поместить в Unassigned. Нельзя незаметно
потерять событие или запустить его дважды из двух партий.

Extract материализует самостоятельный Pattern и только используемые routes,
вычитает начало диапазона из onset и сохраняет provenance. Границы [begin,end)
полуоткрытые. Выбор `BoundaryPolicy::onsets_only` обязателен: начала внутри
диапазона включаются, длительность за правой границей не обрезается. Число
таких gates и gates, начавшихся раньше диапазона, возвращается отдельно.
Предыдущие gates не запускаются повторно. Длину звучания one-shot sample
из длины MIDI gate вывести нельзя; фактические sample tails/seek здесь не
планируются и не имитируются. Пустой результат Extract допустим.

Create Variation копирует самостоятельные данные, получает новый Pattern ID
и parent ID. Изменение вариации не меняет Pattern или исходную секвенцию.
`replace` проверяет expected revision, корректность нового tag-3 события,
onset в пределах Pattern и соответствие source note его route; при успехе
повышает revision. Это командный примитив, а не отдельный Undo stack.
Пока есть создание пустого Pattern с произвольной положительной длиной
(например, 2880 ticks для одного такта 3/4). Insert/Delete/Copy/Paste в пустой
Pattern, клипы и CHOP на курсоре ещё не подключены.

## Ограничения исполнения

Все операции предназначены только для control/music command boundary,
не render callback. Используются стандартные vector/shared_ptr/map; native
owner pointers и ресурсы не сохраняются. Parts разделяют один immutable
Snapshot; самостоятельные Pattern/Variation владеют копиями. На старте
ограничены 32768 source events, 1024 routes и 128 Parts. Это предварительные
бюджеты модели, не измерение допустимой аппаратной нагрузки MPC.

Ошибки валидации возвращают Result, сохраняя прежний результат целиком.
Нехватка памяти может бросить исключение: до публикации результат готовится
локально; будущий command boundary должен перехватить исключение. Принудительная
инъекция allocation failure пока не проверена. Сериализации C++/ABI structs
через fwrite нет; формат XPJ/sidecar этим этапом не меняется.

## Проверки

`verify_pattern_model.py`: host, host ASan/UBSan, ARM QEMU и ARM UBSan.
Проверяются сценарий смешанной секвенции Drums/Bass/Sample, обе автоматические
группировки, ручное разделение одинаковых событий одного pad, неизменность
входных данных, tail длительностью 10000 ticks при Pattern 3840 ticks,
независимая variation, stale edit, exclusive end, неизвестная шина, неверные
membership/route/source note, неподдержанный variant и overflow tick+duration.
Проверены предел 32768 событий, отказ при 32769, лимиты routes/parts и
сохранность предыдущего успешного результата после ошибки.

Тестовые native события — value fixtures; Tune getter — явно обозначенный
fixture. Это не чтение native Sequence tree. Proof/logs/source hashes:
`build/pattern-model/verification.json`. Ни устройство, ни UI, native
snapshot adapter, Save/Load, Undo/Redo или проигрывание этим тестом не доказаны.

## Следующие шаги

1. Изолированный snapshot adapter `pattern_native.h/.cc` реализован и
   проверен на штатных getter/tree/choice функциях в ARM QEMU, normal/UBSan.
   Контракт и ограничения — `PATTERN_NATIVE_ADAPTER_RU.md`. Следующая часть:
   production охват Project/Program/event-list mutators. Concrete SourceDomain
   lease уже реализован: change fences, generation, New/Read/Load/Program scopes,
   stale completed-result rejection; см. `PATTERN_SOURCE_LIFETIME_RU.md`.
   Executor request с native context counter/cancel также готов. Полный runtime
   охват всех writers и их thread/lock order пока не доказан; UI admission выключен.
2. Команды Insert/Delete/Copy/Paste с одним общим history boundary, совместимым
   с существующим редактором; затем adapter источника событий Piano Roll.
3. Versioned codec и транзакционный archive до пользовательского сохранения.
4. Arrangement Clip model: source window, move/repeat и CHOP без дублирования
   Pattern. Native playback и экран подключать после проверок этих контрактов.

Полная проверка нового Pad-to-MIDI/Synth приложения остаётся отдельной
незавершённой частью Stage 2. Изолированная модель Stage 3a не объявляет её
законченной и не заменяет будущие native XPJ/VM/аппаратные проверки.
