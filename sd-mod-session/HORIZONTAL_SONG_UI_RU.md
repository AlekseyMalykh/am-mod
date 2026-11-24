# Stage 4c — первый экран Arrange в SongScreen

Экспериментальный редактор блоков подключён к существующему SongScreen под
`AM_MOD_PATTERN_TOOLS_TRIAL=1`. По умолчанию флаг выключен. Используются
штатные графические функции, палитра и размер 320×240; новый clock, второй
Piano Roll и параллельные native Sequence не создаются.

## Как открыть и редактировать

1. Подготовить Parts/Patterns через существующий Piano Roll:
   Shift+B3 Actions → Parts / Patterns → B2 Actions → Split/Empty/Extract.
2. Остановить транспорт. В Song нажать B1 Export, повернуть DATA до
   Horizontal Arrange, нажать B3 Open. Это дополнительный пункт локального
   меню; прежние Export, Shift+B1, Clear и Duplicate сохраняют свои жесты.
3. B2 Actions → Add track. Затем Insert pattern → выбрать паттерн → B3.
   Создаётся блок длиной паттерна на текущей строке/курсоре.
4. DATA двигает курсор по сетке, фейдер выбирает строку, +/- меняют строку.
   Shift+DATA меняет горизонтальный масштаб, Shift+фейдер — число видимых
   строк. K1 прокручивает строки вместе с выбором, K2 задаёт сетку,
   K3 прокручивает время. Шкала пока в четвертях (`q`, 960 ticks), а не
   в тактах: отображение переменного размера native Sequence ещё впереди.
5. B3 Pin block закрепляет блок под курсором. После этого курсор и строку
   можно менять независимо от выбранного блока. B3 Unpin снимает закрепление.
6. Chop режет выбранный блок строго внутри на курсоре; правая часть
   становится выбранной. Pattern не разрезается физически, gate сохраняется.
   На границе или вне блока операция отклоняется. Join next block соединяет
   соседние части одного паттерна с совместимой фазой.
7. Actions: Copy/Paste, Move selected here, Extend one cycle, Trim end to
   cursor, Mute/Solo track, Delete block с подтверждением, Song +/-16 beats.
   На одной строке блоки не перекрываются: при отказе данные не меняются.
   Для Move сначала закрепить исходный блок, затем выбрать место назначения.
8. Shift+-/+ — общая история Store. B1 Sequential возвращает прежний Song.

Copy запоминает значения блока (Pattern ID, length, offset, mute), а не
указатель на него. Paste после удаления/разреза исходного блока сохраняет
исходную скопированную область. Ноты остаются в библиотеке в единственном
экземпляре. Если сам паттерн удалён, вставка отклоняется.

New/Load закрывают редактор по project epoch и очищают локальный clipboard.
Очередь Song несёт числовой input epoch и arrival-time Shift. После смены
режима устаревшие события поглощаются, matching release освобождает ownership;
K2/K3 не перенаправляются в фоновые native FX при выходе из Arrange.
Неизвестный будущий формат отображается read-only; Store отклоняет запись.
Данные редактора используют архив и историю этапа 4b.

## Что пока недоступно

Play/Record в Arrange перехватываются с сообщением о неподключённом playback;
Stop остаётся штатным. Редактор блоков пока не воспроизводит аранжировку и
не записывает новые takes. Пэды на временной шкале Song сохраняют штатный
выбор Sequence; внутри редактора Pattern выбирают его строки без прослушивания.
Имена строк пока автоматически Track 1, Track 2; переименование впереди.

B3 закрепляет блок; Shift+B3 открывает его ноты. Прямой переход в тот же
Piano Roll реализован в 4d: `PATTERN_HOST_STAGE4_RU.md`. B1 Arrange возвращает
временную шкалу с прежним контекстом. Можно также открыть Pattern через
Events → Piano Roll → Parts / Patterns: это один материал с тем же ID.

Waveforms/render-to-audio, automation lanes, Arrange export/mixdown, native
playback с voice ownership и запись в этом экране остаются дальнейшей работой.

## Проверки и визуализация

`verify_arrangement_ui.py` запускает ARM QEMU normal/UBSan для production
overlay и полную прежнюю Sequential/Export suite внутри него, а также две
отдельные baseline-конфигурации без trial. Проверяются действия контролов,
Chop/join, copy/paste, недопустимое наложение, история, mute/solo, Delete
confirm, остановленный вход, перехват Play/Record, reset и парные release.
Native Song steps сравниваются с исходным снимком после операций Arrange.

Сценарий использует настоящий pinned SongScreen vtable hook, а графику,
native Song step builders, контекст транспорта и проект — fixtures. Это
не запуск полного приложения. Proof: `build/arrangement-ui/verification.json`.
Регрессия Store/Piano Roll: `verify_pattern_tools.py`, без сборки приложения.

`render_arrangement_ui.py` строит PNG и `build/arrangement-ui/workflow.gif`
из production draw calls QEMU-теста: пустой экран → вставка 8 четвертей →
курсор после 3 → Chop на части 3+5 → явный Join до 8 → копия склеенного
блока на другой строке → zoom/scroll. Проверяются обе длины, их сумма,
offset правой части и неизменность исходных нот/длины Pattern. Строка статуса
различает длины Block и Pattern. У GIF есть пояснения вне экрана устройства;
отдельные PNG сохраняют native layout без подписей. Подменён шрифт. Это превью реального
layout теста, а не фотография устройства. Аппаратная проверка ещё впереди.
