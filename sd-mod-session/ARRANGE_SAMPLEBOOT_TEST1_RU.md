# Кандидат Arrange Test 1 для SampleBoot

Ветка `codex/pad-midi-recording`, версия `r30d-arrange-test1`.
Сборка основана на проверенном release envelope r30d-beta4 и сохраняет
его исправления SampleBoot. `build_arrange_sampleboot.py` проверяет SHA
релизного архива и штатного ELF, соответствие таблицы хуков, собирает
библиотеку с отдельной версией и выполняет полный constructor + input routes.
Флаги Pattern Tools, Pad MIDI и native Synth включены. Native source reader,
VM probes и обходы записи выключены. Arrange Play/Record остаются закрыты.

`package_arrange_sampleboot.py` проверяет хеши исходников и результатов,
пересчитывает цепочку library → controller → entry → autorun.check и
использует существующий `Package` для проверки payload. Итоговая структура:
`autorun/mods/AM-Mod-Arrange-Test1/`, название `Am MOD Arrange Test 1`.
Корневые autorun.sh/config и `autorun/mods/AM-Mod/` не включаются.
Изолированная распаковка проверяет побайтовое сохранение загрузчика и всего
стабильного payload. На настоящую карту ничего не записывается.

## Кнопка питания

Сохранён штатный синхронный диалог. Constructor меняет только его вызов
виртуального диспетчера и расширяет принадлежащие ему options тремя кнопками:
B1 Cancel (ID1), B2 Bootloader (ID3), B3 Power off (ID2).
Нативные string slots создаёт и освобождает сама штатная функция; подписи
меняются через её string assignment. Указатели на диалог не сохраняются.
Перед установкой проверяются pinned prologue и BLR x2; обе записи находятся
в одной странице и выполняются до старта native main/audio.

B2 и B3 отменяют штатный немедленный shutdown, запрашивают Stop и используют
существующий `project_exit` с ожиданием durable project copy. Только после
успеха создаётся отдельный запрос `mod-off.request` либо `power-off.request`.
При ошибке устройство остаётся включённым; Retry сохраняет выбранное действие.
About показывает версию и репозиторий; пункт выхода и действие DATA убраны.
Song/Arrange показывает тот же экран сохранения/ошибки при выходе.

`verify_power_button.py`: AArch64/PAC, настоящий установленный BL bridge,
фикстура options, отдельные запросы и retry, host shutdown receiver.
`verify_power_native.py`: исходная функция Power из pinned ELF целиком,
native строки/деструкторы и три результата; GUI manager и save — фикстуры.
Это не подтверждает вид диалога и физические кнопки на устройстве.

## Навигация и подменю

Фейдер в Arrange и вертикальных списках: вверх (127) выбирает верхнюю строку,
вниз (0) — нижнюю. В Piano Roll сохранён рост pitch вверх; его строки
рисуются снизу вверх, поэтому инверсия музыкального значения не требуется.
Shift+фейдер Arrange: вверх увеличивает высоту строк (3), вниз показывает
больше строк (6). Направление энкодера/поворотных ручек не меняется.

Actions Arrange, выбор паттерна, библиотека Parts/Patterns и Actions
Piano Roll получили узкие scrollbar справа и полные окна по шесть строк
в конце списка. Текущая позиция показывается как N / Total; подсказки
внизу используют две строки и не перекрывают список. Timeline показывает
вертикальный scrollbar, при zoom out верхняя строка ограничивается так,
чтобы последние дорожки не оставляли лишнюю пустоту.

QEMU проверяет оба конца фейдера и промежуточные значения, отдельное
направление K1, шесть видимых пунктов у конца длинного меню, концы
scrollbar, bounds каждого draw primitive и возврат из редактора.
`render_arrange_menus.py` воспроизводит реальную трассу draw calls с
заменой шрифта. Старый полный `verify_pad_eq.py` остановился на устаревшем
названии первого таба Pad lighting; он не заявляется прошедшим. Текущий
About проверен отдельно `verify_about_power.py` в normal/UBSan.

## Совместное владение входом Engine

Первый full constructor выявил пересечение Synth hook с существующим
phone mute/preview gate и offline Render. Synth теперь устанавливается
после fast renderer. Renderer публикует только проверяемый адрес уже
установленного dispatch relay для холодной композиции. Synth проверяет
entry и этот relay; bare ELF и точный r30d phone gate поддерживаются отдельно.
При несовпадении установка закрывается, проверки не отключаются.

В расширенном варианте заменяется лишь первая инструкция Engine, native+8
остаётся нетронутым. Original trampoline подписывает LR и входит в прежнюю
цепочку Render → mute/preview → native. Это сохраняет ранние возвраты и
исходное выделение стека. `verify_synth_live_native.py` проверяет bare,
phone-gate и fast+phone-gate в normal/UBSan: семь реальных Engine dispatch,
три одновременные ноты, Synth PCM, хвосты, ratchets и Track cancellation,
отказ повреждённого gate и muted/preview veto.

Полный GUI boot, фактический XPJ save/load из приложения, sampler audio/FX и
аппаратный тест ещё нужны. Установку выполнять на копии проекта: Projects
общая между модами, стабильный мод может не сохранить новые sidecar-поля XPJ.
