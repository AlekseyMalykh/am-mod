# Notice / Правовая информация

**English.** Am MOD is an unofficial community project. It is not affiliated
with, endorsed by or supported by inMusic Brands, Inc. or Akai Professional. Akai
Professional and MPC are trademarks of inMusic Brands, Inc.; they are used here only to
name the device the mod runs on.

- This repository and the release packages contain **no firmware, no proprietary Akai application code, no
  factory content and no Akai artwork**. The mod copies the stock app that is already on
  your MPC Sample into RAM and applies its own small patches to that copy each time it
  starts. The installed firmware and boot loader are never written.
- Addresses, structure layouts and protocol notes in the documents were found by
  examining a lawfully obtained copy of firmware 1.3.0.12, for interoperability of the
  mod with that firmware.
- The release packages include `xdg-dbus-proxy` (LGPL-2.1-or-later), unmodified Debian
  binaries, together with their copyright file and full source under `licenses/`.
- r30b also includes a RAM-only named USB audio function derived from the official
  inMusic GPL Linux kernel source. Its corresponding source, kernel config, build
  transformation and licence are included in `SOURCE/usb-audio-names/` in the ZIP.
  The complete matching upstream kernel source is published at
  https://github.com/inmusic-software/az0x-open-source/tree/mpc-sample/1.3.0; see
  [build inputs and exact hashes](docs/BUILD.md). This GPL component is separate
  from the MIT-licensed mod code and does not contain the proprietary MPC app.
- Screenshots and GIFs show the stock app with the mod running, for documentation.
- Use at your own risk; see the warranty disclaimer in [LICENSE](LICENSE).

**Русский.** Am MOD — неофициальный проект сообщества. Он не связан с
inMusic Brands, Inc. и Akai Professional, не одобрен и не поддерживается ими. Akai
Professional и MPC — товарные знаки inMusic Brands, Inc.; здесь они используются только
для указания устройства, на котором работает мод.

- В репозитории и пакетах релизов **нет прошивки, проприетарного кода приложения Akai, заводского контента и
  графики Akai**. Мод при каждом запуске копирует в RAM штатное приложение, уже
  установленное на вашем MPC Sample, и вносит в эту копию собственные небольшие патчи.
  Установленная прошивка и загрузчик не перезаписываются.
- Адреса, структуры и описания протоколов в документах получены изучением законно
  полученной копии прошивки 1.3.0.12 для совместимости мода с этой прошивкой.
- Пакеты релизов содержат `xdg-dbus-proxy` (LGPL-2.1-or-later) — неизменённые бинарные
  файлы Debian, вместе с файлом авторских прав и полными исходниками в `licenses/`.
- В r30b также включена RAM-функция имён USB-аудио на основе официального
  GPL-ядра inMusic. Соответствующие исходники, конфигурация, преобразование
  сборки и лицензия находятся в `SOURCE/usb-audio-names/` ZIP. Полное исходное
  дерево ядра публикуется inMusic; [точные входы и хэши](docs/BUILD.md). Этот
  GPL-компонент отделён от MIT-кода мода и не содержит проприетарного приложения MPC.
- Снимки экрана и GIF показывают штатное приложение с работающим модом и служат
  документацией.
- Используйте на свой риск; см. отказ от гарантий в [LICENSE](LICENSE).
