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

В iterator erase аргумент — адрес переменной с node pointer, не сам node.
Исполняются настоящий native поиск, tree rebalance, освобождение узла и
обновление служебного дерева удалённых событий. Вызываемые PLT функции
libstdc++/allocator разрешаются стендом как в существующих native проверках.
Notification services заменены наблюдателями/no-op; это не проверка реального
GUI refresh, Undo/Redo, движка воспроизведения или физических пэдов.

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

## Границы результата

Проверены только inline tag-3 события. Цикл из native erase в стенде не означает
покрытие самостоятельных native clear/bulk/history entries. Не проверены
pointer/refcounted event variants, ошибки выделения памяти внутри native
writer, callbacks реальных services, thread affinity и caller lock order.
Production hooks не установлены, admission остаётся закрытым.

Логи и hashes: `build/pattern-event-writer/verification.json`.
`analyze_pattern_writers.py` добавляет отдельный `native_event_contract_probe`
только при совпадении stock SHA и hashes всех исходников. Существующий direct
branch inventory остаётся ограниченным прежним набором targets; наличие
event proof не делает его полным call graph.

Далее: найти самостоятельные clear/bulk/history boundaries и проверить их
потоки/lock order, прежде чем подключать live snapshots к интерфейсу.
