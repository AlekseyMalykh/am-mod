# Mixer archive bridge — experimental

04.11.2026; `feature/mixer-song-stems`.
Реализация: [`mixer_archive.h`](../sd-mod-session/src/mixer_archive.h),
[`mixer_archive.cc`](../sd-mod-session/src/mixer_archive.cc).
Модель/формат: [MIXER_DATA_MODEL.md](MIXER_DATA_MODEL.md).

Модель Mixer связана с существующими Save/Load/New Project callbacks под
**`MPC_MIXER_ARCHIVE`**. Флаг включается только builder экспериментального
`build/mixer-candidate/phone-input.so`; обычные builder/release flags не изменены.
Mixer UI и live audio routing по-прежнему выключены. Это проверенный bridge
метаданных, но ещё не подтверждённый полный XPJ Save/Save As/Mod Off на MPC.

## Один project owner

Владелец хранит immutable snapshot `mixer_project`, mode, revision и при
необходимости исходный opaque payload. UI/controller получает копию с revision;
`mixer_archive_replace()` принимает изменение только при совпадении revision.
Загрузка, New Project и другое принятое изменение делают прежний edit ticket
устаревшим. Нельзя сохранить UI-копию поверх нового проекта.

Snapshot/replace/save принадлежат non-realtime control/model стороне. Короткий
mutex защищает только указатель на immutable State. Во время native callbacks
mutex не удерживается; аудиопоток эти функции не вызывает. Будущий audio adapter
получает отдельную подготовленную RT-модель. Общая project queue обязана
сериализовать Load/New/edit; читать её состояние из paint для расчёта аудио нельзя.

| Mode | Load | Save / редактирование |
|---|---|---|
| Legacy | Нет `amMixerV1`: unity, все 128 Outputs = Master, пустые FX/Scenes | Поле не добавляется до первого реального изменения Mixer |
| Known | Валидный V1 проходит полный decode | Сериализуется snapshot модели; revision проверяется при изменениях |
| Opaque | Неизвестная версия или повреждённая **строка** extension | Mixer read-only; исходные байты сохраняются без изменений, включая NUL и строки больше V1-лимита |

Opaque string копируется по **объявленной длине native JSON**, не через strlen
или ограничение в 64 KiB, поэтому будущие данные не обрезаются. JSON value должен
оставаться строкой, как определено V1. Дубликат ключа, неверный тип/структура
archive либо allocation failure дают ошибку загрузки/сериализации; нельзя
молча выпускать успешный save без сохранённого extension.

## Callback flow

```text
H_MUSIC_READ -> mixer_archive_read
  -> existing music_archive_read -> original native Read
       H_MUSIC_LOAD -> mixer_archive_load
         -> existing music_archive_load -> original native Load
         -> stage Mixer field from native object frame
  -> native success: publish staged State; native failure: keep previous owner

H_MUSIC_SAVE -> mixer_archive_save
  -> immutable model/opaque snapshot and all own allocations prepared first
  -> existing music_archive_save -> original native Save fields + mpcSampleModV1
  -> native named-string writer adds amMixerV1
  -> acknowledge serialization only after writer returned

confirmed New Project -> existing native New + music_reset + Mixer reset
```

Native serializer продолжает владеть object framing, compression, files и commit.
Bridge не переписывает XPJ, не создаёт sidecar и не назначает новый путь записи.
Счётчик `mixer_archive_saved_count()` означает завершённую **сериализацию поля**,
а не durable запись файла. Политика native write futures и SD остаётся штатной.

Внутри Load ошибки capture сохраняются в C++ scope и передаются после возврата
старого `music_archive_read()`: через его C malloc/TLS scope новая exception не
пробрасывается. RAII очищает Mixer transaction при успехе/ошибке. Nested read
отклоняется до запуска второго native Read. Разобранный Mixer State публикуется
только после успеха native Read.

Это гарантия transaction **Mixer owner**, не обещание rollback всего native
проекта. Если native Read успел изменить свои объекты до ошибки optional field,
восстановление native проекта определяется штатной загрузкой. Этот случай,
native disk-save exception handling и concurrent project operations нужно
проверить в полном приложении до включения флага в аппаратный/публичный пакет.

## Проверенный ABI

- Input adapter следует четырём ссылкам wrapper+8 к JSONInputArchive.
- Frame находится по `archive+0x150 - 0x38`; object type = 1, member stride = 48 B,
  каждое имя/value — 24 B. Дерево не изменяется.
- String bit10, inline bit12; long string length u32+0, pointer+8. Для inline
  `length = 21 - byte[21]`. Проверяется формат до чтения значений.
- Native writer `1d27d90` получает named field и std::string-compatible view
  `{data,size,...}`; пишет ровно size bytes с JSON escaping.
- Настоящий native reader `1457dd0` подтвердил чтение обычного V1 payload из
  fixture frame. Расположение callback в текущем UI/сохранении не изменяется.

Pinned ELF и адреса — как в [native audit](MIXER_NATIVE_AUDIT.md). Это не API
других прошивок; будущий install должен сохранить существующую signature/ABI
проверку и запретить работу на несовпадающем ELF.

## Выполненные проверки и границы

```sh
MPC_RUNTIME_ROOT='/path/to/runtime' python3 verify_mixer_archive.py
MPC_RUNTIME_ROOT='/path/to/runtime' python3 build_mixer_core_candidate.py
```

`verify_mixer_archive.py` выполняет host, host ASan+UBSan, ARM/QEMU и ARM UBSan
для owner/transactions. Дополнительно два ARM native-теста выполняют настоящие
writer/escaping/reader вместе с production `music_archive` и `music_settings`.
Object framing/key-prefix/output-service — fixtures; full XPJ parser/compressor,
filesystem и physical device в этом тесте не исполняются.

Проверены старый проект без нового поля; общий model round-trip; Pad Output/Scene;
устаревшие edit tickets; immutable snapshot при native callback; opaque inline,
пустые, >64 KiB и embedded-NUL строки; duplicate/non-string/invalid frame; failed
native Read/Save и writer exception без ложного acknowledgement; восстановление
scope после ошибки и concurrent snapshot/replace. Native writer подтвердил
сосуществование штатного sentinel, старой music extension и нового Mixer field.

Builder отдельно компилирует `phone_native.c` с `MPC_MIXER_ARCHIVE`, остальные
baseline objects остаются собранными обычным builder. Проверяется полная линковка
с `--no-undefined` и реальный preload constructor в ARM/QEMU. Проверка не запускает
native audio main и не делает archive round-trip на SD.

Отчёты: ignored `build/mixer-archive/verification.json` и
`build/mixer-candidate/compile-verification.json`. Следующий интеграционный gate:
полный native project Save/Save As/Load/failed load/Mod Off с повторным открытием,
включая старый проект и неизвестную extension. Пока аппаратного пакета нет.
