

# TWRP Device configuration for Huawei Ascend Y210

## Device specifications:

Basic    | Spec Sheet
--------:|:----------------------
Chipset  | Qualcomm Snapdragon MSM7627A (Snapdragon S1)
CPU      | 1.0 GHz Cortex-A5 (Single-core)
GPU      | Adreno 200
Memory   | 256 MB RAM
Storage  | 512 MB ROM, microSD up to 32GB
Supported Models | Huawei Ascend Y210-0010, Y210-2010
Shipped Android version | Android 2.3.6 (Gingerbread)
Codename | y210
Battery  | Li-Ion 1750 mAh, removable
Display  | 3.5 inch TFT, 320 x 480 pixels (HVGA), 160 dpi
Rear Camera  | 3.15 MP, 2048 x 1536 pixels, autofocus
Front Camera | No
Release Date | January 2013

## How to Compile:

```
source build/envsetup.sh
lunch omni_y210-eng
make recoveryimage -j2
```

## How to Install:
> - Reboot into bootloader mode: power off the device and hold Volume Down + Power.
> - Flash the recovery.img using fastboot:
>   ```
>   fastboot flash recovery recovery.img
>   ```
> - Reboot into recovery mode: power off the device and hold Volume Up + Power.
> - Done! You can now use TWRP on your Huawei Y210.

## Working Features List:
> - [x] Mount /system, /data, /cache (yaffs2)
> - [x] Backup/restore de particiones básicas
> - [x] Instalación de zips
> - [x] Wipe de caché/datos
> - [x] ADB (forzado en recovery)
> - [ ] MTP (no soportado)
> - [ ] Decrypt /data (no aplica en Android 2.3)
> - [ ] OTG (no soportado por hardware)
> - [ ] exFAT/NTFS (excluidos en build slim)

## Special thanks to:

> [TeamWin](https://github.com/TeamWin) for the Recovery Project.
> 
> [OmniROM](https://github.com/omnirom) for the development base.
> 
> All testers and developers from the Huawei Y210 community.
