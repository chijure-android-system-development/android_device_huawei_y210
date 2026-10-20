#Copyright (c) 2009, QUALCOMM USA, INC.
#All rights reserved.
#Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
#·         Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
#·         Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution. 
#·         Neither the name of the QUALCOMM USA, Inc.  nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission. 
#THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Y210: copia de hardware/qcom/gps (CM7 c05e328) para AMSS 50000; el de
# hardware/qcom/gps queda fuera porque BoardConfig no define
# BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE.
Y210_GPS_AMSS_VERSION := 50000

ifeq (true,true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := gps.y210

LOCAL_STATIC_LIBRARIES:= \
    libloc_api-rpc 

LOCAL_SHARED_LIBRARIES := \
    librpc \
    libutils \
    libcutils

LOCAL_SRC_FILES += \
    loc_eng.cpp \
    loc_eng_ioctl.cpp \
    loc_eng_xtra.cpp \
    loc_eng_ni.cpp \
    loc_eng_log.cpp \
    loc_eng_cfg.cpp \
    gps.c

LOCAL_CFLAGS += \
    -fno-short-enums \
    -DAMSS_VERSION=$(Y210_GPS_AMSS_VERSION)

ifeq ($(Y210_GPS_AMSS_VERSION),50000)
    LOCAL_CFLAGS += -DLIBLOC_USE_GPS_PRIVACY_LOCK=1
endif
ifeq ($(Y210_GPS_AMSS_VERSION),20000)
    LOCAL_CFLAGS += -DLIBLOC_USE_GPS_PRIVACY_LOCK=1
endif
ifeq ($(Y210_GPS_AMSS_VERSION),1240)
    LOCAL_CFLAGS += -DLIBLOC_USE_GPS_PRIVACY_LOCK=1
    LOCAL_CFLAGS += -DLIBLOC_USE_DEFAULT_RESPONSE_TIME_AND_ACCURACY=1
endif

LOCAL_C_INCLUDES:= \
	$(TARGET_OUT_HEADERS)/libloc_api-rpc \
	$(TARGET_OUT_HEADERS)/libloc_api-rpc/inc \
	$(TARGET_OUT_HEADERS)/libcommondefs-rpc/inc \
	$(TARGET_OUT_HEADERS)/librpc

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
# JB libhardware writes hmi->dso into HAL_MODULE_INFO_SYM; under RELRO that
# page is read-only and system_server SIGSEGVs (SEGV_ACCERR) loading gps.y210.
LOCAL_LDFLAGS += -Wl,-z,norelro

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
endif
