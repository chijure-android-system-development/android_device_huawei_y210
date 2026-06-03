LOCAL_PATH := $(call my-dir)

# Y210CameraWrapper needs ICS camera API porting.
# The vendor blob (libcamera.y210.so) handles camera instead.
# Disabled: ifeq never matches so the module is never built.
ifeq ($(TARGET_BOARD_PLATFORM),DISABLED_UNTIL_PORTED)

include $(CLEAR_VARS)

LOCAL_MODULE := libcamera
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libcamera_client \
    libui \
    libbinder \
    libdl \
    libsurfaceflinger_client

LOCAL_C_INCLUDES += \
    frameworks/base/services/camera/libcameraservice

LOCAL_SRC_FILES := Y210CameraWrapper.cpp

include $(BUILD_SHARED_LIBRARY)

endif # DISABLED_UNTIL_PORTED
