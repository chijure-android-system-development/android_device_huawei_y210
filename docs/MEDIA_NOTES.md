# Media Notes (Galería / miniaturas / media scanner)

## Miniaturas PNG: ~1 de cada 2 fallaba — RESUELTO (2026-10-02)

**Síntoma:** en logcat, al abrir la Galería o al escanear la SD:

```text
D/skia    ( 528): --- decoder->decode returned false
W/MediaThumbRequest( 528): Can't create mini thumbnail for /mnt/sdcard/screen.png
```

44 de 58 PNG sin miniatura. No dependía del formato: archivos idénticos en
estructura (320x480, mismo color type, mismos chunks) fallaban o no. La pista
fue la DB de medios: los `_id` **alternaban** exactamente (178 falla, 179 ok,
180 falla, ...), y en logcat cada fallo venía justo después de un
`GC_EXTERNAL_ALLOC`.

**Causa raíz:** `ThumbnailUtils.createImageThumbnail()` (frameworks/base,
`media/java/android/media/ThumbnailUtils.java`) hacía:

```java
FileDescriptor fd = new FileInputStream(filePath).getFD();
```

El `FileInputStream` quedaba sin referencia. Un GC entre el decode de bounds
(`inJustDecodeBounds=true`) y el decode real lo finalizaba, el finalizer
cerraba el fd, y el segundo `decodeFileDescriptor()` fallaba. Además el stream
nunca se cerraba explícitamente (leak de un fd por miniatura). JPEG casi no se
ve afectado porque normalmente sale por la miniatura EXIF.

**Fix:** `patches/frameworks_base_thumbnailutils_fd.patch` — guardar la
referencia al stream y cerrarlo en `finally`. Es el mismo fix que trae AOSP ICS.

**Validado en equipo:** 0 fallos PNG, 0 `decode returned false`; en una sola
apertura de la Galería se generaron 15 miniaturas que antes fallaban.

## Videos `.m4v` con `duration=0` — RESUELTO (2026-10-02)

**Síntoma:** todos los videos de la cámara (`DCIM/Camera/VID_*.m4v`) quedaban
con `duration=0` en la tabla `video` de la DB de medios (la Galería/lista de
videos mostraba 0:00), mientras que los `.mp4` sí tenían duración.

**Causa raíz:** `VideoCamera` guarda `OutputFormat.MPEG_4` con extensión
`.m4v` (`packages/apps/Camera/.../VideoCamera.java`), pero
`FileHasAcceptableExtension()` en
`frameworks/base/media/libstagefright/StagefrightMediaScanner.cpp` no la
incluía → el scanner rechazaba el archivo sin extraer metadata. El stock no lo
sufría porque la cámara de Huawei usa `.3gp`/`.mp4` (su `libstagefright.so`
tampoco acepta `.m4v`). Las props `media.stagefright.*` son idénticas al stock.

**Fix:** `patches/frameworks_base_scanner_m4v.patch` — agregar `".m4v"` a la
lista. `MPEG4Extractor` se elige por sniff del contenedor, no por extensión.

**Validado en equipo:** los `.m4v` válidos reportan su duración real
(6.2–10.5 s, coherente con `ffprobe`).

Los archivos ya escaneados no se reprocesan solos (el scanner solo mira archivos
con `date_modified` nuevo). Para forzarlo: `busybox touch <archivo>` y
`am broadcast -a android.intent.action.MEDIA_MOUNTED -d file:///mnt/sdcard`.
No se pueden borrar filas de la DB con `sqlite3` desde shell: los triggers
llaman a `_DELETE_FILE()`, que solo existe dentro del MediaProvider.

## Fallos esperados (NO son bugs)

- **Grabaciones del bring-up (30–31/05/2026, hasta `VID_20260531_011909`) y
  `recording*.3gp`:** archivos corruptos, sin átomo `moov` (uno pesa 32 bytes).
  `MetadataRetrieverClient: failed to capture a video frame` es correcto.
- **H.264 High profile** (ej. `test-video.mp4`, 640x360 High@3.0): el decoder
  de video del MSM7x27a solo soporta Baseline. Un H.264 Constrained Baseline
  (`test-video-gb.mp4`) sí genera miniatura.
- **`MetadataRetrieverClient: failed to extract an album art`**: MP3 sin
  carátula embebida.

## Validación rápida

```bash
DB=/data/data/com.android.providers.media/databases/$(adb shell ls /data/data/com.android.providers.media/databases/ | grep '^external.*\.db' | tr -d '\r')
# PNG con/sin miniatura
adb shell "sqlite3 $DB \"select count(*), sum((select count(*) from thumbnails t where t.image_id=i._id)>0) from images i where _data like '%.png';\""
# duración de los videos de cámara
adb shell "sqlite3 $DB \"select _data,duration from video where _data like '%.m4v';\""
# fallos de miniatura tras abrir la Galería
adb logcat -d | grep -E "Can't create mini|decode returned false"
```
