# r27q — что проверено и как собрать

Ветка `claude/ui-polish-r27q` от r27p (`c07cbfc`). Сделано без кросс-компилятора, поэтому
проверено только на ПК.

## Проверено здесь

- Все изменённые `src/*.c` и `src/*.cc` компилируются host gcc/g++ с флагами сборки и `-Werror` (`-fsyntax-only`).
- `check_music_lights.c` (ASan+UBSan): раскладки Monochrome/Piano/Scale/Degrees, главные ступени I/IV/V
  для всех восьми ладов, отдельный цвет V, погашенные ноты вне лада, сохранение ID 4, формат MPCS18 только при изменённом цвете V.
- `check_branding.c` (ASan+UBSan): сглаженный текст версии и адреса только в нижних полосах, логотип и поля строк не тронуты.
- `lab/ui_preview/preview.py`: PNG-превью настоящего кода отрисовки (Search, Rear Routing, Mod Settings,
  панель Lights). Нативные вызовы заменены записью прямоугольников и текста; шрифт приблизительный.

## Нужно запустить в WSL

```sh
cd sd-mod-session
python3 build_chords_candidate.py          # phone-input.so + проверка preload
python3 verify_branding.py && python3 verify_early_drm.py && python3 verify_early_startup.py
python3 verify_resample_flow.py && python3 verify_resample_native.py   # изменены тексты ошибок
python3 verify_steps.py && python3 verify_r17a.py                      # check_music_lights на ARM
```

Пакет: `package_palette_recovery.py` закреплён на хешах r27h–r27n; для r27q нужно добавить
ревизию `r27q` с базой `r27p` (как делалось для r27p), затем проверка в VM и на MPC.

## Что смотреть на MPC

1. Sample → Tune → Play → Lights: K1 Layout → Degrees; Shift+K2/K3 — группы I Tonic / IV / V / Key notes / Original; изменить цвет V, сохранить, открыть заново.
2. Sample Select → Shift+B3: живые совпадения, Shift+Erase, повторное открытие с прежним запросом, список из 5 строк.
3. Sample Record → B2 Routing и Input Config → Shift+B2: новое окно, Shift меняет панели K2/K3 на Level.
4. Project → Mod Settings: список, подписи соседних вкладок, Restore recommended и рамка подтверждения.
5. Заставка при запуске: «Am MOD» одной высоты, версия r27q.
