#!/bin/bash
# Build ar6000.ko (Atheros AR6kSDK 3.1 RC.734, wifi/AR6kSDK) from source
# against the CM7 kernel and install it as the prebuilt the ROM ships.
#
# CM7 uses a prebuilt zImage (TARGET_PREBUILT_KERNEL = device/huawei/y210/kernel)
# built from github.com/chijure/android_kernel_huawei_y210, so the module is
# built against that same kernel tree and its O= output, exactly like the
# kernel itself (see docs/KERNEL_NOTES.md). Run on the host:
#
#   device/huawei/y210/wifi/build_ar6000.sh
#
# Override KSRC/KOBJ/CROSS_COMPILE if the kernel lives elsewhere.

set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
SDK="$HERE/AR6kSDK/host"
KSRC="${KSRC:-$HOME/android_kernel_huawei_y210}"
KOBJ="${KOBJ:-$HOME/kernel-out-y210}"
CROSS="${CROSS_COMPILE:-$HOME/arm-eabi-4.4.3/bin/arm-eabi-}"
OUT="${1:-$HERE/../prebuilt/system/wifi/ar6000.ko}"

if [ ! -f "$KOBJ/.config" ]; then
    echo "E: $KOBJ has no .config; build the kernel first (docs/KERNEL_NOTES.md)" >&2
    exit 1
fi

make -C "$KSRC" O="$KOBJ" M="$SDK/os/linux" \
    ARCH=arm CROSS_COMPILE="$CROSS" \
    ATH_SRC_BASE="$SDK" \
    ATH_HIF_TYPE=sdio ATH_BUS_TYPE=sdio ATH_BUS_SUBTYPE=linux_sdio \
    ATH_OS_SUB_TYPE=linux_2_6 \
    ATH_ANDROID_ENV=yes ATH_BUILD_P2P=yes ATH_HTC_RAW_INT_ENV=yes \
    ATH_DEBUG_DRIVER=yes ATH_PREALLOC_IN_KERNEL=yes \
    modules

"${CROSS}strip" --strip-debug -o "$OUT" "$SDK/os/linux/ar6000.ko"
echo "ar6000.ko -> $OUT"
echo "kernel: $(git -C "$KSRC" log -1 --format=%h 2>/dev/null) $(md5sum "$KOBJ/arch/arm/boot/zImage" | cut -d' ' -f1)"
