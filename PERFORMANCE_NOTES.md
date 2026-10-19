# Performance Notes — Huawei Y210 CM9 (ICS)

Última actualización: 2026-06-03

---

## Mapa de memoria física (MSM7225A)

El Y210 tiene 256 MB físicos pero solo ~165 MB llegan a Android. La diferencia
la reserva el **ARM9** (modem AMSS) antes de que arranque el kernel Linux.

```
Mapa de /proc/iomem:

00000000-001FFFFF  (2 MB)   → Bootloader / vectores (no visible al kernel)
00200000-0A9FFFFF  (~167 MB) → System RAM (Android)
0AA00000-0CCFFFFF  (~35 MB)  → RESERVADO: ARM9 / AMSS firmware
0CD00000-0D4FFFFF  (8 MB)   → System RAM (Android)
0D500000-0D5FFFFF  (1 MB)   → hw_share_mem / SMEM (puente Android↔modem)
0D600000-0FFFFFFF  (~42 MB) → RESERVADO: ARM9 / AMSS firmware
```

Total usable: ~175 MB visible al kernel, ~165 MB reportado en MemTotal
(el kernel mismo consume ~10 MB).

### Por qué el ARM9 necesita tanta RAM

El MSM7225A es un SoC heterogéneo: contiene un ARM11 (Android) y un ARM9
independiente que corre su propio OS propietario Qualcomm (AMSS). El ARM9
maneja toda la radio (GSM/UMTS/GPS/FM) y tiene su propio heap, pilas, y
código firmware. El bootloader le reserva ese bloque antes de entregar el
control a Linux — esto es hardware-fijo y no puede cambiarse sin modificar
el bootloader o el firmware AMSS.

La comunicación entre Android y el ARM9 usa SMEM (Shared Memory, 1 MB) y
los siguientes daemons visibles en `ps`:
- `rpcrouter` / `rpcrotuer_smd_x` — RPC sobre SMD (Serial Mux Driver)
- `qmuxd` / `qmiproxy` — QMI (Qualcomm MSM Interface)
- `rild` — traduce comandos AT de Android a mensajes QMI/ONCRPC para el ARM9

Los threads del kernel en estado `D` que inflan el load average
(`krtcclntd`, `kbatteryclntd`, `khsclntd`, etc.) son clientes RPC del kernel
esperando respuesta del ARM9. Son normales para este SoC.

---

## LMK (Low Memory Killer)

### Valores actuales (2026-10-18): los calcula ActivityManager

El LMK ya no se fija desde init. En ICS, `ProcessList.updateOomLevels()`
escribe `adj` y `minfree` al arrancar, a partir de la RAM total y el tamaño
de pantalla. Para el Y210 (~165 MB, 320x480) da los valores mínimos de ICS:

```
minfree: 2048,3072,4096,6144,7168,8192   (páginas de 4 KB = 8,12,16,24,28,32 MB)
```

Antes, `init.huawei.rc` (al terminar la bootanimation) e `init.mem.rc`
pisaban `minfree` con valores propios (último `6144` = 24 MB). El problema
es que AMS sigue usando *sus* niveles para decidir `MemoryInfo.lowMemory`
(umbral de apps ocultas ≈ 28 MB): con el kernel matando recién a 20-24 MB,
el sistema quedaba casi siempre por debajo del umbral de AMS y reportaba
`lowMemory=true`. WebKit consulta ese flag (`canSatisfyMemoryAllocation`) y
rechazaba asignaciones, lo que crasheaba el navegador con SVG. Se quitaron
el override de `init.huawei.rc` e `init.mem.rc` (cuyos `ro.*_MEM` ICS
ignora).

### zram

`ro.zram.default=18` (`system.prop`): `handle_compcache` de CM arma ~30 MB
de swap comprimido en `/dev/block/zram0` (driver built-in) con
`swappiness=30`.

### Carveouts del kernel

`pmem_gpu0`/`pmem_gpu1` (4 MB cada uno) se quitaron del board file: ningún
blob Adreno referencia esos nodos (usan KGSL, `IOCTL_KGSL_GPMEM_ALLOC`) y
ningún proceso los mapeaba. +8 MB para el sistema.

---

## Apps de sistema deshabilitadas (2026-06-03)

Estas apps corrían en background sin utilidad para el usuario, consumiendo
~100 MB de RSS combinado:

| Package | RSS antes | Motivo de deshabilitar |
|---|---|---|
| `com.android.email` | 20 MB | Sin cuenta de email configurada |
| `com.android.exchange` | 18 MB | Exchange sync innecesario |
| `com.cyanogenmod.updater` | 18 MB | No hay OTA para este port |
| `com.bel.android.dspmanager` | 17 MB | Sin funcionalidad real en Y210 |
| `com.android.voicedialer` | 17 MB | Sin uso |

RAM libre antes: **~5.7 MB** | después: **~16 MB**

Comando para reaplicar tras factory reset:

```bash
adb shell pm disable com.android.email
adb shell pm disable com.android.exchange
adb shell pm disable com.cyanogenmod.updater
adb shell pm disable com.bel.android.dspmanager
adb shell pm disable com.android.voicedialer
```

---

## CPU governor

El MSM7225A usa el governor `ondemand` por defecto. Valores esperados:

```bash
adb shell cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# ondemand

adb shell cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq
# 300000  (300 MHz mínimo)

adb shell cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq
# 600000  (600 MHz máximo)
```

`init.qcom.post_boot.sh` aplica el perfil de CPU para `y210` al final del boot.

---

## Validación de estado en runtime

```bash
# Memoria
adb shell cat /proc/meminfo | grep -E "MemTotal|MemFree|Cached|SwapTotal"

# LMK actual
adb shell cat /sys/module/lowmemorykiller/parameters/minfree
adb shell cat /sys/module/lowmemorykiller/parameters/adj

# CPU
adb shell cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
adb shell cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq

# Load average
adb shell cat /proc/loadavg

# Top procesos por RAM
adb shell ps | awk '{print $5, $9}' | sort -rn | head -20

# Apps deshabilitadas
adb shell pm list packages -d
```

Salida esperada de LMK:

```
1536,2048,3072,5120,6144,8192
0,2,4,6,7,15
```
