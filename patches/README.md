# Parches de plataforma (CM10)

Lo que se puede resolver en el device tree vive en el device tree (HALs,
shims, módulos, prebuilts, props). Aquí solo quedan los cambios que no tienen
otro lugar posible. Se reconstruyen después de un `repo sync` con:

```bash
device/huawei/y210/patches/apply.sh
```

`apply.sh` aplica cada parche con `git am -3` sobre el proyecto que indica el
nombre de la carpeta (`frameworks_base` -> `frameworks/base`) y salta los que
ya están aplicados (compara el asunto del commit).

| Proyecto | Parche | Por qué no puede ir en el device tree |
|---|---|---|
| `frameworks/base` | reintento en `NoSuchAdapter` | Carrera dentro del event loop de Bluetooth del framework: `bluetoothd` se registra en D-Bus antes que `hci0`. |
| `system/netd` | `SoftapControllerATH` | Hotspot del AR6003 (WEXT, `athap0`, hostapd stock). netd elige el controlador de SoftAP en su propio `Android.mk` y no carga código externo; el de ICS solo trae nl80211 y TI. |

El parche de `system/netd` es el de CM9 adaptado a JB (la elección del
controlador vive en la cadena TI del `Android.mk` de este netd; macros
`ALOGx`). El de `external/webkit` (`scratchContext`) no hace falta en CM10:
JB ya crea ese contexto directamente en la rama `PLATFORM(ANDROID)`.

El resto de la plataforma CM10 queda tal cual el manifest (sin los cambios
MTK de los LG vee4ss/vee5ss, respaldados en
`/mnt/fuentes/cm10-backups/tree-changes-20261020/`, fuera del árbol porque el build escanea todos los `Android.mk`). `hardware/atheros/wlan` viene de
`.repo/local_manifests/y210.xml` (rama `jellybean` de CyanogenMod).

Resuelto fuera de los parches (2026-10-18/20):

- RIL: `ril/rilshim` (`libril-y210-shim.so`) y `ril/telephony`
  (`y210-telephony.jar` con `HuaweiY210RIL`, en el `BOOTCLASSPATH`).
- APN Claro PE: `prebuilt/system/etc/apns-conf.xml` (prioridad en `cm.mk`).
- adb root: `init.huawei.rc` + `cm.mk`.
- Wallpapers/VideoEditor de CM, Exchange2 y el motor de temas (no entran en
  `system`): `overrides/` (`Y210Overrides`, paquete vacío con
  `LOCAL_OVERRIDES_PACKAGES`).
- Wallpapers de tablet de Trebuchet (`sw600dp`/`sw720dp`) y diccionarios de
  otros idiomas: filtro de aapt (`PRODUCT_AAPT_CONFIG ... sw320dp` y
  `PRODUCT_LOCALES := en_US es_US`).
- Arranque con cargador: `BOARD_CHARGING_CMDLINE_*` como `-D` globales en
  `BoardConfig.mk` (el `init/Android.mk` de JB no los reenvía).
- NAP de Bluetooth: era `CONFIG_BT_BNEP` apagado en el kernel.
- `libos_compat` + SurfaceFlinger y `egl_cache` de `libEGL`: innecesarios con
  el `libgsl.so` del e400 (ver `RENDER_NOTES.md`).
- `sed -i` en `build/core/Makefile` y `vendor/cm/tools/squisher`: problema
  del entorno Docker Desktop, no de la ROM; lo resuelve
  `sed-inplace-wrapper.sh` en la imagen (`~/android-build-docker/cm10-docker`).

Para agregar un parche: commitear en el repo de plataforma y exportarlo con
`git format-patch -o device/huawei/y210/patches/<proyecto> <base>..HEAD`.
