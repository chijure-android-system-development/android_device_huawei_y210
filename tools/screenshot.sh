#!/bin/bash
# screenshot.sh — captura de pantalla del Y210 (CM7) desde el PC.
#
# Usa /system/bin/screenshot (helper nativo de CMScreenshot): captura via
# SurfaceFlinger (ScreenshotClient) y escribe $EXTERNAL_STORAGE/tmpshot.bmp
# (32 bpp, colores ya corregidos). El screencap de GB no sirve aqui: solo
# escribe RAW por stdout y "adb shell" corrompe binarios en el pipe.
#
# Uso:
#   bash device/huawei/y210/tools/screenshot.sh [salida.png] [-s SERIAL]

set -e

OUT="screenshot-$(date +%Y%m%d-%H%M%S).png"
ADB=(adb)
while [ $# -gt 0 ]; do
    case "$1" in
        -s) ADB=(adb -s "$2"); shift 2 ;;
        *)  OUT="$1"; shift ;;
    esac
done

TMP="$(mktemp --suffix=.bmp)"
trap 'rm -f "$TMP"' EXIT

"${ADB[@]}" shell "rm -f /mnt/sdcard/tmpshot.bmp; /system/bin/screenshot" >/dev/null
"${ADB[@]}" pull /mnt/sdcard/tmpshot.bmp "$TMP" >/dev/null
"${ADB[@]}" shell rm -f /mnt/sdcard/tmpshot.bmp

if command -v python3 >/dev/null && python3 -c 'import PIL' 2>/dev/null; then
    python3 -c 'import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[2])' "$TMP" "$OUT"
elif command -v convert >/dev/null; then
    convert "$TMP" "$OUT"
else
    OUT="${OUT%.png}.bmp"
    cp "$TMP" "$OUT"
fi
echo "$OUT"
