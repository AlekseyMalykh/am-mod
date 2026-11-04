# Mixer: штатный ввод и единый обработчик входа

04.11.2026, `feature/mixer-song-stems`, firmware1.3.0.12.
SHA-256 исходного ELF:
`b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508`.

## Проверенный штатный путь

**Fader Settings открываются при первом down Shift+Pad9.** Ранее в design
предполагалось наличие отдельного штатного long-press timer. На проверенном пути
такого ожидания нет: ARM/QEMU исполняет native callbacks и получает запрос окна
до любого release/tick. Это исправление предположения, а не изменение цели:
короткое нажатие должно открывать Mixer, долгое — существующий Fader Settings.

| Native VA | Роль и результат |
|---|---|
| `159f2f0` | Raw button ID/edge → конкретное поле GlobalControls; проверены обе границы |
| `1a3aba0` / `1a3a6d0` | Физический pad callback и общий pad path при новом down выбирают Shift observable `mapper+f0`; held/repeat не открывает окно повторно |
| `1a3ddd0` | Реальное уведомление listeners; тест регистрирует listener штатной `14b6160` |
| `14af280…14af2a0` | Штатный registration: `mapper+f0` → `14ad170` |
| `14ad170`, pad index8 | Pad9 → очередь native action `14abaa0` → запрос mode `30ef` |
| `15b1628`, `16009f0`, `1602f60` | Factory `30ef` создаёт FaderModeScreen |
| `14ab500` / `14ab600` / `14ab680` | Default native actions запрашивают соответственно `20a7` / `20a5` / `20ab` |

Fixture заменяет model/queue services и mode-router body; вызывает очередь inline
для наблюдения её action. Реальные pad processing, listener registration,
notification и action body исполняются из private ELF mapping. Native app,
framebuffer, driver и полный asynchronous queue scheduler не запускаются.
Проверка не доказывает работу всей физической цепочки на MPC.

| Raw code | GlobalControls offset | Mixer consumer |
|---|---|---|
| `00` | `21a0` | KNOB FX |
| `01` | `20f0` | PAD FX |
| `0c/0d/0e` | `1bc8/1c20/1c78` | B1/B2/B3 |
| `6f` | `2d50` | Encoder press / Select |
| `31` | `2250` | Observe Shift; forward обе границы |
| `06/07` | `1f90/2040` | Закрыть Mixer, forward штатного Sample/Seq action |

Native modes получают исходные события и свои модификаторы. Mixer не подменяет
native IDs назначения и не имитирует собственные команды транспорта. Bank23,
Plus36 и Minus37 ещё не подключены к Assign/Inspect.

## Реализованный input adapter

[`mixer_input.c`](../sd-mod-session/src/mixer_input.c) переводит подтверждённые
raw IDs в существующий controller. Вне Mixer возвращается forward. Удержанные
FX/B-кнопки продолжают поглощать свой release после Sample/Seq exit через
controller ownership. CC/pads обрабатываются существующим controller API.

В этом же модуле `mixer_entry` содержит **единственный** short/long decision.
Начальный порог500ms — политика AM Mod, не найденная константа прошивки.
Порог задаётся при init и требует оценки удобства на устройстве.

```text
ready + no conflicting modal + Shift + Pad9 down -> retain raw down, arm
Pad9 release before500ms                      -> consume pair, open Overview
tick at/after500ms while held                 -> replay native down once
physical release after native down           -> forward paired native up
release at/after500ms before tick             -> replay captured native pair
Load/New/fault/modal/epoch change before long -> cancel, swallow owned late up
```

Shift принадлежит gesture по состоянию на down. Отпускание Shift первым не
превращает gesture в обычный pad. После long никогда не выдаётся OPEN на release.
Повторный down не сбрасывает начало. При проверке времени используется elapsed,
а не потенциально переполняющийся `start+timeout`.

## Общая очередь ввода и экран Sample

Все вызовы adapter/entry/controller выполняет один UI FIFO consumer. Producer
передаёт timestamp, ticket и исходные inline events через существующую очередь;
он не вызывает UI controller и не читает native Program/Component.

До entry down нельзя вызывать stock pad handler: он открывает Fader сразу.
Кэш raw down должен храниться до решения. LONG_DOWN/LONG_PAIR возвращают
обязанность replay исходного native события; NATIVE_UP — обязательный release.
Если Shift уже отпущен, replay должен временно восстановить **штатный** Shift
из захваченного event template и затем вернуть реальное состояние. Это не
разрешение оставлять Shift включённым либо конструировать неизвестный MIDI формат.

FIFO обрабатывается до timer decision; short определяется по timestamp edge,
а не задержке UI. Запросы новых input events не могут обгонять queued mode exit.
На cancellation сохраняется ownership позднего up. Вне Mixer и без подтверждённой
готовой audio session весь путь должен оставаться stock.

`phone_native` использует этот путь после явной привязки сессии через
`mixer_phone_bind_ui`. Вход доступен с видимого экрана Sample при готовом
аудиотракте. Успешное перехваченное нажатие резервирует место для отпускания
в общей очереди. Load/New отзывает доступ старой сессии. Отпускания уже
переданных штатных нажатий сохраняют своё назначение.

Экран Sample рисует Mixer в существующем слое AM Mod. Тик индикаторов
инвалидирует область меток и уровней. Смена страницы, параметров или
подтверждённой ревизии запрашивает полную отрисовку.

Candidate автоматически создаёт сессию по снимку live аудиовызова.
Подключённые обработчики, создание и снятие сессии проверены на ARM/QEMU.
Подсветка пэдов и аппаратная проверка остаются впереди.

## Проверки

Из `sd-mod-session` в WSL/Linux:

```sh
python3 verify_mixer_input.py
MPC_RUNTIME_ROOT='/path/to/runtime' python3 verify_mixer_input_native.py
MPC_RUNTIME_ROOT='/path/to/runtime' python3 build_mixer_core_candidate.py
```

4 controller/gesture runs: host, ASan+UBSan, AArch64, ARM UBSan. Проверены
short/long exclusivity, exact500ms boundary, delayed tick, Shift-first release,
готовность/модальный запрет, stale ticket, cancel, сохранение native up, raw keys,
возврат stock FX вне режима и неизменённая передача Play/Stop.

2 native runs: normal/UBSan. Проверены открытие Fader на первом native down,
отсутствие повторного запроса на held/release, отсутствие native down при short,
ровно один реальный Fader request после long decision, штатная button dispatch
table и default actions Sample/PadFX/Seq. Все изменения памяти private; исходный
ELF и SD не изменяются.
