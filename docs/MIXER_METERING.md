# Mixer PCM metering core

03.11.2026; `feature/mixer-song-stems`.
Реализация: [`mixer_meters.h`](../sd-mod-session/src/mixer_meters.h),
[`mixer_meters.c`](../sd-mod-session/src/mixer_meters.c).
Native taps/восемь audio buses/UI пока не подключены. Core работает с переданным
PCM; нельзя объявлять работающим meter трека до проверки его реального источника.

## Что измеряется

Один экземпляр получает согласованные блоки десяти шин: Track 1–8, Master Direct
и final Master. Восьми Track соответствует индексация 0…7, Direct — 8, Master — 9.
Для каждого tap передаются borrowed L/R pointers, mono/stereo и native clear flag.
Pointers действуют только во время `mixer_meters_push()` и не сохраняются.
Adapter обязан подтвердить их lifetime и capacity >= frames; core не может
проверить доступность произвольного ненулевого адреса native объекта.

- Peak = `max(abs(sample))` для каждого L/R. В mono оба показания одинаковы.
- Отображение в dBFS: `20*log10(peak)`; ноль — `-inf`/пустая шкала.
- `abs(sample) >= 1.0` фиксирует sample clipping с удержанием 1 s по sample clock.
  Это не true-peak и не индикатор положения gain-fader.
- Peak envelope имеет exponential decay с time constant 300 ms. Ballistics
  считаются по числу frames/rate, независимо от скорости UI Timer.
- `clear=1` означает настоящий silent tap; память может содержать старый PCM
  или отсутствовать. Она не читается, envelope затухает.
- `channels=0` означает недоступный tap, не тишину. Его valid bit очищен, bar
  должен стать N/A. Неверный format, отсутствующий обязательный pointer или
  NaN/Inf в PCM даёт fault bit и N/A.
- Fader Level, note activity, MIDI velocity или random generators в API отсутствуют.

Выбор правильного native post-FX/post-Level/post-gate Track tap и final Master
после Master FX остаётся задачей adapter из [native audit](MIXER_NATIVE_AUDIT.md).
Данный core не меняет и не нормализует источник, не создаёт фиктивные шины.

## Передача audio → UI

Create/destroy выполняются вне RT; между ними ровно один audio producer и один
UI consumer. Для live и offline engines нужны отдельные экземпляры. В audio path
нет allocations, IO/logging, mutex, ожидания читателя или retry loop.

SPSC ring содержит четыре целых numeric snapshot. Audio публикует slot через
release write-index; UI получает его через acquire, копирует и release read-index
возвращает право записи producer. Пока slot принадлежит UI, producer его не меняет.
Shared struct не читается одновременно с записью; неатомарного seqlock/data race нет.
Индексы — always-lock-free unsigned, это проверяется при компиляции/создании.

Если UI отстаёт, audio продолжает считать envelope/clip и pending peak,
возвращая `queue full` без ожидания. Максимальный реальный peak до следующей
успешной публикации сохраняется, поэтому transient в пропущенном блоке не исчезает.
Один UI read обрабатывает не более четырёх slots; быстро работающий producer
не может превратить чтение в бесконечный drain loop. Пиковое значение может
отражать накопленный интервал публикации; это не мгновенный RMS.

Каждый snapshot содержит epoch и frame_end. Возрастающий epoch сбрасывает audio
history; UI запрашивает только актуальное поколение. Старые slots освобождаются,
но не выдаются за новый проект. UI owner обязан также очистить свой cache при
Load/новом epoch и показывать N/A, если счётчик frames не обновлялся >250 ms без
явного корректного stopped snapshot. Wall-clock/UI stale logic в audio core нет.

Размер состояния с ring и всеми десятью meters — **720 B** на проверенных host
и AArch64 builds. Это не результат измерения CPU на MPC Sample.

## Выполненные проверки

`python3 verify_mixer_meters.py` из `sd-mod-session` в Linux/WSL:

- Host, host ASan+UBSan, AArch64/QEMU и AArch64 UBSan.
- Независимые L/R peaks, mono, dBFS, сохранность исходного PCM.
- Native clear при NULL backing store, missing tap, обязательный NULL pointer
  при clear=0 и неконечный PCM.
- Queue full, short transient между успешными публикациями, clip hold ровно
  по sample clock, новая epoch и отбрасывание stale epoch.
- Конкурентные producer/consumer threads: 20000 audio blocks, coherent snapshots,
  монотонный frame counter и согласованная stereo peak pair.

Дополнительно `verify_mixer_native.py`: настоящий native `2769bf0` формирует
стереосумму `.375 / -.1875`, новый meter core измеряет её peaks `.375 / .1875`.
Источник в этом стенде — синтетические samples через реальную native функцию;
соответствие slot0 конкретной live-шине тест не заявляет.

Отчёты лежат в ignored `build/mixer-meters/` и `build/mixer-native/`.

## Экспериментальная полная линковка

```sh
MPC_RUNTIME_ROOT='/path/to/local/runtime' python3 build_mixer_core_candidate.py
```

Builder проверяет feature-ветку, использует существующую baseline-сборку и
добавляет model/meter objects в отдельную `build/mixer-candidate/phone-input.so`.
С 04.11 отдельно пересобирает `phone_native.c` с experimental archive flag:
[контракт сохранения](MIXER_ARCHIVE.md). Проверяется full preload constructor на
закреплённом ELF. Mixer mode/audio taps не включаются, release/SD package не
создаётся. Кандидат проверяет полную линковку, не готовность функции на устройстве.
