# Stage 3a — native Sequence → immutable Snapshot

Реализован `src/pattern_native.h/.cc`, пока отдельно от приложения.
Адаптер читает одну выбранную Sequence для **текущего native Track** из
`CurrentThings+0x368`, как существующие batch/MIDI export пути. Это не обход
всех TrackClipMaps и не реализация Horizontal playback. Сборка приложения,
preload или установочного пакета не выполнялась.

## Подтверждённые контракты

Исходные точки — `batch_events.cc`, `midi_file_native.c` и
`piano_roll_native.inc`. Для pinned штатного ELF SHA-256
`b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508`:

| Данные | Используемый путь |
|---|---|
| Project | vptr `image+0x6ecc648`, sequences array `+0x1250`, count `+0x125c` |
| Sequence | настоящий getter `0x2909be0(project,index)`, vptr `image+0x6ecdff0` |
| Длина | настоящий `0x29571f0(sequence)`, читает ticks `+0x348` |
| Program | `things+0xb8` должен совпасть с `native Track+0xbb8` |
| Program key | native string pointer `Program+0xdc8`, bounded copy до 127 bytes |
| Pad ↔ source note | `Program+0x12a8` и `+0x14a8`, проверяются взаимно |
| Clip event list | настоящий `0x2957ae0(sequence,track,0)`: отсутствие clip не создаёт новый |
| Inline событие | node `+0x20`, только tag 3, существующий `am_midi_event_copy` |
| Tune/Chop | настоящий choice getter `0x27e4230`, без повторного encode |

Список событий использует тот же libstdc++ RB-tree ABI, что batch/Piano Roll:
header `list+0x68`, count `+0x88`, parent/left/right node offsets 8/16/24.
Вместо неограниченного native successor на повреждённом дереве используется
bounded in-order обход с проверкой parent reciprocity, разных children,
first/last, count и глубины. ABI подтверждён static assertions и реальными
native trees; порядок дополнительно сравнивается с настоящим successor
в тестовом fixture. Лимит — 32768 событий, stack обхода — 128 pointers.
Это не полная проверка RB colours или доступности произвольной памяти.

## Срок жизни и неизменность

`read_native` требует `NativeReadAccess` с enter/valid/leave. Enter должен
на настоящем владельце захватить lifetime lease, запретить mutation/destruction
на время операции и только затем вернуть Project/Things, generation и снимок
маршрутизации. Read-safe flag сам по себе этого не обеспечивает. Нельзя вызывать
адаптер из UI с захваченными ранее native pointers, из render callback или
из постороннего потока без подтверждённого executor contract.

**Production реализация этого lease ещё не подключена.** В тесте он явно
смоделирован владельцем fixture-объектов. Vptr checks и два чтения не заменяют
реальное владение, блокировку или сериализацию на executor: concurrent free
может привести к fault до очередной проверки. Catch C++ exceptions не является
защитой от SIGSEGV. Enter отвечает за уборку частичного захвата при false/throw;
после true адаптер вызывает leave ровно один раз при любом результате.
Leave обязан не бросать исключение.

Под lease выполняются два чтения с прямым сравнением значений и native
identities. После успешной проверки второй buffer освобождается до создания
Snapshot. До публикации снова проверяются generation/routing epochs.
Ни один native pointer/refcounted owner не попадает в результат. Выполняются
только getters и копирование; native commands/history и writer не вызываются.

Маршрутизация передаётся **копией** с её владельца, индексированной physical
pad. Используется существующее mixer encoding: 0 Master Direct, 1…8 A…H,
−1 Unknown. В модель оно преобразуется в −1, 0…7 и −2 соответственно.
Будущий production enter/valid обязан сверить соответствие этой копии
тому же Program и routing revision. Адаптер не читает UI `desired.output`
с чужого потока. Unknown допустим для Split by Pads, но Split by Mixer
не выполняется без известных маршрутов.

## Ревизия и идентичность

Подтверждённого подходящего native revision counter в этих путях нет.
Как в Piano Roll, вычисляется 64-bit content fingerprint: source key,
project generation, routing revision, длина, Program key, обе pad mapping
таблицы, outputs и музыкальные поля событий. Selection bit и ABI padding
не считаются музыкальной ревизией. Это не монотонный history counter и не
криптографическая защита. Согласованность двух чтений проверяется прямым
сравнением, а не только совпадением hash.

`snapshot_id` и устойчивый `sequence_key` передаёт проектный слой.
Source key должен различать Sequence **и выбранный Track scope**; native
sequence index служит только lookup hint. Реестр этих identities и проверка
соответствия key/index при reorder/load ещё относятся к будущему owner adapter.
Перед будущим edit/commit понадобится повторная проверка источника через тот
же command boundary. Сейчас адаптер ничего не записывает обратно.

## Проверки и пределы доказательства

`verify_pattern_native.py --stock ...` исполняет отдельный ARM executable в
QEMU user mode, normal и UBSan. Используются настоящие native sequence/list/
length getters, list constructor/event insert и packed Tune/Chop decoder.
Project/Sequence/Track/Clip storage и scope ownership — fixtures; native
Clip shared_ptr control blocks в fixture null. Два конечных event notification
service заменены no-op, приложение и GUI не запускаются.

Проверены:

- две одинаковые одновременно записанные ноты с разными occurrence IDs;
- Tune +7, Chop 3, channel 7, mute, probability 73 и три ratchets;
- pad 0 → source note 36 и pad 16 → source note 52, Master Direct/шина C;
- сохранность исходных native событий и Extract с gate 10000 ticks;
- стабильная музыкальная ревизия при смене selection, изменение при смене
  routing metadata или project generation;
- пустой существующий Clip и отсутствие Clip без его создания;
- неверный Project/Sequence, несогласованный Program, сломанная обратная
  pad mapping, неподдержанный variant, несогласованный count/first/parent,
  child cycle, event limit и overflow tick+duration;
- смена данных между сканами, отказ owner проверки в конце capture,
  исключение owner callback, unavailable/unsafe scope;
- 27 отдельных allocation failure points, включая Snapshot construction:
  предыдущий результат сохраняется, lease всегда освобождается.

Proof с hashes: `build/pattern-native/verification.json`. Эти проверки не
доказывают production thread/lifetime contract, native project Save/Load,
пользовательский Split UI или устройство. Форматы XPJ/sidecar, Sequential
Song Mode, Piano Roll и штатное воспроизведение этим кодом не изменены.

Executor request с безопасной публикацией immutable результата реализован
в `pattern_request.h/.cc`; протокол native context lifetime/cancel проверен
на настоящей очереди и destructor в QEMU. Подробности —
`PATTERN_REQUEST_LIFETIME_RU.md`. Concrete Project/Program/event-list lease
реализован как SourceDomain с отдельными lifetime/currency проверками;
см. `PATTERN_SOURCE_LIFETIME_RU.md`. Полный production writer coverage ещё
предстоит подтвердить. До этого модули не включены в `phone_build.py` и не вызываются
из production UI. Затем — общие history commands и event-source adapter
существующего Piano Roll.
