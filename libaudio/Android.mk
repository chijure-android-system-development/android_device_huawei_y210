LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=               \
    AudioPolicyManager.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libmedia

LOCAL_STATIC_LIBRARIES := libmedia_helper

LOCAL_WHOLE_STATIC_LIBRARIES := libaudiopolicy_legacy

LOCAL_C_INCLUDES += \
    hardware/libhardware_legacy/include

LOCAL_MODULE:= libaudiopolicy

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE := libaudio

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libmedia \
    libhardware_legacy

LOCAL_C_INCLUDES += \
    device/huawei/y210/include

ifeq ($TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl
endif

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

LOCAL_SRC_FILES += AudioHardware.cpp

LOCAL_CFLAGS += -fno-short-enums

LOCAL_STATIC_LIBRARIES += libaudiointerface libmedia_helper

include $(BUILD_SHARED_LIBRARY)

# ICS audio HAL wrapper: wraps libaudio (legacy AudioHardwareInterface)
# so AudioFlinger can load it as audio.primary.y210.so.
# audio_hw_hal.cpp calls createAudioHardware() which libaudio.so provides.
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    ../../../../hardware/libhardware_legacy/audio/audio_hw_hal.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libmedia \
    liblog \
    libaudio

LOCAL_C_INCLUDES += \
    hardware/libhardware_legacy/include \
    hardware/libhardware_legacy/include/hardware_legacy \
    hardware/libhardware/include

LOCAL_MODULE := audio.primary.y210
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

include $(BUILD_SHARED_LIBRARY)
