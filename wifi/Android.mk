# ar6000.ko for the Y210's AR6003, built from source (Atheros AR6kSDK 3.1
# RC.734, wifi/AR6kSDK) against the kernel that build/core/tasks/kernel.mk
# compiles. BoardConfig.mk hooks Y210_AR6000_MODULE into that kernel build
# through TARGET_KERNEL_MODULES, so it runs after TARGET_KERNEL_BINARIES and
# before boot.img/system.img. Replaces the stock binary module (RC.776).

LOCAL_PATH := $(call my-dir)

Y210_AR6K_SDK := $(ANDROID_BUILD_TOP)/$(LOCAL_PATH)/AR6kSDK/host
Y210_AR6K_OUT := $(TARGET_OUT)/wifi/ar6000.ko

# Same configuration as build_ar6000.sh (SDIO on the Linux MMC stack,
# Android glue, P2P, raw HTC and the board-file DMA buffers).
Y210_AR6K_ARGS := \
    ATH_SRC_BASE=$(Y210_AR6K_SDK) \
    ATH_HIF_TYPE=sdio ATH_BUS_TYPE=sdio ATH_BUS_SUBTYPE=linux_sdio \
    ATH_OS_SUB_TYPE=linux_2_6 \
    ATH_ANDROID_ENV=yes ATH_BUILD_P2P=yes ATH_HTC_RAW_INT_ENV=yes \
    ATH_DEBUG_DRIVER=yes ATH_PREALLOC_IN_KERNEL=yes

.PHONY: Y210_AR6000_MODULE
Y210_AR6000_MODULE: TARGET_KERNEL_BINARIES
	$(MAKE) -C $(KERNEL_SRC) O=$(KERNEL_OUT) M=$(Y210_AR6K_SDK)/os/linux \
	    ARCH=$(TARGET_ARCH) $(ARM_CROSS_COMPILE) $(Y210_AR6K_ARGS) modules
	mkdir -p $(dir $(Y210_AR6K_OUT))
	$(ARM_EABI_TOOLCHAIN)/arm-eabi-strip --strip-debug \
	    -o $(Y210_AR6K_OUT) $(Y210_AR6K_SDK)/os/linux/ar6000.ko
