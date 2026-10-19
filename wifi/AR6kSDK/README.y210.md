# AR6kSDK 3.1 RC.734 (Huawei Y210)

Driver Atheros AR6003 (`ar6000.ko`) compilado desde fuente. Reemplaza al
módulo binario de Huawei (`AR6kSDK.build_3.1_RC.776`, U8685), que es el mismo
SDK: 741 de sus 753 funciones existen con el mismo nombre en RC.734.

Origen: `AR6kSDK.build_3.1_RC.734/{host,include}` de
https://github.com/CyanogenMod/AR6kSDK.3.1 (commit `7b6e0ff`, sin `host/tools`, que son
utilidades de userspace). Se compila con `wifi/Android.mk` vía
`TARGET_KERNEL_MODULES`; `wifi/build_ar6000.sh` hace lo mismo a mano.

Cambios para el Y210 (todos replican lo que hace el módulo stock):

- `os/linux/ar6000_android.c`: GPIO de reset/encendido 6 en vez de 134, y sin
  `gpio_request`/`gpio_free` (`WLAN_GPIO_OWNED_BY_BOARD`): el board file lo
  reserva como `ATH_WLAN_REG` al arrancar. Los rails (`bt` 3.3 V, `wlan4`
  1.8 V) y el voto del reloj A0 ya coincidían.
- `os/linux/include/ar6000_drv.h`: nombres de firmware REV3 planos en
  `/system/wifi/ath6k/` y board data en `/data/misc/wifi/load/caldata.bin`
  (la copia `init.huawei.rc` desde `caldata_MSM7225A_Y210.bin`).
- `os/linux/ar6000_drv.c`: interfaz por defecto `eth0` (`wifi.c` carga el
  módulo sin argumentos); sin `ar6000_prealloc_init/deinit` cuando el kernel
  provee los buffers DMA (`ATH_PREALLOC_IN_KERNEL`).
- `os/linux/Makefile`: no compila `prealloc/` con `ATH_PREALLOC_IN_KERNEL`
  (el board file ya exporta `ar6000_dma_buffer_alloc/free`).
- `os/linux/ar6k_pal.c`: este kernel 2.6.38 de MSM sigue llamando
  `HCI_80211` a lo que kernels posteriores llaman `HCI_AMP`.

Diferencias conocidas con el stock: falta `ar6000_cal_powerTuned` (ajuste de
potencia TX de Huawei desde `/data/misc/wifi/Cal_powerTuned_pa.bin`) y la MAC
ahora sale del EEPROM del chip en vez de generarse al azar en cada arranque.

## Validación (2026-10-19)

- 16 min conectado (32 muestras cada 30 s): sin cortes ni recargas del módulo.
  El corte cada ~5 min del blob (`cscan` rechazado -> `DRIVER HANGED`) no
  aparece.
- Suspensión real sin cable USB (con USB, `usb_bus_active` impide dormir):
  6 ciclos en ~8 min, `ar6000_suspend_ev`/`ar6000_resume_ev` en cada uno,
  0 errores `CMD5`/`card was removed`, WiFi conectado y con ping al
  despertar. Es el bug del blob documentado en el `WIFI_NOTES.md` de CM7
  (no pide `MMC_PM_KEEP_POWER`); `hif.c` de RC.734 sí lo hace.
