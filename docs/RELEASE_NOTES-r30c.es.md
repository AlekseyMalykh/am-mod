# Am MOD r30c beta1 — MIDI, proyectos, sintetizador y Piano Roll

> **BETA — r30c-beta1.** Esta es una versión beta. **Aún falta probar la compatibilidad con otros mods (Stock, SampQOL, D.S.B y otros)**: no la uses con la única copia de proyectos importantes; haz antes una copia de seguridad. Por favor, informa de los problemas.

Para el **firmware original de Akai MPC Sample 1.3.0.12**. Continúa r30b; el firmware instalado no se reescribe. Los puntos marcados como *pendiente* han pasado las pruebas en host/emulador, pero aún falta comprobarlos en el MPC real.

## MIDI

- **Modos de MIDI Input** (B3 los alterna): Drum map, Channels, Selected pad e Instrument. Al cambiar de modo se conservan las asignaciones de canal; Shift+B3 restablece solo los ajustes del modo.
- **Channels:** cada canal MIDI recibe un pad y un rol: All pads, Tune, Chop o Synth. La grabación de Tune y Chop desde un teclado MIDI está confirmada en el MPC.
- **Selected pad:** K1 fija el canal de entrada (All o 1–16), K3 la nota raíz; el teclado toca Tune/16 Levels del pad seleccionado.
- **MIDI Tools → MIDI Buttons:** lista de botones y faders del controlador asignados a botones y mandos del MPC. B3 asigna (o pulsa el control del MPC), B2 borra la entrada seleccionada o cancela, B1 vuelve. Se pueden asignar faders; se admiten combinaciones Shift+botón.
- **Input Config → B3 MIDI In** abre los mismos ajustes de MIDI Input (B2 Map/Learn, B1 atrás).

## Proyectos y almacenamiento

- Las carpetas de usuario de Internal (Projects, Samples/User) se guardan de forma permanente. Las exportaciones de audio a Internal permanecen en la memoria interna.
- La recuperación recuerda el soporte original (Internal o SD) y la carpeta del proyecto. El primer guardado vuelve al mismo lugar y nunca sustituye un proyecto del mismo nombre de otro soporte. Si se desconoce el origen, la pantalla muestra **«Original location unknown; save a new copy»**.
- Al arrancar se carga el proyecto más reciente entre la tarjeta, Internal y el autoguardado temporal, sin importar qué firmware (Stock, SampQOL o Am MOD) lo guardó. Los datos de audio del proyecto se preparan para que el grabador nativo no devuelva un sample antiguo con el mismo nombre.

## Sample Record y sintetizador

- **B2 To Seq:** una toma de micrófono inserta la nota del pad de destino en la secuencia en reproducción en el momento de empezar. Con la reproducción detenida, la entrada espera a Play.
- **Sintetizador:** 8 voces (seno, sierra, cuadrada, triangular), ADSR y filtro paso bajo. Se toca por MIDI (modo Instrument o rol Synth) y desde los pads. Parámetros: Mixer, pad 13 → páginas K1–K3 (Tone, Envelope, Keys).
- **Ruta de entrada** (Mixer → Input → ROUTE → Input): *Clean* (señal directa a los pads), *Track* (por la pista Input y sus FX, valor por defecto) y *Synth* (el sintetizador es la fuente de la pista Input; *Mic* añade el micrófono encima). *Direct out* añade el sintetizador procesado a la salida cuando la monitorización está desactivada.
- **16 Levels del sintetizador** usa el mismo motor y los mismos colores que el 16 Levels del proyecto: Monochrome, Piano, Scale y Degrees, con la tonalidad y la escala del proyecto por defecto. Transpose mueve las notas a lo largo de la escala (grados en Degrees, semitonos en el resto); la tonalidad se mantiene y las luces de los pads siguen a las notas. En Sample Record el teclado empieza apagado y el botón 16 Levels lo enciende (el botón está iluminado mientras el teclado está activo).
- Pad Bank + mando restablece un parámetro del sintetizador a su valor por defecto. Las letras A–H de Pad Bank permanecen apagadas en la página del sintetizador, igual que en Master e Input.

## Efectos de Henry Skillz

- El Mezclador incorpora tres efectos de **Henry Skillz**: **HS Saturator**, **HS Bass Focus** y **HS Heat**. DSP de [hs-mpc-plugins](https://github.com/Holla-Back-Entertainment/hs-mpc-plugins), incluido con permiso del autor. Aún sin probar en el MPC.

## Piano Roll, Song y Mixer

- Piano Roll muestra los valores de K1–K3 sobre los mandos (desplazamiento, zoom, movimiento, longitud, velocity, cuadrícula).
- Mute en la fila seleccionada alterna el mute local del pad en la secuencia; una M roja marca las filas silenciadas y una SM roja aparece en las cabeceras de Piano Roll y Mixer.
- Ventana Song: tabla de repeticiones con los colores de los pasos; las luces de los pads del Mixer siguen a la pista seleccionada.

## Limitaciones

- El sintetizador, la nueva ruta de Input y la página MIDI Buttons se desarrollaron después de la última comprobación completa en el dispositivo y están *pendientes* de verificación en el MPC.
- Al guardar sobre un nombre existente, el diálogo Save nativo aún no muestra una advertencia.
- Un proyecto guardado por última vez por otro mod puede indicar una ubicación original desconocida; guarde una copia nueva.
- La carga/guardado cruzado de proyectos Internal (Am MOD, Stock, SampQOL) no está certificada por completo.

Véase también las [limitaciones conocidas](KNOWN_ISSUES.md) y el [registro de cambios](../CHANGELOG.md).
