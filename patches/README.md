# Parches de plataforma (CM9)

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
| `external/webkit` | `scratchContext` | Bug de WebKit: el navegador crashea con SVG cuando Android reporta poca memoria. |

Resuelto fuera de los parches (2026-10-18/19):

- RIL: `ril/rilshim` (`libril-y210-shim.so`) y `ril/telephony`
  (`y210-telephony.jar` con `HuaweiY210RIL`, en el `BOOTCLASSPATH`).
- APN Claro PE: `prebuilt/system/etc/apns-conf.xml` (prioridad en `cm.mk`).
- adb root: `init.huawei.rc` + `cm.mk`.
- Wallpapers/VideoEditor de CM (no entran en `system`): `overrides/`
  (`Y210Overrides`, paquete vacío con `LOCAL_OVERRIDES_PACKAGES`).
- NAP de Bluetooth: era `CONFIG_BT_BNEP` apagado en el kernel.
- `libos_compat` + SurfaceFlinger y `egl_cache` de `libEGL`: innecesarios con
  el `libgsl.so` del e400 (ver `RENDER_NOTES.md`).
- `sed -i` en `build/core/Makefile` y `vendor/cm/tools/squisher`: problema
  del entorno Docker Desktop, no de la ROM; lo resuelve
  `~/android-build-docker/cm9-docker/sed-inplace-wrapper.sh` en la imagen.

Para agregar un parche: commitear en el repo de plataforma y exportarlo con
`git format-patch -o device/huawei/y210/patches/<proyecto> <base>..HEAD`.
