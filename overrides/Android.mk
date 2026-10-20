# Y210Overrides: empty package (manifest only, no code, no launcher entry)
# whose only job is LOCAL_OVERRIDES_PACKAGES. vendor/cm/config/common.mk adds
# CM's wallpaper/live-wallpaper packages and VideoEditor, which do not fit in
# the Y210 system partition (system.img ~179MB of 186MB). On CM10 it also
# drops Exchange2 (build/target generic_no_telephony.mk) and the T-Mobile
# theme engine (vendor/cm themes_common.mk). build/core/main.mk
# drops overridden packages from the install, so this replaces the old
# build/core/product_config.mk filter patch.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PACKAGE_NAME := Y210Overrides
LOCAL_MODULE_TAGS := optional
LOCAL_CERTIFICATE := platform
LOCAL_SDK_VERSION := current
LOCAL_OVERRIDES_PACKAGES := \
    CMWallpapers \
    VideoEditor \
    LiveWallpapers \
    LiveWallpapersPicker \
    MagicSmokeWallpapers \
    Galaxy4 \
    HoloSpiralWallpaper \
    NoiseField \
    PhaseBeam \
    VisualizationWallpapers \
    Exchange2 \
    ThemeManager \
    ThemeChooser \
    CMFileManagerThemes

include $(BUILD_PACKAGE)
