# Подготовка воспроизведения Synth через существующий секвенсор

Исторический checkpoint `4d4ca42`. Следующий изолированный этап — [привязка проекта и native playback trial](SYNTH_NATIVE_PLAYBACK_TRIAL_RU.md); ограничения ниже относятся к состоянию ядра до этого подключения.

Продолжение после контрольного коммита `1f58658` в `codex/pad-midi-recording`. По указанию пользователя установочный пакет и полный preload этого этапа не собирались. Выполнены только изолированные проверки модулей под AArch64 user-QEMU, включая UBSan; проверка полной программы из предыдущего этапа не распространяется на этот код автоматически.

## Реализовано

В `synth.c/.h`, под прежним opt-in флагом, появился отдельный API владения голосом: `am_synth_owned_on/off/reset`. Каждая принятая нота получает неповторяемый runtime ID и поколение. Off завершает именно этот голос. Две ноты одной высоты могут иметь разные velocity и независимые отпускания. Поздний Off украденного голоса не завершает его замену. Обычный pad/MIDI Off и его аварийный путь при переполнении не затрагивают owned-голоса; явные CC120/123 по-прежнему означают общий all-notes-off.

On использует ту же ограниченную MPSC очередь и тот же Synth DSP. Очередь передаёт скопированный value packet, не указатель на native событие или UI. Отказ On не меняет выходной handle. Отказ Off требует повторения, сохраняя владение. Stop/reset меняет поколение вне очереди: уже стоящие в ней старые On не оживут после Stop, в том числе за временно незавершённым producer slot. ID и поколения не оборачиваются; исчерпание счётчика даёт явный отказ. Audio consumer по-прежнему не блокируется и обрабатывает не более 64 команд за buffer.

`synth_sequence.c/.h` — приёмник уже принятых штатным движком событий, а не второй секвенсор. У него нет собственной временной шкалы, event pool, длительностных таймеров или повторного расчёта probability/Note Repeat. Маленький токен голоса предназначен для размещения в уже существующем `live_note` из `midi_live.inc`. Носитель A16, открытый экран и текущий выбранный пэд не используются этим модулем для определения инструмента.

Приёмник проверяет source epoch, transport epoch и occurrence ID, преобразует Tune относительно MIDI60 с явным transpose, передаёт captured velocity и принимает конкретный Off. Transport epoch фиксируется при принятии native occurrence: подстановка текущего epoch в отложенный callback запрещена. Stop инвалидирует старый transport, Load/rebind — source epoch. Повторное использование памяти `live_note` не даёт старому Off завершить новое событие. Токен занятого occurrence не перезаписывается новым On; при отказе очереди сохраняется состояние для retry. Объект и токены принадлежат одному native music executor.

Runtime IDs, transport epochs, токены и состояние голосов **не сохраняются в XPJ**. Формат native событий и сохранения текущих проектов на этом этапе не изменён. Прежний Pad-to-MIDI worker пока продолжает использовать свой проверенный live-путь; новый API не активирован автоматически.

## Проверки

`verify_synth_queue.py` выполняет normal/UBSan для трёх наборов:

- MPSC: четыре producer × 20 000 составных команд, все поля packet, порядок и однократная доставка, полный buffer, резерв Off, неопубликованный slot и wrap queue counters.
- Owned Synth: независимые унисоны и velocity, duplicate/stale Off, voice stealing, отказ и повтор Off, reset при полном buffer, legacy isolation, глобальный CC123, незавершённый producer, исчерпание IDs, конечный PCM и затухание после освобождения.
- Sequence sink: наложенные ноты, новый occurrence следующего loop, переиспользованный native токен, отсутствие второго probability/ratchet решения и локального таймера, transpose, mute/Slice rejection, retry, Stop, отложенный ранее не доставленный On старого transport и смена source epoch.

Все шесть прогонов прошли. Также прошли прежние шесть normal/UBSan проверок `verify_piano_synth_input.py`: обычный Piano input, тот же input с trial-флагом и существующий live Synth worker. Это модульные fixtures с настоящими native очередями / Synth DSP, а не доказательство включённого playback в полной программе.

Доказательства: `build/synth-queue/verification.json` и `build/piano-synth-input/verification.json`, с source hashes и отдельными логами.

## Следующее подключение

Исследованные точки: `midi_native.c:event`, `midi_live.inc:live_on`, `live_release_note`, `live_scheduled_release`, а также транзакционный `mixer_archive.cc` и `music_archive.c`.

Важное ограничение: `live_start` вызывается **до** `original(track,...)`; сам факт этого callback не доказывает принятие ноты после native probability/filter/ratchet. Новый sink нельзя вызывать оттуда безусловно. `live_on` наблюдает принятый MIDI command, но появляется только при фактической MIDI-маршрутизации. Перед активацией требуется проверить принятую native границу для Synth без зависимости от наличия внешнего MIDI-порта. Длительности должен завершать due-heap release с захваченным occurrence, а не UI timer.

Также нужно проверить единицы и фазу native `offset` относительно Synth audio callback. Текущая Synth очередь применяется на границе buffer; её нельзя объявлять sample-accurate native playback без отдельной проверки графа.

После определения этих точек новый token встраивается в существующий `live_note`, сохраняется до успешного Off, а штатные Stop/New/Load вызывают reset на том же executor. Затем добавляется optional versioned Synth extension по образцу Mixer: атомарный load commit, сохранение неизвестного payload без перезаписи, отсутствие поля у legacy-проектов. Оно должно хранить назначение инструмента, carrier и preset, а не runtime pointers. Устойчивая native track identity и её поведение при rename/Program replacement ещё требуют проверки; pointer, номер MIDI-канала и mixer bus не заменяют такую identity.

Автоматическое проигрывание сохранённой партии и сохранение Synth binding пока **не включены**. Horizontal Song Mode и CHOP на timeline этим этапом не активируются.
