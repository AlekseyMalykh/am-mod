# Mixer / Song: native audit, этап 0

03.11.2026; ветка `feature/mixer-song-stems`; исходная ревизия `160cee8`.
Политика интерфейса: [MIXER_DESIGN.md](MIXER_DESIGN.md).
Это результаты локального анализа и ARM/QEMU, не аппаратная проверка Mixer.
Продолжение04.11: [первый пакет audio/updates/painter](MIXER_STEP1.md).
Он добавляет установленный experimental hook layer и primary-output Master;
нижняя таблица сохраняет границы исходного Phase0 аудита.
ELF 1.3.0.12: SHA-256
`b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508`.

## Установленные точки

| Native VA / код мода | Что подтверждено | Ограничение |
|---|---|---|
| `18ccbc0` | Destination 1 принимает только subIndex 0, возвращает router+0x10. Destination 2 возвращает physical[subIndex], stride 0x130, с проверкой `0 <= subIndex < router.count(+0x140)`. Остальные destination/index дают null | Нельзя объявить subIndex 1…8 Master-ветки восемью пользовательскими Track. Увеличение physical count само не создаёт независимый DSP граф |
| `2769bf0` | Читает настоящий source AudioBuffer, route channel indices и gain; копирует/прибавляет L/R в destination. Clear source не суммируется; source PCM не изменяется | Проверено с синтетическим PCM, без живых голосов и Pad FX |
| Новый [`mixer_meters.c`](../sd-mod-session/src/mixer_meters.c) после native sum в стенде | Реальная сумма `.375 / -.1875` даёт peaks `.375 / .1875` | Это supplied-PCM тест, native live taps ещё не установлены; [metering contract](MIXER_METERING.md) |
| `286be30`, call `286bf44 -> 2769bf0` | В цикле DrumProgram есть индивидуальный source buffer/route до суммирования. Перед этим вызывается virtual processor инструмента | Есть оптимизированные/альтернативные пути дальше по функции; один этот callsite не является полным покрытием всех пэдов и настроек |
| [`pad_eq_native.c`](../sd-mod-session/src/pad_eq_native.c), hook `2802f90` | Уже существующий wrapper вызывает native instrument processing, затем Pad EQ; проверяет pad id +0x2168, program +0x2310, sample rate property, PCM capacity/clear | Образец безопасной идентификации источника; закрытие/выключение UI не должно отключать новую маршрутизацию |
| `2226fb0` | Общий mixer entry вызывает `220a4a0`, `2226d10`, затем `18c79f0` | Не считать его аргумент x1 готовым финальным PCM: существующий resample уже учитывает direct physical output |
| `18c79f0` / `28a17f0` / `28c4ee0` | Есть дополнительные program/insert/physical-output стадии и native summing | Окончательный порядок всех Master FX и gain ещё не подтверждён; точку final Master meter выбрать после полного аудита |
| `18c6330` (Song), `18c62f0` (Seq) | Оба входа вызывают `18c5a00`; source selector 0 для Song, 2 для Seq. Проверены передача services/path/rate/depth/callback и неизменённые entry wrappers | Тест заменяет dispatch body перехватчиком; Song Parts и реальные WAV этим тестом не рендерятся |
| `2a05054 -> 18c6330` | Статически найден реальный вызывающий путь Song Export | Полный UI/service graph не исполнен |
| RELRO `7167218` / `7167220` | Существующий resample перехватывает только Seq slot, отдельно проверяя соседний Song slot | Новый Mixer не должен расширять этот перехват на чужие задания |
| RTTI `Mixdown::SongThread`, vptr `6d93598`, run `18c32c0` | Song имеет собственный native worker; общий export entry не доказывает совпадение traversal live/offline | Нужно ещё установить source SequenceChain, tempo/repeat/part boundaries и применение Scene |
| RTTI `SongMode`, getters `1f0cad0`, `1f0cb00`, `1f0cb40` | Native live SongMode читает 8-byte записи из собственного cached vector (+0x138/+0x140) | Это не доказательство формата сериализованных Song Parts или repeat semantics; поле +4 нельзя заранее назвать repeats |

Адреса относятся к virtual image offsets. Writable PT_LOAD имеет file offset,
отличающийся от VA на 0x10000; при изучении RTTI/vtable учитывался mapping ELF,
а не предположение `file_offset == VA` для всего файла.

## Вывод для восьми шин

Обработанный индивидуальный PCM доступен в существующем instrument processing
пути. Для Mixer нужен отдельный адаптер, который ровно один раз направляет его
в Master Direct или выбранную Track, а затем сохраняет штатную Master обработку.
Не подставлять вымышленные destination/subIndex в native resolver и не добавлять
обработанный Track поверх прежнего Pad сигнала: это даст тишину или двойной звук.

04.11 добавлен [eight-bus PCM core](MIXER_BUSES.md), не устанавливающий native
adapter. Native-тест теперь передаёт результат настоящей `2769bf0` через Track 1
и Master нового core, проверяя совпадение PCM и отсутствие Direct-дубликата.

Далее реализован [scoped native adapter](MIXER_NATIVE_ADAPTER.md) с исполнением
настоящего `286be30`, обычной ветки и одного `2803470` optimized merge. Проверены
preflight всего блока, native route gain/channel mapping, idle-entry removal,
отсутствие double sum, FX tails и восстановление native shadow при ошибке.
`286bf44` — общая точка sum обеих исследованных отдельных Pad-buffer веток;
`286bfa4` — flush перед первой Program stage. Shared/one-buffer путь может выйти
из `286be30` ещё до Program stages и пока целиком остаётся native.
Это проверка приватного ELF mapping с fixture DSP, не включённый device adapter.
Окончательный Master tap после native Program/general mix по-прежнему не доказан.

Позднее в первом пакете исполнен `2226fb0 -> 18c79f0` с настоящими установленными
entry hooks, native Program output sum `28a17f0` и primary destination2/index0.
Новый Master расположен после возврата engine block, отдельно от scoped Program
Master, и измеряет фактический PCM output pair0. Downstream insert/direct-route
services в тесте — fixtures; электрический выход, hardware volume/monitor и
прочие режимы/пары не объявляются покрытыми. Native session/UI binding пока
не активируется автоматически. Полный контракт: [MIXER_STEP1.md](MIXER_STEP1.md).

Дополнительный DSP suite уточняет buffer ownership: исследованный Instrument
constructor `28100c0 -> 27714f0` создаёт2 buffers; RenderableProgram —1. Native
`276b9a0` и reset `276a830` исполнены: dirty reset очищает оба PCM-канала, уже
clear buffer пропускается. Штатный Pad destination0/subindex0 конструируется
`2774a40` и следует через null resolver в default Program buffer; destination1
возвращает отдельный router buffer, не allocated Program buffer.
`286c3b0/286c2a0`, реальные Instrument/Program wrappers, установленный AM Pad EQ
и native stereo gain теперь проверены вместе с graph. Генератор voice PCM и
сервисы — fixtures; полное note/voice lifecycle покрытие остаётся открытым.
Track/Direct native adapter telemetry публикуется через meter core, global Master
остаётся unavailable: см. [контракт и четыре ARM-прогона](MIXER_NATIVE_ADAPTER.md).

Перед установкой адаптера необходимо покрыть обычные/оптимизированные ветки,
Pad Link, multi-output, preview/live input, sequenced notes, resample/offline,
mute и tail, а также существующий Pad EQ hook. Захват после `2802f90` с обнулением
буфера ещё не считается безопасным решением: нужно проверить native clear flags,
определение idle, дальнейшие стадии и сохранение активных голосов.

Дальнейшая реализация может использовать небольшой общий bus core и имеющийся
FX backend. Native adapter должен доставлять туда индивидуальный PCM; UI и Song
render targets читают те же данные. Bus core с тестовым PCM отдельно от реального
adapter не объявляется работающей функцией устройства.

## Совместимость и сохранение

[`music_archive.c`](../sd-mod-session/src/music_archive.c) добавляет необязательное
именованное поле в штатный JSON archive, не переписывая XPJ самостоятельно.
Native serializer продолжает владеть compression/temp/commit. Это подходящая
точка для последующей интеграции версии Mixer extension, с независимой валидацией
и атомарным принятием при Load. Устаревший/битый Mixer payload не должен повреждать
штатный проект; при отсутствии extension Outputs = Master, gain unity, FX пусты.

Самостоятельная модель не может гарантировать Save/Load/Mod Off: до проверки
native archive bridge она остаётся независимым core. Нельзя добавлять данные Mixer
только к временной UI session или строить pad routing по имени Sample: один и тот
же Sample может быть назначен разным Pad с разными Outputs.

## Воспроизведение проверки

Из Linux/WSL, в `sd-mod-session`:

```sh
MPC_RUNTIME_ROOT='/path/to/local/runtime' python3 verify_mixer_native.py
```

Нужны AArch64 GCC, `qemu-aarch64`, точный локальный ELF и sysroot. При необходимости
указать `MPC_QEMU_SYSROOT`; значение по умолчанию соответствует существующим
проверкам проекта. В isolated worktree входные файлы берутся из runtime root,
результаты — только в локальный ignored `build/mixer-native/`.

Проверяются normal и UBSan, хеш ELF и неизменность тестируемых исходников.
Никакие firmware-файлы не меняются: native entry подменяется только в private
mapping тестового процесса. Полная сборка мода и SD package этим скриптом
не создаются; audio device/SD/штатный GUI не открываются.

## Открытые обязательные пункты Phase 0

- Полное покрытие путей индивидуального PCM и native route ownership.
- Порядок Master FX/gain и точка final PCM meter.
- Live Song traversal и offline SongThread: Part, repeats, tempo/time signature,
  event boundaries, active voices/tails, отмена и snapshot восстановления.
- CPU/RAM/underrun budget на MPC Sample при восьми шинах.
- Native Save/Save As/Load/Mod Off с Mixer extension.

До закрытия этих пунктов Mixer не включается в публичную сборку. Сам факт
успешной native ABI проверки не означает завершение полного Phase 0.
