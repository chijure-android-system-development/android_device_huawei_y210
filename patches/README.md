# Parches de plataforma (CM10.1)

Lo que se puede resolver en el device tree vive en el device tree (HALs,
shims, módulos, prebuilts, props). Aquí solo quedan los cambios que no tienen
otro lugar posible. Se reconstruyen después de un `repo sync` con:

```bash
device/huawei/y210/patches/apply.sh
```

`apply.sh` aplica cada parche con `git am -3` sobre el proyecto que indica el
nombre de la carpeta (`system_netd` -> `system/netd`) y salta los que ya están
aplicados (compara el asunto del commit).

| Proyecto | Parche | Por qué no puede ir en el device tree |
|---|---|---|
| `system/netd` | `SoftapControllerATH` | Hotspot del AR6003 (WEXT, `athap0`, hostapd stock). netd elige el controlador de SoftAP en su propio `Android.mk` y no carga código externo. En 4.2 además faltan `softap start/stop` (`startDriver`/`stopDriver`), que el framework sigue mandando con `config_wifiApStartInterface`; vuelven bajo `ATH_SOFTAP`, como en el netd cm-10.1 de legaCyMod. |

La plataforma es CyanogenMod cm-10.1 tal cual el manifest, más los HALs
Qualcomm legacy de legaCyMod en `.repo/local_manifests/y210.xml`
(`hardware/qcom/display-legacy`, `hardware/qcom/media-legacy`,
`hardware/atheros/wlan`).

Ya no hace falta en CM10.1:

- `frameworks/base` (reintento en `NoSuchAdapter`): era del event loop de
  BlueZ; 4.2 usa Bluedroid, con `libbt-vendor/` del device tree.
- `init.rc` propio: se usa el de 4.2; lo del Y210 va en `init.huawei.rc`.
- `y210-telephony.jar` en el `BOOTCLASSPATH`: `HuaweiY210RIL` entra a
  `telephony-common` con `BOARD_RIL_CLASS`.

Para agregar un parche: commitear en el repo de plataforma y exportarlo con
`git format-patch -o device/huawei/y210/patches/<proyecto> <base>..HEAD`.
