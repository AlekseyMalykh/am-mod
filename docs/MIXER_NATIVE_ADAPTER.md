# Scoped native PCM adapter

04.11.2026, только `feature/mixer-song-stems`.
Код: [`mixer_route_native.c`](../sd-mod-session/src/mixer_route_native.c),
[контракт API](../sd-mod-session/src/mixer_route_native.h).

Реализован экспериментальный адаптер **одного подтверждённого separate-buffer
пути DrumProgram** pinned 1.3.0.12. ARM-тест исполняет настоящий native цикл пэдов,
resolver и summing с новым bus core. Подключение двух call sites существует
только в приватном ELF mapping теста. В сборке устройства нет installer,
автоматического входа в адаптер или действующего live Mixer.

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
Следовательно, это ещё не сквозной тест настоящего Pad EQ/native FX с живыми voices.

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
операцию. Текущий production-код вообще не патчит ELF и не устанавливает hooks.

Дополнительные PCM scratch+shadow занимают `4*capacity*sizeof(float)` = 4096 B при
256 frames; вместе с bus core — 26624 B (26 KiB), без metadata и FX. Native sum
считается дважды для резервной суммы и маршрутизированного пэда. Это осознанный
проверочный overhead; CPU budget и возможность иной политики требуют измерений.
В linked adapter/core нет allocation/free, mutex, logging или IO в callback.
Адаптер/graph/FX уничтожаются только после stop/join producer и readers.

## Meters и дальнейшая интеграция

Track/Direct diagnostic views содержат настоящий post-gate PCM нового графа.
Но его Master пока расположен **до** оставшихся native Program FX/gain и общего
engine output mix. API поэтому не публикует meter snapshots и явно обозначает
это ограничение. Нельзя подключить этот Master view к финальному Master meter
или объявить его завершённым mixdown/stem tap.

Остаются открыты shared/one-buffer path, остальные ветки optimized processor,
Pad Link, physical/multi-output, preview/live input, остальные Program renderers,
resample/offline, voices и смена программ. При неподдерживаемом блоке не применяются
Track routing/Level/FX: это сохранение штатного звука, а не полная работа Mixer.
Замороженный граф пока не меняется во время игры; нет block-boundary graph exchange,
ramp/crossfade, live Scene switching или согласованного UI/audio acknowledgement.
Все эти пункты нужны до включения Mixer, затем проверки CPU/underruns на MPC.

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

Grouping Track может изменить порядок float additions относительно общего native
sum; побитовая проверка этих fixtures не обещает побитового совпадения любых
перегруппированных сигналов. Native objects, исходные samples и DSP services здесь
fixtures; полный app lifecycle, истинная полифония и устройство не исполняются.

Результаты находятся только в ignored `build/mixer-route-native/`. Builder включает
адаптер в отдельную experimental library и проверяет full link/preload, оставляя
UI и audio routing выключенными. Штатный ELF, SD и release artifacts не меняются.
