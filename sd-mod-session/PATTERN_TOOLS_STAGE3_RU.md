# Этап 3 — Parts / Patterns

Реализован экспериментальный путь через существующий Piano Roll: запись
остаётся штатной, после неё пользователь создаёт независимые партии и
паттерны. Native Sequence не переписывается при Split/Extract/Variation.
Код, UI и XPJ hooks готовы к будущей интеграционной сборке. Приложение,
preload и установочный пакет не собирались; физический MPC не проверен.

## Почему этап больше не ждёт покрытия всех native writers

Новый экран получает **уже принадлежащие UI значения `pr_note`** из текущего
Piano Roll. Он не вызывает `pattern_native`/`enqueue_snapshot` и не открывает
их закрытый SourceDomain. Существующий способ получения данных Piano Roll
и его project/ticket lifecycle сохранён. Program key, pad→native note mapping
и routing catalog копируются на том же UI owner; никакие native pointers
в библиотеке/архиве не сохраняются. Это не сертификация прежнего native
reader и не доказательство полного writer coverage.

Parts — независимые снимки на момент Split, не live aliases изменяемых native
деревьев. Последующая запись в исходную секвенцию не переписывает старую
партию. При необходимости можно сделать новый Split. Исходная секвенция
доступна в прежнем виде; вставка библиотеки в неё — отдельное действие Paste.

## Как пользоваться

1. Открыть существующий Piano Roll из Events/Steps.
2. **Shift+B3 → Parts / Patterns**.
3. **B2 Actions** открывает список операций. DATA выбирает строку,
   B3/DATA click выполняет её; фейдер тоже выбирает строку.
4. Split by Pads создаёт партии по Program+pad. Split by Mixer использует
   снимок A–H/Master; неизвестная маршрутизация даёт отказ, без подстановки
   фиктивной шины. Manual создаёт Selected part и Unassigned по выделению
   исходного Piano Roll: два одинаковых события одного pad можно разнести.
5. Строка с **P** — партия; B3 Open открывает её в **том же** Piano Roll.
   DATA/фейдер, range selection, zoom, Move/Length/Velocity, Copy/Cut/Paste,
   Quantize и Insert используют существующий редактор и его plan API.
   Физические пэды выбирают строки; в этой библиотечной стадии они не
   запускают запись в скрытую native секвенцию.
6. Create Variation создаёт независимую копию с parent ID. Редактирование
   вариации не меняет оригинал. Shift−/Shift+ внутри библиотеки/паттерна —
   локальный Undo/Redo библиотеки (4 операции); в обычном Piano Roll
   сохраняется штатная история секвенции.
7. Extract range задаёт полуоткрытый диапазон [Start, End): DATA меняет Start,
   Shift+DATA — End, K1/K2 задают границы, B2 возвращает полный диапазон.
   Позиции показываются в ticks (960 на четверть), шаг — текущая сетка.
   Onset переносится относительно Start; gate за правой границей сохраняется.
   Начавшиеся до Start ноты не запускаются заново. Аудиохвост one-shot sample
   и seek внутри вокального сэмпла из MIDI gate не вычисляются.
8. Empty pattern создаёт пустой паттерн длиной текущей секвенции; более короткий
   пустой диапазон получается через Extract range. Insert и Paste работают
   сразу, включая выбор пустой строки пэдом.
9. **Copy to sequence** переносит события в общий clipboard и возвращает
   обычный Piano Roll. Поставить курсор, Actions → Paste. Только этот Paste
   меняет текущую native секвенцию через её прежний native history command.
   Paste использует текущую pad/program mapping и проверяет совместимость.
10. Сохранить проект обычным Save. Отдельного файлового диалога библиотеки нет.

B1 из Pad view паттерна возвращает в библиотеку, B1 из Tune/Chop — сначала
в Pad view. В библиотеке B1 возвращает в исходный Piano Roll. Delete pattern
требует подтверждения. New/Load закрывают текущий editor и сбрасывают локальную
историю; успешный Load восстанавливает сохранённую библиотеку.
UI owner привязывает операции к project epoch при открытии библиотеки;
проверка выполняется до подготовки и перед публикацией изменения. Старый
экран не может изменить библиотеку нового проекта даже при совпавших
индексах/revisions и повторном использовании общего empty state.

**Play сейчас воспроизводит штатную секвенцию.** Библиотека этапа 3 — редактор
и источник для Paste, а не второй транспорт. PAT/название и footer отличают
режим, native playhead/sequence mute overlays в нём не показываются. Запись
в паттерн/аранжировку, независимое воспроизведение, mute/solo дорожек и CHOP
блоков относятся к Horizontal Song Mode. В обычном Piano Roll Chоp/Tune и
запись продолжают работать как раньше.

## Хранение и ограничения

`amPatternsV1` — опциональная строка внутри штатного XPJ data object.
Compression, временные файлы и disk commit остаются штатными. Если библиотека
не использовалась, поле не добавляется. Старый проект без поля загружается
с пустой библиотекой. Неизвестная версия сохраняется непрозрачно и доступна
только для чтения; повреждённая AMP1/duplicate field отклоняется, без частичной
публикации библиотеки. Library commit выполняется после успешного outer Read.
Это не утверждение о полном rollback native Project при ошибке его загрузки.

AMP1 хранит ID/parent/revision, имена, part flag, Program key, pad/source-note,
снимок routing, tick, native musical flags, 37 известных inline tag-3 bytes
и provenance IDs. Integer fields имеют явное LE encoding; C++ layouts,
паддинг и native pointers не сериализуются. Hex envelope содержит checksum.
Обработчики сохраняют прежнюю цепочку Music/Mixer/Synth archive hooks.
Одна запись AMP1 использует один Program catalog; неизвестный multi-Program
catalog сохраняется непрозрачно, вместо неоднозначного назначения Sound rows.
Старые версии Am MOD/Stock не умеют сохранять новую библиотеку: её перенос
при пересохранении проекта в них этим расширением не гарантируется.

Начальные бюджеты: 32 library entries, 32768 событий суммарно, 8192 события
в открытом Piano Roll, 4 шага local history, 4 MiB binary payload. Превышение
лимита/нехватка памяти отменяет операцию целиком. Два режима Song и полные
канонические MIDI instrument bindings этим этапом не заменяются. Parts
копируют только ноты/sample triggers, представленные существующим Piano Roll;
automation и иные native event variants остаются в исходной секвенции.
Предел длины/gate соответствует текущему Piano Roll (960×128×64 ticks):
архив с большим диапазоном сохраняется read-only вместо передачи переполненных
значений в UI. Массовое удаление использует multiset счётчики и один проход
по событиям, без квадратичного сдвигания массива; проверено удаление и Undo
8192 событий, включая одинаковые.

Флаг будущей сборки: `AM_MOD_PATTERN_TOOLS_TRIAL=1` →
`MPC_PATTERN_TOOLS_TRIAL`. По умолчанию выключен; он независим от
`AM_MOD_PATTERN_SOURCE_TRIAL`. Ветку не выдавать за проверенную сборку MPC.

## Файлы и проверки

- `pattern_model.*`: validated restore поверх прежней модели.
- `pattern_library.*`, `pattern_codec.cc`: операции и bounded codec.
- `pattern_tools.*`: C bridge, immutable publication, history, XPJ hooks.
- `pattern_roll.inc`, `piano_roll_native.inc`: UI и существующий editor.
- `phone_native.c`: композиция Save/Load/New; `build_mixer_r27r.py`: opt-in.
- `verify_pattern_tools.py`: 4 host/ARM/sanitizer варианта библиотеки и 2
  native UI executables; 6 syntax-only phone/step конфигураций. Исходники
  фиксируются hashes в `build/pattern-tools/verification.json`.
- `verify_pattern_model.py`: регрессия прежних immutable parts/boundaries/limits.

Проверены сохранность native source tree, packed Tune/Chop с настоящим ARM
decoder/setter, вариации, edit/insert/delete/copy/paste, ручное разделение
дублей, длинные gates, неизвестные шины, старое/ошибочное/будущее extension,
архивный roundtrip, New/Load UI reset, 19 allocation failures и library limit
(после интеграции общего Store этапа 4b; см. `HORIZONTAL_SONG_STORE_RU.md`).
Native архивные frames/writer и GUI/command executor в UI-тесте — fixtures;
полный compressed XPJ file roundtrip приложения, реальная карта и физические
кнопки требуют будущей разрешённой сборки и проверки устройства.

`render_pattern_workflow.py` строит PNG/GIF из настоящих draw calls этого
QEMU UI-теста. Подменяется шрифт, не layout; это preview, не фото MPC.
