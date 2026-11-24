# Stage 3: штатное удаление событий и защита снимков

`verify_pattern_event_writer.py --stock ...` исполняет pinned stock ELF 1.3.0
в отдельных AArch64 executables через QEMU, normal и UBSan. Приложение,
preload и пакет не собираются. Это продолжение `PATTERN_NATIVE_WRITERS_RU.md`.

## Исполненные native entries

| Entry | Проверенный контракт |
|---|---|
| `0x27eef10` | `(EventList*, iterator*)`, удаляет одно событие, возвращает следующий node/header |
| `0x27ef360` | `(EventList*, event key*)`, ищет точное совпадение и вызывает iterator erase; при отсутствии возвращает header |
| `0x27ee5d0` | Добавляет inline tag-3 событие; используется также для повторного заполнения |
| `0x27f0ee0` | Применяет пару деревьев remove/add, в том числе набор целиком и обратную операцию |
| `0x27f0500` | Single replacement path, вызывается из native delta replay |
| `0x19d8f70` / `0x19d52c0` | Native методы удаления/обратной вставки inline DeleteEvent command |

В iterator erase аргумент — адрес переменной с node pointer, не сам node.
Исполняются настоящий native поиск, tree rebalance, освобождение узла и
обновление служебного дерева удалённых событий. Вызываемые PLT функции
libstdc++/allocator разрешаются стендом как в существующих native проверках.
Notification services заменены наблюдателями/no-op; это не проверка реального
GUI refresh, менеджера Undo/Redo, движка воспроизведения или физических пэдов.

Проверены:

- Удаление одной из двух одинаковых нот: второй node сохраняет адрес и
  музыкальные данные, соседнее событие тоже остаётся неизменным.
- Возвращаемый successor совпадает с заранее вычисленным native iterator.
- Exact-key wrapper не удаляет соседей при отсутствии совпадения по времени,
  высоте или длительности; для 32 одинаковых нот удаляет только одну.
- Удаление последнего узла восстанавливает пустой root и header links.
- Повторное заполнение 32 событиями и удаление всех через native entries.
  Служебное дерево удалённых событий получает соответствующие записи.

## Точка защиты должна предшествовать изменению

В threaded case настоящий erase запускается внутри `SourceDomain::change`.
Пока reader удерживает lease, writer отзывает currency, но не меняет count
и не вызывает notification. После reader leave исполняется native erase.
Доступ остаётся закрытым; старый owner не может прочитать изменённый список.
После явного publish новый `read_native` получает корректный снимок, в том
числе пустой. Ранее созданные Snapshot и извлечённый Pattern сохраняют свои
ноты после освобождения всех исходных native nodes.

Наблюдатель `0x27ee100` при удалении видит **уже уменьшенный** main-list count.
При native batching flag EventList+0x48 = 1 три insert и три erase проходят
без единого notification; count и deletion tree при этом меняются.
Следовательно, post-change notification не может быть единственной lifetime
границей. Закрывать чтение нужно перед writer/owner transaction, до любого
изменения или освобождения node. Поздний refresh не заменяет такую защиту.

## Native command methods и массовое применение изменений

Добавлена отдельная серия проверок:

- Команда DeleteEvent с настоящим vptr `0x6e44288` вызывает native slot 8
  (`0x19d8f70`, удаление), slot 9 (`0x19d52c0`, обратная вставка) и снова
  slot 8/9 для redo/undo. Один из дублей удаляется и восстанавливается,
  соседняя нота не меняется. Сам command fixture инициализирован вручную:
  list+0x50, inline event+0x58, nested command+0x90=null. Native constructor,
  manager enqueue, выбор команды из истории и освобождение command не
  исполняются; это проверка методов, не всей истории пользователя.
- `0x27f0ee0` получает независимые remove/add деревья, заполненные native
  `0x1574460`. Набор из трёх нот с дублями полностью удаляется и возвращается
  в четырёх циклах. Исходные delta trees сохраняют counts. Здесь удалением
  управляет native bulk loop, а не C++ цикл из отдельных erase.
- Два singleton sets проходят native replacement fast path `0x27f0500`:
  меняются время и высота одной ноты, затем возвращаются; вторая одинаковая
  нота сохраняется.
- При внешнем batch counter = 2 bulk remove/restore возвращает счётчик к 2
  и не вызывает post notifications. После явного publish адаптер видит
  восстановленные три ноты.
- SourceDomain ждёт активный read lease перед native virtual execute;
  по завершении команды admission закрыт. После каждой операции новое
  чтение проверяет число нот, старый Snapshot/Pattern остаётся неизменным.

Связь с составными командами подтверждена disassembly: `0x19d8f70` в nested
ветке вызывает `0x27f0ee0` на +0x80/+0xb0 trees, а `0x19d52c0` передаёт их
в обратном порядке. Эта nested-ветка самой команды пока не исполнялась.
Наличие такого общего механизма не доказывает, что найден GUI entry Clear.

## Dispatcher и владение командой

Следующая серия использует существующий подход из `check_batch_dispatch.c`:
настоящие CommandManager constructor `0x19cf2b0`, dispatcher `0x19d0a00`,
transaction `0x19cc1f0` и command state wrapper `0x19c9a50`. Model monitor,
manager enter/trim и хранение истории остаются адаптерами; open transaction
задана стендом. Native queue и полный менеджер истории не запускаются.

Сам DeleteEvent теперь создаётся настоящим constructor `0x19d8ba0` в native
allocation размером 0x98. Используется mode 2, отключающий optional nested
command: native GUI builder `0x194fa30` выбирает другой mode, поэтому его
полный путь этой проверкой не покрыт. Constructor инициализирует native
strings, inline event, vptr и command state. В этой серии вручную записывать
состояние команды перед выполнением не приходится.

Проверены следующие условия:

- До dispatch исходный list не меняется; command state = 1.
- SourceDomain отзывает epoch, но ждёт read lease до входа в настоящий
  dispatcher; ни enter-adapter, ни команда, ни history-adapter раньше не
  исполняются.
- Native dispatch выполняет удаление одной из одинаковых нот, переводит
  state в 0 и передаёт shared ownership в history-adapter ровно один раз.
- После reset ссылки вызывающей стороны команда продолжает жить за счёт
  history-adapter. Native reverse wrapper `0x19c9c40` возвращает state в 1
  и восстанавливает ноту; `0x19c9a50` снова удаляет её и переводит state в 0.
- Повторный Perform в state 0 и Undo в state 1 возвращают ошибку, сохраняют
  число нот/command state и не добавляют запись истории.
- После финального Undo ноты совпадают с исходными, а reset history-adapter
  вызывает настоящий native deleting destructor ровно один раз. Weak pointer
  подтверждает прекращение владения объектом.

Для caller/history ссылок используется совместимый libstdc++ shared_ptr ABI,
как в существующих мостах проекта. Это проверка реального dispatch и
владения командой на стенде, но не доказательство, что ссылки на Project/List
удерживаются полной native историей при New/Load. Переходы выбираются стендом,
а не кнопкой Undo/Redo; selection/trimming реального history stack ещё нужны.

## Границы результата

Проверены только inline tag-3 события. Покрыты указанные bulk/command методы
и dispatch с history-adapter, но GUI Clear и полный history manager не проверены. Не проверены
pointer/refcounted event variants, ошибки выделения памяти внутри native
writer, callbacks реальных services, thread affinity и caller lock order.
Production hooks не установлены, admission остаётся закрытым.

Логи и hashes: `build/pattern-event-writer/verification.json`.
`analyze_pattern_writers.py` добавляет отдельный `native_event_contract_probe`
только при совпадении stock SHA и hashes всех исходников. Существующий direct
branch inventory остаётся ограниченным прежним набором targets. Дополнительный
`event_direct_branch_inventory` перечисляет прямые переходы к erase, delta и
двум command methods с неподтверждёнными entry hints. Virtual/indirect calls
этот скан не покрывает; наличие event proof не делает его полным call graph.

Далее: найти GUI Clear и manager history boundaries и проверить их
потоки/lock order, прежде чем подключать live snapshots к интерфейсу.
