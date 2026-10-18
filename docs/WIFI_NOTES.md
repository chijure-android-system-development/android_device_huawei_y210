# Wi-Fi Notes

## Estado actual

El bug ya no esta en el bring-up basico del radio. El estado actual es:

- `wlan.driver.status=ok`
- `wifi.interface=eth0`
- `init.svc.ath_supplicant=running`
- `ps` muestra `/system/bin/ath_supplicant`
- `dumpsys wifi` llega a `runState=Running`
- `Supplicant state=INACTIVE`

Eso mueve el problema a la entrega de resultados de scan al framework.

## Hallazgo clave del stock

Del telefono stock se confirmo:

- usa `eth0`
- usa `ath_supplicant`, no `wpa_supplicant`
- `wpa_supplicant.conf` stock contiene solo:

```text
update_config=1
ctrl_interface=DIR=/data/misc/wpa_supplicant GROUP=wifi
```

- el stock reporta:
  - `init.svc.ath_supplicant=running`
  - `dumpsys wifi` en `runState=Running`
  - `Supplicant state=COMPLETED`
  - `Latest scan results` poblado

El archivo extraido del stock se guardo en:

- `/home/chijure/cm7/stock-capture/wpa_supplicant.conf`

## Cambios persistidos en el arbol

### Propiedades

En `device/huawei/y210/BoardConfig.mk`:

```make
BOARD_HAS_EXTRA_SYS_PROPS := true
```

Motivo:

- el sistema estaba clavado en `247` props
- `wlan.driver.status` no podia crearse

### Init

En `device/huawei/y210/prebuilt/init.huawei.rc`:

- se crean `/data/misc/wpa_supplicant` y `/data/misc/wifi/sockets`
- se ajustaron al patron stock `wifi:wifi` y `0775`
- se dejo `wifi.interface=eth0`
- ya no se vuelve a crear el symlink legacy a `/data/system/wpa_supplicant`

### Config del supplicant

En `device/huawei/y210/prebuilt/system/etc/wifi/wpa_supplicant.conf`:

- se reemplazo la configuracion basada en `/data/misc/wifi/sockets`
- ahora sigue el stock:

```text
ctrl_interface=DIR=/data/misc/wpa_supplicant GROUP=wifi
update_config=1
```

### libhardware_legacy

En `hardware/libhardware_legacy/wifi/wifi.c`:

- `wifi_load_driver()` para `ar6000` usa el flujo Huawei con `wlan.driver.status=insmod`
- `wifi_start_supplicant()` usa `ath_supplicant` para `ar6000`
- `wifi_start_supplicant()` espera a que exista `/sys/class/net/eth0` antes de arrancar el supplicant
- `wifi_connect_to_supplicant()` ya conoce `/data/misc/wpa_supplicant`
- para `ath_supplicant`, `wifi_connect_to_supplicant()` prioriza `/data/misc/wpa_supplicant/eth0`
- se limpian sockets remotos stale antes de arrancar el supplicant
- se añadieron trazas `WifiHW` para ver:
  - carga del driver
  - espera de `eth0`
  - `ctl.start` del supplicant
  - path usado para abrir el socket de control

## Estado validado tras el ultimo boot.img

Con el `boot.img` nuevo y la limpieza del symlink/socket stale:

- `wlan.driver.status=ok`
- `init.svc.ath_supplicant=running`
- `ps` muestra `/system/bin/ath_supplicant`
- `dumpsys wifi` ahora queda en:

```text
interface eth0 runState=Running
Supplicant state: INACTIVE
Latest scan results:
```

- `wpa_cli -p/data/misc/wpa_supplicant -ieth0 scan_results` devuelve APs reales:
  - `DEPA2`
  - `MOVISTAR_BFE6`

Eso ya descarta:

- fallo basico de `insmod`
- fallo basico de arranque del proceso `ath_supplicant`
- fallo de conexion inicial al socket de control

## Hallazgo nuevo

El telefono conserva sockets `eth0` en dos rutas:

- `/data/misc/wpa_supplicant/eth0`
- `/data/misc/wifi/sockets/eth0`

Y todavia existe un symlink viejo en el dispositivo:

- `/data/system/wpa_supplicant -> /data/misc/wifi/sockets`

Eso no viene ya del arbol actual, sino de pruebas anteriores y estado persistido en `/data`.

Eso explicaba por que antes el framework quedaba en `UNINITIALIZED`.

Despues de priorizar `/data/misc/wpa_supplicant` y borrar el symlink viejo
`/data/system/wpa_supplicant`, el framework ya pasa a `Running/INACTIVE`.

La hipotesis mas fuerte ahora es:

1. `ath_supplicant` ya corre
2. el framework ya se conecta al socket de control
3. pero no esta recibiendo o procesando bien `SCAN_RESULTS`
4. por eso `Latest scan results` sigue vacio aunque `wpa_cli` si vea APs

## Notas de depuracion utiles

- arrancar `ath_supplicant` demasiado pronto despues de `insmod` puede fallar
- despues del ajuste para esperar `eth0`, el proceso ya puede quedar vivo
- los logs de `WifiHW` ahora deberian mostrar exactamente:
  - si el driver llega a `ok`
  - si `eth0` estuvo listo
  - qué servicio intentamos arrancar
  - qué path de socket intenta abrir `wifi_connect_to_supplicant()`

## Siguiente paso

Prioridad inmediata:

1. revisar por que `WifiMonitor` o `WifiStateTracker` no materializan `SCAN_RESULTS`
2. comprobar si `ath_supplicant` emite `CTRL-EVENT-SCAN-RESULTS` por el monitor socket
3. si hace falta, instrumentar la capa Java (`WifiMonitor` / `WifiStateTracker`) para ver si el evento se pierde o si el parseo falla

## Hallazgo posterior

Las pruebas siguientes confirmaron algo mas preciso:

- `wpa_cli -p/data/misc/wpa_supplicant -ieth0 scan_results` sigue devolviendo APs reales
- `dumpsys wifi` sigue en `runState=Running` con `Supplicant state=INACTIVE`
- `Latest scan results` sigue vacio

Eso deja el bug restante entre el monitor/eventos y el framework, no en el radio.

Tambien aparecio una diferencia importante contra stock:

- la `libhardware_legacy.so` stock contiene referencias a `wifi.wpa_supp_ready`
- la `libhardware_legacy.so` actual del port no contiene esa ruta

No hay referencias a `wifi.wpa_supp_ready` en el framework Java del arbol, asi que
esa logica extra vive dentro de la libreria nativa stock.

Siguiente experimento recomendado:

1. probar en vivo la `libhardware_legacy.so` stock con backup de la actual
2. validar si con esa libreria el framework ya publica scan results
3. si mejora, portar solo la diferencia necesaria a [`wifi.c`](/home/chijure/cm7/hardware/libhardware_legacy/wifi/wifi.c)

## Resultado de la prueba con libreria stock

La prueba directa con la `libhardware_legacy.so` stock fue negativa:

- se hizo backup de la libreria CM7 en `/system/lib/libhardware_legacy.cm7.bak`
- se empujo la libreria stock y se reinicio
- el telefono subio parcialmente, pero `service wifi` desaparecio
- conclusion: la libreria stock no es ABI-compatible con este framework CM7

Se restauro la libreria CM7 original en el telefono.

## Fix aplicado en framework

Como `startScan` y `getScanResults()` ya funcionaban pero el cache/broadcast no se
actualizaba solos, se agrego un fallback en [`frameworks/base/services/java/com/android/server/WifiService.java`](/home/chijure/cm7/frameworks/base/services/java/com/android/server/WifiService.java):

- despues de un `scan()` exitoso, se agenda una lectura diferida de `SCAN_RESULTS`
- si la lista no viene vacia, se emite `SCAN_RESULTS_AVAILABLE_ACTION`

Con eso, la validacion actual ya muestra resultados tambien en `dumpsys wifi`:

```text
Latest scan results:
  DEPA2
  MOVISTAR_BFE6
  PENA PAULINO
```

Eso confirma:

- el parser de `SCAN_RESULTS` funciona
- el cache `mScanResults` del framework ya se llena
- el problema practico estaba en la notificacion, no en el escaneo del radio
## Redes Wi-Fi no persistían tras reinicio

Síntoma:
- La red se guarda durante runtime y aparece en `/data/misc/wifi/wpa_supplicant.conf`.
- Tras reiniciar, el archivo vuelve al template mínimo y se pierden las redes.

Causa:
- En [init.huawei.rc](/home/chijure/cm7/device/huawei/y210/prebuilt/init.huawei.rc) había un `copy /system/etc/wifi/wpa_supplicant.conf /data/misc/wifi/wpa_supplicant.conf` dentro de `on boot`.
- Ese `copy` pisaba en cada arranque el archivo persistente generado por `wpa_supplicant`.

Fix:
- Reemplazar el `copy` por una inicialización condicional:
  - crear `/data/misc/wifi/wpa_supplicant.conf` desde `/system/etc/wifi/wpa_supplicant.conf` solo si el archivo no existe.
- Mantener `chown wifi wifi` y `chmod 0660` en cada arranque.

Validación:
1. Guardar una red WPA desde Settings.
2. Confirmar que aparece un bloque `network={...}` en `/data/misc/wifi/wpa_supplicant.conf`.
3. Reiniciar el teléfono.
4. Verificar que el bloque `network={...}` sigue presente después del boot.

## Fastboot al flashear

`adb reboot bootloader` no entra a fastboot. No es un bug del port: el
2026-10-02 el mismo comando en el Y210 stock (`78F5FD28C4C4`,
`Y210-0151V100R001C40B851`) solo reinició Android y `fastboot devices` no
mostró el equipo. Es el bootloader de Huawei. Entrar a fastboot a mano y
comprobar con `fastboot devices` antes de `fastboot flash`.

## Sin parchear libhardware_legacy (2026-10-18)

`hardware_libhardware_legacy.patch` se retiró. El `wifi.c` de CM7 hace lo
mismo con sus hooks y el rc del device tree:

- `insmod`: `wifi.c` carga `/system/wifi/ar6000.ko` con
  `WIFI_DRIVER_MODULE_ARG := "targetconf=default devmode=sta submode=none
  ifname=eth0"` (los argumentos por defecto de `wlan_tool loaddriver`).
- `WIFI_FIRMWARE_LOADER := wlan_loader`: servicio del rc que corre
  `wlan_loader_y210.sh`; espera a que `wmiconfig -i eth0 --getRTS` responda,
  hace `ifconfig eth0 up` y pone `wlan.driver.status=ok`. Es lo que hacía
  `wlan_tool loaddriver` después del insmod; con eso el supplicant ya no
  arranca antes de que exista `eth0`.
- unload: `rmmod ar6000` de `wifi.c`, igual que `wlan_tool unloaddriver` con
  `CLK_HELPER=0`.
- supplicant: el servicio `wpa_supplicant` del rc ejecuta `ath_supplicant`
  (antes había un `wpa_supplicant -Dwext` que no se usaba y un servicio
  `ath_supplicant` aparte).
- socket: `symlink /data/misc/wpa_supplicant /data/system/wpa_supplicant`
  (la ruta que usa `wifi.c`); no hace falta tocar el `wpa_supplicant.conf`
  guardado en `/data`.

**Causa real de por qué el parche desviaba el insmod a `wlan_tool`:**
`ar6000.ko` escribe `/sys/devices/platform/msm_sdcc.2/polling` y
`detect_change` con `android_readwrite_file()` (`filp_open` desde el kernel,
con las credenciales del proceso que hace el insmod) para que el host SDIO
detecte el chip. Desde `system_server` (uid system) la escritura fallaba en
silencio: nunca aparecía `mmc1: new high speed SDIO card`, no había `eth0`,
`wifi.c` hacía timeout y el `rmmod` de un driver a medio inicializar reiniciaba
el teléfono cada 1-2 minutos. El stock lo resuelve con `chown system system`
de los `polling` (líneas del init.qcom.rc stock que estaban en
`init.y210.rc`, que ya no va al ramdisk); ahora están en `init.huawei.rc`,
más `detect_change` como hacía `wlan_tool init`.

Validado: 3 ciclos `svc wifi disable/enable` (módulo descargado, `ok` en ~2 s,
escaneo con APs), arranque con Wi-Fi encendido que reconecta a la red
guardada con DHCP, ping a gateway e internet, sin reinicios ni tombstones.
Logs esperados:

```
I/wlan_loader: ar6000 listo en eth0
V/WifiStateTracker: Connection to supplicant established, state=INACTIVE
kernel: mmc1: new high speed SDIO card at address 0001
```

El primer `Unable to open connection to supplicant on
"/data/system/wpa_supplicant/eth0"` es normal: el supplicant todavía no creó
su socket y `WifiMonitor` reintenta.

## Wi-Fi sin datos después de suspender (bug del stock, en investigación 2026-10-18)

Síntoma: tras un deep sleep con Wi-Fi asociado, al despertar `eth0` sigue
arriba con IP y el supplicant en `COMPLETED`, pero no pasa tráfico (ping al
gateway falla) hasta apagar y prender el Wi-Fi. El usuario confirma que el
stock tiene el mismo bug.

Secuencia en dmesg:

```
ar6000: Setup WoW successfully / ar6000_suspend_ev:Suspend for wow mode 3
(resume) mmc1: req failed (CMD5): -110, retrying...   x3
mmc1: error -110 during resume (card was removed?)
```

y nunca aparece el resume del driver (`ar6000_resume_ev`).

Causa: `ar6000.ko` (binario) arma WoW y deja el chip alimentado por la placa,
pero nunca llama a `sdio_set_host_pm_flags(MMC_PM_KEEP_POWER)` (no está entre
sus símbolos). Sin el flag el core MMC del 2.6.38 cree que la tarjeta se apagó
y la reinicializa al despertar; el chip, que sigue en WoW, no responde a CMD5.
El `sdc2_plat_data` y el código de `msm_sdcc.c` del kernel stock son idénticos
(comparado byte a byte contra el zImage stock descomprimido), así que no es una
regresión del kernel propio.

Intento 1 (`f452f06b`, revertido en `eef19b77`): forzar `MMC_PM_KEEP_POWER`
para el slot Wi-Fi en `msmsdcc_runtime_suspend`. Con eso el teléfono **no
despertó**: cuelgue duro con pantalla negra, recuperado manteniendo el botón
de encendido (`poweron_irq=0x1` = tecla, sin datos en apanic). Probablemente el
resume del driver queda bloqueado esperando al chip. No usar
`MMC_CAP_NONREMOVABLE` fijo como alternativa: `mmc_rescan` dejaría de detectar
el apagado del chip en cada Wi-Fi off/on.

Intento 2 (en prueba): apagar WoW con `wmiconfig -i eth0 --setwowmode
disable` para que el driver suspenda sin dejar el chip esperando. Si al
despertar el Wi-Fi recupera datos solo, pasa a `wlan_loader_y210.sh`.
