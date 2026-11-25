# Stage 4f — собственный native Events для Arrange

Ветка `codex/pad-midi-recording`. Реализована подготовка источника событий,
но Play/Record на экране Arrange пока не подключены. Полная сборка и запись
на карту этим этапом не выполняются.

## Владение источником

`src/arrangement_native_material.h/.cc` создают самостоятельный штатный Events:
constructor `0x27eb120`, insertion `0x27ee5d0`, non-deleting destructor
`0x27edba0`. Размер `0x148` подтверждён deleting destructor `0x27ee0d0`,
передающим этот размер в sized delete. Проверяются entry fingerprints и
vtable `0x6dbbcc0`. Внешний C++ owner содержит весь объект и освобождает его
внутренние деревья штатным деструктором; пользовательский Clip/Sequence
не изменяется, фиктивный Clip вокруг частично заполненных полей не создаётся.

Owner хранит собственный `NativeArrangementPlan` и numeric origin index.
Project, Track, Program, CurrentThings и нативный контекст UI не удерживаются
и не разыменовываются. Можно освободить Library/PreparedArrangement/исходный
plan после подготовки. Consumer обязан удерживать owner, пока native reader
или iterator использует Events. Создание и последнее освобождение выполняются
на control/native owner, не на audio thread. Shared ownership этого источника
не даёт lease на назначение и не разрешает Start.

Перед native allocation повторно проверяются inline tag-3, физический пэд,
взаимное назначение pad/note, длина, порядок onset, границы и overflow gate,
метаданные и остановленный transport snapshot. Неподдерживаемые варианты,
unassigned instrument и превышение 32768 событий отвергаются. Частичная
подготовка уничтожается штатным кодом; прежний output остаётся доступным.

## Порядок и происхождение событий

Дерево использует штатный comparator, включая события с одинаковым onset.
Визуальный порядок строк не является другим правилом сортировки native notes.
Точные дубликаты остаются отдельными событиями. Tune/Chop, velocity, channel,
mute, probability, repeats и длительности, включая хвосты за концом блока,
сохраняются. Chance и Note Repeat заново не вычисляются этим контейнером.

Каждый полученный от native insertion узел связан со своим индексом plan.
`origin(event_address)` ищет эту связь без allocation и без разыменования
входного указателя; возвращает musical track, clip, pattern, event и cycle
из удерживаемых значений. Одинаковые ноты одного пэда на разных строках
имеют разные origin. Это основа будущего адресного управления голосами;
live mute/solo и их note-off этим этапом ещё не подключены.

При проверке выявлено штатное поведение копирования tag-3: оно пишет time,
type и 37 байт payload, оставляя ABI padding `12..15` и `53..55`
неопределённым. Сравнение музыкального содержимого учитывает все эти
значимые поля, не требует равенства padding. Хранение XPJ этим не меняется.

## Подготовка из Store

`pt_prepare_native_material` сначала получает проверенный plan с runtime
receipt, создаёт собственный Events вне mutex Store, затем снова проверяет
receipt/epoch/revisions перед выдачей результата. Изменение, Undo/Redo или
Reset во время подготовки отвергают публикацию. Старый output сохраняется.

Обмен shared owners выполняется под mutex, но уничтожение прежнего Events
отложено до выхода из mutex. Тестовый native allocator вызывает Store edit
или Reset непосредственно между native allocations; native delete повторно
читает Store и подтверждает отсутствие удерживаемого mutex при очистке.
Это проверка данной границы, не доказательство всех native lifetime/lock
contracts. Program/Track destination по-прежнему требует отдельного lease.

## Подтверждение и ограничения

`verify_arrangement_native_material.py --stock ...` выполняет обычный ARM/QEMU
и UBSan проходы настоящих ctor/insert/comparator/iterator/tail query/dtor.
Проверены равные ноты с разными origin, пустой контейнер, 32768 событий,
отказ на 32769, удержание после отпускания producer, ошибки ABI/данных и
19 allocation-failure points с сохранением output и освобождением native
nodes. Для реальных исключений в частной ELF image регистрируется её
`.eh_frame`; production registration и патчи не добавляются.

Пик учитываемых native allocations в limit-сценарии — 2 884 288 байт.
Это измерение тестовых native allocations, включая удерживаемый маленький
контейнер и временные узлы; оно не включает C++ plan/origin index, накладные
расходы allocator и остальное приложение и не заменяет замер на MPC.
Дополнительно компилируются два затронутых C++ модуля с рабочими O2/PIC flags.

`verify_synth_live_native.py` теперь проверяет цепочку compiler → owned
Events → штатный iterator → sequence input `0x211a230` → Engine → Synth.
До доставки On исходные Library/PreparedArrangement/plan уничтожаются.
Семь событий 2/4/8 тактов сохраняют семь различных origin и дают три
одновременных ноты, ненулевой конечный PCM и корректные хвосты/отмену.
Проверка повторена с phone gate и fast-render gate, включая UBSan.

Clock/delivery, Track/Program metadata и часть native служб остаются fixtures.
Реальный frame transport, lease назначения, Stop без отмены чужих голосов,
полный sample/FX тракт, GUI-запуск, New/Load полного приложения и устройство
ещё требуют проверки. Общий native source reader сохраняет закрытое
production admission. Дальше нужен session owner с native binding и
публикацией источника в штатный scheduler; отдельный UI-таймер не вводится.
