# Stage 3a — первые lifecycle hooks и writer inventory

## Подключённые точки

`phone_native.c` теперь использует общий `pattern_phone_lifecycle.inc` для
трёх callback entries: `H_MUSIC_LOAD`, `H_MUSIC_READ`, `H_NEW_PROJECT`.
Без `MPC_PATTERN_SOURCE_TRIAL` macros указывают ровно на прежние функции.
Save/Key callbacks и остальные entries таблицы не меняются. Новых native
bridge offsets, trampoline или patch sites этот шаг не добавляет.

При включённом trial-флаге внешний guard вызывает прежние составные функции
целиком. Поэтому цепочка music archive → Mixer → Synth и существующие New
reset действия сохраняются. Перед Original выполняется SourceDomain change
fence. Read возвращает исходный int; параметры Load/New не преобразуются.
Отмена New не инвалидирует источник. Вложенный Read→Load поддержан; ни выход
из callback, ни успешный Read не считаются подтверждённым completion live
Project и не открывают чтение автоматически.

Процессный domain находится в `pattern_lifecycle.cc`. В production у него
**нет API публикации binding**: он закрыт по умолчанию и остаётся закрытым,
пока не подтверждён полный охват writers. Test-only accessor компилируется
только с `AM_PATTERN_LIFECYCLE_TEST_ACCESS`; отдельная проверка production
object подтверждает отсутствие этого символа. Новый guard не позволяет
включить небезопасный пользовательский Split при неполном исследовании.

Если первое создание metadata domain не удалось, допущенных читателей быть
не может; native callback сохраняется работоспособным. Это не механизм
обхода защиты уже открытого источника. При обычной работе mutex domain
не удерживается на время вызова Original. Render lock не берётся.

`build_mixer_r27r.py` поддерживает отдельный opt-in
`AM_MOD_PATTERN_SOURCE_TRIAL=1`, добавляющий только `pattern_source.cc`,
`pattern_lifecycle.cc` и macro. Флаг по умолчанию выключен; Pattern UI,
native snapshot requests и автоматический publisher не включаются этим
флагом. Builder учитывает общий include в hashes даже при выключенном
trial-флаге. Builder **не запускался**, preload и installable package не созданы.

## Проверки

`verify_pattern_lifecycle.py` компилирует и исполняет точный общий C include,
используемый phone callback table, с test originals. Host, ASan/UBSan,
ARM QEMU и ARM UBSan проверены в baseline/trial вариантах — восемь прогонов.

- Baseline использует original callbacks и не меняет epoch domain.
- Trial сохраняет аргументы, число original вызовов и Read result.
- Nested Read→Load проходит обе границы и оставляет admission закрытым.
- Cancel New сохраняет epoch; Confirm New инвалидирует старый owner.
- При активном read lease original Load не вызывается до reader leave.

Дополнительно выполнен только `-fsyntax-only` реального `phone_native.c`
в трёх поддержанных конфигурациях: обычные Mixer flags, новый trial и его
сочетание с Pad MIDI/Synth trials. Общие flags прочитаны как AST data из
builder, сам builder не импортируется и не выполняется. Это не link/load
проверка полной библиотеки. В isolated C tests originals — fixtures, а не
New/Load полной запущенной MPC программы.

Proof: `build/pattern-lifecycle/verification.json` с hashes исходников и
отдельными логами восьми прогонов.

## Машиночитаемый аудит writers

`analyze_pattern_writers.py --stock ...` только читает pinned ELF. SHA-256
проверяется до анализа; inventory содержит hash самого анализатора.
Результат — `build/pattern-writers/inventory.json`:

- шесть кандидатов `STR Xt,[Xn,#0xbb8]` в диапазоне native Track;
- 147 прямых BL/B к исследуемым Program assignment, Sequence Clip getters,
  event insertion, Sequence/Project destructors и Project teardown;
- проверенные vtable anchors Project/Sequence и инструкции Program setter;
- entry hints отмечены как **не сертифицированные**, не используются для hooks.

Это карта дальнейшего исследования, **не полный call graph и не доказательство
coverage**. В частности, не учтены автоматически indirect/virtual calls,
adjusted-base stores, все erase/clear/bulk/history paths, thread affinity и
lock order. Поля `coverage_complete` и `production_read_admission` — false.
Скрипт не устанавливает hooks и не собирает приложение.

Следующая работа: классифицировать эти writers, проверить реальное выполнение
и безопасные границы Program/Clip mutations, затем подключать их по одной
под тем же trial flag. До этого native source admission остаётся закрытым.
Текущие Sequence, Sequential Song, XPJ и привычный pad workflow не меняются
при стандартной конфигурации.

Продолжение: `verify_pattern_writer.py` исполняет Program setter и два пути
снятия совпадающей ссылки, а также исправляет Clip map-selector contract.
В актуальном inventory 38 прямых переходов относятся к read-only getter
targets; 109 — к оставшимся mutator targets. Исполненный entry не означает
готовность blocking hook: thread/lock/lifetime contracts ещё неполны.
Подробности — `PATTERN_NATIVE_WRITERS_RU.md`.

## База SampleBoot: r30d-beta4

В ветку `codex/pad-midi-recording` включён официальный тег `r30d-beta4`
(release commit `e0a846f`, merge `57995a8`). Перенесены изменения запуска,
установщика 1.5.1 и диагностики в `AmMod/` вместе с документацией.
В diff релиза от общей базы нет изменений native sequencer/MIDI/Synth или
экспериментальных lifecycle hooks; merge прошёл без конфликтов.

После merge проверены: installer unittest (37, из них 2 skipped),
`verify_power_audit.py` (AArch64 helper в QEMU, временная карта),
`verify_pattern_lifecycle.py` (8 baseline/trial вариантов host/ASan/ARM/UBSan
и 3 syntax-only конфигурации `phone_native.c`). Приложение и пакет не
собирались. Запуск через физический SampleBoot этими проверками не доказан.
Будущие сборки этой ветки должны сохранять launcher и пути из beta4;
экспериментальные флаги MIDI/Pattern не означают готовность нового релиза.
