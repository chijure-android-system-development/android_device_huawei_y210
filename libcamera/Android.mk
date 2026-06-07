LOCAL_PATH := $(call my-dir)

# libcamera_compat.so — GB→ICS symbol stubs, loaded RTLD_GLOBAL before the blob
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS        := optional
LOCAL_MODULE             := libcamera_compat
LOCAL_SRC_FILES          := libcamera_compat.cpp
LOCAL_SHARED_LIBRARIES   := liblog
include $(BUILD_SHARED_LIBRARY)

# camera.y210.so — ICS HAL wrapper
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS    := optional
LOCAL_MODULE_PATH    := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE         := camera.$(TARGET_BOOTLOADER_BOARD_NAME)
LOCAL_SRC_FILES      := Y210CameraWrapper.cpp camera_compat.cpp camera_compat_asm.S

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libdl \
    libutils \
    libcutils \
    libhardware \
    libbinder \
    libcamera_client \
    libui \
    libsurfaceflinger_client \
    libcamera_compat

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    frameworks/base/services/ \
    frameworks/base/include \
    hardware/libhardware/include \
    hardware/libhardware/modules/gralloc

include $(BUILD_SHARED_LIBRARY)
