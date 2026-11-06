# Eight Track PCM core

04.11.2026; `feature/mixer-song-stems`.
Код: [`mixer_buses.h`](../sd-mod-session/src/mixer_buses.h),
[`mixer_buses.c`](../sd-mod-session/src/mixer_buses.c).

Реализованы настоящие отдельные PCM-буферы Track 1–8, Master Direct и final Master
с обработкой FX, mix и metering. Источник передаёт в core обработанные samples;
Scoped adapter и [live слой первого пакета](MIXER_STEP1.md) проверены в QEMU.
Experimental candidate устанавливает dormant hooks, но не привязывает session;
штатное приложение пока не использует новые шины при игре. Это ещё не доступный
пользователю Mixer или Stem Export.

## Тракт и владение

```text
post-individual-Pad PCM (mono/stereo, immutable borrowed input)
  -> one project Pad Output
     -> Track 1..8 buffer -> own FX -> Level/Pan -> Mute/Solo -> Track tap
     -> Master Direct buffer --------------------> Solo gate -> Direct tap
  -> sum Track taps + Direct tap
  -> own Master FX -> Master Level/Pan -> final Master tap
  -> optional real PCM meter snapshots
```

`mixer_buses_prepare()` выполняется вне realtime. Она валидирует model и FX на
реальном rate, разрешает Scene конкретного Part, копирует routing, рассчитывает
mix gains и создаёт **отдельные** FX instances. Изменение UI/project после prepare
не может изменить текущий audio graph. Никаких Component/Sequence/Project native
pointers в графе нет. Неизвестный/unavailable FX, неверные параметры или resource
failure прерывают prepare и освобождают уже созданные процессоры.

Каждый активный graph принадлежит одному audio owner:

1. `begin(frames)` очищает собственные buffers.
2. `accumulate(pad, L/R, mono/stereo, clear)` складывает любое число голосов/пэдов
   в единственный назначенный bus. Входные samples не изменяются и не удерживаются.
3. `finish()` обрабатывает все Track, включая tails при отсутствии новых sources,
   суммирует Master и публикует meters, если передан соответствующий экземпляр.
4. `view(bus)` даёт readonly PCM и точное число frames до следующего begin/destroy.

Незавершённый/ошибочный блок не даёт view. Неконечный input/FX output или overflow
отменяет выдачу блока, очищает буферы и публикует unavailable meter taps. Native
adapter должен выбрать и проверить аудиополитику отказа (fallback/silence/error)
до включения live routing; неполный buffer нельзя выдавать за успешный stem.

Внутренний realtime путь не выделяет память, не читает файлы, не пишет лог и не
использует mutex. Контракт внешнего `fx_backend.process` также требует безопасного
RT исполнения; наличие этого интерфейса не доказывает CPU-бюджет любого native FX.
Destroy вызывается после stop/join producer/readers, вне audio callback.

## Gain, Pan, Solo и FX

Level применяется как `10^(dB/20)`, Off даёт ноль. Панорама здесь — stereo balance:
центр сохраняет unity обоих каналов; при смещении уменьшается только противоположный
канал. Индивидуальный Pad pan остаётся upstream и не переназначается. Этот закон
зафиксирован для нового core; перед аппаратным подключением проверить его
согласованность со штатным Pad gain/pan и Master, избегая двойного применения gain.

Solo одного или нескольких Track заглушает остальные Track **и Master Direct**
на входе Master. Explicit Mute имеет приоритет над Solo данного Track. Routing
не меняется, FX-state продолжает считать хвосты. Master Mute/Solo запрещены моделью.
Direct tap соответствует слышимому post-Solo Direct; Track tap — post-FX/mix/gate.
В будущем отдельный pre-gate sidechain tap не должен подменять эти meters/stems.

Существующий `fx_chain` задаёт порядок сверху вниз и до пяти слотов. Track FX
выполняются до Track Level/Pan/gates, Master FX — после суммы и до Master Level.
Chain bypass не меняет persistent config; native/band параметров не теряет.
Bypass graph создаётся отдельным prepare, мгновенная live-смена ещё не реализована.

Send Amount >0 возвращает `MIXER_BUS_UNSUPPORTED`: returns/export policy пока нет.
Sidechain пока не реализован. В UI эти функции должны оставаться Disabled;
сохранённое поле данных не означает действующий Send или Compressor sidechain.

## Память и независимые выходы

Один graph выделяет десять planar stereo buffers и один interleaved stereo scratch
для FX: `22 * max_frames * sizeof(float)`. При 256 frames — **22528 B (22 KiB)**,
без metadata, allocator overhead и памяти FX. Диапазон block capacity — 1…8192.
`begin()`/`finish()` для части блока поддерживают меньший frames, без перераспределения.

Bus indices согласованы с meter API: 0…7 Track, 8 Direct, 9 Master. Это готовая
точка render targets для будущего streaming WAV writer. Все views одного finish
имеют одну block boundary и frame count, без накопления целого Song в RAM.
Граф не пишет файлы, не воспроизводит Song и не переключает native Solo для stems.
Сумма pre-Master Track+Direct равна pre-Master mix; нелинейные Master FX отдельно
озвучены в исходном задании и не обещают null-test с post-Master Mix.

## Проверки

Из `sd-mod-session` в Linux/WSL: `python3 verify_mixer_buses.py`.
Host, host ASan+UBSan, AArch64/QEMU и AArch64 UBSan проверяют:

- все восемь stereo bus, mapping разных банков, повторные голоса, Direct isolation;
- независимость audio snapshot от дальнейшего изменения модели;
- Track Level/Pan/Off/Mute/Solo и отдельный Master gain;
- Part Scene без изменения project/Output;
- порядок/состояние/хвосты отдельных FX, placement Master FX, bypass и cleanup;
- meters с реальных окончательных bus buffers;
- неверный порядок вызовов, clear silence, NaN/overflow/FX failure и отказ Send.

Samples и внешний test FX backend — fixtures, сам routing/mix core настоящий.
Дополнительный `verify_mixer_native.py` исполняет native sum `2769bf0`, передаёт
полученный `.375 / -.1875` PCM через новый Track 1 → Master и проверяет точное
совпадение, отсутствие Direct-дубликата и независимые meter levels. Это доказывает
PCM interoperability, не правильность ещё не подключённых live tap/voice hooks.

Builder включает core в отдельную experimental library, проверяя link/preload;
он не включает audio routing mode и не создаёт SD/release package.

04.11 добавлен [scoped native adapter](MIXER_NATIVE_ADAPTER.md): реальный native
DrumProgram separate-buffer цикл и optimized two-buffer merge проверяются вместе
с bus core в QEMU. Это следующий уровень доказательства после supplied-PCM теста;
установленных device hooks и полного покрытия engine paths по-прежнему нет.

## Что необходимо для live adapter

Следовать [native audit](MIXER_NATIVE_AUDIT.md): один обработанный Pad buffer должен
попасть в новый граф ровно один раз, а штатный summing path для него должен быть
согласованно заменён. Не добавлять Track output поверх исходного Pad PCM.
Нужно покрыть оптимизированные ветки, Pad Link, multi-output, preview, sequenced
events и resample; сохранить voice/clear/idle ownership и индивидуальный Pad EQ.

При смене routing/Level/Scene теперь доступны полные off-RT update packets,
commit в begin(), 5ms ramps/crossfades и revision ack. Unchanged FX сохраняют
history, заменённые FX выводятся после crossfade; полный tail старой цепи не обещан.
UI не должен уничтожать/менять audio-owned graph напрямую. Control collect
освобождает retired packets/FX; audio не вызывает allocator или ожидания.
Подробности, проверки и ограничения — [первый пакет](MIXER_STEP1.md).
Включение native UI и аппаратные CPU/underrun проверки остаются впереди.
