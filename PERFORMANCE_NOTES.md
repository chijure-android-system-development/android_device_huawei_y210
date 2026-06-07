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

### Valores actuales (init.mem.rc — 2026-06-03)

Ajustados para los ~165 MB reales disponibles, no los 256 MB físicos:

```
minfree: 1536,2048,3072,5120,6144,8192   (en páginas de 4 KB)
adj:     0,2,4,6,7,15
```

En MB equivalente:

| Nivel | adj | Threshold | Qué proceso mata |
|---|---|---|---|
| Foreground | 0 | **6 MB** | App en primer plano (no tocar) |
| Visible | 2 | **8 MB** | App visible pero no activa |
| Perceptible | 4 | **12 MB** | App perceptible (música, etc.) |
| Home | 6 | **20 MB** | Launcher |
| Secondary | 7 | **24 MB** | Servicios secundarios |
| Background | 15 | **32 MB** | Apps en background (principal mejora) |

El threshold de background subió de 24 MB (original Huawei) a **32 MB** para
forzar al LMK a liberar apps ociosas antes de que el sistema llegue al límite
crítico de ~5 MB libre que producía la lentitud.

### Archivos de configuración

- `device/huawei/y210/prebuilt/init.mem.rc` — valores del LMK (ejecutado en `on boot`)
- `device/huawei/y210/prebuilt/init.y210.rc` — NO contiene LMK (init.mem.rc lo sobreescribiría de todas formas)

### Orden de ejecución de init

```
init.rc
  └─ init.huawei.rc
       ├─ init.y210.rc   (on boot ejecuta primero)
       └─ init.mem.rc    (on boot ejecuta después → sus valores de LMK ganan)
```

Por esto el LMK debe configurarse en `init.mem.rc`, no en `init.y210.rc`.

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
