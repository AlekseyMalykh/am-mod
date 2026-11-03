# Mixer model V1 — независимый core

03.11.2026; `feature/mixer-song-stems`.
Код: [`mixer_model.h`](../sd-mod-session/src/mixer_model.h),
[`mixer_model.c`](../sd-mod-session/src/mixer_model.c).

Модель и codec реализованы, собраны и проверены на host/ARM. Они пока **не
подключены** к native archive, audio routing или UI. Наличие encode/decode не
означает проверенный Save/Load на MPC. Незавершённая функция не включена в
производственный build/constructor и не попадает в release artifacts.

## Владение и данные

`mixer_project` — обычный объект одного control owner. Нет глобального project,
Component/native pointers, audio buffers, locks или зависитимости от вкладок.
Его нельзя менять из audio callback; будущая RT-интеграция должна публиковать
подготовленные snapshots отдельно.

| Поле | Представление / политика |
|---|---|
| Track 1–8 + Master | `mixer_channel`: bounded UTF-8 name (до 31 байта), RGB24, `mixer_mix`, FX bypass и существующий `fx_chain_config` |
| Level | float dB, -60…+6; `-INFINITY` означает Off. Положительная infinity и NaN запрещены. Это диапазон собственного Mixer V1, не переназначение штатного Master/fader range |
| Pan / Send | -1…1 / 0…1. Send сохраняется для будущего DSP; наличие поля не делает его активным контролом |
| Mute / Solo | отдельные boolean. Master Mute/Solo запрещены валидацией |
| FX | До 5 slots; native id + все 32 параметра, либо полный EQ с до 8 bands. Chain bypass и stage bypass сохраняются отдельно |
| Pad Output | Ровно одна таблица из 128 элементов, 0 = Master Direct, 1…8 = Track; индекс `bank*16+pad`. Нет Sequence id, имени Sample или UI-таблицы маршрутов |
| Mix Scenes | До 128 optional записей по стабильному ненулевому 64-bit Song Part id; отдельная маска присутствующих Level/Pan/Send/Mute/Solo для каждого Track |

RAM объекта V1 в проверенных host/ARM сборках — **42032 байта**. Это ёмкость
модели с 128 сценами, не измерение общей RAM мода. Новая сцена сверх лимита
возвращает `MIXER_FULL`, ничего не вытесняет и не обрезает Song. Реальный максимум
native Song Parts ещё не установлен; ограничен только этот optional scene store.

Стабильный Song Part id должен назначить будущий Song adapter. Он обязан
сопровождать перемещение/вставку/удаление Part и сохраняться в extension; текущий
индекс строки или Sequence id не подходят. Сам native Part identity пока не
расшифрован. Две части с одной Sequence получают разные ids/Scenes.

## Scene и групповой уровень

`mixer_scene_resolve()` каждый раз начинает с **обычного проектного микса**,
затем накладывает только присутствующие поля текущей Part. Следующая часть без
Scene не наследует Mute/Level предыдущей. Сам project, Pad Output, Sequence и
выбор трека не меняются. FX/sidechain automation в Scene V1 отсутствуют: новые
field bits требуют новой совместимой версии, а не скрытого повторного смысла.

`mixer_shift_levels()` применяет общий delta в dB к конечным уровням выбранных
Track. Delta ограничивается пересечением диапазонов участников, поэтому не
теряется баланс при достижении потолка/пола. Off остаётся Off; полностью Off
группа требует одиночного выбора для поднятия. Master в track bitmask не входит.
Это только арифметика после pickup: физический Fader, anchors и soft takeover
реализуются отдельным controller, не codec.

## Формат optional payload

Планируемое отдельное native JSON field — `amMixerV1` со строковым payload.
Этот ключ пока не добавлен в [`music_archive.c`](../sd-mod-session/src/music_archive.c).
Существующее `mpcSampleModV1` и штатные поля не изменены.

```text
"AMMX:" + hex(binary)
binary = version:u16 + reserved:u16 + payload_length:u32 + payload + crc32:u32
```

Все integers — little-endian; float/double — явные IEEE binary32/binary64 bits.
CRC32 (стандартный polynomial 0xedb88320) покрывает header+payload. Hex выбран
для простой передачи через существующий string archive без стороннего parser
и двоичных символов. Максимум строки с NUL — 65536 байт. Наибольший разрешённый
V1 проект (9 имён по 31 байту, 45 EQ8 stages, 128 полных Scenes) занимает
**53793 байта без NUL**; проверен тестом.

Порядок payload: 8 Track channels → Master channel → 128 Outputs → число Scenes
u16 → активные Scenes. Channel содержит length-prefixed name, RGB, mix values,
FX bypass и chain. Stage сериализует только активный kind: native id/32 значения
либо EQ count/bypass/output и bands. Scene — part id, затем для каждого Track
field mask и только присутствующие значения. Пустые slots, struct padding,
неиспользуемые поля Scene и native/runtime handles в формат не попадают.

`mixer_encode(NULL buffer, 0)` возвращает точный требуемый размер; короткий
buffer или невалидный project не меняют выход. `mixer_decode()` полностью
проверяет envelope/length/CRC/диапазоны/UTF-8/уникальность ids в локальном объекте
и только затем заменяет результат. Отсутствующий payload (`NULL,0`) даёт legacy
defaults: все Outputs в Master, unity gain, пустые FX и Scenes.

Повреждённый payload не применяется частично. Неизвестная версия/header flags/
Scene fields возвращают `MIXER_UNSUPPORTED`, не затирая текущий объект. Будущий
archive bridge должен сохранить исходную неизвестную строку или запретить
перезапись именно extension, сохранив штатное сохранение проекта; нельзя тихо
заменять будущие данные на V1 defaults. Actual native archive round-trip,
Save As, Mod Off и восстановление ещё требуют интеграции и отдельных тестов.

FX в persistent модели проверяются в широком диапазоне частот (через существующий
validator при 192 kHz); перед prepare обязательно повторить проверку на **реальной**
частоте движка. Codec не создаёт DSP, не обещает доступность неизвестного native id
и не включает неподдерживаемый FX в интерфейсе.

## Проверки

В Linux/WSL, из `sd-mod-session`: `python3 verify_mixer_model.py`.
Не нужны firmware, SD или подключённое MPC; для ARM нужен действующий sysroot
(`MPC_QEMU_SYSROOT`, как в native audit).

- Host, host ASan+UBSan, AArch64/QEMU и AArch64 UBSan.
- Legacy defaults, все 128 Pad Outputs, отказы без изменения данных.
- Группа -3/-9/-6 → +2 → -1/-7/-4; общий clamp и Off.
- Разные Part ids, optional поля, fallback, update/remove/full store, неизменный Output.
- Codec round-trip для UTF-8, Off, 32 native parameters, EQ8 и 128 Scenes.
- Каждое усечение default payload, неизвестные версии, неверные имена, 400 мутаций
  с корректным CRC для проверки внутренних length/count/value, без частичного commit.
- Одинаковый payload на host/ARM и независимая проверка Python `struct`/`zlib.crc32`.

Отчёт: ignored `build/mixer-model/verification.json`. Следующее подключение —
единый project owner/archive bridge и реальные PCM snapshots после закрытия
native routing ownership из [аудита](MIXER_NATIVE_AUDIT.md). UI и Song должны
пользоваться одним объектом, а не копировать independent project tables.
