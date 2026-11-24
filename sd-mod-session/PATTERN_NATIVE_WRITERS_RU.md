# Stage 3a — исполненные native Program paths и Clip selector

Статус: новые isolated ARM/QEMU normal/UBSan probes. Приложение/preload не
собирались, новые Program/Clip hooks не установлены. Source admission остаётся
закрытым. Этот шаг уточняет контракты для следующих перехватов.

## Исправление контракта Clip getter

Прежняя подпись `0x2957ae0(create=0)` в metadata была ошибочной. Исследован
реальный `0x29579e0(sequence,track,selector)` и его wrapper `0x2957ae0`:

- selector 0 читает default Track→Clip map;
- selector 1 читает другую существующую карту из набора, на который указывает
  Sequence+0x620; тестовая строка map имеет размер 0xe0;
- отсутствующий Track и out-of-range selector возвращают null;
- обе карты, их headers и число событий не меняются; создание Clip не
  включается третьим параметром.

В тесте default map содержит три события, alternate map — одно другое.
Исполняются штатные getters, list constructor, event factory и insert.
Clip shared_ptr control blocks в map fixtures null. Нельзя распространять
этот тест на полное владение живыми Clip или точный музыкальный смысл всех
alternate maps без дальнейшего исследования.

Наш adapter по-прежнему правильно передаёт 0. Его scope теперь явно описан
как **текущий native Track и default Clip map**, а не объединение всех
TrackClipMaps секвенции. Дополнительные карты не конвертируются и не удаляются.
Комментарии и proof metadata исправлены; snapshot adapter regression прошёл
повторно. Оба getter targets отмечены в inventory как read-only относительно
музыкальных данных; временные native shared_ptr операции этим не отрицаются.

## Program assignment `0x29c9490`

Настоящая функция принимает Track, Program pointer и native number. Проверены:

1. Pre-change notification slot 0x30 видит прежние pointer/number.
2. Native code присваивает Track+0xbb8 и +0xbc0.
3. Old/new listener callbacks уже видят новый pointer/number.
4. Post-change notification slot 0x38 видит новый binding.

Повтор того же **ненулевого** Program — native no-op: новое number игнорируется,
уведомлений нет. Null detach/reattach работает иначе. Даже null→null проходит
notifications и обновляет number; пропускать этот вызов только по равенству
указателей нельзя. Wrapper обязан передавать original вызов без такой
самостоятельной оптимизации.

В отдельном threaded case настоящий setter вызван через `source_program`.
При активном read lease writer успевает инвалидировать epoch, но ни один
notification и store не выполняется до reader leave. После вызова admission
остаётся закрытым. Это проверка concrete SourceDomain перед реальным writer,
а не только setter mock.

## Два пути снятия совпадающей ссылки

Исполнены также:

| Entry | Receiver | Store |
|---|---|---|
| `0x29c93a0` | Track | `0x29c9440`: Program pointer → null |
| `0x29c7a80` | Track+0x910 | `0x29c7b38`: Program pointer → null |

Обе функции игнорируют null/другую Program и очищают только совпавшую текущую
ссылку. При этом native number берётся из Program+0x1234. Для первого пути
конечный `0x29c9210` rebuild captured fixture; для второго исполняется native
порядок listener removal / post notification. Приёмники notifications и
subscriber attach/detach — fixtures. Реальный Program destructor этим тестом
не выполняется; нельзя заключать, что данные ещё не начали освобождаться
до входа в эти callbacks. Место окончательного lifetime fence надо искать
также выше по цепочке удаления.

Разные receiver offsets подтверждены исполнением. Ставить на обе точки один
hook, считающий x0 прямым Track, было бы ошибкой.

## Program replacement `0x29c9670`: базовый путь

Исполнен настоящий entry `(Track*, Program*)` с захватом service boundaries.
Проверены замена old→new, снятие new→null, назначение null→old и повторные
назначения одинаковых ненулевых и null pointers. Пройдены оба native stores:
`0x29c9884` (новая Program) и `0x29c98e0` (null). Number берётся из
новой Program+0x1234 при назначении и прежней Program при снятии.

В отличие от `0x29c9490`, равенство pointers не делает весь вызов no-op:
перед сравнением выполняются reset `0x29c65b0` и pre-notify, а после — refresh
`0x29c68a0` и rebuild `0x29c9210`. Null→null проходит ветку равенства;
чтения number через null в этом случае нет. Все captured callbacks до store
наблюдают прежние pointer/number, refresh и rebuild — итоговые.

Отдельный threaded test вызывает этот native entry внутри `SourceDomain::change`:
до reader leave не исполняется даже начальный reset; epoch уже отозван.
После завершения replacement доступ к чтению остаётся закрытым.

RTTI `__dynamic_cast` в этом тесте явно возвращает null. Reset, парные binding
services, name binding, refresh и rebuild заменены fixtures с проверкой
аргументов/порядка и наблюдением полей. Поэтому исполнение stores доказано
для базового пути, но работа subtype connections (+0x2f00), настоящих
подписчиков, их reentrancy, thread affinity и безопасность production hooks
ещё не доказаны. Это не исполнение полного native instrument replacement.

## Ограничения и следующая работа

Native assignment в этом стенде не обновляет CurrentThings: соответствующие
listener services заменены fixtures. Следовательно, возврат из setter не
доказывает готовность всего binding к публикации. Thread affinity и caller
lock order также не доказаны. Entry может быть проверен исполнением и всё
ещё не быть безопасным blocking hook для audio/MIDI потока.

`verify_pattern_writer.py --stock ...` сохраняет logs/hashes в
`build/pattern-writer/verification.json`. `analyze_pattern_writers.py` связывает
inventory с этим proof только при совпадении всех исходных hashes:

- пять из шести store sites имеют исполненные entries и receiver contracts
  (две точки replacement проверены только для базового пути Program);
- две Clip getter targets исключены из списка writers;
- 147 direct branches всего, из них 109 к оставшимся mutator targets;
- `hook_ready`, `coverage_complete`, `production_read_admission` остаются false.

Далее: subtype connections replacement path `0x29c9670`, инициализация
`0x29c9920`, источники вызовов/потоки проверенных entries, затем реальные
event erase/clear/bulk/history paths. Пока нет основания открывать live чтение
или считать весь путь New/Load/Program безопасным в работающей программе.
