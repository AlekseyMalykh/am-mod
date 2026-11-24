# Stage 4d — открытие Pattern из Arrange в существующем Piano Roll

История первоначального handoff. Новые кнопки, прямое создание и variation
из Arrange описаны в [ARRANGE_DIRECT_WORKFLOW_RU.md](ARRANGE_DIRECT_WORKFLOW_RU.md).

Реализовано под прежним opt-in `AM_MOD_PATTERN_TOOLS_TRIAL=1`. Приложение,
preload и пакет не собирались. Это редактор значений паттерна; native
воспроизведение/запись аранжировки и аппаратная проверка остаются впереди.

## Workflow

- В Arrange поставить курсор в блок или закрепить блок B3.
- Shift+B3 открывает его Pattern. Есть также Actions → Edit pattern.
- При входе курсор переводится из времени песни в source phase паттерна:
  `(clip.offset + relative_cursor) mod pattern.length`. У разрезанного блока
  справа сохраняется offset. Если курсор вне закреплённого блока, открывается
  его начальная source phase.
- Открывается тот же `piano_roll_native.inc`, с прежними слоями Pad/Tune/Chop,
  DATA, фейдером, диапазонами и Shift+B3 Actions. DATA press/release выбирает
  ноту или завершает выделение области. Физические пэды выбирают строки;
  Bank листает локальные банки. Они не запускают звук и не пишут события.
- Редактирование, Copy/Paste, Transpose через Tune, Quantize, изменение
  времени/длительности/velocity и ручной Insert используют существующие
  `pr_ops`, с commit в Pattern Store. Shift+-/+ — общая история Store.
- B1 последовательно отменяет preview/закрывает Actions/возвращает Pad view;
  из Pad view B1 Arrange возвращает временную шкалу. Курсор, масштаб, строка
  и выбранный блок сохраняются, если этот блок ещё существует.
- Редактируется общий Pattern ID: изменения видны во всех его блоках.
  Сообщение при входе это поясняет. Для независимой вариации пока нужно
  создать её через Parts / Patterns и вставить отдельным блоком.
  После Chop открывается весь исходный Pattern с курсором в нужной фазе;
  граница блока не превращается в новую длину Pattern. Сам блок сохраняет
  свою отдельную length/offset, как показано в исправленном GIF этапа 4c.

Chop зависит от контекста: на временной шкале режет блок, внутри Piano Roll
переключает существующий слой Chop. Play/Record в новом host перехватываются:
Arrange playback/record ещё не подключены. Stop остаётся штатным.
Переход из внешнего host в Parts/Patterns или native MIDI import/export
заблокирован: эти операции пока требуют Events host. Ручной Insert работает.
Подсветка пэдов нового Song host ещё не доведена до поведения Events.

## Граница native-контекста

`pattern_host.inc` — UI-owner facade над единственным существующим `roll`.
Ни Events controller, ни native Project/Program, ни EventList в него не
передаются. Он получает стабильный Pattern ID, числовую фазу и callback
перерисовки без объекта. Чтение/commit используют immutable Store. При
изменении индексов Library ID разрешается заново; исчезновение ID или смена
project epoch закрывает редактор.

Из путей Pattern убраны скрытые native reads: sequence mutes/time signature,
pad mapping при ручном Insert, определение pad при preview и native sample
chop count. Пэды/ноты разрешаются по сохранённому каталогу Pattern. Шкала
паттерна пока использует существующий fallback 4/4; переменный метр предстоит
хранить/показывать отдельно. Inline note factory/choice getter/setter остаются
штатными, без доступа к native инструменту или аудиодвижку.

UI Timer не закрывает внешний host из-за скрытого Events. Перерисовка идёт
через Song callback. Внешний host не публикует обычный Events input ticket.
Song публикует числовое владение кнопками/пэдами; события FIFO несут input
epoch и arrival-time Shift. Вход/выход меняют epoch, устаревший ввод поглощается.
New/Load/закрытие Song закрывают host и снимают callback. Захваченные отпускания
кнопок и пэдов продолжают поглощаться после закрытия.

Пэды Pattern — только выбор значений: native Note On не отправлялся, поэтому
их matching up не требует отправки Note Off. Ранее пропущенный в native down
по-прежнему получает native up через прежний `passed` путь. Новая ветка не
перехватывает уже начатый native голос. При переполнении FIFO используется
существующий queue fault/закрытие редактора.

## Файлы и проверки

- `pattern_host.inc`, `piano_roll_native.inc`, `step_native.c/.h` — один editor,
  внешний host, изоляция native зависимостей и lifecycle.
- `pattern_tools.cc/.h` — value-only reverse pad lookup; неоднозначная
  note → pad mapping не угадывается.
- `arrangement_ui.inc`, `song_ui.c/.h` — вход, phase, маршрутизация и возврат.
- `phone_native.c`, `phone_events.h` — доставка пэдов в UI FIFO и парный swallow.

`verify_pattern_tools.py` проверяет реальный существующий editor на ARM/QEMU
с native inline factory/packed choices. В тесте native root/controller/
Sequence/EventList заменены недействительным адресом `1`. Проверены paint
Pad/Tune, preview и commit перемещения/высоты/длины, Undo/Redo, ручной Insert,
DATA release, выбор пэда без голоса, отказ выхода в native library actions,
сдвиг индекса Library при сохранении ID и закрытие после reset. Исходное
native дерево событий сравнивается с прежним снимком.

`verify_arrangement_ui.py` отдельно проверяет Song handoff через явный fixture
редактора: ID/phase разрезанной части, контролы и оба DATA edge, пэды, возврат
в тот же контекст, stale input tickets и New/reset. Это два связанных набора
проверок, а не полный запуск приложения или реальный roundtrip на устройстве.
Оба набора сохраняют прежние регрессии Sequential/Export и Piano Roll.
Дополнительно `verify_piano_roll.py --native-only --stock <ELF>` проверяет
штатный native Piano Roll без trial: normal и UBSan. Скрипт не собирает
приложение; только два отдельных test executables.

Proof: `build/pattern-tools/verification.json`,
`build/arrangement-ui/verification.json`, `build/piano-roll/verification.json`.
Визуализация самого внешнего host строится `render_pattern_host.py` из
production draw calls QEMU-теста.

Далее: native playback adapter подготовленного общего потока, проверка
Program/pad mapping и native длины, voice ownership/Stop/tails, запись в
Arrange и Pattern. Затем export/mixdown и проверка физического MPC после
отдельно разрешённой сборки.
