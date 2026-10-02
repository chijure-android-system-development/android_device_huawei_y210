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

# copybit del Y210: copia de hardware/msm7k/libcopybit (CM7 gingerbread,
# commit 5336b50 + el antiguo patches/hardware_msm7k.patch), mantenida en el
# device tree. Se instala como copybit.y210 (ro.product.board).

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_SRC_FILES := copybit.cpp
LOCAL_MODULE := copybit.y210
LOCAL_C_INCLUDES += hardware/msm7k/libgralloc
LOCAL_CFLAGS += -DCOPYBIT_MSM7K=1
include $(BUILD_SHARED_LIBRARY)
