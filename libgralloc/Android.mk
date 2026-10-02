# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# gralloc del Y210: copia de hardware/msm7k/libgralloc-qsd8k (CM7 gingerbread,
# commit 5336b50), mantenida en el device tree en vez de parchear msm7k.
# Se instala como gralloc.y210: hw_get_module prueba ro.product.board (y210)
# antes que ro.board.platform (msm7k).

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils libGLESv1_CM

LOCAL_SRC_FILES := 	\
	allocator.cpp 	\
	framebuffer.cpp \
	gpu.cpp			\
	gralloc.cpp		\
	mapper.cpp		\
	pmemalloc.cpp

LOCAL_MODULE := gralloc.y210
LOCAL_CFLAGS:= -DLOG_TAG=\"$(TARGET_BOARD_PLATFORM).gralloc\"

ifeq ($(BOARD_USE_QCOM_PMEM),true)
  LOCAL_CFLAGS += -DUSE_QCOM_PMEM
endif

ifeq ($(BOARD_USE_FRAMEBUFFER_ALPHA_CHANNEL),true)
  LOCAL_CFLAGS += -DUSE_FRAMEBUFFER_ALPHA_CHANNEL
endif

include $(BUILD_SHARED_LIBRARY)
