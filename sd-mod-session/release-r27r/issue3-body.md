## Known issues / Известные проблемы

**Updated / обновлено: 6 November 2026.** Status index; use individual reports for reproducible bugs.

**Latest / последний релиз: [Am MOD r27r](https://github.com/AlekseyMalykh/am-mod/releases/tag/r27r).** Recall no longer freezes the MPC; Chop, Copy, Mute, Erase and Pad Bank lighting follow one set of rules; Seq Colour, Mix, Scaled takeover and autoload screen fixed; search gains Internal and All; Lights is now Scales.

**r27r:** Recall больше не вешает MPC; подсветка Chop, Copy, Mute, Erase и Pad Bank по единым правилам; исправлены Seq Colour, Mix, Scaled и экран после автозагрузки; в поиске есть Internal и All; Lights теперь Scales.

- [Known issues — English](https://github.com/AlekseyMalykh/am-mod/blob/main/docs/KNOWN_ISSUES.md)
- [Известные проблемы — русский](https://github.com/AlekseyMalykh/am-mod/blob/main/docs/KNOWN_ISSUES.ru.md)
- [Report a bug / Сообщить о баге](https://github.com/AlekseyMalykh/am-mod/issues/new?template=bug_report.yml)
- [Feature request / Запрос функции](https://github.com/AlekseyMalykh/am-mod/issues/new?template=feature_request.yml)
- [Questions and help / Вопросы](https://github.com/AlekseyMalykh/am-mod/discussions/categories/q-a)
- [Testing feedback / Результаты тестирования](https://github.com/AlekseyMalykh/am-mod/discussions/categories/general)

### Fixed in r27r / Исправлено в r27r

- ✅ BUG-07 Recall freeze / зависание Recall — confirmed on MPC
- ✅ BUG-02 Seq Colour (Shift+K1) — confirmed
- ✅ BUG-03 Mix encoder cycle / цикл энкодера Mix — confirmed
- ✅ BUG-04 / BUG-05 Chop colours after Extract and after leaving / подсветка Chop — confirmed
- ✅ BUG-06 screen after autoload / экран после автозагрузки — confirmed
- ✅ BUG-01 Scaled takeover after EQ → Filter
- ✅ Copy source / targets, Mute, Erase, Pad Bank and selected-sequence lighting / подсветка — confirmed
- ✅ Search Internal / All, SD results, K3 Volume, Internal / External tab / поиск — confirmed

### Still open / Открыто

- Sample / Seq do not leave 16 Levels or Chop / Sample и Seq не выводят из 16 Levels и Chop.
- Wider testing: Copy Sequence colour transfer, leaving windows with Sample / Seq / нужна проверка: перенос цвета при Copy Sequence, выход из окон кнопками Sample / Seq.
- Physical FAT32 confirmation / проверка FAT32 на устройстве.

Special thanks to **Leusha** for careful, detailed testing — almost every r27r fix started from those reports. Thanks also to mxt1iga and everyone helping. / Особая благодарность **Leusha** за внимательное тестирование!
