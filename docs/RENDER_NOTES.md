# Y210 — Render / Display Notes

## Hardware

- Display: 320×480 px, 32 bpp (RGBA_8888, `r=24:8 g=16:8 b=8:8`)
- Panel: **MIPI DSI en modo comando** (`hx8357c`, `panel_info.type = 9`, `fb_num = 3`)
- GPU: Adreno 200 (MSM7225A), `gpuclk` 133 MHz (= `max_gpuclk`)
- Framebuffer: `msmfb30_90000`, `yres_virtual = 1440` (3 buffers de 480 líneas), stride 1280 bytes

## Stack (verificado 2026-10-02)

```
SurfaceFlinger (system_server)
  └─ EGL/GLES Adreno 200 (HW: "renderer: Adreno (TM) 200", /dev/kgsl-3d0)
  └─ gralloc.y210.so   (device/huawei/y210/libgralloc, ex libgralloc-qsd8k)
       └─ /dev/graphics/fb0 — page-flip por yoffset (alterna 0/480)
  └─ copybit.y210.so   (device/huawei/y210/libcopybit) — solo se carga con
       capas push-buffer (video/cámara) vía LayerBuffer
```

La nota anterior de que SurfaceFlinger componía por software (`SLOW_CONFIG`) **ya no es cierta**: compone la Adreno 200. gralloc y copybit viven en el device tree como `*.y210` (hw_get_module prueba `ro.product.board=y210` antes que `ro.board.platform=msm7k`); `hardware/msm7k` ya no se parchea.

## Posting del framebuffer: FB_ACTIVATE_NOW (2026-10-02)

`eglSwapBuffers` en reposo (medido igual en ambos: abrir Calculadora, HOME, esperar 3 s, `dumpsys SurfaceFlinger`):

| | swap |
|---|---|
| CM7 con `FB_ACTIVATE_VBL` | ~29.5 ms (2 vsync, techo ~33 fps) |
| **CM7 con `FB_ACTIVATE_NOW`** | **~15.4 ms** |
| Stock | ~0.3–4.8 ms |

### Posting asíncrono (2026-10-02)

`fb_post` ya no hace el pan: lo encola a un hilo propio de gralloc (`fb_post_thread`, prioridad URGENT_DISPLAY), igual que el gralloc CAF del stock. El pan del kernel (`mdp_dma2_update`) solo vuelve cuando terminaron la DMA y el motor DSI (`wait_for_completion(&mfd->dma->comp)` + `dsi_mdp_comp`). Así, cuando el pan del buffer A vuelve, A ya está en la GRAM del panel y SF puede volver a dibujarlo. `fb_post(B)` espera a que termine el pan anterior antes de soltar A (mismo orden `unlock`/`lock` que el camino síncrono), copia `m->info` (por `setUpdateRect`) y vuelve. Hay un solo pan en vuelo.

| | `eglSwapBuffers` en reposo |
|---|---|
| `FB_ACTIVATE_VBL` (original) | ~29.5 ms |
| `FB_ACTIVATE_NOW` síncrono | ~15.4 ms |
| **`FB_ACTIVATE_NOW` + posting asíncrono** | **~0.3–13 ms** (~3 ms típico) |
| Stock | ~0.3–4.8 ms |

Se desactiva con `setprop debug.gr.async_post 0` (se lee al abrir fb0, hay que reiniciar el framework). Log al arrancar: `async framebuffer posting enabled (3 buffers)`.

**Efecto secundario conocido, sin impacto visible:** con el posting asíncrono aparecen `E/copybit: copyBits failed (Invalid argument)` durante la reproducción de video (blit RGB565 352×288 → 288×320 rotado, `flags=00020008`). `0x20000` es `MDP_BLEND_FG_PREMULT`, y en MDP30 (`CONFIG_FB_MSM_MDP30=y`, no MDP31) `mdp_ppp_blit` lo rechaza siempre con `EINVAL`; en ese caso `LayerBuffer` vuelve a dibujar la capa por GL. Se ve más con el posting asíncrono porque SF compone más frames por segundo. Validado a ojo: el video se ve bien. Los `mpd_ppp: src img of zero size!` de dmesg son del mismo flujo.

`msm_fb_pan_display()` termina en `mdp_set_dma_pan_info(..., activate == FB_ACTIVATE_VBL)` + `mdp_dma_pan_update()`; con VBL además sincroniza con vsync, y en un panel en modo comando eso duplicaba la espera. El stock usa el gralloc CAF más nuevo, que tiene un **hilo de posting asíncrono** (`pthread_create`/`pthread_cond_*`, `framebufferStateName`, props `debug.gr.swapinterval`/`debug.gr.numframebuffers`): `fb_post` delega y vuelve al instante. Ese hilo ya está portado: ver "Posting asíncrono" arriba.

Descartado con medición: CPU/governor (con `performance` a 1 GHz sigue igual), reloj GPU (ya al máximo), composición por software, falta de page-flip.

**`W/msm7k.gralloc: FBIOPUT_VSCREENINFO failed, page flipping not supported` es engañoso.** Lo produce solo el `FBIOPUT` inicial de `mapFrameBufferLocked`, que pide `transp.length = 0` y `msm_fb_check_var` exige `transp.length == 8` para 32 bpp. Después gralloc relee la configuración del kernel (`yres_virtual = 1440`), calcula `numBuffers = 3` y el flip por `yoffset` funciona (verificable con `cat /sys/class/graphics/fb0/pan` mientras hay animación; con la pantalla apagada no hay frames).

## copybit: desfase de ABI que NO hay que "arreglar" (2026-10-02)

`libcopybit` se compila con `hardware/msm7k/libgralloc/gralloc_priv.h` (gralloc msm7201A), pero los handles vienen del gralloc qsd8k. `private_handle_t` coincide en los 7 primeros campos (`fd`…`base`); el 8º es `map_offset` en el header de copybit y `lockState` en qsd8k, y el bit `0x4` es `PRIV_FLAGS_USES_GPU` en uno y `PRIV_FLAGS_USES_PMEM_ADSP` en el otro.

Se probó compilar copybit contra el header correcto (`memory_id = hnd->fd` siempre): **la reproducción de video pasó a fallar en cada frame** (`copyBits failed (Invalid argument)`, src 352×288 → dst 288×320 rotado), mientras que con el copybit original da 0 fallos. Es decir, el camino "GPU" (`memory_id = gpu_fd`, offset extra, RGBA→BGRA) es el que hace funcionar el blit de video en este equipo. Se revirtió. No tocar sin entender qué deja el decoder en esos campos.

## Bug: Statusbar/lockscreen "ghosting" corruption — FIXED (2026-05-02)

### Symptom

Persistent solid-color blocks covering parts of the lockscreen and statusbar area.
The blocks matched boot-animation colors (green, red, blue) and never disappeared.
Visible on every boot; only went away if the entire screen was forced to redraw.

### Root cause

`SWAP_RECTANGLE` optimization in SurfaceFlinger:

1. Adreno 200 EGL advertises `EGL_ANDROID_swap_rectangle` (confirmed in logcat).
2. `DisplayHardware::init()` detects the extension, calls
   `eglSetSwapRectangleANDROID`, and sets the `SWAP_RECTANGLE` flag.
3. `SurfaceFlinger::handleRepaint()` checks `SWAP_RECTANGLE`:
   ```cpp
   if ((flags & SWAP_RECTANGLE) || (flags & BUFFER_PRESERVED)) {
       mDirtyRegion.set(mInvalidRegion.bounds()); // dirty rect only
   } else {
       mDirtyRegion.set(hw.bounds()); // full screen
   }
   ```
4. SF only redraws the dirty region per frame. Both framebuffer pages start with
   boot-animation content; the non-dirty areas are never overwritten → permanent
   colored-block corruption.

Key evidence:
- Disabling ALL copybit blits (`stretch_copybit` returning `-EINVAL`) did **not** fix
  the corruption — proves the issue is not in the MDP/copybit path.
- Corruption was consistently solid-color blocks matching boot animation palette.
- `BUFFER_PRESERVED` is not set (Adreno 200 EGL default is `EGL_BUFFER_DESTROYED`).
- Previous incomplete fix: `dev->device.setUpdateRect = 0` in `framebuffer.cpp` was
  intended to prevent this, but it only disabled `PARTIAL_UPDATES`, which does **not**
  affect the `SWAP_RECTANGLE` code path (they are independent flags).

### Fix

`device/huawei/y210/libgralloc/framebuffer.cpp`:

Assign a no-op `fb_setUpdateRect_noop` (non-NULL function pointer) instead of `0`.
`FramebufferNativeWindow::isUpdateOnDemand()` returns `(fbDev->setUpdateRect != 0)`.
When non-NULL:
- `DisplayHardware` sets `PARTIAL_UPDATES` (0x00020000).
- `if (mFlags & PARTIAL_UPDATES) mFlags &= ~SWAP_RECTANGLE;` clears SWAP_RECTANGLE.
- `handleRepaint()` falls into the `else` branch → `mDirtyRegion = hw.bounds()` → **full-screen redraw every frame**.
- `flip()` calls `setUpdateRectangle()` → our no-op (does nothing).

The no-op intentionally does **not** write `reserved[0] = 0x54445055` ("UPDT"), which
would trigger the MSM kernel's partial-scan path via `FBIOPUT_VSCREENINFO`.

### Verification

After push of `gralloc.y210.so` + `copybit.y210.so` and `stop; start`:

```
I/SurfaceFlinger: extensions: ... EGL_ANDROID_swap_rectangle ...
I/SurfaceFlinger: flags = 00060000
```

`0x00060000` = `PARTIAL_UPDATES (0x00020000)` | `SLOW_CONFIG (0x00040000)`.  
`SWAP_RECTANGLE (0x00080000)` is not set. ✓

## KGSL permissions

`/dev/kgsl-3d0` requires group `graphics` access. On some boots the node comes up
with permissions that exclude the graphics group — verify with:

```sh
adb shell ls -l /dev/kgsl-3d0
```

Expected: `crw-rw---- root graphics`. If wrong, the ueventd rule in
`device/huawei/y210/ueventd.y210.rc` should cover this; check that it is included
in the ramdisk.

## Useful logcat filters

```sh
# SF startup flags and EGL info
adb logcat -v time -d | grep -E "SurfaceFlinger|flags ="

# Copybit activity (errors or format mismatches)
adb logcat -v time -d | grep -iE "copybit|libagl.*copy"

# Framebuffer open + geometry
adb logcat -v time -d | grep -i "y210.gralloc"
```
