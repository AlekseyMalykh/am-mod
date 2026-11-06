**Fixed in [Am MOD r27r](https://github.com/AlekseyMalykh/am-mod/releases/tag/r27r)** — the reports from Leusha's testing:

| Report | Status |
|---|---|
| BUG-07 Recall (Shift + Sample Record) froze the MPC | **fixed**, confirmed on MPC (Resample and Rear sources) |
| BUG-02 Seq Colour (Shift+K1) kept the old controls | **fixed**, confirmed on MPC |
| BUG-03 Mix encoder cycle after K2 Pad Colour | **fixed**, confirmed; Shift keeps Volume / Pan, only K2 is Kit Volume |
| BUG-04 / BUG-05 Chop colours after Extract, dim pads after leaving Chop | **fixed**, confirmed: Chop stays lit in the source colour |
| BUG-06 incomplete screen after autoload until a pad press | **fixed**, confirmed; the “Restored” note repaints cleanly |
| BUG-01 Scaled takeover jump after EQ → Filter | **fixed** |
| Copy: source pad not lit, targets blinking | **fixed**, confirmed: source blinks, free targets green, occupied red |
| Mute: everything green, hard to see playing pads | **changed**, confirmed: own colours brighter, muted pads red |
| Erase: colours lost while held | **changed**, confirmed: colours kept |
| Pad Bank green flash, selected sequence hard to see | **fixed**, confirmed: no flash; selected sequence blinks white |
| Lights → Scales rename | **done** |
| Search: Internal missing, few SD results, flickering K3 Volume, missing tab dots, Knob FX over the browser | **fixed**, confirmed |
| Filter spectrum paler than EQ | **changed**: same colour as EQ |
| First Chop light on the first bar (L06) | looks fixed; please report if you still see it |

Still open: Sample / Seq do not leave 16 Levels or Chop yet; Copy Sequence colour transfer and leaving windows with Sample / Seq need wider testing. Details: [known issues](https://github.com/AlekseyMalykh/am-mod/blob/main/docs/KNOWN_ISSUES.md).

**Huge thanks to Leusha** for careful, patient testing and precise reports — almost every fix in r27r started from them.

---

**Исправлено в r27r** по отчётам Leusha: зависание Recall (BUG-07), Seq Colour (BUG-02), цикл энкодера в Mix (BUG-03), подсветка Chop после Extract и после выхода (BUG-04/05), экран после автозагрузки (BUG-06), скачок Scaled после EQ (BUG-01) — подтверждено на MPC. Подсветка Copy, Mute, Erase, Pad Bank и выбранной секвенции приведена к единым правилам; Lights переименована в Scales; поиск получил Internal и All. Открыто: выход кнопками Sample / Seq из 16 Levels и Chop.

**Огромное спасибо Leusha** за внимательное, терпеливое тестирование и точные описания — почти каждое исправление r27r началось с них.
