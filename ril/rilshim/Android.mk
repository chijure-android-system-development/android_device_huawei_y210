LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libril-y210-shim
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -DRIL_SHLIB
LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl
LOCAL_SRC_FILES := rilshim.c

include $(BUILD_SHARED_LIBRARY)
