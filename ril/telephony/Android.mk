LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# RIL class for ro.telephony.ril_class; loaded by PhoneFactory from the
# BOOTCLASSPATH (see init.rc), so it shares the boot class loader and the
# com.android.internal.telephony runtime package with framework.jar.
LOCAL_MODULE := y210-telephony
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_JAVA_LIBRARIES := framework

include $(BUILD_JAVA_LIBRARY)
