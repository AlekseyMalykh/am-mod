# Передача AM Mod r30c — 21 ноября 2026

Продолжать этот checkout `releases/r30c-public`, ветка `codex/release-r30c`. Предыдущая база — `50672ec5c21c392055bbead4ca8489f6046038a9`, содержащая работу Claude. Не заменять checkout старым `am-mod-illustrated-fx/sd-mod-session` и не выполнять reset/clean поверх новых изменений.

## Что завершено

- Shift+Sample → Input Config → B3 MIDI In открывает общие MIDI Input settings, B2 Map/Learn; B1 возвращает в контекст Input Config.
- Recovery AMR2 сохраняет Internal/SD и относительную папку проекта, читает AMR1. Первый Save восстанавливает исходную цель; последующие изменения имени/носителя пользователем сохраняются. Недоступная цель не подменяется другим носителем.
- Internal Projects и Samples/User сохраняются постоянно. Internal Export больше не является bind-подменой каталога SD. Сняты старые перехваты K3 при наличии проверенной Internal capability; legacy-сеансы без неё остаются SD-only.
- В исходниках MIDI Input B3 Mode переключает Drum map → Channels → Selected pad без потери назначений каналов. Shift+B3 сбрасывает настройки режима, отдельно от Map/Learn. Selected pad: K1 — входящий канал All/1–16, K3 — root; игра Tune/16 Levels выбранным пэдом. Для чопов по каналам используется Channels.
- Предыдущие изменения Piano Grid value, row mute/red M, red SM и работа Claude сохранены. Пользователь подтвердил, что спектр больше не перекрывает линии; не возвращать этот пункт в список ошибок без новой регрессии.

## Что установлено на MPC

Полный пакет **r30c-storage-dev2** установлен в D.S.B `autorun/os/AM-Mod`, все файлы проверены. Пользователь после проверки K3 сообщил «вроде ок» — это предварительное подтверждение K3, а не всех сценариев Save/Load.

- Package SHA256: `d7dcf9ba1fcb83c8d64dd0d93429513bb1734187ab54b39482feb87f4f5df57d`.
- Installed library SHA256: `128fd2fcfe28be469574a59fe2bab9375dfe53d68112e9fbe0bb0cafc9962489`.
- Backup: `C:/Users/Алексей/AM-Mod-Backups/20261121-111636`.
- D.S.B, другие OS и `mpcqol.conf` не изменились; receipt вне checkout: `releases/r30c-storage-dev2/card-installation.json`.

**Новое переключение B3 Mode/Selected pad в этот ZIP не входит.** Его библиотека из текущих исходников: `3cbff9350395cb7e8b4dafbf585a48518c8f83d7ed6a587d9d3ad4b06433a3b1`, build directory `build/midi-selected-ui`. Header пока сообщает storage-dev2: перед следующей упаковкой согласовать новую версию и актуальные proofs.

## Проверки и эмулятор

Linux mirror: `/home/user/am-mod-r30c-build/sd-mod-session`. Канонические исходники редактируются в Windows checkout. Внешний utility `releases/sync_r30c_build_sources.py` синхронизирует source без удаления build cache. Изменения `lab/` синхронизировать отдельно.

Пройдены targeted `verify_project_recovery.py`, `verify_project_exit.py`, `verify_midi_tools.py`, `verify_input_config_ui.py`, `verify_internal_storage.py`, `verify_internal_controller.py --namespace`, `verify_internal_flush.py`, `verify_early_startup.py --project-only`, `verify_song_ui.py`; полные builds с constructor и production routes. MIDI fixtures проверены host/ARM64 normal и UBSan, включая captured release при смене пэда/режима и codec roundtrip. Полный исторический early-startup suite не заявлять.

В full-system VM через штатный интерфейс сохранены Internal XPJ с ProjectData, сэмпл и WAV Mixdown. После штатного выключения и холодного запуска байты всех трёх совпали. Реальная exit-copy содержит AMR2 Internal + `Projects/User/project.xpj`; восстановление исходной идентичности подтверждено журналом. WAV Mixdown на External сохранён отдельно, Internal файл не изменён. K3 повторно проверен в native Save Project и Save Sample в обоих положениях. Новое B3 Mode/Selected pad ещё не проходило full-system GUI и аппаратную приёмку.

Актуальная VM: `/opt/mpc-sample-lab/out/run/r30c-storage-dev2`. Проверить процессы перед стартом; не запускать вторую VM и не монтировать live images. Wrapper `sd-mod-session/lab/internal_storage_vm.py stage/run r30c-storage-dev2`; stage только при выключенной VM. Custom QEMU `/opt/mpc-sample-lab/qemu/bin/qemu-system-aarch64`; ports 8765 panel, 2022 SSH, 5554 QMP. Последнее состояние GUI — native Save Sample, External, без выполнения записи в этом повторном тесте.

Внешние локальные доказательства: `releases/r30c-storage-verification/`, `releases/r30c-selected-midi-verification/`. Source SHA сверены с canonical и Linux mirror. Snapshots `releases/checkpoint-20261121-storage` и `releases/checkpoint-20261121-selected-midi` неизменяемые. Firmware, VM disks, SSH keys и private diagnostics не публиковать.

## Следующие задачи

1. Проверить новый MIDI экран в full-system GUI, подготовить согласованную следующую сборку. Сохранить drum map и channel → pad Tune/Chop; не свести канал к единственной обычной ноте пэда.
2. Реализовать Record Sample → sequence workflow, сверив поведение SampQOL.
3. Показать значения остальных параметров Piano; проверить Erase+Pad как удаление всех событий пэда с подтверждением, включая Tune/Chop и Undo.
4. Проверить сброс sequence-local mute при копировании пэда/секвенции, cancel/overwrite/Undo. Пользователь хочет сброс мьютов в обеих копиях; это нужно точно описать в руководстве.
5. MIDI `.mid` export пока SD-only. Audio Mixdown проверен на обоих носителях; Stems destination покрыт software fixtures, native Stems render отдельно проверить.
6. Проверить cross-mod Load/Save Internal проектов в AM, Stock и SampQOL на MPC. Не считать проверку AM cold recovery подтверждением cross-mod совместимости.
7. Сверить исправления r30b–r30e, FX/Mixer/tuner/UI и подготовить публичный r30c: EN/RU/ES руководства, иллюстрации, release notes. ES user guide ещё требует завершения.
8. Согласовать packaging с новой работой Claude Mod Off → D.S.B из `autorun.template.sh`: dev2 использует предыдущую базу загрузчика и не подтверждает эту функцию. Не потерять аппаратно подтверждённый USB shared-mount fix.

VST, эффекты Henry и общий протокол возврата в D.S.B/HS Remote — будущий roadmap. Не выдавать за реализованное и не отправлять сообщения авторам без инструкции пользователя.

Подробная историческая передача сохранена вне checkout: `releases/CLAUDE_HANDOFF_R30C_RU.txt` и `releases/AM_MOD_CHECKPOINT_2026-11-21.txt`. Новые дополнения в начале имеют приоритет над старыми статусами ниже.
