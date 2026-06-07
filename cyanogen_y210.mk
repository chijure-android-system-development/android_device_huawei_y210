# Inherit AOSP device configuration for Y210.
$(call inherit-product, device/huawei/y210/device_y210.mk)

# Inherit some common cyanogenmod stuff.
$(call inherit-product, vendor/cyanogen/products/common_full.mk)

# Trim optional CM apps to keep system.img within the Y210 partition budget.
PRODUCT_PACKAGES := $(filter-out CMWallpapers VideoEditor,$(PRODUCT_PACKAGES))

# Include GSM stuff (disabled for now, no RIL)
# $(call inherit-product, vendor/cyanogen/products/gsm.mk)

# Broadcom FM radio
$(call inherit-product, vendor/cyanogen/products/bcm_fm_radio.mk)

#
# Setup device specific product configuration.
#
PRODUCT_NAME := cyanogen_y210
PRODUCT_BRAND := Huawei
PRODUCT_DEVICE := y210
PRODUCT_MODEL := HUAWEI Y210-0151
PRODUCT_MANUFACTURER := HUAWEI
# Camera blob (libcamera.y210.so) reads ro.build.product to detect the sensor
# target type. It only recognizes msm7625a/msm7627a/etc., not "y210".
# PRODUCT_DEFAULT_PROPERTY_OVERRIDES → /default.prop, loaded before build.prop,
# so this wins over the buildinfo.sh-generated ro.build.product=y210.
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.build.product=msm7625a

# RIL / Telephony
PRODUCT_PROPERTY_OVERRIDES += \
    ro.telephony.default_network=0 \
    ro.telephony.ril_class=HuaweiQualcommRIL \
    ril.subscription.types=NV,RUIM
# PRODUCT_BUILD_PROP_OVERRIDES += PRODUCT_NAME=y210 BUILD_ID=GRK39F BUILD_DISPLAY_ID=GWK74 BUILD_FINGERPRINT=Huawei/Y210/hwy210-0151:2.3.6/HuaweiY210-0151/C40B855:user/ota-rel-keys,release-keys PRIVATE_BUILD_DESC="passion-user 2.3.6 GRK39F 189904 release-keys"

# Release name and versioning
PRODUCT_RELEASE_NAME := Y210
PRODUCT_VERSION_DEVICE_SPECIFIC :=
-include vendor/cyanogen/products/common_versions.mk

PRODUCT_COPY_FILES +=  \
     vendor/cyanogen/prebuilt/mdpi/media/bootanimation.zip:system/media/bootanimation.zip
# Y210 system partition (186MB) is too small for all CM default apps.
# Remove heavy optional packages last so all inherit-product calls are done.
PRODUCT_PACKAGES := $(filter-out CMWallpapers VideoEditor LiveWallpapers     LiveWallpapersPicker MagicSmokeWallpapers Galaxy4 HoloSpiralWallpaper     NoiseField PhaseBeam VisualizationWallpapers,$(PRODUCT_PACKAGES))
