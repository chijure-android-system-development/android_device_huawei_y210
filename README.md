# Device Tree for Huawei Y210 (y210)

Copyright 2026 - chijure.

The Huawei Y210 (codenamed _"y210"_) is an entry-level smartphone from Huawei,
built on the Qualcomm MSM7x27a (Snapdragon S1) platform. It shipped with
Android 2.3 Gingerbread.

This tree is an unofficial CyanogenMod 7 port for the Y210, targeting real
device stability on legacy Qualcomm MSM7x27A hardware.

| Basic                    | Spec Sheet                                             |
| ------------------------:|:------------------------------------------------------- |
| CPU                      | Single-core Qualcomm Scorpion, 1 GHz (1008 MHz)         |
| Chipset                  | Qualcomm MSM7227A / MSM7x27a (Snapdragon S1)             |
| GPU                      | Adreno 200                                               |
| Memory                   | 256 MB RAM (~173 MB usable, resto reservado para radio/GPU) |
| Shipped Android Version  | 2.3 Gingerbread                                          |
| Storage                  | ~512 MB internal, microSD expandable                     |
| Display                  | 320 x 480 pixels (HVGA), ~3.5"                           |
| Camera                   | 2 MP rear (foto hasta 1600x1200), video H.263 352x288 (sin encoder de hardware, VGA no soportado) |
| Connectivity             | Wi-Fi (Atheros AR6000/AR6005, SDIO), Bluetooth + FM (Qualcomm WCN2243), HSPA |

## Estado del port

Todo el port está validado como **OK** en hardware real: arranque, gráficos,
Wi-Fi, Bluetooth (pairing + A2DP), sensores, RIL/telefonía (llamadas, SMS,
datos), audio, Radio FM (sintonía y audio real), cámara completa (foto y
video), GPS, deep sleep.

Fuente de verdad detallada (checklist + comandos de log por área):
[`docs/FUNCTION_MATRIX.md`](docs/FUNCTION_MATRIX.md).

Documentación técnica por área: [`docs/`](docs/) — audio, cámara, gráficos,
Wi-Fi, RIL, FM, GPS, status bar, rendimiento, logging.

## Sistema de patches

`frameworks/base` y varios `packages/apps/*` no son propios de este repo —
son checkouts de CyanogenMod compartidos con otros ports del mismo árbol
(LG C660, Huawei Y220). Lo que se puede resolver desde el device tree vive acá,
sin parchear el árbol compartido:

| Área | En el device tree | Mecanismo |
|---|---|---|
| RIL | `ril/libril` (libril de CM7 + stub `RIL_setRilSocketName`) | `TARGET_PROVIDES_LIBRIL := true` |
| RIL | `ril/rilshim` (`libril-y210-shim.so`): traduce la API RIL v6 del blob stock a GB (SIM status, IMSI, SIM_IO, data calls) | `rild.libpath` |
| Wi-Fi | `wifi.c` de CM7 sin modificar + `wlan_loader_y210.sh` | `WIFI_DRIVER_MODULE_ARG`, `WIFI_FIRMWARE_LOADER`, `init.huawei.rc` |
| GPS | `gps/` (loc_api de `hardware/qcom/gps` para AMSS 50000) | sin `BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE` |
| Gráficos | `libgralloc`, `libcopybit` (`gralloc.y210`, `copybit.y210`) | `hw_get_module` carga `*.y210` |
| Recursos | nine-patch de title_bar, `status_bar_expanded.xml`, `config.xml` de FM, APN de Claro PE | `DEVICE_PACKAGE_OVERLAYS` |

Lo que no tiene punto de extensión en CM7 sigue como `.patch` en
[`patches/`](patches/) (11 al 2026-10-18): cámara (`frameworks/base` y
`packages/apps/Camera`), FM (JNI, API y app), status bar, la clase RIL en
`PhoneFactory`, el GID 3006 de Zygote, `ThumbnailUtils` y `.m4v` en el media
scanner.

```bash
# Aplicar todos los patches sobre un árbol CM7 limpio
cd /path/to/cm7
bash device/huawei/y210/patches/apply-patches.sh

# Revertir todo (ej. antes de portar a otro dispositivo sobre el mismo árbol)
bash device/huawei/y210/patches/revert-patches.sh
```

`apply-patches.sh` verifica todos los patches antes de aplicar (todo o nada).
**Ojo con `revert-patches.sh`:** hace `reset --hard` de `frameworks/base` a
`aa02ad1445e`, y eso también borra commits locales que no son patches (hoy
`92cf124`, el fallback de escaneo Wi-Fi). Revisar antes de usarlo.

Los layouts v6 del shim RIL salen de desensamblar el `libril.so` stock del
backup TWRP (B855), no de inferirlos por crashes; ver
[`docs/RIL_NOTES.md`](docs/RIL_NOTES.md).

## Build

```bash
source build/envsetup.sh
breakfast y210
brunch y210
```

Con el contenedor Docker `cm7-builder` (`~/android-build-docker/cm7-docker`),
usando el script con tiempos/log del árbol (su default es `c660`):

```bash
docker exec -it cm7-builder bash -c 'DEVICE=y210 ./timed_build.sh'
```

El contenedor corre en Docker Desktop (VM): si git reemplaza un archivo del
host con el contenedor encendido, adentro puede quedar la versión vieja.
Antes de compilar, comparar `md5sum` host contra
`docker exec cm7-builder md5sum ...` y forzar con `docker cp` si difieren.

`mka bacon` compila todo `Android.mk` del árbol aunque no esté en
`PRODUCT_PACKAGES`; por eso las apps de fábrica decompiladas (`ProjectMenuAct`,
`MMITest_II`, todavía WIP) solo se incluyen con `Y210_FACTORY_APPS=true`.

## Smoke test

```bash
bash device/huawei/y210/tools/test_y210.sh            # completo
bash device/huawei/y210/tools/test_y210.sh --fast     # omite suspensión
```

Resultado esperado (con SIM real): 54 PASS / 0 FAIL.
