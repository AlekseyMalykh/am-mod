# Arrange — прямое создание и единая навигация

Следующая модель работы с несколькими пэдовыми строками одного блока:
[ARRANGE_COMPOUND_BLOCKS_RU.md](ARRANGE_COMPOUND_BLOCKS_RU.md).

Продолжение Stage 4e, только opt-in Pattern Tools в ветке
`codex/pad-midi-recording`. Существующий Piano Roll, Pattern Store, история
и два optional XPJ-поля используются без изменения формата. Штатные
Sequence и Sequential Song не меняются. Новая схема ещё не установлена
на карту: установленный Arrange Test 1 остаётся на коммите `245d502`.

## Основной цикл

Остановить транспорт → Song / Export / Horizontal Arrange → B3 New.
Пустой паттерн, блок и первая дорожка создаются одной операцией Store.
Сразу открывается существующий Piano Roll. В его pattern host B3 New на
пустой ячейке вызывает прежний Insert с той же валидацией и историей.
Пэды выбирают строку; 16 Levels открывает Tune, Chop — соответствующий слой.
Это ручная расстановка событий, не запись физической игры и не audition.
B1 из Tune/Chop возвращает Pad view, следующий B1 — Arrange.

В Arrange B3 на блоке без выделения — Open. Нажатие DATA выделяет блок,
повторное снимает выделение. B3 с выделением — Edit. DATA меняет положение,
фейдер — дорожку (вверх = верхняя), K1 — положение, K2 — дорожку, K3 —
длину блока. Это preview от сохранённого блока; Store не меняется до B3
Apply. B1 Cancel возвращает прежние курсор/строку/прокрутку. Пересечение
соседнего блока отклоняется при Apply, preview остаётся для исправления.
Применение движения и длины атомарно, без промежуточной коллизии.

Shift+B3 — один плоский Actions в Arrange и Piano Roll. Сохранены
Insert pattern, Copy/Paste, Move selected here, Extend, Trim, Join,
Mute/Solo, Delete с подтверждением, изменение длины песни. Добавлены
New empty pattern и Create variation. Variation создаёт новый Pattern ID
и заменяет ссылку только выбранного блока одной операцией Undo. Прочие
блоки продолжают ссылаться на исходный паттерн. Chop режет блок на курсоре.
Shift −/+ — общий Undo/Redo. B1 сначала отменяет edit/закрывает меню/
снимает выделение, затем возвращает Sequential Song.

## Длина и назначение

B2 Range или удержание DATA с поворотом энкодера выделяет время на текущей
строке. New берёт этот диапазон, включая конечную ячейку сетки. Без Range
берётся последняя использованная длина в текущем проекте; начальная —
4 четверти. При необходимости New атомарно увеличивает длину песни.
Пока выделяется один блок; групповое выделение/редактирование нескольких
блоков и прямоугольник на нескольких строках — следующий UI этап.

Новый паттерн наследует только каталог назначений пэдов ближайшего слева
блока той же строки, либо первого паттерна библиотеки. События не копируются.
Если библиотеки ещё нет, создаются 128 редактируемых Pad rows с явным
зарезервированным назначением `am:unassigned` (короткая строка без отдельной
heap-аллокации для каждого имени). Редактор показывает Unassigned instrument.
Это не настоящее имя Program. Native preflight запрещает доставку таких
событий даже при совпадении строки с destination. Выбор/смена инструмента
для этого draft ещё впереди; New не читает непроверенные живые Project/
Program pointers и не открывает закрытый native source reader.

## Проверки

`verify_pattern_tools.py`: atomic New/автодорожка/расширение песни,
копирование только routes, single-reference variation, stale/overlap,
один Undo, сохранение/загрузка двух optional archive-полей в фикстуре.
283 точки отказа аллокации при New не оставляют сиротских объектов и
не меняют выходной ID. Host/ASAN и ARM/UBSan. Реальный Piano Roll получает
новый unassigned Pattern при намеренно невалидных native Events pointers:
B3 New вставляет Pad/Tune события; Undo/Redo и возврат работают.
Production-конфигурации step/phone дополнительно компилируются с O2.

`verify_arrangement_ui.py`: настоящий Song overlay, новый цикл New/Open/
Edit/Apply/Cancel, DATA press/release, Range, variation, старые Chop/Join/
Copy/Mute/Solo/Sequential/Export, границы отрисовки и project reset.
Song → editor handoff здесь фикстурный; реальный редактор проверен отдельно.
`verify_arrangement_native_plan.py` запрещает unassigned dispatch.
`verify_synth_live_native.py` сохраняет native Engine/Synth proof.

Play/Record в Arrange остаются закрыты. Штатная последовательная запись
сохранена. Native executor sink/clock/voice ownership, аппаратная запись,
полный XPJ roundtrip через GUI, Mixdown и физическое устройство этим UI
этапом не подтверждены. Полная библиотека/SD-пакет здесь не пересобирались.
