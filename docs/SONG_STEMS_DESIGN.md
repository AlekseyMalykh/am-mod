# AM Mod — Song Mode и экспорт стемов

Статус: проект и native-аудит, 06.11.2026. Ветка `feature/mixer-song-stems-r27r`
(Mixer `8572893`, перенесённый на r27r `6335237`).
Связанные документы: [MIXER_DESIGN.md](MIXER_DESIGN.md),
[MIXER_DATA_MODEL.md](MIXER_DATA_MODEL.md), [MIXER_NATIVE_AUDIT.md](MIXER_NATIVE_AUDIT.md).

Решения пользователя (06.11):

- сначала перенести Mixer на r27r — **сделано**, см. §6;
- стемы группируются по **Track A–H Mixer'а** (плюс Master Direct и mixdown);
- экспортируются и **Sequence**, и **Song**;
- для Song в первой очереди: Song → Sequence, удобное редактирование частей,
  мьюты треков по частям.

Всё ниже — статический анализ ELF 1.3.0.12
(SHA-256 `b6584cba…b508`). ARM/QEMU-проверок и проверки на устройстве ещё нет.

## 1. Как это сделано в SampQOL OS (для сравнения)

SampQOL OS 0.1.5 — закрытый мод (`mpcqol.so` + autorun с SD, лицензия
«All rights reserved»). Кода в репозитории нет, поэтому сравниваем только
поведение по changelog и manual:

| SampQOL | Как там | Что берём / делаем иначе в AM Mod |
|---|---|---|
| Export | Shift+Pad16 → B3. Mixdown + по WAV на каждый drum track **секвенции**, 24-bit/44.1 kHz | Источник — Sequence **и** Song; стем = шина Track A–H Mixer'а |
| Стемы | «С submix-эффектами трека, без master» | То же: стем = post-FX/Level/Pan/Mute Track bus до суммы; Master FX только в mixdown |
| Хвост | Настраиваемый tail для реверба/дилея | Тот же tail для всех файлов; общая длина и общий старт |
| Имена/папки | `Samples/User/Export/<проект>/`, опция BPM в имени, разделитель `_`/` - `, Keep Both/Replace | Взять ту же схему (§4.4) |
| Отмена | «Нельзя остановить экспорт» | Делаем отмену: удалить неполные файлы |
| Мьюты по частям | Мьюты трека хранятся в каждой секвенции; Song Export → Sequence сохраняет их. **Известный баг:** стоковый MPC Sample играет такую секвенцию без мьютов | Мьюты храним как Mix Scene части Song. При Song → Sequence **не пишем mute-события**, а просто не копируем ноты заглушённых треков в новую секвенцию — результат одинаково звучит в стоке |
| Resample Pad | Отдельная функция, tail 0–10 с | У AM Mod уже есть Fast Resample / render |

## 2. Что уже есть в стоковом Song Mode

По User Guide 1.3 (стр. 44–45): Shift+Pad12 открывает список шагов; пэдом
выбирается секвенция, B3 вставляет её после выделенного шага, B2 удаляет,
энкодер/−/+ листают, Shift+Play играет с выделенного шага. B1 Export:
B2 — audio mixdown, B3 — **новая секвенция** (Song → Sequence уже есть,
лимит 128 тактов для редактирования длины).

**Повторов (repeats) в стоковом UI нет**, хотя модель их хранит (§3).

## 3. Native Song API (найдено 06.11)

В ELF есть Lua-обёртка текущей песни (`ScriptableCurrentSongWrapper`,
регистрация в `13da5e0`): `SetCurrentSong`, `SetName`, `PushStep(sequenceIndex, repeats)`,
`InsertStep(stepIndex, sequenceIndex, repeats)`, `DeleteStep`, `ModifySequence`,
`ModifyRepeats`, `Clear`; getters шага `SequenceIndex`, `SequenceName`,
`Repeats`, `NumBars`, `Tempo`. Обёртка лишь вызывает общие native-функции
редактирования, которые и стоит использовать моду:

| VA | Действие (из лямбд обёртки) | Аргументы |
|---|---|---|
| `19a0b50` | Конструктор 8-байтного editor-контекста: `*ctx = sequencer` | `(ctx*, sequencer*)` |
| `19a26b0` | Push Step (в конец) | `(ctx*, song*, seq, repeats)` |
| `19a2d20` | Insert Step | `(ctx*, song*, stepIndex, seq, repeats)` — порядок по лямбде `13d7a10`, проверить |
| `19a1880` | Delete Step | `(ctx*, song*, stepIndex)` |
| `19a29e0` | Modify Sequence шага | `(ctx*, song*, stepIndex, seq)` |
| `19a20d0` | Modify Repeats шага | `(ctx*, song*, stepIndex, repeats)` |
| `19a3a40` | Clear | `(ctx*, song*)` |

Каждая функция создаёт `make_shared` команду (0x80 байт для Repeats) и отдаёт её
в менеджер команд `sequencer+0x12c0` через `19d0a00`. Значит, изменения
**штатно undoable** и проходят тем же путём, что и стоковые правки — мод не
пишет в вектор шагов сам.

Выбор текущей песни (одинаково во всех лямбдах): `sequencer = owner+0x98`;
`i = [sequencer+0xb38]`; при `i < 0` — `song = [sequencer+0x1270]`, иначе
`song = songs[i]`, `songs = [sequencer+0x1260]`, count `[sequencer+0x126c]`.
Шаги песни — вектор 8-байтных записей `[song+0x30 .. song+0x38)`; поле repeats
подтверждено тем, что `ModifyRepeats` и getter `Repeats` работают с тем же шагом.
Остальные имена команд из таблицы `19d1800`: Insert / Replace / Move / Split /
Set Song Step / Set Song Step Repeats / Delete / Add / Convert Song To Sequence.
Builders для Move/Split ещё не найдены; до этого Move = Delete+Insert в одной
undo-группе (проверить, есть ли группировка в `19d0a00`).

Открыто: как получить `sequencer*` в UI-потоке мода (кандидаты — уже
используемые объекты проекта в `phone_native.c`/`music_archive.c`), потоковая
безопасность вызова из UI Timer, поведение при игре Song.

## 4. Экспорт стемов

### 4.1 Поведение

Вход: из Song (B1 Export → новый пункт **Stems**) и из Seq (тот же экран для
текущей секвенции). Выход:

- `Mixdown.wav` — финальный Master (с Master FX и Level);
- `A … H` — по файлу на каждый **непустой** Track (есть назначенные Pad и
  хотя бы одна нота в диапазоне), post-FX/Level/Pan/Mute/Solo, без Master FX;
- `Direct` — Master Direct (Pad'ы с Output = Master), если не пуст.

Все файлы: один frame origin, одна длина (длина + tail), WAV 24-bit, частота
движка (44.1 kHz). Mute/Solo экспортируются как есть («что слышишь — то и
получаешь»); отдельной кнопки «все треки» не делаем в первой версии.
Сумма стемов не обязана совпасть с mixdown при нелинейных Master FX — это
пишется на экране.

### 4.2 Рендер — один проход

Не делать N проходов через последовательное Solo (дольше, и рандомизированные
события/LFO разойдутся). План:

1. Использовать штатный offline mixdown: Song `18c6330`, Seq `18c62f0`, общий
   `18c5a00`, worker `Mixdown::SongThread` (`18c32c0`). Fast Resample уже
   перехватывает Seq-slot (`7167220`) — переиспользовать этот опыт.
2. **Обязательная проверка:** проходит ли offline-рендер через тот же
   DrumProgram path (`286be30` → `286bf44`), где стоит Mixer adapter. Если да —
   adapter в offline-контексте пишет Track/Direct блоки в стем-писатели, а
   штатный writer пишет mixdown. Если нет — нужен отдельный offline adapter.
3. Писатели: блоки копируются в lock-free кольцо, запись на SD — в отдельном
   потоке (как `resample_worker.c`), без IO в render-потоке.

### 4.3 UI

Экран в стиле Mixer (320×240, верхние B1–B3): B1 Cancel, B2 Options, B3 Do It.
Options: Tail (0–10 s, K1), BPM в имени (Off / Mixdown / All, K2),
Существующие файлы (Keep Both / Replace, K3). Прогресс-бар и Cancel во время
рендера; после — путь к папке.

### 4.4 Имена

`Samples/User/Export/<Project>/<Song|Seq name>[_<BPM>BPM]_<A…H|Direct|Mix>_<Track name>.wav`,
например `MyTrack_90BPM_C_Drums.wav`. Недопустимые символы FAT/exFAT заменяются `_`.

## 5. Song: улучшения первой очереди

### 5.1 Редактирование частей

Новый слой поверх стокового Song-экрана (стоковые B1/B2/B3 сохраняются):

| Жест | Действие | Native |
|---|---|---|
| K1 на выделенном шаге | Repeats 1…99 | `19a20d0` |
| Shift+B3 | Replace: заменить секвенцию шага выбранной пэдом | `19a29e0` |
| Shift + −/+ | Move шага вверх/вниз | Move builder, иначе Delete+Insert |
| Shift+B2 | Duplicate шага (Insert той же секвенции после) | `19a2d20` |
| Undo | Штатный undo | менеджер `+0x12c0` |

На экране у каждого шага: номер, имя секвенции, `×N`, такты, начало в тактах.
Окончательную раскладку согласовать с пользователем до кода — в стоке
B1–B3 и Shift уже заняты частично.

### 5.2 Мьюты треков по частям

Используем готовые **Mix Scenes** модели Mixer (до 128, ключ — стабильный
64-bit Song Part id, маска присутствующих полей). Мьют части = Scene, где
задано только поле Mute для Track A–H. Применяет playback owner на границе
шага, не UI. Часть без Scene играет обычный микс проекта.

Нужен **стабильный Part id**: в native-шаге его нет (только seq + repeats).
Мод держит параллельный массив id и обновляет его в хуках Insert/Delete/Move
(все правки — и стоковые, и наши — идут через одни builders, §3). При
расхождении длины (проект из стока) id переназначаются по позиции и Scenes
таких частей помечаются «требует проверки», а не применяются молча.

Жест (предложение): в Song выделить шаг → Shift+Pad9 открывает Mixer в режиме
Scene; Shift+B1 Mute работает для этой части; выход возвращает в Song.

### 5.3 Song → Sequence с мьютами

Стоковая конверсия остаётся. Добавляем вариант «с мьютами частей»: после
штатной конверсии в **новой** секвенции удаляются ноты Pad'ов, у которых
Output = Track, заглушённый Scene этой части (по диапазону тиков части).
Исходные секвенции не меняются, результат одинаково звучит в стоке —
в отличие от SampQOL, где стоковый MPC Sample игнорирует такие мьюты.

## 6. Перенос на r27r (выполнено)

`feature/mixer-song-stems-r27r` = merge `8572893` в `6335237`. Конфликты:
`phone_events.{c,h}`, `phone_native.c` (15 мест), `verify_pad_colour_rgb.py`.
Везде сохранены обе стороны: r27r (knob reset, tail preview, bank pads,
rear routing, splash, recovery) остаётся как в релизе; Mixer добавляет свои
блоки под `MPC_MIXER_*`. Новые `PHONE_MIXER_*` добавлены **после**
`PHONE_REAR_PAD`, значения r27r не сдвинуты.

Проверено: clang aarch64 `-fsyntax-only` для всех исходников с флагами r27r и
r27r+Mixer (ошибки только в файлах, зависящих от arch-заголовков, без изменений
относительно r27r). **Не проверено:** полная сборка GCC, bootstrap preload,
`verify_*` в QEMU — нужен WSL-стенд. Скрипты Mixer проверяют имя ветки
`feature/mixer-song-stems`; для новой ветки это условие нужно обновить.

## 7. Порядок работ

1. WSL: собрать r27r+Mixer, прогнать `build_r27r.sh` и `verify_mixer_*`.
2. Аудит offline-рендера (§4.2 п.2) в QEMU: попадает ли SongThread в adapter.
3. Song native: получить `sequencer*` в UI, ARM-тест вызовов §3 на частной
   копии ELF (как в `verify_mixer_native.py`).
4. Stems Seq → Stems Song → Song Repeats/Replace/Move → Scenes/мьюты →
   Song → Sequence с мьютами. Каждый этап — отдельный коммит и проверки.
5. Аппаратная проверка пользователем; без публикации релиза.
