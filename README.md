# TWRP Device configuration for Huawei Ascend Y210

## Device specifications

Basic    | Spec Sheet
--------:|:----------------------
Chipset  | Qualcomm Snapdragon MSM7x27A (Snapdragon S1)
CPU      | 1.0 GHz Cortex-A5 (Single-core)
GPU      | Adreno 200
Memory   | 256 MB RAM
Storage  | 512 MB NAND (yaffs2), microSD up to 32 GB
Supported Models | Huawei Y210-0151
Shipped Android version | Android 2.3.6 (Gingerbread)
Codename | y210
Battery  | Li-Ion 1750 mAh, removable
Display  | 3.5" TFT, 320×480 px (HVGA), 160 dpi
Framebuffer | 32bpp RGBX_8888, triple-buffered
Rear Camera  | 3.15 MP
Front Camera | No
Release Date | 2013

## How to Compile

```bash
source build/envsetup.sh
lunch omni_y210-eng
make recoveryimage -j$(nproc)
```

The build produces `out/target/product/y210/recovery.img`.

## How to Install

This device uses **NAND flash** — there is no fastboot partition flashing.

```bash
# Flash via ADB from a running recovery:
adb push recovery.img /tmp/
adb shell "flash_image recovery /tmp/recovery.img"

# Or from the bootloader (hold Vol+ + Power at boot):
# Use the built-in update menu if available.
```

## Custom Theme

The `theme/` directory contains a hand-tuned 320×480 HVGA theme:

- **`ui.xml`** — layout variables calibrated for 320×480 (tabs, dialogs, navbar)
- **`splash.xml`** — splash screen
- **`images/`** — all button, tab, slider, and animation assets resized for HVGA

Key layout values (`ui.xml`):

| Variable | Value | Notes |
|---|---|---|
| `tab_height` | 31 | Proportional to original 52px @ mdpi |
| `dialog_button_x` | 107 | Centers OK button in 300px dialog |
| `dialog_height` | 175 | 3px bottom margin for 31px button |
| `navbar_y` | 448 | Bottom navbar, 32px tall |
| `partitionlist_storage_height` | 133 | Storage selector list |

## BoardConfig notes

| Variable | Value | Reason |
|---|---|---|
| `TARGET_RECOVERY_PIXEL_FORMAT` | `RGBX_8888` | Actual framebuffer format (not RGB_565) |
| `BOARD_FLASH_BLOCK_SIZE` | `262144` | MTD erasesize = 0x40000 |
| `BOARD_SYSTEMIMAGE_PARTITION_SIZE` | `195035136` | Actual mtd4 size from `/proc/mtd` |
| `TW_CUSTOM_THEME` | `device/huawei/y210/theme` | Custom HVGA theme |
| `TW_ADDITIONAL_RES` | `bootable/recovery/gui/theme/common/portrait.xml` | Common UI pages |

## Known issues / limitations

- **No fastboot**: NAND storage — `TW_NO_REBOOT_BOOTLOADER := true`
- **No /misc NAND support**: `E:Only emmc /misc is supported` is a known TWRP limitation on MTD; does not affect functionality
- **No SELinux in recovery kernel**: `tw_get_context failed` at boot is expected
- **No MTP, no exFAT, no crypto**: hardware/kernel limitations

## Working features

- [x] Mount /system, /data, /cache, /cust (yaffs2)
- [x] Mount /external_sd (vfat)
- [x] Backup / restore partitions
- [x] Install zips from sdcard or sideload
- [x] Wipe cache / data / dalvik
- [x] ADB (forced on in recovery)
- [x] Lock screen with swipe-to-unlock slider
- [x] Custom 320×480 HVGA theme
- [ ] MTP (not supported)
- [ ] Decrypt /data (N/A on Android 2.3)
- [ ] OTG (not supported by hardware)
- [ ] exFAT / NTFS (excluded)

## Credits

[TeamWin](https://github.com/TeamWin) — TWRP / TeamWin Recovery Project  
[OmniROM](https://github.com/omnirom) — build system base
