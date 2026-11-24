# Stage 3a — защита музыкального источника и смена поколений

Добавлен конкретный `SourceDomain` / `NativeReadOwner` в
`src/pattern_source.h/.cc`. Это уже реализация синхронизации, а не fixture
интерфейса. Пока она не подключена к production mutators/экрану и не включена
в сборку приложения. Native объекты остаются собственностью штатного движка.

## Протокол

Domain создаётся с закрытым доступом. На подтверждённом owner можно явно
опубликовать согласованные image, Project, Things, Program и копию mixer
outputs. `publish` проверяет epoch и отсутствие читателей/изменений, затем
открывает чтение. Устаревший publisher отказывается **до** разыменования
старых pointers. Native admission не открывается автоматически на UI tick.

Owner создаётся только для уже открытого источника, захватывает его epoch
и удерживает только domain metadata. При `enter` он сначала проверяет domain,
затем разрешает pointers из native context и сверяет Program. До завершения
`leave` source reader count защищает Project/Program/event-list от
**участвующих в протоколе** изменений. Owner не блокируется в ожидании writer:
если доступ закрыт или epoch устарел, enter отказывает до native чтения.

Перед любым изменением/удалением writer обязан вызвать `change()`:

1. Доступ немедленно закрывается, epoch повышается. Старые owners и готовые
   результаты теряют актуальность.
2. Writer ждёт выхода всех допущенных читателей. Они могут увидеть revoked
   validity, но их native данные ещё нельзя освобождать до leave.
3. После ожидания metadata mutex освобождён. Native New/Load/Program work
   выполняется без удержания domain mutex или render lock.
4. Конец Change, включая исключение, **не** открывает доступ.
5. Только подтверждённый completion owner публикует новый binding с текущим
   epoch. Для отложенной работы требуется её реальное завершение; возврат из
   инициирующего New/Load сам по себе completion не считается.

Change tokens поддерживают вложенные/отложенные операции. Если токен перенесён
в async completion, publish запрещён до его завершения. `retire` навсегда
закрывает domain, ждёт readers и активные Change tokens, очищает binding.
Его нельзя вызывать из собственного read lease или пока тот же поток удерживает
незавершённый Change, который некому завершить. До фактического освобождения
native объектов должен завершиться retire либо соответствующий change fence.

Границы `source_new`, `source_read`, `source_load`, `source_program` реализуют
этот протокол вокруг переданного original callback. Отмена New (choice != 1)
не меняет epoch. Failed Read/Load и исключение оставляют доступ закрытым;
повторное открытие требует проверки действительного состояния источника.
Границы не сериализуют native writers между собой и не заменяют их штатную
очередь/транзакции: их задача — исключить пересечение чтения с изменением.

## Готовый результат тоже может устареть

В `pattern_request.*` добавлен owned `ReadCurrency`. Enqueue требует non-null
value-only stamp, иначе отказывает. Concrete SourceDomain stamp проверяет
только metadata и не обращается к native памяти даже после teardown.

После смены поколения уже Complete request показывает `stale`, а `take()`
отказывается, сохраняя прежний результат caller. Проверка производится перед
выдачей; это не обещание запретить изменения после возврата. Перед будущим
native edit/commit потребуется отдельная проверка в command boundary.

## Что найдено в штатном коде

- В `phone_native.c` уже есть предупреждение о реальном device hang при
  удержании render lock вокруг команд, ожидающих audio thread. Его не используем.
- `H_NEW_PROJECT` / `0x1642a00` вызывает `0x14fc480`, который делегирует
  stored callback. По этому UI entry нельзя объявить доказанным момент
  завершения изменения live Project.
- `H_MUSIC_READ` / `0x1490650` и `H_MUSIC_LOAD` / `0x148d830` уже составлены
  с модульными archive wrappers. Их возврат также нельзя автоматически считать
  публикацией всех live native owners без исследования дальнейшего пути.
- Project `0x2909be0` возвращает raw Sequence pointer из OwnedArray.
  Добавлять предположительный refcount к этому указателю нельзя.
- `0x29112f0` (Project destructor из vtable `0x6ecc648`) вызывает
  `0x290cee0`, выполняющий teardown музыкальных данных. Найдены также direct
  callers `0x13a9b8c` и `0x14ec850` в shutdown/destructor путях; это не
  доказательство охвата всех New/Load мутаций.
- В Track диапазоне обнаружены stores Program pointer `+0xbb8` на
  `0x29c7b38`, `0x29c9440`, `0x29c9518`, `0x29c9884`, `0x29c98e0`,
  `0x29c9de0`. `0x29c9490` читает прежний Program и присваивает новый,
  но один этот setter не покрывает остальные stores/indirect callers.

**Полный production охват writer paths ещё не доказан.** Нельзя обойти этот
долг флагом read_safe: каждый путь удаления/замены Project, Program, Clip и
event tree, включая native edits, должен участвовать в serialization либо
иметь подтверждённую общую защиту. Нужно также проверить thread affinity и
lock order: change fence вызывается до native locks, нужных читателю, и не
из render/audio callback. Поэтому production admission/UI остаётся выключенным.
Новые scope wrappers пока не подменяют штатные callbacks в phone_native.c.

## Проверки

`verify_pattern_source.py`: host, host ASan/UBSan, ARM QEMU, ARM UBSan.
В тесте writer действительно освобождает heap buffers Project/Things/Program.
Он не может сделать это до reader leave; revoked owner и stale publisher
после освобождения не обращаются к этим buffers. Проверены:

- закрытый старт, явная публикация, неверный outputs и повторный enter;
- смена epoch во время чтения, ожидание writer и отказ старого owner;
- отменённый/подтверждённый New, nested Read→Load, failed Read и исключение;
- Program invalidation, nested/async Change tokens, ожидание retire;
- metadata currency после уничтожения domain без native доступа.

`verify_pattern_request.py` расширен настоящим SourceDomain в цепочке real
native queue → native getters → Snapshot. Проверены stale take уже готового
результата, отказ old owner при submit, смена источника до worker исполнения,
восстановление после подтверждённого publish. Прежние native cancellation,
Copy destructor wait, queue wrap/full и allocation-failure проверки сохранены.

Source domain — настоящий новый код, однако native lifecycle mutations в
этих проверках callbacks/fixtures, не полный New/Load в работающем приложении.
Proofs: `build/pattern-source/verification.json`,
`build/pattern-request/verification.json`. Сборка приложения/пакета и проверка
на физическом устройстве не выполнялись. XPJ, playback и native history
не изменены.
