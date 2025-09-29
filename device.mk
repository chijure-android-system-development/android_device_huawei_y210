# device.mk para TWRP minimal en Huawei Y210-0151

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/recovery.fstab:recovery/root/etc/recovery.fstab

PRODUCT_DEVICE := y210
PRODUCT_NAME := omni_y210
PRODUCT_BRAND := Huawei
PRODUCT_MODEL := Y210-0151
PRODUCT_MANUFACTURER := HUAWEI
