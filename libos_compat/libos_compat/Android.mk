LOCAL_PATH := $(call my-dir)

# Qualcomm OSAL compat shim for Adreno200 blobs.
# Added as a dependency of libEGL.so so any process that uses EGL
# gets these symbols before libEGL_adreno200.so is loaded.
include $(CLEAR_VARS)
LOCAL_MODULE           := libos_compat
LOCAL_MODULE_TAGS      := optional
LOCAL_SRC_FILES        := os_compat.c
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_C_INCLUDES       := $(LOCAL_PATH)
include $(BUILD_SHARED_LIBRARY)
