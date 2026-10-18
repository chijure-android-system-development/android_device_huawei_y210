#!/bin/bash
# apply-patches.sh — aplica todos los patches del Y210 sobre un árbol CM7 limpio.
# Ejecutar desde la raíz del árbol de CM7.
#
# Uso:
#   cd /path/to/cm7
#   bash device/huawei/y210/patches/apply-patches.sh

set -e

PATCHES_DIR="$(dirname "$0")"
CM7_ROOT="$(pwd)"
CHECK_ONLY=1

apply() {
    local project="$1"
    local patch="$2"
    if [ "$CHECK_ONLY" = "1" ]; then
        (cd "$CM7_ROOT/$project" && git apply --check "$CM7_ROOT/$PATCHES_DIR/$patch")
    else
        echo "  Aplicando $patch en $project..."
        (cd "$CM7_ROOT/$project" && git apply "$CM7_ROOT/$PATCHES_DIR/$patch")
    fi
}

copy_new() {
    local src="$1"
    local dst="$2"
    if [ "$CHECK_ONLY" = "1" ]; then
        [ -f "$CM7_ROOT/$PATCHES_DIR/$src" ]
    else
        echo "  Copiando archivo nuevo: $dst"
        cp "$CM7_ROOT/$PATCHES_DIR/$src" "$CM7_ROOT/$dst"
    fi
}

run_all() {

# hardware/ril — libril: solo el stub RIL_setRilSocketName. El blob lo enlaza
#                y el linker de GB no lo busca en el shim (dlopen). El resto
#                del desfase v6 lo resuelve ril/rilshim.
apply hardware/ril                    hardware_ril.patch

# hardware/libhardware_legacy — wifi.c: soporte ath6k/AR6003 completo
apply hardware/libhardware_legacy     hardware_libhardware_legacy.patch

# system/netd — SoftAP ATH: declarar RSN pairwise para WPA2/CCMP
apply system/netd                     system_netd.patch

# frameworks/base — cámara: CameraParameters (HFR/denoise/redeye/YV12),
#                   StagefrightRecorder, CameraService (pmem_adsp restart),
#                   TextureManager (conversión NV21->RGB565 por software)
apply frameworks/base                 frameworks_base_camera.patch

# frameworks/base — Zygote: GID 3006 (AID_QCOM_ONCRPC) para system_server.
#                   Lo de DataConnection/RIL.java/GsmDataConnectionTracker lo
#                   resuelve ril/rilshim.
apply frameworks/base                 frameworks_base_telephony.patch

# frameworks/base — PhoneFactory: rama faltante para instanciar HuaweiQualcommRIL
#                   (ya vive en device/huawei/y210/ril/, inyectado via
#                   FRAMEWORKS_BASE_SUBDIRS en device_y210.mk, pero nunca se
#                   seleccionaba). Ver device/huawei/y210/docs/RIL_NOTES.md.
apply frameworks/base                 frameworks_base_ril_class.patch

# frameworks/base — FM: hookup JNI (Android.mk, BOARD_FM_DEVICE=qcom) +
#                   implementación nativa + stack Java (FmRxControls, etc.)
apply frameworks/base                 frameworks_base_fm.patch
copy_new frameworks_base_android_hardware_fm_qcom.cpp \
         frameworks/base/core/jni/android_hardware_fm_qcom.cpp
apply frameworks/base                 frameworks_base_fm_java.patch

# frameworks/base — status bar: dedup de setIcon + workaround de ghosting
#                   (copy-back + zero back buffer) en superficies <=26px alto
apply frameworks/base                 frameworks_base_statusbar.patch

# frameworks/base — panel expandido: title_bar_portrait.9.png tenía la región
#                   de estiramiento horizontal degenerada (1px), dejaba una
#                   franja gris fija a la izquierda de power_and_carrier;
#                   status_bar_expanded.xml sin paddingLeft explícito heredaba
#                   el padding del mismo nine-patch; StatusBarView.onLayout()
#                   caía a ancho completo (tapando iconos) cuando la fecha no
#                   podía "encajar" contra ningún ícono. Análisis completo con
#                   capturas reales del equipo, sesión 2026-07-13.
#                   El .9.png y el layout viven en
#                   overlay/frameworks/base/packages/SystemUI; el parche
#                   solo lleva la parte Java.
apply frameworks/base                 frameworks_base_statusbar_expanded.patch

# title_bar_medium/portrait/shadow/tall: mismo defecto de nine-patch que
# arriba, en core/res y en las copias de PackageInstaller, Tag y Contacts.
# Ya no se parchea: los .9.png corregidos viven en overlay/ (sesión
# 2026-07-13, movidos a overlay 2026-10-18).

# frameworks/base — ThumbnailUtils.createImageThumbnail(): el FileInputStream
#                   quedaba sin referencia; un GC entre el decode de bounds y
#                   el decode real lo finalizaba y cerraba el fd → skia
#                   "decoder->decode returned false" + "Can't create mini
#                   thumbnail" en ~1 de cada 2 PNG (alternado en la DB de
#                   medios). Además nunca se cerraba (leak de fd). Mismo fix
#                   que AOSP ICS. Sesión 2026-10-02.
apply frameworks/base                 frameworks_base_thumbnailutils_fd.patch

# frameworks/base — StagefrightMediaScanner: ".m4v" no estaba en la lista de
#                   extensiones aceptadas → los videos de la cámara (VideoCamera
#                   guarda MPEG_4 como .m4v) quedaban con duration=0 y sin
#                   metadata en la DB de medios. MPEG4Extractor detecta el
#                   contenedor por sniff, no por extensión. Sesión 2026-10-02.
apply frameworks/base                 frameworks_base_scanner_m4v.patch

# packages/apps/Camera — adaptaciones Y210 al lifecycle de cámara
apply packages/apps/Camera            packages_apps_Camera.patch

# packages/apps/FM — app FM radio: FMRadio, FMRadioService, FmSharedPreferences
#                    (config.xml: overlay/packages/apps/FM)
apply packages/apps/FM                packages_apps_FM.patch

# hardware/qcom/gps — NMEA AMSS 50000: el XDR escribe 1200 bytes y el
#                     struct tenía 200, así que cada reporte pisaba el evento.
apply hardware/qcom/gps               hardware_qcom_gps.patch

# packages/apps/Settings — ya no se parchea: el fallback a gsm.operator.numeric
# tapaba la SIM en UNKNOWN, resuelto por ril/rilshim (ver docs/RIL_NOTES.md).

# vendor/cyanogen — APN Claro Perú (recortado: se saco un hunk que borraba
# ~60 entradas de products/AndroidProducts.mk y rompia breakfast/lunch de
# otros dispositivos del arbol sin dar ningun beneficio a y210)
apply vendor/cyanogen                 vendor_cyanogen.patch
copy_new vendor_cyanogen_y210.mk      vendor/cyanogen/products/cyanogen_y210.mk
}

echo "=== Verificando que todos los patches aplican limpio (sin tocar el árbol) ==="
run_all
echo "=== Verificación OK, aplicando ==="
CHECK_ONLY=0
run_all
echo "=== Todos los patches aplicados ==="
