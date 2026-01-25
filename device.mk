# device.mk para TWRP minimal en Huawei Y210-0151

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/recovery.fstab:recovery/root/etc/recovery.fstab \
    $(LOCAL_PATH)/ramdisk/recovery/twrp.fstab:recovery/root/etc/twrp.fstab \
    $(LOCAL_PATH)/ramdisk/recovery/sbin/charge:recovery/root/sbin/charge \
    $(LOCAL_PATH)/ramdisk/recovery/sbin/linker:recovery/root/sbin/linker \
    $(LOCAL_PATH)/ramdisk/recovery/sbin/ncm.sh:recovery/root/sbin/ncm.sh \
    $(call find-copy-subdir-files,*,$(LOCAL_PATH)/ramdisk/recovery/res,recovery/root/res)

PRODUCT_PACKAGES += adbd

PRODUCT_DEVICE := y210
PRODUCT_NAME := omni_y210
PRODUCT_BRAND := Huawei
PRODUCT_MODEL := Y210-0151
PRODUCT_MANUFACTURER := HUAWEI
