# Must come before the inherits: the first PRODUCT_COPY_FILES entry for a
# destination wins, and this replaces vendor/cm's apns-conf.xml, which has
# no Peru (MCC 716) entries.
PRODUCT_COPY_FILES := \
    device/huawei/y210/prebuilt/system/etc/apns-conf.xml:system/etc/apns-conf.xml

# languages_full.mk ships ~50 locales, bloating framework-res.apk and every
# app's resources.arsc — no room for that on this device's ~186 MiB /system
# partition. Set just the locales actually needed instead of inheriting it.
PRODUCT_LOCALES := en_US es_US

$(call inherit-product, vendor/cm/config/gsm.mk)

TARGET_BOOTANIMATION_NAME := vertical-320x480

$(call inherit-product, vendor/cm/config/common_mini_phone.mk)
$(call inherit-product, device/huawei/y210/device_y210.mk)

# Enable ADB + root by default (CM setting: apps+adb).
# Note: default.prop is fed by PRODUCT_DEFAULT_PROPERTY_OVERRIDES in this tree.
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += persist.service.adb.enable=1
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += persist.sys.root_access=3


PRODUCT_RELEASE_NAME := Y210
PRODUCT_DEVICE := y210
PRODUCT_NAME := cm_y210
PRODUCT_BRAND := Huawei
PRODUCT_MANUFACTURER := HUAWEI
PRODUCT_MODEL := HUAWEI Y210-0151

PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.build.product=msm7625a
