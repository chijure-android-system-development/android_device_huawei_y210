LOCAL_PATH := $(call my-dir)

# ── libos_compat: Qualcomm OSAL shim for Adreno200 blobs ──────────────────
include $(CLEAR_VARS)
LOCAL_MODULE           := libos_compat
LOCAL_MODULE_TAGS      := optional
LOCAL_SRC_FILES        := os_compat.c
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_C_INCLUDES       := $(LOCAL_PATH)
include $(BUILD_SHARED_LIBRARY)

# ── libgsl.so wrapper ──────────────────────────────────────────────────────
# Replaces the vendor libgsl.so blob with a shim that first loads
# libos_compat (OSAL stubs) and libgsl_real (the real blob, renamed).
# Any process that loads libgsl gets the OSAL symbols for free.
include $(CLEAR_VARS)
LOCAL_MODULE           := libgsl
LOCAL_MODULE_TAGS      := optional
LOCAL_SRC_FILES        := libgsl_wrapper.c
LOCAL_SHARED_LIBRARIES := libos_compat libgsl_real
include $(BUILD_SHARED_LIBRARY)
