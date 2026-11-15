# r29za

- Меню Project (Shift+Pad16): штатный пункт «SD Card Access» убран в обоих режимах (доступ к карте по USB не работает внутри изолированной копии приложения, см. README). Пункты: Load, Save, New, Key / Scale / Tempo, Mod Settings, Screen Mirror.
- Mod Settings: строки «Record via / Record track / Record FX» в режиме Stock некликабельны (работают только в Am MOD), подсказка это объясняет.
- Диагностика: строки `RESAMPLE_MIX HEAD` в консольном логе показывают первые блоки захвата при ресемплинге сэмпла в Am MOD (пик по блокам) для поиска причины потери начала звука пэда.
