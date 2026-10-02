# Init Notes (carga de .rc en el init de CM7/GB)

## Qué lee `init` de verdad

`androidboot.hardware=huawei` viene en la cmdline del bootloader (`ro.hardware=huawei`), así que:

| Proceso | Archivos, en orden |
|---|---|
| `init` | `/init.rc` → `/init.huawei.rc` → `/init.target.rc` |
| `ueventd` | `/ueventd.rc` → `/ueventd.huawei.rc` |

`ueventd.msm7x27a.rc`, `init.goldfish.rc` y `ueventd.goldfish.rc` están en el ramdisk pero no se leen (no coinciden con `ro.hardware`).

## Reglas de precedencia (verificadas en el código)

- **Servicios duplicados: gana la PRIMERA definición.** La segunda se ignora entera, opciones incluidas (`user`, `group`, `socket`, `oneshot`...), con `init: ... ignored duplicate definition of service`.
- **ueventd: gana la ÚLTIMA regla** para la misma ruta: `get_device_perm()` recorre la lista al revés (`system/core/init/devices.c`, `list_for_each_reverse`). `ueventd.huawei.rc` pisa a `ueventd.rc`.
- **Mismo trigger en varios archivos:** las acciones corren en orden de parseo. El `on boot` de `init.rc` termina con `class_start default`, así que cualquier cosa que deban heredar zygote y los servicios (por ejemplo `setrlimit`) va en **`on early-boot`**, no en `on boot`.
- **Triggers que existen en este init:** `early-init`, `init`, `early-fs`, `fs`, `post-fs`, `boot-pause`, `early-boot`, `boot`, y `property:*`. **`post-fs-data` NO existe** (llegó en ICS).
- **`import` es un COMANDO** (`do_import` en `builtins.c`), no una sección. Fuera de un `on ...` el parser lo descarta sin avisar. Además, aunque se ejecutara dentro de una acción, las acciones `fs`/`boot` del archivo importado ya no se encolarían, porque `init.c` encola todos los triggers al arrancar.
- **Comandos fuera de una sección** (cabecera de un archivo sin `on ...`) se descartan.

## Bug corregido (2026-10-02): `init.y210.rc` e `init.mem.rc` nunca se cargaban

`init.huawei.rc` hacía `import /init.y210.rc` e `import /init.mem.rc` al principio, fuera de toda sección, así que nunca se parsearon. Pruebas: `init` no reportaba los duplicados de `init.y210.rc` (sí los de `init.huawei.rc`), ninguno de sus 21 servicios exclusivos existía, no se montaban `/cust` ni `/data/HWUserData`, y `init.mem.rc` pone `ro.HOME_APP_ADJ 3` pero el teléfono tenía 6.

En el stock ese contenido estaba dentro de su `init.rc`. Además, el `init` de Huawei está modificado y carga `/init.mem.rc` y `/init.highmem.rc` por su cuenta (aparecen en los strings del binario). El de CM7 no hace nada de eso.

Tampoco corría la sección `on post-fs-data` de `init.huawei.rc`. `/data/radio` existía solo porque un `mkdir` duplicado en un trigger de propiedad lo cubría.

### Qué se migró a `init.huawei.rc` (con los valores del `init.rc` stock)

| Qué | Dónde | Validado |
|---|---|---|
| `setrlimit 7 2048 2048` (archivos abiertos) | `on early-boot` | `init` y zygote: 2048 (antes 1024) |
| `rmem_max` / `wmem_max = 1220608` | `on early-boot` | `/proc/sys/net/core/*`: 1220608 (antes 110592) |
| `/data/drm`, `/data/drm/rights` (`system:sdcard_rw`) | `on post-fs` | Creados. Los necesita `libdrm1` (OMA DRM v1), que abre `/data/drm/rights/*` sin crear el directorio |
| `/data/radio` | `on post-fs` (antes `post-fs-data`, que no corría) | Creado |

### Qué se descartó y por qué

- `net.tcp.buffersize.hsdpa/hspa/lte/evdo_b` de `init.y210.rc`: **el stock del Y210 no define hsdpa/hspa** (en HSPA usa `default`, igual que CM7). lte/evdo_b no aplican a este radio.
- Cabecera de `init.y210.rc` (`export BOOTCLASSPATH` con `qcnvitems.jar`/`qcrilhook.jar`, que no existen): **rompería Dalvik**.
- `on cust`: usa comandos propios del init de Huawei (`custsymlink`, `custdircopy`) que este init no tiene.
- `on emmc-fs`: es para eMMC; el Y210 es MTD.
- Montajes de `/cust` y de la SD virtual `/HWUserData` (`sd.img` de 18 MB en `userdata2`): funcionalidad de Huawei. El `vold` de GB maneja una sola SD. Quedaron fuera a pedido.
- `/data/misc/sensors`, `/data/system/sensors`, `/data/huawei_hwvefs`, `/cache/huawei_ota`: nada en `/system` los referencia (`hwvefs` no existe).
- `chown system` de sysfs de `mmi_key_dev`/`msm_hsusb`: solo los usan las apps de fábrica (todavía no portadas).
- Servicios de `init.y210.rc`: los que arrancarían solos no tienen binario (`nfc_check`, `test_diag`, `filebackup`, `callife`, `cplusw`); el resto ya está en `init.rc`.

### Limpieza

- Fuera del ramdisk: `init.y210.rc`, `init.mem.rc` e `init.huawei.usb.rc` (este último tampoco lo cargaba nadie; sus triggers de adb ya están en `init.rc` de CM7). Quedan en `prebuilt/` como referencia, marcados en la cabecera.
- `service map` duplicado en `init.huawei.rc` (idéntico al de `init.rc`, que es el que gana): eliminado.
- `onrestart /system/bin/log ...` de `atfwd`: inválido (`onrestart` solo acepta comandos de init) y además `ATFWD-daemon` no existe. Eliminado.

## Mensajes de `init` en dmesg que son esperables

```text
init: too many mtd partitions             # MAX_MTD_PARTITIONS=16; quedan fuera APPSBL y FOTA (mtd16/17), nadie los monta por nombre
init: exec: pid N exited with return code X   # códigos variables: SIGCHLD reapea al hijo antes que el waitpid de exec ("untracked pid N exited")
init: cannot find '/system/bin/cnd' ...   # servicios Qualcomm/Huawei sin binario (cnd, pcm-bridge, wiperiface, ATFWD-daemon)
init: service 'console' requires console
```

El `exec /system/bin/sysinit` (`run-parts` sobre `/system/etc/init.d`) devuelve 2 en equipos sin partición `sd-ext`. Es comportamiento estándar de CM7.

## Validación rápida

```bash
adb shell ls / | grep rc
adb shell "cat /proc/sys/net/core/rmem_max; grep 'open files' /proc/1/limits; ls -ld /data/drm/rights /data/radio"
adb shell dmesg | grep -E 'init: .*(duplicate|invalid)'   # debe salir vacío
```

## Permisos de nodos `0666` revisados (2026-10-02)

La tabla efectiva se calcula con `ueventd.rc` + `ueventd.huawei.rc` (gana la última regla) más los `chmod` de `init.huawei.rc`. Comparada contra el `ueventd.rc` stock:

| Nodo | Stock | Antes en CM7 | Ahora | Quién lo abre |
|---|---|---|---|---|
| `/dev/graphics/fb0` | 0660 root:graphics | 0666 (chmod en init.huawei.rc) | **0660 root:graphics** | Solo `system_server` (SurfaceFlinger, grupo `graphics` 1003). Ninguna app lo tiene abierto. |
| `/dev/radio0` | 0644 fm_radio | 0666 root (ueventd + chmod) | **0660 root:root** | Solo `fminit`, que es setuid root (`android_filesystem_config.h`) y pasa el fd a la app por `SCM_RIGHTS`. Ademas: un `open()` de radio0 reinicia el chip y borra el firmware, asi que con 0666 cualquier app podia cortar el FM. |
| `/dev/msm_fm` | 0660 system:audio | 0666 root (ueventd + chmod) | **0660 system:audio** | Solo `mediaserver` (`AUDIO_START`/`AUDIO_STOP`, grupo `audio` 1005). |
| `kgsl*`, `msm_hw3dc`, `genlock` | 0666 | 0666 | sin cambios | Las apps abren la GPU desde libEGL. |
| `msm_voicememo` | 0666 system:audio | igual | sin cambios | Igual que stock. |
| `/dev/diag` | 0666 radio:radio | igual | **sin cambios (igual que stock)** | Pendiente de decisión: abierto al mundo permite a cualquier app hablar con el modem por diag. Candidato a 0660 radio:radio (default AOSP). |

Validado en equipo, con boot limpio (sin `chmod` en vivo): FM con audio, Galería (GL)/Navegador/Calculadora en frío sin `EACCES`, `run_fm_audmgr_session: AUDIO_START succeeded`.

`W/msm7k.gralloc: FBIOPUT_VSCREENINFO failed, page flipping not supported` es **previo** a este cambio (aparece igual con fb0 en 0666). Ver PERFORMANCE_NOTES / RENDER_NOTES.

**Cómo probar el FM sin reiniciar:** `FMRadioService` solo lanza `fminit` si `hw.fm.init != 1`, y `fminit` lo pone en 1. Si se mata `fminit`, la app no lo relanza y queda en `Tune -1` hasta reiniciar (bug latente). Para una prueba limpia: matar la app y `fminit`, `setprop hw.fm.init 0`, y recién ahí abrir la app. No lanzar `fminit` desde `adb shell`, porque muere con el SIGHUP al cerrar la sesión.
