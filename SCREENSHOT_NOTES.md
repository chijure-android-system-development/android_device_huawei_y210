# Screenshot — Huawei Y210 CM9 (ICS)

## Estado CM9 (2026-06-06): OK con ruta HW EGL/Adreno

El screenshot desde el power menu ya funciona en la configuración actual con
`debug.sf.hw=1` y SurfaceFlinger usando Adreno 200.

Conclusión actual: el fallo documentado abajo correspondía a la ruta software
PixelFlinger, donde no había `GL_OES_framebuffer_object` (FBO). Al pasar el
compositor a la ruta HW EGL/Adreno, SurfaceFlinger sí puede capturar pantalla.

Mantener `debug.sf.hw=1` como ruta normal. Si se vuelve a `debug.sf.hw=0`, el
fallo de screenshot puede reaparecer.

---

## Historial: fallo en ruta software (2026-06-03)

El screenshot desde el power menu y los thumbnails de Recent Apps muestran
"Couldn't save screenshot, Storage may be full" aunque haya espacio disponible.

---

## Diagnóstico

### Síntoma exacto

```
W/WindowManager: Failure taking screenshot for (120x180) to layer 21000
```

Y en GlobalScreenshot:
```
mScreenBitmap = Surface.screenshot(dims[0], dims[1])
→ null → notifyScreenshotError → "Couldn't save screenshot"
```

### Causa raíz

`SurfaceFlinger::captureScreen()` retorna `INVALID_OPERATION` porque requiere
`GL_OES_framebuffer_object` (FBO) que **PixelFlinger no tiene**.

SurfaceFlinger en este dispositivo usa PixelFlinger (software GL,
`libGLES_android.so`) para compositing — la Adreno 200 usa `EGL_SLOW_CONFIG`.
PixelFlinger no expone `GL_OES_framebuffer_object` en sus extensiones GL.

El código fuente en `SurfaceFlinger.cpp`:
```cpp
// captureScreen() — línea 2682
if (!GLExtensions::getInstance().haveFramebufferObject())
    return INVALID_OPERATION;   // ← siempre retorna esto

// captureScreenImplLocked() — línea 2489
if (!GLExtensions::getInstance().haveFramebufferObject())
    return INVALID_OPERATION;   // ← segundo check, nunca llega aquí
```

---

## Fix intentado y revertido

### Approach: fallback a `/dev/graphics/fb0`

Implementamos un fallback en `captureScreenImplLocked()` que lee directamente
del framebuffer device cuando no hay FBO:

```cpp
if (!GLExtensions::getInstance().haveFramebufferObject()) {
    int fbfd = open("/dev/graphics/fb0", O_RDONLY);
    // leer frame activo via yoffset, convertir RGB565→RGBA8888
    // retornar MemoryHeapBase con los pixels
}
```

**`screencap` de línea de comandos funcionó** (129 KB, 99% pixels no negros).

**Problema 1 — null Bitmap.Config:**
Cuando `screenshotApplications()` llama `rawss.getConfig()` para los thumbnails,
retorna null → NPE → FATAL EXCEPTION → reboot. El bitmap retornado por nuestra
ruta tenía config inválida en el mapeo C++/Java de SkBitmap.

**Problema 2 — SIGSEGV en SurfaceFlinger:**
Las llamadas a `open()`, `ioctl()`, `malloc()`, `read()` desde el thread de
rendering de SurfaceFlinger causaban SIGSEGV en ciertas condiciones de timing.
SurfaceFlinger tiene restricciones sobre llamadas bloqueantes en su thread principal.

**Ambos problemas causaban reboot del dispositivo → revertido.**

---

## Por qué es difícil

Para hacer el fallback de fb0 correctamente hay que:

1. **Moverlo a un thread separado**: las llamadas bloqueantes (open, read) no
   pueden hacerse en el GL/rendering thread de SurfaceFlinger.

2. **Sincronización con el display**: leer fb0 mientras SurfaceFlinger está
   escribiendo puede dar un frame parcial o corrupto (tearing).

3. **`Bitmap.getConfig()` null**: el fallback retorna un Bitmap cuyo SkBitmap
   config no mapea correctamente a `Bitmap.Config` de Java. Requiere investigar
   el mapeo en `convertPixelFormat()` y `sConfigs[]`.

4. **Layer filtering**: el path normal filtra por Z-order (`minLayerZ/maxLayerZ`).
   El fallback de fb0 retorna toda la pantalla sin filtrado.

---

## Alternativa potencial: `screencap` binary

El binario `/system/bin/screencap` funcionó correctamente con el fix de fb0
(antes del reboot issue). Una opción sería:
- Triggear `screencap` como proceso externo desde TakeScreenshotService
- Leer el resultado y procesarlo en Java

Pero esto requiere modificar `GlobalScreenshot.java` y `TakeScreenshotService`.

---

## Archivos relevantes

| Archivo | Relevancia |
|---|---|
| `frameworks/base/services/surfaceflinger/SurfaceFlinger.cpp` | `captureScreen()` + `captureScreenImplLocked()` |
| `frameworks/base/packages/SystemUI/src/.../GlobalScreenshot.java` | Toma y guarda el screenshot |
| `frameworks/base/packages/SystemUI/src/.../TakeScreenshotService.java` | Servicio invocado por el power menu |
| `frameworks/base/services/java/.../WindowManagerService.java` | `screenshotApplications()` para thumbnails |
| `frameworks/base/core/jni/android_view_Surface.cpp` | `doScreenshot()`, `ScreenshotPixelRef` |
| `frameworks/base/libs/gui/SurfaceComposerClient.cpp` | `ScreenshotClient::update()` |

---

## Extensiones GL disponibles en SurfaceFlinger

```
GL_OES_byte_coordinates GL_OES_fixed_point GL_OES_single_precision
GL_OES_read_format GL_OES_compressed_paletted_texture GL_OES_draw_texture
GL_OES_matrix_get GL_OES_query_matrix GL_OES_EGL_image
GL_OES_compressed_ETC1_RGB8_texture GL_ARB_texture_compression
GL_ARB_texture_non_power_of_two GL_ANDROID_user_clip_plane
GL_ANDROID_vertex_buffer_object GL_ANDROID_generate_mipmap
```

Notablemente **ausente**: `GL_OES_framebuffer_object`
