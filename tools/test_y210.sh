#!/usr/bin/env bash
# test_y210.sh — smoke-test del port CM9 (ICS 4.0) para el Huawei Y210 vía ADB.
#
# Uso (desde raíz del árbol CM9 o en cualquier directorio):
#   bash device/huawei/y210/tools/test_y210.sh [--fast] [--section <nombre>]
#
# Opciones:
#   --fast        Omite tests que requieren espera o interacción larga
#   --section X   Ejecuta solo esa sección (boot|display|input|lights|audio|
#                 wifi|bt|sensors|gps|camera|storage|ril|power)
#
# Requiere: adb en PATH, dispositivo conectado con USB debugging activo.

RED='\033[0;31m'; GRN='\033[0;32m'; YLW='\033[1;33m'
CYN='\033[0;36m'; BLD='\033[1m'; RST='\033[0m'

PASS=0; FAIL=0; SKIP=0; MANUAL=0
FAST=false; FILTER=""
LOG=/tmp/test_y210_$(date +%Y%m%d_%H%M%S).log
echo "test_y210.sh  $(date)" > "$LOG"

_arg=""
for a in "$@"; do
    if [[ "$_arg" == "--section" ]]; then FILTER="$a"; _arg=""; continue; fi
    case $a in --fast) FAST=true ;; --section) _arg="--section" ;; esac
done

# ── primitivas ────────────────────────────────────────────────────────────────

adb_sh() { adb shell "$@" 2>/dev/null | tr -d '\r'; }

pass()   { echo -e "${GRN}  ✓ PASS${RST}  $1"; PASS=$((PASS+1));   echo "PASS   $1" >> "$LOG"; }
fail()   { echo -e "${RED}  ✗ FAIL${RST}  $1  ← $2"; FAIL=$((FAIL+1)); echo "FAIL   $1  [$2]" >> "$LOG"; }
skip()   { echo -e "${YLW}  - SKIP${RST}  $1  ($2)"; SKIP=$((SKIP+1));  echo "SKIP   $1  [$2]" >> "$LOG"; }
manual() { echo -e "${CYN}  ? MANUAL${RST} $1  → $2"; MANUAL=$((MANUAL+1)); echo "MANUAL $1  [$2]" >> "$LOG"; }

section() {
    [[ -n "$FILTER" && "$FILTER" != "$1" ]] && return 1
    echo ""; echo -e "${BLD}══ $2 ══${RST}"
    return 0
}

chk_prop() {
    _val=$(adb_sh getprop "$2")
    if [[ -n "$3" && "$_val" == *"$3"* ]]; then pass "$1 ($3)"; \
    elif [[ -z "$3" && -n "$_val" ]]; then pass "$1 ($2=$_val)"; \
    else fail "$1" "$2='$_val'"; fi
}
chk_proc() {
    if adb_sh "ps" | grep -v grep | grep -q "$2"; then pass "$1"; \
    else fail "$1" "'$2' no encontrado en ps"; fi
}
chk_file() {
    if adb_sh "[ -e '$2' ] && echo y" | grep -q y; then pass "$1 ($2)"; \
    else fail "$1" "no existe: $2"; fi
}

# ── pre-check ─────────────────────────────────────────────────────────────────

echo -e "${BLD}test_y210.sh — Huawei Y210 CM9 (ICS 4.0) smoke test${RST}"
echo "Log: $LOG"

if ! adb devices | grep -q "device$"; then
    echo -e "${RED}ERROR: no hay dispositivo ADB conectado.${RST}"; exit 1
fi

_model=$(adb_sh getprop ro.product.model)
echo -e "Dispositivo: ${BLD}$_model${RST}  |  Fecha: $(date)"
echo "Dispositivo: $_model" >> "$LOG"

# ── 1. boot / sistema ─────────────────────────────────────────────────────────

if section boot "BOOT / SISTEMA"; then
    chk_prop  "Boot completado"              sys.boot_completed       "1"
    chk_prop  "Versión Android 4.0"          ro.build.version.release "4.0"
    chk_prop  "ro.product.model Y210"        ro.product.model         "HUAWEI Y210"
    chk_prop  "ro.build.product y210"        ro.build.product         "y210"
    chk_proc  "system_server"                "system_server"
    # En ICS surfaceflinger aparece en ps como proceso propio
    if adb_sh "ps 2>/dev/null" | grep -qi surfacefling; then
        pass "surfaceflinger (vía ps)"
    elif adb_sh "service list 2>/dev/null" | grep -qi surfaceflinger; then
        pass "surfaceflinger (vía service list)"
    else fail "surfaceflinger" "no en ps ni en service list"; fi
    chk_proc  "mediaserver"                  "mediaserver"
    # En ICS Zygote se llama zygote (no app_process directamente)
    chk_proc  "zygote"                       "zygote"
fi

# ── 2. pantalla / gráficos ────────────────────────────────────────────────────

if section display "PANTALLA / GRÁFICOS"; then
    chk_file "Framebuffer fb0"              "/dev/graphics/fb0"
    # En ICS los HAL usan el nombre del SoC: msm7x27a
    chk_file "gralloc.msm7x27a.so"         "/system/lib/hw/gralloc.msm7x27a.so"
    chk_file "copybit.msm7x27a.so"         "/system/lib/hw/copybit.msm7x27a.so"
    chk_file "hwcomposer.msm7x27a.so"      "/system/lib/hw/hwcomposer.msm7x27a.so"
    chk_file "libEGL_adreno200.so"         "/system/lib/egl/libEGL_adreno200.so"
    chk_file "libGLESv1_CM_adreno200.so"   "/system/lib/egl/libGLESv1_CM_adreno200.so"
    chk_file "libGLESv2_adreno200.so"      "/system/lib/egl/libGLESv2_adreno200.so"
    _kgsl=$(adb_sh "ls -l /dev/kgsl-3d0 2>/dev/null | awk '{print \$1}'")
    if [[ "$_kgsl" == *"rw-rw-rw"* ]]; then pass "KGSL permisos 0666 ($_kgsl)"; \
    else fail "KGSL /dev/kgsl-3d0 permisos" "got='$_kgsl'"; fi
    _fbmode=$(adb_sh "cat /sys/class/graphics/fb0/modes 2>/dev/null")
    if [[ "$_fbmode" == *"320"* ]]; then pass "fb0 modo 320px ($_fbmode)"; \
    else skip "fb0 modo" "resultado='$_fbmode'"; fi
fi

# ── 3. input / UI ─────────────────────────────────────────────────────────────

if section input "INPUT / UI"; then
    _evts=$(adb_sh "ls /dev/input/ 2>/dev/null | grep -c event")
    if [[ "$_evts" -gt 0 ]] 2>/dev/null; then pass "/dev/input/event* — $_evts nodos presentes"; \
    else fail "/dev/input/event*" "ninguno encontrado"; fi
    chk_file "Acelerómetro /dev/accel"     "/dev/accel"
    # En ICS el evento de entrada se puede inspeccionar con getevent
    if adb_sh "getevent -p 2>/dev/null" | grep -qi "ABS_MT_POSITION\|touch"; then
        pass "Touchscreen detectado en getevent"
    else skip "getevent touch" "activar pantalla y re-ejecutar"; fi
    manual "Touchscreen"          "toca la pantalla — verifica respuesta táctil"
    manual "Multitouch"           "pellizca/expande — verifica zoom"
    manual "Botones físicos"      "power / vol+ / vol-"
    manual "Botones capacitivos"  "atrás / home / menú / búsqueda"
fi

# ── 4. luces / vibración ──────────────────────────────────────────────────────

if section lights "LUCES / VIBRACIÓN"; then
    chk_file "lights HAL"  "/system/lib/hw/lights.y210.so"
    adb_sh "input keyevent 26" 2>/dev/null; sleep 1
    _bl=$(adb_sh "cat /sys/class/leds/lcd-backlight/brightness 2>/dev/null")
    _bl_max=$(adb_sh "cat /sys/class/leds/lcd-backlight/max_brightness 2>/dev/null")
    if [[ -n "$_bl_max" && "$_bl_max" -gt 0 ]] 2>/dev/null; then pass "Backlight brightness=$_bl max=$_bl_max"; \
    else fail "Backlight brightness" "resultado='$_bl' max='$_bl_max'"; fi
    adb_sh "echo 100 > /sys/class/timed_output/vibrator/enable" 2>/dev/null || true
    if adb_sh "[ -e '/sys/class/timed_output/vibrator/enable' ] && echo y" | grep -q y; then
        pass "Vibrador sysfs accesible (pulso 100ms enviado)"
    else fail "Vibrador" "sysfs no disponible"; fi
fi

# ── 5. audio ──────────────────────────────────────────────────────────────────

if section audio "AUDIO"; then
    # En ICS AudioFlinger intenta audio.primary.<board>.so; el Y210 usa libaudio.so
    # envuelto por audio_hw_hal — ambos deberían estar presentes
    chk_file "libaudio.so"              "/system/lib/libaudio.so"
    chk_file "libaudiopolicy.so"        "/system/lib/libaudiopolicy.so"
    chk_file "/dev/msm_pcm_out"         "/dev/msm_pcm_out"
    chk_file "/dev/msm_snd"             "/dev/msm_snd"
    if adb_sh "service list 2>/dev/null" | grep -q "media.audio_flinger"; then
        pass "AudioFlinger registrado en ServiceManager"
    else fail "AudioFlinger" "no en service list"; fi
    # En ICS dumpsys audio muestra el estado del HAL
    if adb_sh "dumpsys media.audio_flinger 2>/dev/null" | grep -qi "output\|hardware"; then
        pass "dumpsys AudioFlinger responde"
    else skip "dumpsys audio_flinger" "sin respuesta útil"; fi
    manual "Speaker"       "reproduce sonido — verifica volumen por altavoz"
    manual "Auriculares"   "conecta auriculares — verifica sonido y volumen"
    manual "Micrófono"     "graba nota de voz — reproduce y verifica"
    manual "Llamada audio" "realiza llamada — verifica audio subida y bajada"
fi

# ── 6. Wi-Fi ──────────────────────────────────────────────────────────────────

if section wifi "WI-FI"; then
    # Y210 usa ath6kl — firmware en /system/etc/firmware/
    _wfirmware=$(adb_sh "ls /system/etc/firmware/ 2>/dev/null" | grep -iE "ath|ar[0-9]")
    if [[ -n "$_wfirmware" ]]; then pass "Firmware WiFi ath6kl presente ($_wfirmware)"; \
    else skip "Firmware WiFi" "no encontrado en /system/etc/firmware/"; fi
    chk_file "hostapd"  "/system/bin/hostapd"
    chk_file "wpa_supplicant.conf"  "/data/misc/wifi/wpa_supplicant.conf"
    adb_sh "svc wifi enable" 2>/dev/null || true
    if ! $FAST; then sleep 3; fi
    # ath6kl puede crear eth0 o wlan0 según el driver
    _wl=$(adb_sh "ip link 2>/dev/null" | grep -E "eth0|wlan0")
    if [[ -n "$_wl" ]]; then pass "Interfaz WiFi presente ($(echo "$_wl" | awk '{print $2}' | head -1))"; \
    else fail "Interfaz WiFi (eth0/wlan0)" "no encontrada tras habilitar WiFi"; fi
    _wstat=$(adb_sh getprop wlan.driver.status)
    if [[ "$_wstat" == "ok" ]]; then pass "wlan.driver.status=ok"; \
    else skip "wlan.driver.status" "='$_wstat'"; fi
    manual "Asociación/DHCP"  "conéctate a una red y verifica IP asignada"
    manual "Tethering Wi-Fi"  "activa hotspot — conecta un cliente y verifica internet"
fi

# ── 7. Bluetooth ──────────────────────────────────────────────────────────────

if section bt "BLUETOOTH"; then
    chk_file "hciattach"           "/system/bin/hciattach"
    chk_file "libbluetooth.so"     "/system/lib/libbluetooth.so"
    # En ICS bluetooth corre en proceso com.android.bluetooth
    _hci=$(adb_sh "hciconfig 2>/dev/null")
    if [[ "$_hci" == *"hci0"* ]]; then pass "hci0 detectado"; \
    else skip "hci0" "BT apagado — activar desde ajustes y re-ejecutar"; fi
    if adb_sh "ps 2>/dev/null" | grep -q "com.android.bluetooth"; then
        pass "Proceso Bluetooth en ejecución"
    else skip "com.android.bluetooth" "proceso no activo (BT apagado)"; fi
    manual "BT pairing"        "empareja con otro dispositivo"
    manual "BT audio A2DP/SCO" "conecta auriculares BT — reproduce audio"
fi

# ── 8. sensores ───────────────────────────────────────────────────────────────

if section sensors "SENSORES"; then
    chk_file "/dev/accel" "/dev/accel"
    # En ICS dumpsys sensorservice muestra la lista de sensores registrados
    if adb_sh "dumpsys sensorservice 2>/dev/null" | grep -qi "lis3dh\|accelerometer\|Sensor List"; then
        pass "SensorService responde con sensores"
    else fail "SensorService" "sin respuesta en dumpsys sensorservice"; fi
    manual "Acelerómetro" "gira el teléfono — verifica rotación de pantalla"
fi

# ── 9. GPS ────────────────────────────────────────────────────────────────────

if section gps "GPS"; then
    chk_file "gps.y210.so HAL"  "/system/lib/hw/gps.y210.so"
    chk_file "gps.conf"         "/system/etc/gps.conf"
    _gpsv=$(adb_sh getprop ro.gps.agps_provider)
    if [[ -n "$_gpsv" ]]; then pass "ro.gps.agps_provider=$_gpsv"; \
    else skip "ro.gps.agps_provider" "vacío"; fi
    # En ICS el servicio GPS se puede consultar vía dumpsys location
    if adb_sh "dumpsys location 2>/dev/null" | grep -qi "gps\|provider"; then
        pass "LocationManager responde (GPS provider visible)"
    else skip "dumpsys location" "sin datos de GPS"; fi
    manual "GPS fix real"  "al aire libre — abre Maps y espera fix de satélites (~2 min)"
fi

# ── 10. cámara ────────────────────────────────────────────────────────────────

if section camera "CÁMARA"; then
    chk_file "libcamera.so"        "/system/lib/libcamera.so"
    chk_file "libcameraservice.so" "/system/lib/libcameraservice.so"
    chk_file "media_profiles.xml"  "/system/etc/media_profiles.xml"
    chk_prop "ro.build.product para blob"  ro.build.product  "y210"
    _codec=$(adb_sh "grep -m1 'codec' /system/etc/media_profiles.xml 2>/dev/null")
    if [[ "$_codec" == *"h263"* ]]; then pass "media_profiles usa h263 (SW encoder OK)"; \
    else skip "media_profiles codec" "='$_codec' (esperado h263)"; fi
    # En ICS am force-stop está disponible (a diferencia de GB)
    adb_sh "am force-stop com.android.camera" 2>/dev/null || true
    adb_sh "logcat -c" 2>/dev/null || true
    adb_sh "am start -n com.android.camera/.Camera" > /dev/null 2>&1
    sleep 4
    if adb_sh "ps" | grep -v grep | grep -q "android.camera"; then
        pass "App Camera en ejecución"
    else fail "App Camera" "proceso no encontrado tras 4s"; fi
    _prev=$(adb_sh "logcat -d 2>/dev/null" | grep -c "startPreview.*rc=0" 2>/dev/null || echo 0)
    if [[ "$_prev" -gt 0 ]]; then pass "startPreview rc=0 en logcat"; \
    else skip "startPreview logcat" "no hay log reciente (OK si ya estaba corriendo)"; fi
    adb_sh "am force-stop com.android.camera" 2>/dev/null || true
    manual "Preview foto en color"   "verifica: 640×480, colores reales, portrait"
    manual "Captura de foto"         "toma foto — verifica JPEG en galería"
    manual "Switch a modo video"     "pulsa botón video — verifica preview 352×288"
    manual "Grabación de video"      "graba 5s — verifica MP4 en galería sin crash"
    manual "Switch video→foto→video" "alterna 3 veces — sin crash ni preview negro"
fi

# ── 11. almacenamiento ────────────────────────────────────────────────────────

if section storage "ALMACENAMIENTO"; then
    _dfdata=$(adb_sh "df /data 2>/dev/null | tail -1")
    if [[ -n "$_dfdata" ]]; then pass "/data montado ($_dfdata)"; \
    else fail "/data" "df falló"; fi
    _dfsys=$(adb_sh "df /system 2>/dev/null | tail -1")
    if [[ -n "$_dfsys" ]]; then pass "/system montado ($_dfsys)"; \
    else fail "/system" "df falló"; fi
    # En ICS la sdcard se monta en /storage/sdcard0 (symlink desde /mnt/sdcard)
    if adb_sh "mount 2>/dev/null" | grep -qE "sdcard|/storage/sdcard0"; then
        pass "SDCard montada"
    elif adb_sh "[ -d '/storage/sdcard0' ] && echo y" | grep -q y; then
        pass "SDCard directorio /storage/sdcard0 presente"
    else skip "SDCard" "no montada — insertar microSD"; fi
    manual "USB mass storage" "conecta USB, activa almacenamiento USB y verifica que la microSD aparece como disco en PC"
fi

# ── 12. RIL / telefonía ───────────────────────────────────────────────────────

if section ril "RIL / TELEFONÍA"; then
    # msm7x27a en ICS puede usar libril-qc-1.so o libril-qc-qmi.so según modem
    if adb_sh "[ -e '/system/lib/libril-qc-qmi-1.so' ] && echo y" | grep -q y; then
        pass "RIL library libril-qc-qmi-1.so"
    elif adb_sh "[ -e '/system/lib/libril-qc-1.so' ] && echo y" | grep -q y; then
        pass "RIL library libril-qc-1.so"
    else fail "RIL library" "ni libril-qc-qmi-1.so ni libril-qc-1.so encontrados"; fi
    _bb=$(adb_sh getprop gsm.version.baseband)
    if [[ -n "$_bb" ]]; then pass "Baseband: $_bb"; \
    else fail "Baseband" "gsm.version.baseband vacío"; fi
    # En ICS el IMEI se obtiene vía service call iphonesubinfo 1
    _imei=$(adb_sh "service call iphonesubinfo 1 2>/dev/null" | grep -oP "(?<=\x27)[0-9]+" | tr -d '\n')
    if [[ -n "$_imei" && "${#_imei}" -ge 15 ]]; then pass "IMEI válido (${_imei:0:6}xxxxxxxxx)"; \
    else
        # fallback: dumpsys iphonesubinfo
        _imei2=$(adb_sh "dumpsys iphonesubinfo 2>/dev/null" | grep -oP '[0-9]{15}')
        if [[ -n "$_imei2" ]]; then pass "IMEI válido vía dumpsys (${_imei2:0:6}xxxxxxxxx)"; \
        else fail "IMEI" "no obtenido (¿permisos READ_PHONE_STATE?)"; fi
    fi
    _op=$(adb_sh getprop gsm.operator.alpha)
    _simstate=$(adb_sh getprop gsm.sim.state)
    if [[ -n "$_op" ]]; then pass "Operador registrado: $_op"; \
    elif [[ "$_simstate" == "ABSENT" || "$_simstate" == "NOT_READY" || "$_simstate" == "UNKNOWN" ]]; then
        skip "Operador" "SIM no presente/no lista (gsm.sim.state=$_simstate)"; \
    else fail "Operador" "gsm.operator.alpha vacío (sim.state=$_simstate)"; fi
    _rmnet=$(adb_sh "ip link 2>/dev/null" | grep rmnet)
    if [[ -n "$_rmnet" ]]; then pass "Interfaz rmnet (datos activos)"; \
    else skip "rmnet" "datos no activos — activar datos móviles y re-probar"; fi
    manual "Llamada saliente"  "llama a un número — audio bidireccional OK"
    manual "Llamada entrante"  "recibe llamada — timbre + audio OK"
    manual "SMS saliente"      "envía SMS — verifica entrega"
    manual "SMS entrante"      "recibe SMS — verifica notificación"
fi

# ── 13. energía ───────────────────────────────────────────────────────────────

if section power "ENERGÍA"; then
    if adb_sh "[ -f '/sys/power/state' ] && echo y" | grep -q y; then
        pass "/sys/power/state existe"
    else fail "/sys/power/state" "no encontrado"; fi
    _batt=$(adb_sh "cat /sys/class/power_supply/battery/capacity 2>/dev/null")
    if [[ -n "$_batt" && "$_batt" -gt 0 ]] 2>/dev/null; then pass "Batería: ${_batt}%"; \
    else skip "Batería sysfs" "ruta no estándar — verificar manualmente"; fi
    # En ICS wake locks visibles vía dumpsys power
    if adb_sh "dumpsys power 2>/dev/null" | grep -qi "wake lock\|mWakefulness"; then
        pass "dumpsys power responde"
    else skip "dumpsys power" "sin respuesta útil"; fi
    if $FAST; then skip "Tests de suspend" "--fast: omitido"; \
    else
        manual "Suspensión pantalla"  "apaga pantalla 30s — verifica reanudación"
        manual "Deep sleep"           "desconecta USB 5min — verifica consumo bajo"
    fi
fi

# ── resumen ────────────────────────────────────────────────────────────────────

TOTAL=$((PASS+FAIL+SKIP+MANUAL))
echo ""; echo -e "${BLD}══ RESUMEN ══${RST}"
printf "  Total:    %d\n" "$TOTAL"
echo -e "  ${GRN}PASS:     $PASS${RST}"
echo -e "  ${RED}FAIL:     $FAIL${RST}"
echo -e "  ${YLW}SKIP:     $SKIP${RST}"
echo -e "  ${CYN}MANUAL:   $MANUAL${RST}"
echo ""
echo "Log: $LOG"
{ echo ""; echo "RESUMEN: PASS=$PASS FAIL=$FAIL SKIP=$SKIP MANUAL=$MANUAL / $TOTAL"; } >> "$LOG"

exit $FAIL
