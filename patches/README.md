# Parches de plataforma (CM9)

Lo que se puede resolver en el device tree vive en el device tree (HALs,
shims, prebuilts, props). Aquí solo quedan los cambios que no tienen otro
lugar posible. Se reconstruyen después de un `repo sync` con:

```bash
device/huawei/y210/patches/apply.sh
```

`apply.sh` aplica cada parche con `git am -3` sobre el proyecto que indica el
nombre de la carpeta (`frameworks_base` -> `frameworks/base`) y salta los que
ya están aplicados (compara el asunto del commit).

| Proyecto | Parche | Por qué no puede ir en el device tree |
|---|---|---|
| `build` | wallpapers / `sed -i` | El filtro de `PRODUCT_PACKAGES` tiene que correr después de resolver el grafo de productos; en un `.mk` del device es un no-op. |
| `vendor/cm` | `squisher` sin `sed -i` | Script de empaquetado de CM. |
| `frameworks/base` | Bluetooth, SurfaceFlinger, libEGL | Código del framework (`BluetoothService`, `libsurfaceflinger`, `libEGL`). |
| `external/webkit` | `scratchContext` | Bug de WebKit: el navegador crashea con SVG cuando Android reporta poca memoria. |

Resuelto en el device tree en vez de parches:

- RIL: `ril/rilshim` (`libril-y210-shim.so`, adapta `libril-qc-1.so` al libril
  de ICS) y `ril/telephony` (`y210-telephony.jar` con `HuaweiY210RIL`, en el
  `BOOTCLASSPATH` de `init.rc`).
- APN Claro PE: `prebuilt/system/etc/apns-conf.xml` (prioridad en `cm.mk`).
- adb root: `init.huawei.rc` + `cm.mk`.

Para agregar un parche: commitear en el repo de plataforma y exportarlo con
`git format-patch -o device/huawei/y210/patches/<proyecto> <base>..HEAD`.
