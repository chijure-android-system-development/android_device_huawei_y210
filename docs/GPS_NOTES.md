# Huawei Y210 (CM7) — GPS Notes

## Hardware

- Chipset: Qualcomm MSM7225A integrado (mismo die que el modem)
- Interfaz: ONCRPC sobre `/dev/oncrpc/` — el GPS corre como servicio en el modem ARM9
- Programa RPC: `LOC_APIPROG = 0x3000008c`, versión `0x00050001`
- HAL: `gps.y210.so` — `hardware/qcom/gps`, generado con `gen-50000`/`inc-50000`

## Estado actual

- Motor GPS iniciado (`Loc API RPC client initialized`): **OK**
- XTRA (efemérides asistidas): **OK** — 41 partes descargadas e inyectadas al arranque
- NTP / UTC injection: **OK** — servidor SNTP responde y se inyecta con éxito
- AGPS (UMTS SLP): **OK** — servidor configurado vía `RPC_LOC_IOCTL_SET_UMTS_SLP_SERVER_ADDR`
- Fix real en exteriores: **Pendiente** — requiere prueba a cielo abierto

## Bugs resueltos

### Bug 1 — `cannot create RPC client` (GID 3006 faltante)

**Síntoma:** `gps.y210` fallaba al inicio con `Error: cannot create RPC client.`

**Causa:** `system_server` no tenía el grupo suplementario `qcom_oncrpc` (GID 3006).
Los nodos `/dev/oncrpc/` tienen modo `0660` con grupo `qcom_oncrpc`, por lo que
`clnt_create()` fallaba al abrir el socket RPC.

**Fix (desde 2026-10-20):** los nodos `/dev/oncrpc/*` son de `system:qcom_oncrpc`
(`prebuilt/ueventd.huawei.rc` y el `chown 1000.3006` de `init.huawei.rc`), así que
`system_server` (uid 1000) los abre como dueño. Antes se agregaba el GID 3006 al
`--setgroups=` de `system_server` en `ZygoteInit.java` (parche de plataforma,
retirado).

**Verificación:**
```bash
adb shell "ls -l /dev/oncrpc/ | grep 3000008c"
# crw-rw---- root     qcom_oncrpc ...  3000008c:00050001
adb logcat -d | grep "Loc API RPC client initialized"
```

### Bug 2 — versión `0x00050000` vs `0x00050001` (FALSA PISTA — descartada)

Durante el debug se observó que el nodo del modem aparecía como
`/dev/oncrpc/3000008c:00050000` en algunos dumps, lo que llevó a pensar que
`clnt_create()` necesitaba la versión `0x00050000`.

Se parcheó `gen-50000/loc_api_rpc_glue.c` para usar `0x00050000` y se creó un
mecanismo `BOARD_VENDOR_QCOM_GPS_LOC_API_GLUE_VERSION` en `Android.mk`.

**Conclusión:** el fix de versión **no era necesario**. Una vez resuelto el Bug 1
(GID 3006), el stack GPS funciona con `LOC_APIVERS = 0x00050001` sin cambio alguno
en `hardware/qcom/gps`. El nodo del kernel acepta la conexión independientemente
del sufijo de versión en el nombre del device node.

El parche a `gen-50000` fue revertido; `hardware/qcom/gps` queda sin modificar.

## Configuración en BoardConfig.mk

```makefile
BOARD_USES_QCOM_GPS := true
BOARD_VENDOR_QCOM_GPS_LOC_API_AMSS_VERSION := 50000
BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE := y210
```

## Configuración en device_y210.mk / gps.conf

- `gps.conf` leído desde `/etc/gps.conf` al arranque
- Servidor AGPS UMTS SLP configurado por el framework desde `gps.conf`

## Comandos de diagnóstico

```bash
# Estado del motor GPS
adb logcat -v time -d | grep -iE "loc_api_rpc|Loc API RPC|cannot create"

# XTRA y NTP
adb logcat -v time -d | grep -iE "xtra|inject_xtra|INJECT_UTC"

# Nodos ONCRPC del modem
adb shell "ls -l /dev/oncrpc/ | grep 3000008c"

# Capacidades reportadas al framework
adb logcat -v time -d | grep "set_capabilities_callback"

# Grupo qcom_oncrpc del system_server (debe aparecer 3006)
adb shell "cat /proc/$(adb shell pidof system_server)/status | grep Groups"
```

## Pendiente

- Fix real (satélites) en exteriores — prueba a cielo abierto sin obstáculos.
  Cuando se obtenga fix, actualizar `FUNCTION_MATRIX.md`:
  `GPS → Fix + ubicación real: OK`

## gps.y210 desde el device tree (2026-10-18)

`hardware_qcom_gps.patch` se retiró. `gps/` tiene la copia de
`hardware/qcom/gps/loc_api` (CM7 `c05e328`) solo para AMSS 50000
(`libloc_api-rpc` con `gen-50000`/`inc-50000` y `libloc_api`), con el buffer
NMEA de 1200 bytes y el reporte hasta el primer NUL ya aplicados. El
`BoardConfig.mk` no define `BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE`, así que
`hardware/qcom/gps/Android.mk` no incluye nada y no hay módulos duplicados.
La `gps.y210.so` resultante es idéntica byte a byte (md5 `dd61c1e3...`) a la
que salía del árbol parcheado.

## NMEA y fecha de las ubicaciones (2026-10-19)

**NMEA vacío.** `loc_eng` registra `RPC_LOC_EVENT_NMEA_1HZ_REPORT` (`0x4`) y el
módem manda ese evento, pero en `gen-50000/loc_api_common_xdr.c` (igual que en
`hardware/qcom/gps` `c05e328`) el `case RPC_LOC_EVENT_NMEA_1HZ_REPORT` estaba en
la unión de **ioctl** (pegado al wiper, código muerto: no hay ioctl 4) y no en
`xdr_rpc_loc_event_payload_u_type`. El cuerpo del evento nunca se decodificaba y
cada reporte llegaba con `length=0` y bytes en cero. En el stock
(`libloc_api-rpc-qc.so` de B855) los eventos `0x4` y `0x8` saltan al mismo
`bl xdr_rpc_loc_nmea_report_s_type`. Además `nmea_sentences` es de 200 bytes
(`xdr_opaque(..., 200)` en el stock; `gen-50000` tenía 1200): es el único de los
17 campos de tamaño fijo que difería. Con ambos cambios llegan `$GPGGA`,
`$GPGSA`, `$GPGSV`, `$GPRMC` y `$GPVTG` completas y `loc_eng_report_nmea` usa
otra vez el `length` del módem (con el primer NUL como respaldo).

**Ubicaciones fechadas en 2007.** Con el NMEA se vio `$GPRMC` con fecha
`050307`: el AMSS es anterior al rollover de semanas GPS de abril de 2019 y
reporta UTC exactamente 1024 semanas atrás (`mTime` de una ubicación de octubre
de 2026 = marzo de 2007). `loc_eng_report_position` suma 1024 semanas a los
timestamps anteriores al 2019-04-07. Validado: `mTime=1792439563000`
(2026-10-19). Las fechas del texto NMEA también se corrigen:
`loc_eng_fix_nmea_dates` (en `loc_eng_report_nmea`) suma las mismas 1024
semanas al campo `ddmmyy` de `$GPRMC` y a día/mes/año de `$GPZDA` cuando son
anteriores al rollover, sin cambiar el largo, y recalcula el checksum.
