RIL / Baseband Notes (Y210 / MSM7x27A, CM9/ICS)

## Estado CM9 (2026-06-03)

| Función | Estado |
|---|---|
| rild / baseband | **OK** — baseband 109808, Claro Perú 71610, HSPA |
| IMEI | **OK** |
| Llamadas voz | **OK** |
| SMS | **OK** |
| SIM (IMSI, operador, APN) | **OK** — ver "SIM / datos en CM9" |
| Datos móviles (rmnet0) | **OK** — IP real validada |
| qmuxd + qmiproxy | **OK** |

**RIL class en CM9:** `com.cyanogenmod.internal.telephony.HuaweiQualcommRIL`
(no `QualcommNoSimReadyRIL` de CM7). El crash `ClassNotFoundException` de
arranque se resolvió añadiendo el jar correcto al classpath del boot.

## SIM / datos en CM9 (2026-10-18)

`libril-qc-1.so` declara RIL v6 pero usa los layouts del libril stock de
Huawei/CAF. Sin adaptarlo, la SIM quedaba en `NOT_READY` (sin IMSI, operador
de la SIM ni APN) y los datos no levantaban. Se resuelve en el device tree,
sin parches de plataforma:

- `ril/rilshim` -> `libril-y210-shim.so` (`rild.libpath` en `system.prop`):
  agrega el AID a `GET_IMSI` (ICS lo despacha con `dispatchVoid` y el blob
  hacía SIGSEGV en `qcril_uim_request_get_imsi`), pasa `SIM_IO` como el
  struct de 40 bytes del stock (AID en `+36`; con el de ICS crasheaba en
  `vfprintf`) y convierte la respuesta de data call de 32 bytes a
  `RIL_Data_Call_Response_v6`.
- `ril/telephony` -> `y210-telephony.jar` con `HuaweiY210RIL`
  (`ro.telephony.ril_class`), en el `BOOTCLASSPATH` de `init.rc`: deriva
  `SIM_READY` de `GET_SIM_STATUS` (el blob nunca lo reporta), escribe el AID
  al final de `SIM_IO` y manda el protocolo PDP en `SETUP_DATA_CALL` (qcril
  rechazaba el `"0"` de `HuaweiRIL`: `invalid ip family`).
- APN Claro PE (`claro.pe`, `claro`/`claro`, PAP) en
  `prebuilt/system/etc/apns-conf.xml`.

`QualcommSharedRIL` no sirve tal cual: lee 4 contadores de PIN por app que el
libril de ICS no escribe (`Unrecognized RIL_AppType`).

Validado: `gsm.sim.state=READY`, `gsm.sim.operator.numeric=71610`, `rmnet0`
con IP, ping/DNS por HSPA, sin tombstones de `rild`. Logs esperados:

```
RILShim: AID GSM/USIM: a0000000871002f972ff0189
RILJ: SIM app state APPSTATE_READY type APPTYPE_USIM
RILShim: SETUP_DATA_CALL: status=0 cid=0 ifname=rmnet0 addr=...
```

---

## Historial CM7 (arquitectura aplica en CM9)

RIL / Baseband Notes (Y210 / MSM7x27A, CM7)
==========================================

Estado actual
-------------

- `rild` funcionando (socket + radio stack activo).
- Baseband reporta valor (ej. `gsm.version.baseband=109808`).
- Registro de operador funciona (ej. `gsm.operator.numeric=71610`, `gsm.operator.alpha=Claro`).
- IMEI disponible vía PhoneSubInfo (ver “Validación rápida”).

Si algo de lo anterior vuelve a romperse tras reboot, casi siempre es porque no se flasheó el ZIP correcto
(ver “Verificación de build instalada”).

Arquitectura (qué necesita el Y210)
-----------------------------------

El Y210 usa el stack Qualcomm “legacy” (QCRIL) y depende de:

- `rild` (daemon) + blob RIL Qualcomm (`/system/lib/libril-qc-1.so` o blob equivalente).
- ONCRPC (`/dev/oncrpc/*`) + `oem_rpc_svc`.
- QMI helpers (`qmuxd` + `qmiproxy`) y el directorio `/data/radio` para sockets locales.

Sin QMI helpers, el RIL puede arrancar pero quedarse en `RADIO_UNAVAILABLE` o no poder inicializar UIM/SIM.

Cambios clave que hicieron que levante radio
--------------------------------------------

1) Propiedades RIL stock (selección del blob y dispositivo)

- `rild.libpath=/system/lib/libril-qc-1.so`
- `rild.libargs=-d /dev/smd0`

Fuente: `device/huawei/y210/system.prop` y referencia stock `device/huawei/y210/props.txt`.

2) QMI helpers en el ramdisk + `/data/radio`

El arranque debe asegurar:

- `mkdir /data/radio 0770 radio radio` (en `on post-fs-data`)
- servicios `qmuxd` y `qmiproxy` disponibles (y arrancados para `ro.baseband="msm"`)

Esto evita fallas tipo:

- `Could not register successfully with QMI UIM Service`
- `ASSERTION FAILED ... qcril_uim.qmi_handle >= 0`

3) Compatibilidad con unsolicited Qualcomm (1031–1037)

Algunos blobs Qualcomm envían unsolicited “extra” (1031–1037). Para evitar spam/errores,
se mantienen mapeados en `libril` (C) como entradas numéricas (sin depender de un `ril.h` moderno).

Nota Y210/QCRIL:

- En este dispositivo se observa `RIL_UNSOL_RESPONSE_DATA_NETWORK_STATE_CHANGED (1037)` en `logcat -b radio`.
  Si el framework no lo reconoce/no notifica, la UI puede quedarse en `No service` hasta reiniciar el RIL.

Validación rápida (runtime)
---------------------------

Desde host:

- Estado general RIL:
  - `adb shell getprop | egrep "rild\\.libpath|rild\\.libargs|init\\.svc\\.(ril-daemon|oem_rpc_svc)"`
  - `adb shell ls -l /dev/socket | egrep "rild"`

- Baseband + operador:
  - `adb shell getprop gsm.version.baseband`
  - `adb shell getprop gsm.operator.numeric`
  - `adb shell getprop gsm.operator.alpha`
  - `adb shell getprop gsm.network.type`

- IMEI (binder; útil aunque UI falle):
  - `adb shell service call iphonesubinfo 1`

- Logs radio (modo debug QCRIL):
  - `adb shell setprop persist.radio.adb_log_on 1`
  - `adb logcat -b radio -d | tail -n 200`

Verificación de build instalada (para evitar “me funcionó y luego se rompió”)
----------------------------------------------------------------------------

Cuando radio funciona “a mano” pero se rompe al reiniciar, típicamente el ramdisk no trae los servicios QMI
o falta crear `/data/radio` en `post-fs-data`.

Checklist post-flash:

- `adb shell grep -n "/data/radio\\|service qmuxd\\|service qmiproxy" /init.huawei.rc`
- `adb shell ls -ld /data/radio && adb shell ls -l /data/radio | head`

Si el ZIP que flasheaste incluía un framework/ramdisk distinto, verifica también el jar:

- `adb shell md5sum /system/framework/framework.jar`

Troubleshooting (síntomas -> causa probable)
--------------------------------------------

- `init.svc.qmuxd=restarting` + `RADIO_UNAVAILABLE` (baseband vacío) aunque `rild` corra:
  - causa típica: falta `/data/radio` (qmuxd/qmiproxy no pueden crear/bindear sus sockets locales).
  - fix inmediato (runtime):
    - `adb shell "mkdir -p /data/radio; chown radio.radio /data/radio; chmod 0770 /data/radio"`
    - `adb shell "stop qmuxd; stop qmiproxy; stop ril-daemon; start qmuxd; start qmiproxy; start ril-daemon"`
  - fix permanente: asegurar `mkdir /data/radio 0770 radio radio` en el ramdisk (`device/huawei/y210/prebuilt/init.huawei.rc`)
    tanto en `on post-fs-data` como en el trigger `on property:ro.baseband="msm"` (por si el orden de triggers cambia tras wipe).
  - validación:
    - `adb shell getprop init.svc.qmuxd` -> `running`
    - `adb shell getprop gsm.version.baseband` -> valor (ej. `109808`)

- `init.svc.ril-daemon=running` pero `gsm.version.baseband` vacío y `RADIO_NOT_AVAILABLE`:
  - falta `qmuxd/qmiproxy` o `/data/radio` en el arranque
  - permisos rotos en `/dev/oncrpc/*` (ver `system/core/rootdir/ueventd.rc` / `ueventd.huawei.rc`)

- `rild` reinicia/“Hit EOS reading message length” repetido:
  - suele indicar que el lado Java se desconecta porque el estado de radio cae a “not available”
    (seguir el `logcat -b radio` con `persist.radio.adb_log_on=1` para ver la causa real).

- `init.svc.ril-daemon=running` pero UI muestra `No service` (`mServiceState=3`) y `gsm.sim.state=UNKNOWN`:
  - workaround inmediato: `adb shell stop ril-daemon; adb shell start ril-daemon`
  - revisar en `logcat -b radio` si el modem está mandando `UNSOL_RESPONSE_DATA_NETWORK_STATE_CHANGED (1037)`

Datos móviles (fix 2026-04-12)
------------------------------

Síntoma:

- Operador/registros OK (`gsm.operator.alpha/numeric`, `HSPA`), `rild` activo.
- APN existe (ej. Claro PE `claro.pe`), `rmnet0` puede levantar IP por DHCP.
- Aun así, la UI queda en `CONNECTING` o se cae `com.android.phone` y termina en `No service`.

Evidencia (logcat main):

- Crash del hilo `GsmDataConnection-*`:
  - `java.lang.NumberFormatException: unable to parse 'null' as integer`
  - `at com.android.internal.telephony.DataConnection.onSetupConnectionCompleted(DataConnection.java:422)`

Causa real:

- El blob `libril-qc-1.so` devuelve una respuesta de `RIL_REQUEST_SETUP_DATA_CALL` que no coincide con lo que
  espera CM7 en `DataConnection`:
  - En vez de `response[0]=cid`, `response[1]=ifname`, `response[2]=ip`, el `cid` llega como `null` y los campos
    útiles aparecen más adelante (se observó `rmnet0` e IP en los índices 4 y 5).
- Esto dispara el `parseInt(null)` y mata el proceso Phone, cortando datos y a veces la señal.

Fix aplicado:

1) Normalización en framework (evita el crash y toma `ifname/ip` correctos)

- Archivo: `frameworks/base/telephony/java/com/android/internal/telephony/DataConnection.java`
- Lógica:
  - Detecta la forma "malformada" (`response[0]==null` y `response[4]/[5]` presentes)
  - Fuerza `cid=0` y usa `response[4]` como `ifname` y `response[5]` como `ip`
  - DNS/gateway preferidos desde `net.<ifname>.dns1/dns2/gw` y fallback a strings de la respuesta si vienen
    poblados.

2) Hardening en `libril` (evita SEGV por punteros basura)

- Archivo: `hardware/ril/libril/ril.cpp`
- Lógica:
  - `sanitizeRilString()` descarta punteros sospechosos (valores muy pequeños como `0x2`) antes de
    `writeStringToParcel()`.
  - Se aplica en `responseStrings()`, `responseString()` y `responseDataCallList()`.

Validación (post-boot)

- Estado datos:
  - `adb shell dumpsys connectivity | grep -n \"Active network\\|type: mobile\\|state\\|extra\"`
- Interfaz:
  - `adb shell ip addr show rmnet0`
  - `adb shell ip route show`
- Props red:
  - `adb shell getprop net.rmnet0.gw`
  - `adb shell getprop net.rmnet0.dns1`
  - `adb shell getprop net.rmnet0.dns2`
- Prueba conectividad:
  - `adb shell ping -c 2 -w 8 google.com`
  - Nota: algunos carriers filtran ICMP a `8.8.8.8`, no usarlo como prueba unica.

## PIN de SIM en CM9 (2026-10-20)

Con el bloqueo de SIM, el PIN al arrancar daba "PIN incorrecto" y `rild` se
reiniciaba: SIGSEGV en `qcril_uim_request_enter_pin` (fault addr 0). El shim
recibía `ENTER_SIM_PIN` con `data = NULL, datalen = 0`: `HuaweiRIL` (base de
CM9 para otros Huawei) escribe `{writeString(null), pin}`; `libril` lee ese
null como count `-1` y despacha un array NULL. `HuaweiY210RIL` ahora envía
PIN/PUK/PIN2/PUK2 y los cambios de PIN en el formato estándar de ICS
(`{n strings, aid}`, con `mAid` si el aid llega nulo), y `ril/rilshim` pone el
AID guardado si falta o es nulo. El blob se caía antes de verificar, así que
esos intentos no gastaron reintentos.

Validado: `ENTER_SIM_PIN {0}`, `LOCKED`→`READY`→`IMSI`→`LOADED`, sin pantalla de
red (con `ro.telephony.ril.v3=qcomuiccstack`), registro en Claro.

## Señal en 0 barras en CM9 (2026-10-20)

Síntoma: barras de señal vacías y Ajustes > Estado mostraba `-32767 dBm 0 asu`
aunque había servicio HSPA. `dumpsys telephony.registry` daba
`SignalStrength: 8 -1 -1 -1 -1 -1 -1 99 -32767 -32767 -32767 -32767`: el GSM (asu 8)
llega bien, pero qcril rellena los campos LTE con 99/-32767 (sus "inválidos")
y `SignalStrength.getLevel()/getDbm()` de ICS solo usa GSM si los cuatro LTE son
exactamente -1; si no, calcula como LTE -> 0 barras.

Arreglo (solo device tree): `ro.telephony.ril.v3=qcomuiccstack,signalstrength`
en system.prop. Con `signalstrength`, `RIL.responseSignalStrength()` lee solo
los 7 enteros GSM/CDMA/EVDO y pone -1 en los LTE. Verificado en el teléfono:
`-97 dBm 8 asu` y barras visibles.
