# libwpa_client y wpa_cli del Y210.
#
# El supplicant del Y210 es el ath_supplicant stock (prebuilt), así que de
# external/wpa_supplicant_6 solo se usaban libwpa_client y wpa_cli. BoardConfig
# pone WPA_SUPPLICANT_VERSION := VER_0_6_X_Y210 para que no se compile ninguno
# de los dos árboles de external/ y se usen estos, con el resto de las fuentes
# tomadas de external/wpa_supplicant_6 sin cambios.
#
# wpa_ctrl.c es el de external/wpa_supplicant_6 con un solo cambio: el hack de
# CAF que devuelve -2 sin enviar nada si init.svc.wpa_supplicant=stopped no se
# aplica a sockets de hostapd. En hotspot el supplicant está parado a
# propósito y netd habla con hostapd por esta librería ("Softap startap -
# connect to hostapd fails").

LOCAL_PATH := $(call my-dir)
WPA_SRC := external/wpa_supplicant_6/wpa_supplicant
WPA_INCLUDES := $(WPA_SRC) $(WPA_SRC)/src $(WPA_SRC)/src/common $(WPA_SRC)/src/utils

include $(CLEAR_VARS)
LOCAL_MODULE := libwpa_client
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -mabi=aapcs-linux -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX
LOCAL_SRC_FILES := wpa_ctrl.c ../../../../$(WPA_SRC)/src/utils/os_unix.c
LOCAL_C_INCLUDES := $(WPA_INCLUDES)
LOCAL_SHARED_LIBRARIES := libcutils
LOCAL_COPY_HEADERS_TO := libwpa_client
LOCAL_COPY_HEADERS := ../../../../$(WPA_SRC)/src/common/wpa_ctrl.h
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := wpa_cli
LOCAL_MODULE_TAGS := debug
LOCAL_CFLAGS := -mabi=aapcs-linux -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX
LOCAL_SRC_FILES := ../../../../$(WPA_SRC)/wpa_cli.c wpa_ctrl.c \
    ../../../../$(WPA_SRC)/src/utils/os_unix.c
LOCAL_C_INCLUDES := $(WPA_INCLUDES)
LOCAL_SHARED_LIBRARIES := libc libcutils
include $(BUILD_EXECUTABLE)
