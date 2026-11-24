# Instalar y actualizar Am MOD r30d-beta4

[Inicio](../README.es.md) · [Manual](USER_GUIDE.es.md) · [English](INSTALL.md) · [Русский](INSTALL.ru.md)

## Preparación

Necesitas un **Akai MPC Sample** con firmware **1.3.0.12** (la pantalla muestra 1.3.0). No se admiten otros modelos o versiones. Copia toda la tarjeta, especialmente `MPC-Sample`, proyectos, samples y ajustes. Se recomienda exFAT. FAT32 es experimental y no admite archivos de 4 GiB o más. Utiliza una etiqueta de volumen ASCII sin espacios, como `MPC`.

El mod ejecuta una copia de la aplicación en RAM sin reescribir el firmware. En el candidato r30d-beta4, **los proyectos, samples y exportaciones de audio de Internal permanecen en la memoria interna**; los demás datos del sistema siguen aislados. K3 selecciona Internal/External en los diálogos Save/Export. Los paquetes antiguos limitados a SD conservan sus restricciones. El sistema original y los mods antiguos pueden perder campos nuevos al guardar; trabaja con copias de los proyectos.

![Tres formas de instalación](media/r30c/install-es.png)

## Recomendado: instalador 1.5.1

1. Descarga `am-mod-r30d-beta4.zip`, `MPC-Sample-Mod-Installer.exe` y `SHA256SUMS.txt` de la [versión r30d-beta4](https://github.com/AlekseyMalykh/am-mod/releases/tag/r30d-beta4). No extraigas el ZIP para usar el instalador.
2. Guarda el proyecto en External. Conecta la tarjeta mediante un lector o Project → SD Card Access. Para la primera instalación usa el acceso USB original o un lector.
3. Abre el instalador, selecciona el ZIP, actualiza la lista y comprueba la unidad, etiqueta y capacidad. Pulsa Install.
4. El instalador valida el paquete, detecta la estructura, guarda el autorun anterior en `~/AM-Mod-Backups/`, copia y verifica los archivos. Esta copia no incluye tus proyectos. No desconectes durante la escritura.
5. Expulsa la unidad de forma segura desde el ordenador. En la pantalla SD de AM Mod pulsa B3 Disconnect y después B3 Return; en el sistema original sigue sus indicaciones.
6. Apaga y enciende el MPC. Copiar la actualización no sustituye la aplicación que ya está en RAM. Selecciona AM Mod en el menú de arranque y comprueba r30d-beta4 en **Project → Mod Settings → About**.

| Tarjeta existente | Destino de AM | Se conserva |
|---|---|---|
| Normal | `autorun/` | Proyectos y samples fuera de autorun |
| mpc-boot-thingy | `autorun/apps/am-mod/autorun/`, con `boot.entry` en la carpeta superior | Menú y otras aplicaciones |
| D.S.B Boot | `autorun/os/AM-Mod/` | Menú y otros sistemas |

D.S.B necesita el nuevo lanzador de AM y `sampleboot.inplace`; no borres ese archivo. Un paquete antiguo incompatible o una instalación incompleta de D.S.B se rechazan antes de escribir. Si aparecen marcadores de dos gestores, hay que resolver la ambigüedad.

El instalador admite un gestor **ya instalado**; no descarga ni instala D.S.B, mpc-boot-thingy o SampQOL. Obtén esos proyectos de sus autores. El ZIP de AM no incluye archivos de SampQOL. Desde AM, *Mod Settings → About → Exit to bootloader* vuelve al menú de D.S.B (véase [BOOTLOADERS.es.md](BOOTLOADERS.es.md)); con otros gestores arranca el firmware original.

## macOS / Linux y terminal

Instala Python 3 y utiliza `installer/mpc_mod_installer.py` del repositorio. La detección de estructuras es la misma.

```bash
python3 installer/mpc_mod_installer.py cards
python3 installer/mpc_mod_installer.py install am-mod-r30d-beta4.zip --card /Volumes/MPC --dry-run
python3 installer/mpc_mod_installer.py install am-mod-r30d-beta4.zip --card /Volumes/MPC
python3 installer/mpc_mod_installer.py status --card /Volumes/MPC
```

En Windows usa `E:` u otra letra real de tu tarjeta; en Linux, su punto de montaje. Dry run no escribe nada. Status comprueba los archivos gestionados. La interfaz gráfica requiere Tk; la terminal no.

## Instalación manual

Con el MPC apagado o la tarjeta compartida de forma segura, guarda los archivos anteriores. En una tarjeta normal reemplaza **toda** la carpeta `autorun/` por la del ZIP. Evita `autorun/autorun/`. No mezcles la biblioteca de una versión con el lanzador de otra.

En **D.S.B**, copia el **contenido** del autorun del ZIP a `autorun/os/AM-Mod/`, incluido `sampleboot.inplace`. En **mpc-boot-thingy**, usa preferiblemente el instalador: crea el autorun anidado y boot.entry. No sustituyas el autorun raíz del gestor por el de AM.

## Actualizar, desinstalar y recuperar

- Actualiza con el mismo procedimiento; el instalador detecta otra vez el gestor. Expulsa y reinicia.
- Para quitar AM instalado con esta herramienta: `python3 installer/mpc_mod_installer.py remove --card /Volumes/MPC`. En modo de menú elimina los archivos gestionados de AM y conserva el gestor y otros sistemas. Mantiene las copias de seguridad.
- Para arrancar de emergencia con el sistema original, apaga el MPC y retira la tarjeta. Para usarla con Stock, desactiva/renombra autorun en el ordenador; si tiene gestor, usa su opción Stock.
- Para restaurar, recupera una copia completa y coherente de autorun con el MPC apagado. No mezcles versiones. Recupera aparte la copia de proyectos si hace falta.
- Si no arranca, revisa firmware, etiqueta, formato, carpetas anidadas y extracción completa. Indica versión, gestor y pasos; el instalador prepara un informe diagnóstico. No incluyas proyectos privados si no quieres compartirlos.

## Primera prueba

Usa una copia de un proyecto pequeño. Toca un pad, abre Mixer y modifica y desactiva un efecto. Guarda en Internal y External con K3; apaga por completo y vuelve a cargar. Comprueba también un sample guardado y una exportación de audio. Prueba MIDI, importación y Sample Swap por separado. El paquete final y los gestores externos aún necesitan pruebas físicas.
