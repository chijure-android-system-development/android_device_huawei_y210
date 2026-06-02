LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE           := libos_compat
LOCAL_MODULE_TAGS      := optional
LOCAL_SRC_FILES        := os_compat.c
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_C_INCLUDES       := $(LOCAL_PATH)

include $(BUILD_SHARED_LIBRARY)
