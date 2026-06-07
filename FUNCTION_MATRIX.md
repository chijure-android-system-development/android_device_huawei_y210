# Huawei Y210 — Matriz de funciones CM9 (Android 4.0.4 ICS)

Última actualización: 2026-06-06

Este documento es la fuente de verdad sobre el estado del port. Refleja CM9 (ICS).
Los notes por área están en los archivos `*_NOTES.md` de este directorio.

## Convención de estado

- **OK** — validado en runtime (uso real).
- **Parcial** — funciona con limitaciones o falta validar un caso clave.
- **No** — falla o no implementado.
- **N/A** — hardware no existe en este modelo.
- **Pendiente** — sin evidencia/logs recientes.

## Hardware del dispositivo

| Elemento | Valor |
|---|---|
| SoC | Qualcomm MSM7225A (ARMv6, Cortex-A5 @ 600 MHz) |
| Modem | ARM9 @ ~200 MHz, AMSS (reserva ~77 MB de los 256 MB físicos) |
| RAM | 256 MB físicos / ~165 MB usables por Android |
| GPU | Adreno 200 (Qualcomm) |
| Pantalla | 320×480 HVGA, msmfb30 |
| Touchscreen | Melfas MMS128 |
| WiFi | AR6003 (ath6kl), interfaz `eth0` |
| Bluetooth | Qualcomm Bahama, HCI-IBS UART `/dev/ttyHS0` |
| Cámara | Sensor OV5640 (blob Qualcomm QualcommCameraHardware) |
| GPS | Integrado en ARM9, ONCRPC `LOC_APIPROG 0x3000008c` |
| FM | Tavarua, I2C `/dev/i2c-1` + `/dev/radio0` |

---

## Matriz de estado (CM9 — 2026-06-06)

### Boot / sistema

| Función | Estado | Nota |
|---|---|---|
| Boot hasta launcher | **OK** | Trebuchet |
| `system_server` estable | **OK** | |
| `surfaceflinger` estable | **OK** | |
| `mediaserver` estable | **OK** | |
| `adb` (USB debugging) | **OK** | |
| `dalvik-cache` limpio | **OK** | Borrar ambos: `/data/dalvik-cache` y `/cache/dalvik-cache` |

### Pantalla / gráficos

| Función | Estado | Nota |
|---|---|---|
| Framebuffer / UI visible | **OK** | 320×480 60 Hz, 3 buffers |
| SurfaceFlinger compositor | **OK** | `debug.sf.hw=1`; renderer Adreno 200 validado en boot |
| GPU Adreno200 — driver | **OK** | Kernel con `IOCTL_KGSL_TIMESTAMP_EVENT`; sin errores genlock/KGSL en boot |
| GPU Adreno200 — apps (HW GL) | **Parcial** | Gallery creó contexto EGL HW; falta prueba amplia de apps 3D |
| hwcomposer | **No** | Desactivado: desalinea touch. SurfaceFlinger usa software |
| Gralloc / copybit | **Parcial** | Local `gralloc.msm7x27a.so` con native handle CAF (`sNumFds=2`); no mezclar HALs e400/proprietarios |
| Ghosting / SWAP_RECTANGLE | **OK** | Fix `fb_setUpdateRect_noop` — ver `RENDER_NOTES.md` |

### Input

| Función | Estado | Nota |
|---|---|---|
| Touchscreen | **OK** | melfas-touchscreen.idc instalado |
| Multitouch (MT tipo A) | **OK** | |
| Botones físicos (power/vol) | **OK** | |
| Botones capacitivos (home/back/menú) | **OK** | Virtual keys del touchscreen; `melfas-touchscreen.kl` evita fallback a `Generic.kl` (`key 102 MOVE_HOME`) |

### Luces / vibración

| Función | Estado | Nota |
|---|---|---|
| Backlight | **OK** | |
| LED notificación (RGB) | **OK** | |
| Vibrador | **OK** | `/sys/class/timed_output/vibrator/enable` |

### Audio

| Función | Estado | Nota |
|---|---|---|
| Salida speaker | **OK** | `audio.primary.y210.so` wraps `libaudio.so` |
| Salida auriculares | **Parcial** | Funciona; volumen percibido bajo vs stock |
| Micrófono (grabación) | **OK** | |
| Llamadas voz (downlink) | **OK** | |
| Llamadas voz (uplink/mic) | **Parcial** | Requiere `send_mic_mute_to_AudioManager=true` en overlay |
| A2DP / SCO Bluetooth | **Pendiente** | |
| Radio FM — init | **Parcial** | `hw.fm.init=1` OK; audio pendiente de validar. Ver `FM_NOTES.md` |

### Wi-Fi

| Función | Estado | Nota |
|---|---|---|
| Encender / scan / asociar | **OK** | AR6003 / ath6kl, interfaz `eth0` |
| DHCP | **OK** | |
| Redes persistentes | **OK** | wpa_supplicant.conf no se sobreescribe en boot |
| Wi-Fi tethering (hotspot) | **OK** | NAT sobre rmnet0 |

### Bluetooth

| Función | Estado | Nota |
|---|---|---|
| Encender / HCI up | **OK** | `hci0 UP RUNNING` |
| Pairing | **OK** | |
| Audio BT (A2DP/SCO) | **Pendiente** | |

### Sensores

| Función | Estado | Nota |
|---|---|---|
| Acelerómetro (LIS3DH) | **OK** | |
| Gravity / Linear acceleration | **OK** | Virtual |
| Proximidad | **N/A** | Hardware no presente |
| Luz / ALS | **N/A** | Hardware no presente |
| Brújula | **N/A** | Hardware no presente |

### GPS

| Función | Estado | Nota |
|---|---|---|
| Motor RPC (Loc API) | **OK** | `Loc API RPC client initialized` |
| XTRA (efemérides asistidas) | **OK** | 41 partes inyectadas |
| NTP / UTC injection | **OK** | |
| AGPS (UMTS SLP) | **OK** | |
| Fix real (satélites) | **Pendiente** | Requiere prueba al aire libre |

Ver `GPS_NOTES.md` para detalles y bugs resueltos.

### Cámara

| Función | Estado | Nota |
|---|---|---|
| Camera HAL (ICS API) | **No** | El wrapper CM7 no es compatible con ICS CameraHardwareInterface. Requiere port. |

Ver `CAMERA_NOTES.md` para el análisis del wrapper CM7 y la estrategia de port a ICS.

### Almacenamiento / USB

| Función | Estado | Nota |
|---|---|---|
| `/data` / escritura | **OK** | |
| SDCard (montaje) | **OK** | |
| UMS (almacenamiento masivo USB) | **OK** | `sdcard2` comparte `/dev/block/vold/179:1` por `lun0`; Ajustes muestra la microSD como Tarjeta SD |

### Radio / Telefonía (RIL)

| Función | Estado | Nota |
|---|---|---|
| Baseband / operador / señal | **OK** | Claro Perú 71610, HSPA, baseband 109808 |
| IMEI | **OK** | `service call iphonesubinfo 1` |
| Llamadas (voz) | **OK** | Entrante y saliente |
| SMS | **OK** | Enviar y recibir |
| Datos móviles (rmnet0) | **OK** | HSPA; IP real validada |

Ver `RIL_NOTES.md` para arquitectura QCRIL y fixes de datos.

### Energía / Performance

| Función | Estado | Nota |
|---|---|---|
| Suspensión / pantalla on-off | **OK** | `early_suspend`/`late_resume` OK |
| Deep sleep | **OK** | WiFi WoW, wakeup periódico modem normal |
| RAM disponible (baseline) | ~15 MB libres | Tras deshabilitar apps innecesarias |
| LMK | Ajustado | 32 MB threshold para apps fondo. Ver `PERFORMANCE_NOTES.md` |

### Sistema de salud / misc

| Función | Estado | Nota |
|---|---|---|
| BatteryStats | **OK** | `xt_qtaguid` portado al kernel desde e400. `/proc/net/xt_qtaguid/stats` disponible con datos reales. |
| `installd` | **OK** | `class main` en `init.y210.rc` |
| Screenshot (power menu) | **OK** | Funciona con ruta HW EGL/Adreno; el fallo era de la ruta PixelFlinger/software |
| Teclado LatinIME | **OK** | Fix overlay: `keyboardHeight` 1.285in→1.1in, `keyboard_bottom_padding` 4.669%→1% |

---

## Apps de sistema deshabilitadas (2026-06-03)

Para liberar RAM en el dispositivo de 165 MB usables:

```bash
adb shell pm disable com.android.email
adb shell pm disable com.android.exchange
adb shell pm disable com.cyanogenmod.updater
adb shell pm disable com.bel.android.dspmanager
adb shell pm disable com.android.voicedialer
```

Estas apps suman ~100 MB de RAM comprometida sin ofrecer valor en este dispositivo.

---

## Bugs pendientes (por prioridad)

1. **Camera HAL** — port del wrapper a ICS CameraHardwareInterface. Bloqueante para cámara.
2. ~~**Screenshot (power menu)**~~ — resuelto con ruta HW EGL/Adreno; PixelFlinger/software sigue sin FBO.
3. **GPU HW GL / Adreno EGL** — boot HW validado; falta prueba amplia de apps 3D y estabilidad prolongada.
4. **hwcomposer** — desactivado por alineación de touch; investigar causa raíz.
5. ~~**BatteryStats**~~ — resuelto: `xt_qtaguid` portado al kernel.
6. **Audio uplink** — validar que el overlay `send_mic_mute_to_AudioManager` funciona en llamadas.
7. **FM audio** — init OK, audio en auriculares pendiente.
8. **BT audio** — A2DP/SCO no validado.
9. **GPS fix real** — motor OK, falta prueba al aire libre.

---

## Comandos diagnóstico rápido

```bash
# Estado general
adb shell getprop | grep -E "ro.build|gsm.operator|init.svc"
adb logcat -v time -d 2>&1 | tail -50
adb shell dmesg 2>&1 | tail -30

# Memoria
adb shell cat /proc/meminfo | grep -E "MemTotal|MemFree|Cached"
adb shell ps | awk '{print $5, $9}' | sort -rn | head -20

# LMK actual
adb shell cat /sys/module/lowmemorykiller/parameters/minfree
adb shell cat /sys/module/lowmemorykiller/parameters/adj

# GPU
adb shell ls -l /dev/kgsl-3d0 /dev/pmem_gpu0 /dev/pmem_gpu1
adb logcat -d | grep -i "EGL\|SurfaceFlinger\|flags ="

# RIL
adb shell getprop gsm.version.baseband
adb shell getprop gsm.operator.alpha
adb shell getprop init.svc.ril-daemon
```

## Build commands

```bash
# Build completo
cd /home/chijure/cm9
source build/envsetup.sh && lunch cm_y210-userdebug
make -j$(nproc)

# OTA ZIP
make otapackage -j$(nproc)
# Resultado: out/target/product/y210/cm_y210-ota-eng.*.zip

# Instalar
adb push out/target/product/y210/cm_y210-ota-eng.*.zip /sdcard/update.zip
# Luego instalar desde TWRP

# Solo boot.img
make -j$(nproc) bootimage
fastboot flash boot out/target/product/y210/boot.img
```
