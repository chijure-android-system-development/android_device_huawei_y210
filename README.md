# CyanogenMod 10.1 para Huawei Ascend Y210

Device tree de CyanogenMod 10.1 / Android 4.2.2 para el Huawei Ascend Y210
(Qualcomm MSM7x27A). Viene del port CM10 (rama `jellybean`), que a su vez
viene del de CM9 (rama `ics`). La plataforma es CyanogenMod `cm-10.1` sin
cambios salvo un parche de netd (ver `patches/`); lo propio del Y210 vive aquí.

## Hardware

| Campo | Valor |
| --- | --- |
| SoC | Qualcomm MSM7225A, Cortex-A5 a 1.008 GHz, un núcleo |
| GPU | Adreno 200 |
| RAM | 256 MB (~177 MB para Linux; el resto es del módem y de los carveouts pmem) |
| Pantalla | HVGA 320x480, mdpi |
| Baseband | QCRIL legacy (`rild` + `libril-qc-1.so`, shim en `ril/rilshim`) |
| WiFi | Atheros AR6003 (`ar6000.ko`, WEXT, se compila en `wifi/`) |
| Bluetooth | Qualcomm Bahama (`hci_qcomm_init`, H4 sobre `ttyHS0`) |
| Kernel | 2.6.38 (`kernel/huawei/y210`, rama `cm-10.1`) |

## Estado (2026-10-21)

| Subsistema | Estado |
| --- | --- |
| Arranque | ✅ |
| RIL: SIM, datos, llamadas (auricular, micrófono, altavoz) | ✅ |
| WiFi (escaneo y conexión) | ✅ |
| Bluetooth (Bluedroid, `libbt-vendor/`) | ✅ enciende; emparejado sin probar |
| Audio | ✅ salida y llamadas; Grabadora sin probar |
| Cámara: foto 1600x1200 | ✅ |
| Cámara: video H.263 352x288 + AMR | ✅ |
| Hotspot, GPS, FM | sin probar en 4.2 |

## Obtener el código

```bash
repo init -u https://github.com/CyanogenMod/android.git -b cm-10.1
mkdir -p .repo/local_manifests
cp <este repo>/manifests/y210.xml .repo/local_manifests/   # o descargarlo
repo sync
device/huawei/y210/patches/apply.sh
vendor/cm/get-prebuilts
```

## Build

Necesita Java 6, así que se compila en Docker (Ubuntu 14.04,
`~/android-build-docker/cm10.1-docker`). `out/` y ccache van en volúmenes de
Docker: escribirlos por la carpeta compartida (virtiofs) hacía caer
`virtiofsd` a mitad del build.

```bash
source build/envsetup.sh
export CM_BUILD=y210          # sin esto lunch cae en roomservice y se cuelga
lunch cm_y210-userdebug
make -j3 bacon                # más jobs: systemd-oomd mata Docker Desktop
```

`adb_keys` (no versionado): una copia de `~/.android/adbkey.pub` que se mete
en el ramdisk como `/adb_keys`. `vendor/cm` fuerza `ro.adb.secure=1`; con esto
esa PC queda autorizada aunque se borre `/data`.

## Particiones (confirmadas contra `/proc/mtd` en hardware real)

| Montaje | Tamaño real | Notas |
| --- | --- | --- |
| `/system` | ~186 MiB (195,035,136 bytes) | El check de tamaño de `mkyaffs2image` en `make` es más permisivo que esto — validar siempre contra `/proc/mtd`, no confiar en el build |
| `/boot` | 5 MB | |
| `/recovery` | 6 MB | |
| `/cache` | ~58 MB | |
| `/data` | ~160 MB | |

Filesystem: yaffs2 sobre MTD raw (no eMMC/ext4).

## Flash con TWRP

```bash
adb push cm-10.1-*-UNOFFICIAL-y210.zip /external_sd/
adb shell twrp install /external_sd/cm-10.1-*-UNOFFICIAL-y210.zip
```

Viniendo de otra versión, borrar `/data` y `/cache` con `rm` desde la shell de
TWRP (`twrp wipe` se cuelga en este equipo); en `/data` dejar `HWUserData`.
Al flashear solo `boot.img`, verificar con `dump_image` + `md5sum` antes de
reiniciar: `flash_image` a veces falla un bloque.

## Notas del Y210 en 4.2

- Composición: solo GPU. El copybit de la MDP rechaza los formatos
  (`copybit stretch failed`) y con `debug.composition.type=mdp` la pantalla
  queda negra.
- Launcher: con "Desplazar fondo de pantalla" apagado (Ajustes de Trebuchet)
  la mediana baja de ~48 a ~31 ms por cuadro; la Adreno 200 recompone el fondo
  de 590x480 en cada cuadro. Trebuchet no lo deja configurar por overlay.
- Cámara: el sensor no tiene compensación de exposición ni control de fps;
  el HAL lo anuncia así (sin HDR por software, perfil de video a 30 fps).
- Probando: no reiniciar el launcher con `am force-stop` (cuelga el
  ActivityManager y el Watchdog reinicia `system_server`); matar el PID.

## Debug rápido

```bash
# Estado general
adb wait-for-device
adb shell getprop sys.boot_completed
adb shell dmesg | grep -iE 'cannot find|add_tid_to_cgroup|fatal|panic'
adb logcat -d | grep -iE 'FATAL|ClassNotFoundException|NoClassDefFoundError'

# RIL
adb shell getprop gsm.version.baseband gsm.sim.state gsm.operator.alpha
adb shell ps | grep -E 'rild|qmuxd|qmiproxy'

# Memoria (dispositivo con solo ~165 MB usables)
adb shell cat /proc/meminfo
adb shell cat /sys/module/lowmemorykiller/parameters/minfree

# Audio
adb shell dumpsys media.audio_flinger | grep -E 'total writes|standby'
```

