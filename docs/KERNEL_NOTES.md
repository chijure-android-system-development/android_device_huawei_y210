# Kernel Notes

## Kernel en uso

Hasta 2026-10-18 el `boot.img` llevaba el zImage **stock de Huawei**
(`2.6.38.6-perf`, `android@localhost`, 22-ene-2013), sin `CONFIG_SWAP` ni
`CONFIG_ZRAM`. Huawei nunca publicó el código completo; el árbol adaptado está
en github.com/chijure/android_kernel_huawei_y210 (rama `android-msm-2.6.38`).

`arch/arm/configs/y210_defconfig` parte de la config stock (`/proc/config.gz`
del equipo) y agrega `SWAP`, `ZRAM`, `LZO`, kernel `LZMA`, `TUN` y MTP. El
`vermagic` (`2.6.38.6-perf preempt mod_unload ARMv7`) coincide con el de
`ar6000.ko`, el único módulo binario externo, y no hay `MODVERSIONS`.

## Compilar

En el host (no hace falta el contenedor), ~1.5 min:

```bash
cd ~/android_kernel_huawei_y210
export ARCH=arm CROSS_COMPILE=~/arm-eabi-4.4.3/bin/arm-eabi-
make O=~/kernel-out-y210 y210_defconfig
make O=~/kernel-out-y210 -j$(nproc) zImage
```

El árbol trata algunos warnings como error ("forbidden warning"). Fue útil:
detectó dependencias ocultas al desactivar drivers (ver abajo).

## Probar un kernel sin rehacer la ROM

```bash
adb shell dump_image boot /data/local/tmp/boot_backup.img      # siempre primero
adb pull /data/local/tmp/boot_backup.img
out/host/linux-x86/bin/unpackbootimg -i boot_backup.img -o cur/
out/host/linux-x86/bin/mkbootimg --kernel ~/kernel-out-y210/arch/arm/boot/zImage \
    --ramdisk cur/boot_backup.img-ramdisk.gz --cmdline "$(cat cur/*-cmdline)" \
    --base $(cat cur/*-base) --pagesize $(cat cur/*-pagesize) -o boot_test.img
adb push boot_test.img /data/local/tmp/ && adb shell flash_image boot /data/local/tmp/boot_test.img
```

Verificar el flasheo con `dump_image` + `cmp` antes de reiniciar. Usar el
`mkbootimg`/`unpackbootimg` de `out/host`, no el del sistema.

## Cambios sobre el árbol del kernel (2026-10-18, commits locales)

| Commit | Cambio | Resultado |
|---|---|---|
| `9821fbe3` | Solo el sensor de cámara `mt9d113` (el del Y210); fuera s5k5ca, mt9p017, ov5647, mt9t113, hi255, mt9e013, s5k4e1, imx072, ov9726, mt9v113. Referencias del board bajo su `#ifdef` | init del kernel 6.62 → 4.09 s |
| `681b6172` | Fuera mma8452, l3g4200d, aps-12d, aps-9900, tpa2028d1, pn544 (NFC, ~0.4 s de reintentos) | 4.09 → 3.50 s |
| `1f37bc38` | `i2c-qup`: NACK (nadie responde en la dirección) a `dev_dbg`; timeouts y otros errores siguen como error | líneas `qup_i2c` 74 → 4 |
| `f452f06b` + `eef19b77` | `MMC_PM_KEEP_POWER` para el slot Wi-Fi en suspend, y su revert | colgaba el resume |
| `968abe96` | Slot Wi-Fi: solo apagar relojes en suspend, sin `mmc_suspend_host()` (como Huawei hacía con Broadcom) | Wi-Fi con datos tras deep sleep (bug del stock) |

El árbol de Huawei es genérico para varias placas msm7x27a y prueba hardware
que el Y210 no tiene (todas las fuentes parecen de Y100/Y200). No se pueden
quitar, aunque sus probes fallen:

- `CONFIG_GPIO_SX150X`: `register_i2c_devices()`, que también registra el
  códec Marimba (BT/FM), está bajo esa opción. Sin ella BT y FM se rompen.
- ADXL346, AK8975 y touch RMI4: definen `sensor_dev`, `hasCompass` y
  `get_synaptics_touch_info`, que usan los demás drivers de acelerómetro
  (incluido el LIS3DH real) y `hardware_self_adapt.c`.
- TPS61310: una función `static` de `mach/camera.h` depende de él.

Hardware real detectado: touch Melfas, acelerómetro LIS3DH/LIS35DE (único
sensor de hardware), cámara mt9d113, Marimba (BT/FM), WCN2243 (FM, `Chip ID
4020205`), AR6005 (Wi-Fi SDIO). Sin proximidad ni brújula.

## dmesg de arranque comparado

Arranque limpio, `boot_completed` + 30 s, mensajes de nivel `<0>`-`<4>`:

| Kernel | errores/advertencias | líneas `qup_i2c` | fin init del kernel |
|---|---|---|---|
| stock Huawei | 337 | 104 | 6.62 s |
| propio (`y210_defconfig` sin cambios) | 337 | 104 | 6.63 s |
| v4 (los tres commits de arriba) | 183 | 4 | 3.50 s |

El kernel propio sin cambios no agrega ningún error respecto al stock (solo
cambian rutas `__FILE__` en dos mensajes y la MAC aleatoria). Lo que queda es
ruido no corregible desde el kernel: `ar6000` imprime con `KERN_ALERT` desde el
binario, `smd_pkt_open DATAx_CNTL failed` lo provoca `qmuxd` (userspace stock)
y los `<4>` del arranque son banners normales.

## Validación del kernel propio

`test_y210.sh --fast` 54 PASS / 0 FAIL con cada versión; Wi-Fi (carga del
módulo binario, DHCP, ping, ciclos on/off), RIL, cámara (foto 1600x1200 y video
H.263 352x288), FM (chip, firmware, `READY_EVENT`), sensores, zram (ver
PERFORMANCE_NOTES.md) y deep sleep continuo. 0 crashes y 0 tombstones en los
benchmarks.

Llamadas con el kernel propio (2026-10-18): entrante y saliente OK, audio en
ambos sentidos por auricular y altavoz confirmado por el usuario, mute OK,
fin de llamada con causa 16 (normal). Los `audmgr_rpc ... DISABLED` de dmesg
son el aviso normal de cierre de sesión de audio.

FM con el kernel propio (2026-10-18): audio limpio por auriculares y
altavoz (confirmado por el usuario), sintonía manual OK (87.5/98.1/97.4/97.3)
y búsqueda hacia arriba OK (`SEEK_COMPLETE`, 97.3 → 100.1 → 101.1). La
búsqueda hacia abajo no termina: el chip responde `tavarua_radio: ERROR
STATE` y no llega `SEEK_COMPLETE` (ver FM_NOTES.md). Falta confirmar si con el
kernel stock pasa lo mismo; la única prueba previa de seek fue hacia arriba.

Pendiente con el kernel propio: Bluetooth A2DP.

## Wi-Fi tras suspensión

Ver WIFI_NOTES.md ("Wi-Fi sin datos después de suspender").
