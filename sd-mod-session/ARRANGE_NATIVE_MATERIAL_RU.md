# Stage 4f — собственный native Events для Arrange

Ветка `codex/pad-midi-recording`. Реализована подготовка источника событий,
но Play/Record на экране Arrange пока не подключены. Полная сборка и запись
на карту этим этапом не выполняются.

## Владение источником

`src/arrangement_native_material.h/.cc` создают самостоятельный штатный Events:
первоначально исследован `UnsafeSequenceEventList` (`0x148`, ctor `0x27eb120`,
destructor `0x27edba0`). Для штатного audio cache источник расширен настоящим
`SafeLifetime::Holder<UnsafeSequenceEventList>`: constructor `0x170a2f0`,
non-deleting destructor `0x14c4a20`, vtable `0x6d8c0e8`. Размер Holder `0x160`
подтверждён его deleting destructor `0x14c4ba0`, передающим этот размер в
sized delete. Insertion остаётся `0x27ee5d0`, основной event-tree ABI не
меняется. Проверяются fingerprints обоих слоёв. Внешний C++ owner содержит
весь объект и освобождает его
внутренние деревья штатным деструктором; пользовательский Clip/Sequence
не изменяется, фиктивный Clip вокруг частично заполненных полей не создаётся.

Owner хранит собственный `NativeArrangementPlan` и numeric origin index.
Project, Track, Program, CurrentThings и нативный контекст UI не удерживаются
и не разыменовываются. Можно освободить Library/PreparedArrangement/исходный
plan после подготовки. Consumer обязан удерживать owner, пока native reader
или iterator использует Events. Создание и последнее освобождение выполняются
на control/native owner, не на audio thread. Shared ownership этого источника
не даёт lease на назначение и не разрешает Start.

RTTI штатного кода подтверждает типы
`UnsafeAudioThreadCache<SafeLifetime::Holder<UnsafeSequenceEventList>, ...>`
и `SequenceEventListAudioThreadCache` (vtable `0x6d906c8`). Это точка
дальнейшего подключения: cache/source switching и его очереди пока
исследуются, production cache этим этапом не создаётся.

В QEMU реальный native helper `0x13b35c0` удерживает lifetime control Holder,
C++ owner целиком передаётся потоку уничтожения, а настоящий destructor
останавливается на штатном fence. Пока reader lease удерживается, native
tail query читает исходное дерево; release `0x123bca0` разрешает завершение
уничтожения. В этом тесте не подменяются destructor, fence, release, syscall
или дерево. Это защита native-источника; origin/plan API всё ещё требует
C++ owner, а назначение Track/Program — отдельной гарантии. Последнее
освобождение Holder способно ждать читателей и запрещено на audio thread.

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
20 allocation-failure points с сохранением output и освобождением native
nodes. Для реальных исключений в частной ELF image регистрируется её
`.eh_frame`; production registration и патчи не добавляются.

Пик учитываемых native allocations в limit-сценарии — 2 884 384 байт.
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

## Копия аудиокэша и происхождение нот

Следующая проверка выполняется через `verify_arrangement_native_material.py
--cache --stock ...`. Штатный `0x170b060` создаёт отдельный Holder с копией
событий; отложенный callback `0x17088c0` обменивает его дерево с деревом
аудиокэша. Узлы копии отличаются от узлов исходного Material, но сохраняют
адреса при этом обмене. Поэтому исходный `Material::origin` нельзя применять
к событиям кэша.

Добавлен `NativePlaybackOriginIndex`: на управляющем владельце сопоставляет
два удерживаемых неизменяемых дерева в штатном порядке обхода. Проверяет
количество и все музыкальные поля, сохраняет отдельные origin одинаковых нот
и владеет копией метаданных. Исходный Material после этого можно освободить.
Ошибка данных или allocation оставляет прежний индекс доступным. Lookup
не выделяет память и не разыменовывает переданный адрес события. Это индекс
одного поколения playback; он не удерживает дерево и не заменяет lease.
При снятии источника индекс нельзя переключать раньше подтверждения audio swap.

В QEMU normal/UBSan проверяются реальные copy/swap/iterator/destructor,
равные ноты разных музыкальных строк, длинные gates, ошибки native allocations
при копировании и C++ allocations при построении индекса. До выполнения
отложенной задачи producer и исходный plan освобождены. В аудиофазе запрещены
оба вида allocations и native delete. Прежнее дерево уничтожается после
callback на управляющей стороне. Проверены пустая публикация и поздний callback
после истечения защиты назначения; последний не очищает непустое дерево.

Очередь исполнения, оболочка cache и привязка её lifetime guard в этом
сценарии — fixtures. Полный native constructor кэша, настоящая ThreadQueues,
Track/Program lease, транспорт и GUI Play этим тестом не подтверждены.
Production reader admission и Play/Record Arrange остаются закрытыми.

## Штатная очередь: bounded publication

`arrangement_native_queue.h/.cc` добавляет публикацию через реальный
`0x2b59300`, используя уже исследованный в `pattern_request.cc` протокол.
Под штатным recursive producer mutex выполняются reclaim `0x2b58c70` и
проверка свободного слота. Только после неё вызывается stock cache copy.
Удержание producer mutex исключает заполнение другим producer; consumer
может лишь освобождать слоты. Путь штатного post, способный ждать или
выполнить задачу inline при переполнении, не допускается.

При публикации сравниваются полное назначение и состояние транспорта.
`clear_native_cache` принимает сохранённую идентичность назначения отдельно:
поздний Stop не должен очищать заново разрешённый cache нового проекта.
Изменение Playing/Recording допускается для очистки того же назначения.
Проверяется и истёкший native weak self. Результат `ok` означает постановку,
а не применение источника: Start требует отдельного порядка/подтверждения.

`verify_arrangement_native_material.py --queue --stock ...` выполняет
неподменённые post, runner `0x2b59200/0x2b59160`, reclaim и recursive mutex.
Consumer запускается отдельным потоком. Проверяются копии и origin после
освобождения producer, хвосты, allocation failures, переполнение с включённым
штатным inline policy без выполнения callback в producer, wrap-around,
Stopped, устаревшие назначения и guard, отменённый до consumer.
Внутри runner запрещены allocations и native delete; retired tree освобождает
reclaim на управляющем владельце.

Storage кольца, оболочка cache, provider `NativeCacheOwner` и метаданные
назначения пока fixtures. Provider обязан реально удерживать native объекты,
сериализовать смену назначения/teardown и обновлять destination revision при
замене cache/Track/Program. В production он не зарегистрирован. Замена такого
lease только проверкой epoch не допускается. Полный constructor cache,
привязка к driver transport и защита Track/Program ещё не подтверждены.

При исследовании Track найдено уведомление удаления и отдельные async
счётчики ожидания. Они не объявлены общим lease Track. Штатный
`TrackingPointer<Track/Program>` обнуляет pointer при удалении, но сам по себе
не удерживает объект между чтением pointer и использованием. Эти механизмы
нельзя использовать как доказательство готовности GUI Play.

## Подтверждение применения и подготовленный пакет

Перегрузки `publish_native_cache` / `clear_native_cache` с
`NativeQueueReceipt` дают состояния Empty, Pending, Running, Applied,
Cancelled. Applied отмечает выполненный stock swap, а не прохождение
дополнительного barrier в очереди. Пакет заранее создаёт отдельный штатный
Holder в native heap, наполняет его штатным insert и строит origin index
до публикации closure. Поэтому consumer может работать сразу: создавать
индекс чтением уже опубликованного кольца больше не требуется.

Closure выполняет настоящий `0x17088c0`. Перед вызовом она удерживает native
strong pin кэша, чтобы внутренний weak-lock штатного swap не мог истечь
между проверкой и исполнением. Job удерживает weak control до reclaim;
последнее освобождение control block, дерева и C++ state не происходит
в consumer. После swap native Holder пакета содержит прежнее дерево кэша,
которое удаляет управляющий reclaim. Индекс продолжает соответствовать
перемещённым узлам и владеет метаданными независимо от исходного Material.

Подтверждённый путь использует один слот. Полное кольцо, allocation failure
и ошибка назначения сохраняют прежний receipt и источник. Нельзя заменить
тот же receipt, пока он Pending/Running. Drop UI receipt безопасен, поскольку
job остаётся в native queue. Native purge без выполнения callback переводит
Pending в Cancelled при reclaim, а не выдаёт ложный Applied.

Cancel не обещает откат: до Running он исключает swap, но во время/после
swap может лишь запретить будущий Start. Тогда нужна отдельная очистка.
Applied — историческое подтверждение; перед Start всё равно обязательны
актуальное поколение сессии/Store и настоящий lease Project/Track/Program.
Для одного native guard идентичность назначения неизменна: provider обязан
retire старый cache при смене назначения, а не переназначать живой cache.

В `--queue` normal/UBSan проверены 20 allocation-failure points подготовки,
удаление receipt до callback, cancel до исполнения и во время настоящего
swap, native slot purge, полное кольцо и повторное использование адреса
wrapper с новым guard до исполнения старого callback. Test-only change
receiver останавливает swap после изменения дерева, до возврата, и проверяет
Running; native swap, queue, runner, fences и reclaim не подменяются.
Кэш/его provider и метаданные назначения по-прежнему fixtures; driver transport,
настоящий native constructor кэша и lifetime Track/Program не подключены.
Play/Record Arrange остаются закрытыми; сборка приложения и SD не выполнялись.

Для следующего исследования binding зафиксирован native getter: `2957ae0`
получает пару object/control через `29579e0`, возвращает `object + 0x688`
и отпускает control. Вариант `2957b60` передаёт пару через hidden return x8.
Сохранение только raw Events поэтому не удерживает исходный map object.
Тип и покрытие strong handle ещё нужно подтвердить; это не доказательство
удержания Track или Program.

## Уточнение реального пути проигрывателя

Дальнейшее чтение native-кода выявило различие двух кэшей. Проверенные ранее
`170b060 / 17088c0` принадлежат `SequenceEventListAudioThreadCache`.
Один из его владельцев — `StepSequencer::NormalMode`; другой —
`ArpeggiatorMidiFilter`. Это не подтверждение подключения списка нот к
проигрывателю секвенций. Существующий адаптер оставлен с проверкой своего
типа и не принимает `ClipAudioThreadCache`.

В штатном выборе секвенции `Sequencer` вызывает `20fb5d0` на `ClipPlayers`.
Тот вызывает `20e8040` для отдельных `ClipPlayer`. Эта функция получает
кэш выбранной партии через `296e9c0(sequence_cache, track, selector)` и
передаёт `cache + 0x130` в `SequenceEventListIterator` через `2158f00`.
Дополнительно она сохраняет ссылки на метаданные `cache + 0xf8` и
`cache + 0x2e0`. Следовательно, одного дерева нот недостаточно:
владелец метаданных клипа должен жить вместе с ним.

RTTI подтверждает `SequenceAudioThreadCache` (`6dbe058`, constructor
`2971180`) и `ClipAudioThreadCache` (`6dbbad8`). Для последнего проверен
настоящий constructor `2974cf0(this, queues, source, track, selector)`
и non-deleting destructor `27c2ac0`; размер `0x3d8` подтверждает deleting
destructor `27c3210`. Это отдельный тип от ранее проверенной оболочки.

`verify_arrangement_native_material.py --clip --stock ...` выполняет:

- Полные native constructor/destructor `ClipAudioThreadCache` с null
  source/Track. Оболочка, weak guard и вложенные объекты не подделываются.
- Constructor отдельного native `Holder<UnsafeClipCache>` размером `0x298`
  через `2973b50`. Ноты вставляются в его собственный Events по смещению
  `0x98`, пока источник никому не опубликован.
- Настоящие native post/runner/reclaim и штатный manager `27bdeb0` со
  swap `27c7ff0`. Consumer работает отдельным потоком; любые C++/native
  allocations и native delete в нём запрещены.
- Семь нот, включая четыре одинаковых события: адреса узлов, музыкальные
  байты и хвост `63040` сохраняются после swap. Очистка переносит старое
  дерево в retired source; освобождение выполняется при control reclaim.
  Индекс принадлежности сохраняет разные track/clip/event для одинаковых
  нот после удаления producer material, swap и reclaim старого source.
- Удаление cache до выполнения callback с последующим заполнением его
  storage значением `0xa5`: истёкший настоящий guard исключает обращение к
  удалённой оболочке; manager освобождает source при reclaim.
- Десять allocation-failure points полного constructor: stock landing pads
  освобождают частично созданные объекты, утечек не остаётся.

Важное различие ABI: native weak-self `ClipAudioThreadCache + 0x28`
указывает на guard info `cache + 0x338`, а не на начало wrapper. Это
проверено constructor, а не предполагается по макету event-list cache.

Ring storage и ThreadQueues storage остаются fixtures. Настоящих
Project/Track/Program, native ClipPlayer instance и transport в этом
изолированном тесте нет. Null-source метаданные не объявляются пригодными
для музыкального воспроизведения произвольной аранжировки. Нужен отдельный
адаптер именно clip cache, сохранение его метаданных и привязка к реальному
владельцу проигрывателя с защитой объектов при Stop/New/Load. GUI Play/Record
не включены, приложение и пакет для карты не собирались.

Для воспроизводимого поиска адресов добавлен read-only инструмент
`lab/inspect_arrange_native.py`. Он проверяет SHA pinned ELF, переводит VA
через PT_LOAD и показывает RTTI/vtable, прямые BL и кандидаты ADRP+ADD/LDR.
Это инструмент исследования: прямой вызов или RTTI сами по себе не
доказывают использование функции в аудиопотоке или время жизни объекта.
