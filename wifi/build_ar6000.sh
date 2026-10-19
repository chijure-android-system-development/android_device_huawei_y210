#!/bin/bash
# Build ar6000.ko (Atheros AR6kSDK 3.1 RC.734) against the kernel that the
# Android build already compiled in out/.../KERNEL_OBJ.
#
# Run from the top of the CM9 tree (inside the build container):
#   device/huawei/y210/wifi/build_ar6000.sh [out.ko]

set -e

TOP="$(pwd)"
SDK="$TOP/device/huawei/y210/wifi/AR6kSDK/host"
KOBJ="$TOP/out/target/product/y210/obj/KERNEL_OBJ"
CROSS="$TOP/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi-"
OUT="${1:-$TOP/out/target/product/y210/ar6000.ko}"

make -C "$TOP/kernel/huawei/y210" O="$KOBJ" M="$SDK/os/linux" \
    ARCH=arm CROSS_COMPILE="$CROSS" \
    ATH_SRC_BASE="$SDK" \
    ATH_HIF_TYPE=sdio ATH_BUS_TYPE=sdio ATH_BUS_SUBTYPE=linux_sdio \
    ATH_OS_SUB_TYPE=linux_2_6 \
    ATH_ANDROID_ENV=yes ATH_BUILD_P2P=yes ATH_HTC_RAW_INT_ENV=yes \
    ATH_DEBUG_DRIVER=yes ATH_PREALLOC_IN_KERNEL=yes \
    modules

"${CROSS}strip" --strip-debug -o "$OUT" "$SDK/os/linux/ar6000.ko"
echo "ar6000.ko -> $OUT"
