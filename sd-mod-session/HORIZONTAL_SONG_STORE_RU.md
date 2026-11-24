# Stage 4b — общий Store и архив горизонтальной аранжировки

Реализовано в ветке `codex/pad-midi-recording`, поверх модели этапа 4a.
Это слой данных и архивные hooks для будущего экрана Arrange. Сам экран,
переключение Sequential/Arrange и native playback пока не подключены.
Приложение, preload и установочный пакет не собирались.

## Совместное состояние

`pattern_tools.cc` хранит immutable State: Library + Arrangement. Изменение
готовится на копии вне metadata mutex, затем проверяются ссылки и версия
проекта. Публикация целиком заменяет прежний State. Четыре шага Undo/Redo
общие для библиотеки и блоков: нельзя восстановить блок без его паттерна.
Попытка удалить используемый Pattern отклоняется, не меняя состояние/историю.
Неиспользуемый Pattern можно удалить обычной операцией библиотеки.

C bridge в `pattern_tools.h` предоставляет собственные значения строк и
блоков, разрешение стабильного Pattern ID в индекс библиотеки, создание,
перемещение, resize, cut/join, удаление блока, mute/solo строки и длину песни.
Эти операции ещё не назначены кнопкам. Будущий Chop вызывает `pt_arr_cut`
на курсоре; открытие блока должно использовать существующий Piano Roll.

`pt_prepare_song` готовит bounded поток модели и проверяет project epoch
и identity Store перед выдачей результата. Ошибка сохраняет прежний output.
`pt_prepared_current` проверяет epoch/revisions под mutex. Это не native
Project/Program lease; такой lease нужен будущему playback adapter отдельно.
New и успешный Load сбрасывают историю, меняют epoch; старый UI-контекст
не может редактировать или подготовить события нового проекта.

## Сохранение и загрузка

Используется существующая цепочка Music Save/Load/Read hooks, дополненная
необязательным строковым полем `amArrangementV1` рядом с `amPatternsV1`.
Native sequence data и список Sequential Song не преобразуются.
Arrangement хранит ссылки на Pattern IDs, а не копии нот на каждый повтор.

Codec `arrangement_codec.cc`: AAR1, явные little-endian целые, hex envelope,
FNV32 checksum, лимит строки 65536 байт. Пишутся ID/revision/length, строки,
блоки/offset/mute. Сырые native structs и указатели не сохраняются.
Проверяются лимиты 32 строки/512 блоков, длины, пересечения и ссылки.

Save берёт единый snapshot и готовит оба payload до вызова native writer.
Load сначала собирает оба поля независимо от порядка, затем декодирует
Library и проверяет Arrangement против неё. Общий State публикуется только
при успешном завершении внешнего Read. Повреждение, дубликаты или висячие
Pattern IDs не публикуют частичное расширение. Это гарантия расширения;
полный rollback native loader здесь не реализуется и не заявляется.

Старый проект без новых полей получает пустые Library/Arrangement. Пока
Arrangement не изменён, его поле не добавляется. Неизвестная версия одного
из расширений сохраняется opaque, а редактирование обоих блокируется:
неизвестные ссылки нельзя повредить изменением известной библиотеки.
Старые сборки Am MOD/штатное ПО могут отбросить незнакомые поля при resave;
сохранение расширений гарантируется только этим новым кодом.

## Проверки и границы результата

`verify_pattern_tools.py`: четыре core executable (host, ASan/UBSan, ARM
QEMU, ARM UBSan), два native UI regression executable и шесть syntax-only
конфигураций. Core требует отдельную строку `PASS arrangement store:`.
Проверены общая история, удаление используемого Pattern, cut/join, C value
readers, invalid index, stale epoch, порядок полей, отказ внешнего Read,
повреждённые/висячие ссылки, будущий формат и New/reset. Прежние тесты
библиотеки включают 19 точек allocation failure без изменения старых данных.

`verify_arrangement.py`: четыре isolated варианта модели и codec, включая
все усечения payload и сохранность output при ошибке декодирования.
Proof/hashes/logs находятся в `build/pattern-tools/verification.json` и
`build/arrangement/verification.json`.

Архив проверен через native frame/writer fixtures. Полный compressed XPJ
roundtrip приложения, реальное аудио/MIDI и устройство требуют дальнейшей
проверки. Код включён только в opt-in `AM_MOD_PATTERN_TOOLS_TRIAL=1`.

Далее: экран Arrange и переключатель режимов Song, операции блоков с
encoder/fader и Chop, переход в существующий pattern editor. После этого —
native playback adapter с проверкой Program/pad mapping, длины Sequence,
voice ownership и Stop/tails; затем export/mixdown и аппаратные сценарии.
