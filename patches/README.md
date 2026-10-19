# Parches de plataforma (CM9)

Cambios del Y210 que viven fuera del device tree. Este directorio es la fuente
de verdad: los repos de plataforma se reconstruyen desde aquí después de un
`repo sync`.

```bash
device/huawei/y210/patches/apply.sh
```

`apply.sh` aplica cada parche con `git am -3` sobre el proyecto que indica el
nombre de la carpeta (`frameworks_base` -> `frameworks/base`) y salta los que
ya están aplicados (compara el asunto del commit), así que se puede correr
más de una vez.

| Proyecto | Parche | Motivo |
|---|---|---|
| `build` | wallpapers / `sed -i` | La partición system no entra con todos los wallpapers; `sed -i` falla en carpetas compartidas de Docker Desktop. |
| `vendor/cm` | root por defecto / `squisher` | Root para apps y adb en userdebug; mismo problema de `sed -i`. |
| `vendor/cm` | APN Claro PE | No había entradas MCC 716; sin APN con credenciales no levantaban los datos. |
| `hardware/ril` | flags de `libril` | El blob `libril-qc-1.so` declara RIL v6 pero usa los layouts del libril stock de Huawei/CAF (`GET_IMSI` con AID, `SIM_IO` de 40 bytes, data call de 32 bytes). Se activan desde `BoardConfig.mk`. |
| `frameworks/base` | Bluetooth, SurfaceFlinger, libEGL | Arreglos previos del bring-up. |
| `frameworks/base` | `HuaweiQualcommRIL` | SIM `READY` derivado de `GET_SIM_STATUS`, AID en `GET_IMSI`/`SIM_IO` y protocolo IP en `SETUP_DATA_CALL`. |

Para agregar un parche: commitear en el repo de plataforma y exportarlo con
`git format-patch -o device/huawei/y210/patches/<proyecto> <base>..HEAD`.
