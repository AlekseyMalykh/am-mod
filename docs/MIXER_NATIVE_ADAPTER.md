# Scoped native PCM adapter

Текущий статус: этот документ описывает низкий scoped слой. Следующий
[live слой первого пакета](MIXER_STEP1.md) уже добавляет pinned installer,
primary-output Master, live update packets и atomic session retirement.
Ограничения scoped слоя ниже не означают отсутствия этих новых компонентов.

04.11.2026, только `feature/mixer-song-stems`.
Код: [`mixer_route_native.c`](../sd-mod-session/src/mixer_route_native.c),
[контракт API](../sd-mod-session/src/mixer_route_native.h).

Реализован экспериментальный адаптер **одного подтверждённого separate-buffer
пути DrumProgram** pinned 1.3.0.12. ARM-тест исполняет настоящий native цикл пэдов,
resolver и summing с новым bus core. Подключение двух call sites существует
в приватном ELF mapping первого теста. Позднее добавлен installer entry hooks
в отдельный experimental candidate; автоматической session/UI activation нет.

## Проверенная граница

```text
286be30: native DrumProgram post-voice renderer
  ordinary: virtual Instrument processor
  optimized: 2803470 -> individual processing of two buffers -> merge
  both -> 286bf44: native sum 2769bf0
           -> original native sum into shadow (fallback only)
           -> original native route mapping/gain into zeroed Pad scratch
           -> accumulate into exactly one Track or Direct
  286bfa4: before native Program stage1
           -> finish Track/Direct/core Master
           -> replace native Program buffer (not add to original Pad sum)
  native Program stages1/2/3, processing and gain/ramp remain afterwards
```

Native identity использует Instrument `+0x2168` (Pad 0…127), `+0x2310`
(принадлежащая Program), active list Program `+0x2820/+0x2828` (stride16).
`276abc0` читает buffer collection `+0xb30`; native AudioBuffer имеет size0x130,
channels `+0`, capacity `+4`, planar channel pointers `+16`, clear byte `+0x128`.
Основной буфер получается через `276abc0(program+0x78,0)`. Нельзя считать все
буферы одинаковыми на основании внешнего вида или заменить collection stride.

До подавления первого штатного sum проверяется **весь** список. Принимаются
валидные stereo buffers достаточной ёмкости, отдельные от Program PCM, два или
более buffers у каждого Instrument, известные Pad id/owner, уникальные entries.
Destination должен разрешаться в тот же Program buffer; null resolver сохраняет
штатный stereo unity fallback. Physical destinations, неподдерживаемые channel
indices, неконечный route gain, shared/непустой Program и неверная ёмкость
возвращают весь блок в native renderer без частичного применения микшера.

Нативный `2769bf0` меняет clear flag при записи первого канала, затем может
прибавить второй. Поэтому scratch очищается **физически для обоих каналов перед
каждым пэдом**. Одного clear flag недостаточно. До отдельного доказательства
native reset ownership адаптер принимает только Program buffer с clear=1
**и нулевыми backing samples**; clear=1 со старыми данными остаётся native.
Shadow хранит точные backing bytes/flag, включая stale storage; он не публикуется
как аудиошина или meter input. Stereo swaps, negative gain и направление двух
входов в один выход выполняет исходный native sum, не его приближённая копия.

Штатные Pad processors остаются перед capture, active/idle list обрабатывает
оригинальный цикл. Обычный и один оптимизированный two-buffer merge исполняются
в тесте; внешние Pad/Program DSP services заменены детерминированными fixtures.
Дополнительный DSP suite ниже исполняет также настоящий Pad EQ и Program gain;
живые voices и непустые native insert chains пока не проверены.

## Native buffers, Pad EQ и полный render entry

Дополнительный [`check_mixer_route_dsp.c`](../sd-mod-session/check_mixer_route_dsp.c)
уточняет прежнюю гипотезу о shared/one-buffer пути. Исследованный штатный
Instrument constructor `28100c0` передаёт extra=1 базовому `27714f0`, тот прибавляет
один и вызывает `276b9a0` с count=2. RenderableProgram передаёт extra=0 → count=1.
RTTI этих constructors подтверждён как `Instrument` и `RenderableProgram`.
Это разные владельцы буферов; нельзя называть Program с одним буфером обычным
однобуферным Pad. Defensive отказ для Instrument с count<=1 остаётся: все возможные
clone/lifecycle пути ещё не исследованы.

Тест выполняет настоящий allocator `276b9a0`, getters/count и reset `276a830`.
При dirty buffer reset физически очищает оба канала и ставит clear=1; при уже
clear=1 пропускает очистку. Поэтому проверка zero backing storage в адаптере
остаётся необходимой, а выставление clear вручную не заменяет native reset.
Вне RT использован fixture listener-registration service; backing PCM и vector
выделяются/освобождаются настоящим native кодом.
Reset между блоками вызывает тестовый owner явно; реальный engine lifecycle
и его порядок вызовов этим стендом ещё не подтверждены.

Штатный Instrument route создаётся `2774a40`: destination=0, subindex=0,
stereo unity. Его resolver даёт null, и `286be30` направляет PCM в Program buffer.
В данном контексте это **штатный default/inherit path**, а не неизвестный маршрут.
Destination1/index0 возвращает отдельный embedded router buffer; это не тот же
объект, что allocated Program buffer. Адаптер не объявляет их тождественными.

DSP suite исполняет `2803430 -> 2802f90` через настоящий установленный AM Pad EQ
wrapper, затем native Program stages `276a310`, `276a520`, stereo gain `2769d60`
и завершающий Program level/ramp. Insert chains пусты; генератор PCM и внешние
сервисы — fixtures. Реальный Bell cut остаётся внутри назначенного Track,
Track Level применяется к post-EQ PCM один раз, Program stereo gain — после
core Master. Другие Track/Direct не получают EQ выбранного пэда.

Проверяется и настоящий вход `286c3b0`: native clock начинается с13, блок2048
frames проходит через33 участка длиной не более64 у каждого из трёх voice,
настоящий `286c2a0` обходит active voice list, затем tail call `286c478 -> 286be30`
в тесте направляется в scoped adapter. Подменён только oscillator body `221ee10`,
который пишет тестовый PCM в реальные native Pad buffers. Проверены сохранность
clock/voice list, unity output и выбранный Track Level. Это ещё не проверка note
scheduling, native oscillator/envelopes, Pad Link, voice stealing или release/tails
на устройстве. Test-only bridge не является production installer.

## Ошибки и владение

Prepare создаёт frozen model/Scene/FX snapshot и резервирует память вне RT.
Один audio owner вызывает scoped render; thread-local scope ограничивает capture
данной Program и двумя конкретными call sites. Вне scope dispatchers вызывают
оригинал. Вложенный scoped render временно выходит из capture и остаётся native.
Нет новых глобальных Sample/Seq/Song/FX UI-состояний или Component pointers.

Shadow складывается исходным `2769bf0` в исходном порядке. При ошибке accumulate,
FX/finish или изменении известного источника он заменяет Program buffer **до**
native Program processing. Оставшиеся пэды идут штатным путём. Не выдаются неполные
views. После ошибки экземпляр выключен до off-RT destroy/reprepare. Если ошибочный
input уже содержит NaN, fallback сохраняет штатный результат; он не исправляет
источник и не выдаёт искусственный ноль за успешное аудио.

Вызов с обоими отсутствующими call-site hooks сохраняет native результат и
выключает адаптер. Частичная установка hooks не поддерживается: будущий installer
обязан проверять и менять обе инструкции вне audio callback как одну согласованную
операцию. Это правило относится к call-site harness. Новый live installer
отдельно проверяет и устанавливает четыре entry hooks до запуска audio.

Дополнительные PCM scratch+shadow занимают `4*capacity*sizeof(float)` = 4096 B при
256 frames; вместе с bus core — 26624 B (26 KiB), без metadata и FX. Native sum
считается дважды для резервной суммы и маршрутизированного пэда. Это осознанный
проверочный overhead; CPU budget и возможность иной политики требуют измерений.
В linked adapter/core нет allocation/free, mutex, logging или IO в callback.
Адаптер/graph/FX уничтожаются только после stop/join producer и readers.

## Meters и дальнейшая интеграция

Track/Direct diagnostic views содержат настоящий post-gate PCM нового графа.
Но его Master пока расположен **до** оставшихся native Program FX/gain и общего
engine output mix. `mixer_route_meters_push()` после scoped render публикует
настоящие post-gate Track0…7/Direct taps; Master имеет channels=0, то есть N/A.
Нельзя подключить diagnostic core Master view к финальному Master meter или
объявить его завершённым mixdown/stem tap.

Один audio owner вызывает meter push ровно один раз на native block; повтор
отклоняется, включая попытку повтора после queue-full. Неподдержанный/recovered
блок публикует unavailable taps и сбрасывает прежние peaks; ошибочный вызов render
не позволяет повторно опубликовать старый PCM. Во время scoped callback views и
meter push недоступны. UI получает только существующий SPSC numeric snapshot.
Передача проверена с реальными post-Pad-EQ bus buffers в полном native entry.

Остаются открыты shared/one-buffer path, остальные ветки optimized processor,
Pad Link, physical/multi-output, preview/live input, остальные Program renderers,
resample/offline, voices и смена программ. При неподдерживаемом блоке не применяются
Track routing/Level/FX: это сохранение штатного звука, а не полная работа Mixer.
Первый пакет теперь добавляет block updates, ramp/crossfade и revision ack;
Song Scene boundary scheduling, native UI/audio acknowledgement и аппаратные
CPU/underrun проверки по-прежнему впереди.

## Воспроизведение

Из `sd-mod-session` в Linux/WSL:

```sh
MPC_RUNTIME_ROOT='/path/to/runtime' python3 verify_mixer_route_native.py
```

Normal и ARM UBSan проверяют точный SHA-256 ELF, исходники и:

- настоящий `286be30`, native routes, sum и один optimized `2803470` merge;
- побитовое unity-совпадение для детерминированных binary-fraction fixtures,
  выбранный Track Level и отсутствие Direct-дубликата;
- native route gain/channel mapping, Pad127, native idle-entry removal;
- whole-block fallback для physical/shared/aliased/stale-clear/one-buffer paths;
- раннюю ошибку capture и позднюю FX fault с восстановлением native audio;
- независимые Track FX tails в пустом блоке, clear silence и native stages после;
- отсутствие linked malloc/calloc/realloc/free в scoped audio callbacks;
- сохранность native результата при отсутствии обоих call-site hooks.

Отдельные normal/UBSan DSP-прогоны дополнительно проверяют native allocation/reset,
реальные Instrument/Program wrappers, существующий Pad EQ, native stereo gain,
повторные reset/render и silence, полный render entry/64-frame voice walk,
а также совпадение Track/Direct telemetry с фактическими bus peaks. Master всегда
unavailable; duplicate/early meter push и публикация после invalid render запрещены.
Queue-full/retry проверяются отдельно: frame clock учитывает каждый блок один
раз, отложенные peaks сохраняются, новая epoch после recovery не содержит старых.

Grouping Track может изменить порядок float additions относительно общего native
sum; побитовая проверка этих fixtures не обещает побитового совпадения любых
перегруппированных сигналов. Native objects, исходные samples и DSP services здесь
fixtures; полный app lifecycle, истинная полифония и устройство не исполняются.

Результаты находятся только в ignored `build/mixer-route-native/`. Builder включает
адаптер и dormant live hooks в отдельную experimental library, проверяет full
link/preload без привязанной session или native UI. Штатный ELF, SD и release
artifacts не меняются.
