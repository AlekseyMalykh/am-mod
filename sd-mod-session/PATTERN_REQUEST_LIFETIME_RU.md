# Stage 3a — запрос снимка через native executor

Добавлены `pattern_request.h/.cc`, пока без production UI/build wiring.
Используются настоящие native ThreadQueues enqueue/runner, producer mutex,
reclaim и протокол уничтожения Copy context. Не создаётся новый worker.

## Что гарантирует реализованный слой

Caller обязан держать context действительным во время submit. Принимается
только pinned Copy context с vptr `image+0x6d8cd88`: для него исследован
деструктор `0x1517c70`. Submit копирует request и удерживает объект
`NativeReadOwner` через shared ownership; native Project/Program/event-list
pointers с UI не захватываются. Они разрешаются только внутри callback.

Тот же путь, что stock Copy `0x1517e34…0x1517e98`:

- context `+0x48`: ThreadQueues owner и начало reference payload;
- context `+0x50`: atomic outstanding callbacks counter;
- owner `+0x220`: настоящий recursive producer mutex;
- owner `+0x1d8`: queue, enqueue `0x2b59300`;
- runner `0x2b59200` / `0x2b59160` вызывает callback позднее.

Очередь копирует payload своим manager. Новый manager удерживает Job без
дополнительного allocation; уничтожение копии освобождает владение. Caller
может отпустить request handle или owner после успешного submit. При callback
и при отмене ещё не запущенной задачи это не оставляет dangling user pointer.
Получить успешный результат можно один раз: только immutable Snapshot.

Callback вызывает `read_native` под отдельным source-owner lease. Проверяются
соответствие image, Project `context+0xc8` и Things `context+0x78`, а также
owner validity и отмена. После выхода из lease очищаются захваченные
owner/context, снимается native counter и только затем публикуется Complete.
Terminal callback не хранит native pointers или большой Snapshot в Job.
Стандартный результат содержит ошибку либо самостоятельные значения.

**Счётчик удерживает контекст callback, но не доказывает срок жизни всех
Project/Program/Clip объектов и всех полей производного Copy context.**
Деструктор сначала освобождает часть производных полей, затем ожидает/отменяет
очередь. Нельзя обращаться к таким полям из Source owner, полагаясь только
на counter. Указатели на музыкальные объекты должен защищать настоящий
owner lease — см. оставшуюся интеграцию ниже.

## Отмена без callback

Исследован настоящий `0x1517c70`: он вызывает `0x2b579a0` по reference tag
и вычитает число отменённых задач из counter. В queue `0x2b590d0/0x2b59000`
pending tag меняется на 0 без вызова callback. Для уже выполняющейся задачи
tag становится 3; native destructor ждёт завершения runner через futex.
Running callback сам снимает свой counter, повторного вычитания нет.

Поэтому Job destructor **не** снимает native counter и не разыменовывает
context: после native purge он уже может быть уничтожен. Он переводит ещё
Queued handle в Cancelled, когда native очередь освобождает payload.
После purge это происходит не сразу: consumer сначала пропускает отменённый
slot, затем reclaim `0x2b58c70` уничтожает closure. До этого payload владеет
небольшими данными запроса/owner. Для немедленного состояния UI нужно явно
вызвать `cancel()`; готовый snapshot от отменённого running request не
публикуется. Cancel синхронизирован с публикацией Complete.

Деструктор `NativeReadOwner` также может быть вызван при таком позднем reclaim:
он не должен разыменовывать borrowed native pointers, в том числе context.

## Очередь без блокирования/inline fallback

`0x2b59300` при переполнении может ждать или выполнять work на потоке
producer (policy byte `queue+0x2c`). Для чтения native объектов это недопустимая
неявная смена контекста. Под producer mutex сначала вызывается только reclaim
завершённых closures, затем проверяются ring indices и свободный current slot.
Full/busy возвращает Busy **до** enqueue, без inline work и без counter leak.
Reclaim до проверки также предотвращает вечный Busy после полного оборота ring.
Это использует штатное условие: producers данной очереди берут тот же mutex.

## Оставшаяся гарантия музыкального источника

`NativeReadOwner` — owned интерфейс enter/valid/leave. Enter обязан на
правильном владельце получить реальную защиту Project/Program/Clip от
изменения/удаления, вернуть согласованный generation и routing snapshot,
а leave — освободить её. Pending/Running request удерживает сам объект owner,
но не выдаёт это за владение его произвольными native pointers.

Конкретный source-owner lease теперь реализован в `pattern_source.h/.cc`:
закрытое admission, reader/writer fence, New/Read/Load/Program boundaries,
явный rebind и value-only currency. SourceDomain подключён к изолированному
native queue тесту; подробности — `PATTERN_SOURCE_LIFETIME_RU.md`.
**Полный production охват mutators ещё не доказан и не подключён.**
Новый слой не выставляет `read_safe=true` автоматически из факта нахождения
в queue. В первоначальной группе тестов source protected mutex остаётся fixture;
новая группа использует настоящий SourceDomain. Project/Things storage и
native lifecycle mutations в стенде по-прежнему подготовлены тестом.
Изученный Project sequence getter `0x2909be0` возвращает raw pointer из
OwnedArray, а не shared_ptr; простое увеличение неизвестного счётчика Sequence
было бы предположением о native ABI. Двойное чтение тоже не защищает от free.

Следующий шаг — подтвердить общий owner/serialization для New/Load и edits,
затем реализовать этот concrete lease и инвалидировать UI requests по
project/routing/sequence identity epochs. До этого пользовательский Split
не подключается. Логический native executor callback проверен; полная
thread-affinity и все production пути teardown ещё требуют system VM проверки.

## Проверки

`verify_pattern_request.py --stock ...`: отдельный ARM executable в QEMU,
normal и UBSan, без preload/app/SD package build. Проверены:

- настоящие queue copy, native recursive mutex, runner и reclaim;
- отложенный запуск: Project owner не вызывается при submit;
- shared callback lifetime при отпускании owner и даже request handle;
- одноразовое получение immutable результата;
- Cancel до запуска и во время чтения, generation/loading rejection,
  owner exception и неверный image, отсутствие counter leak;
- stopped queue и отказ duplicate pending submit с сохранением старого handle;
- full queue с producer-inline policy: отказ без выполнения чтений;
- 24 последовательных оборота маленького ring, восстановление после full;
- три allocation failure points при submit: старый handle/counter сохранены;
- настоящий Copy destructor для pending request: callback не вызывается,
  native purge снимает counter, payload безопасно уничтожается позднее;
- настоящий Copy destructor при running callback: тест дожидается реального
  tag 3 перед разрешением callback продолжить; destructor не завершает context
  до выхода source lease и освобождения native callback reference.

В destructor test base destructor заменён marker, а отмена другой native
queue — пустым fixture. Исследуемые queue/cancel/wait функции исполняются из
штатного ELF; дополнительные производные поля Copy пусты. Это не полный
teardown настоящего UI/Project. Proof/logs/hashes —
`build/pattern-request/verification.json`.

Штатное воспроизведение, history, Sequential Song, save format и UI не меняются.
