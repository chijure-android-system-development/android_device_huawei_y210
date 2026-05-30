# Camera Notes — Huawei Y210

## Estado (2026-05-30) — ESTABLE

Todo el ciclo básico de cámara funciona en dispositivo:

- App Cámara abre — OK
- Preview `640x480` en color, orientación portrait correcta — OK
- Captura de foto — OK: SHUTTER + RAW_IMAGE (294912 B) + COMPRESSED_IMAGE (~55 KB JPEG)
- Preview se reinicia automáticamente tras la foto — OK
- `close → reopen` — OK: `release()` llama blob slot 26; nuevo `open` crea instancia fresca
- `mediaserver` sobrevive todo el ciclo — OK
- Video — **Pendiente**

## Props requeridos

```
ro.build.product=msm7625a   # primer escritor gana en ro.* — build.prop no debe tener ro.build.product=y210 antes
persist.camera.delegate_setparams=1
persist.camera.mode=1
```

El blob `libcamera.y210.so` detecta el target leyendo `ro.build.product` y comparando
con strings internos (`msm7625a`, `msm7627a`, etc.). Si lee `y210` muere con
`Unable to determine the target type`.

## Causa raíz resuelta: vtable misalignment

El blob stock (`QualcommCameraHardware`) fue compilado contra una versión de
`CameraHardwareInterface` con 2 métodos Qualcomm extra insertados entre
`cancelAutoFocus` (slot 18) y `takePicture`. Eso desplaza todos los métodos
siguientes +2 respecto al layout de CM7.

Tabla de slots:

```
CM7 slot  método CM7          blob slot
18        cancelAutoFocus     18   (sin desplazamiento)
19        takePicture         21   (+2 por los 2 extras en 19/20)
20        cancelPicture       22
21        setParameters       23
22        getParameters       24   (no se llama directamente; se usa cache)
23        sendCommand         25
24        release             26
25        dump                27
```

## Implementación del wrapper

`libcamera/Y210CameraWrapper.cpp` usa despacho directo de vtable para todos
los métodos con desplazamiento:

```cpp
static inline void** blobVptr(const sp<CameraHardwareInterface>& iface) {
    return *reinterpret_cast<void***>(iface.get());
}
```

### takePicture — firma distinta

El blob espera `takePicture(const sp<ISurface>&)`, no `takePicture()`.
Se pasa `sp<ISurface> nullSurface` para que el blob omita el postview pero
entregue los callbacks JPEG igualmente:

```cpp
typedef status_t (*BlobTakePictureFn)(void*, const sp<ISurface>&);
BlobTakePictureFn fn = reinterpret_cast<BlobTakePictureFn>(blobVptr(mLibInterface)[21]);
sp<ISurface> nullSurface;
return fn(mLibInterface.get(), nullSurface);
```

Requiere `#include <surfaceflinger/ISurface.h>` y `libsurfaceflinger_client` en `Android.mk`.

### release — slot 26

El slot 24 del blob (al que apunta el puntero virtual de CM7 para `release()`)
es en realidad `getParameters` del blob → crash inmediato. Se fuerza slot 26:

```cpp
typedef void (*BlobReleaseFn)(void*);
BlobReleaseFn fn = reinterpret_cast<BlobReleaseFn>(blobVptr(mLibInterface)[26]);
fn(mLibInterface.get());
```

### Orientación de preview

El sensor está montado físicamente a 90° (sensor landscape en teléfono portrait).
Se fuerza `orientation=90` en `HAL_getCameraInfo()` para que la app de cámara
rote el preview correctamente:

```cpp
if (cameraInfo) {
    cameraInfo->orientation = 90;
}
```

## Pipeline de display del preview

El blob produce frames NV21 (`HAL_PIXEL_FORMAT_YCrCb_420_SP`, format 17).
El path de overlay está deshabilitado (el blob crasheaba al probar metadatos de
overlay en algunos ciclos de vida). SurfaceFlinger usa el path GL de software.

**Dos fixes necesarios en `hardware/msm7k/libcopybit/copybit.cpp`:**

1. `set_image(&req->src)` debe llamarse **antes** de `set_infos()` para que la
   detección YUV en `set_infos` lea `req->src.format` ya inicializado.
2. `set_infos()` suprime `MDP_DITHER` para fuentes YUV — el driver msm7x27
   devuelve `EINVAL` de `MSMFB_BLIT` si ese flag está presente con fuente YUV.

El copybit sigue fallando (los buffers de preview son de `pmem_adsp`, dominio
del ISP; `MSMFB_BLIT` solo acepta `pmem` del dominio display). El path GL es
el camino correcto para este hardware sin overlay.

**Fix en `frameworks/base/services/surfaceflinger/TextureManager.cpp`:**

- `isSupportedYuvFormat()` incluye `HAL_PIXEL_FORMAT_YCrCb_420_SP`.
- En `loadTexture()`, NV21 se convierte a RGB565 por software (BT.601) antes
  de subirlo como textura GL. La conversión procesa ~307 K píx/frame a ~7 fps
  (~30 ms/frame en Cortex-A5 a 600 MHz).

## Notas de build

```bash
# Wrapper (orientación)
make -j$(nproc) libcamera

# Pipeline de display
make -j$(nproc) libsurfaceflinger copybit.msm7k

# Push todo
adb remount
adb push out/target/product/y210/system/lib/libcamera.so /system/lib/
adb push out/target/product/y210/system/lib/libsurfaceflinger.so /system/lib/
adb push out/target/product/y210/system/lib/hw/copybit.msm7k.so /system/lib/hw/
adb shell sync && adb reboot
```

El setParameters puede emitir `rc=-22` para `antibanding` — el blob no soporta
ese parámetro en este sensor (OV5640) y lo ignora de forma benigna.
