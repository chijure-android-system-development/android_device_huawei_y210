# Y210 — Render / Display Notes (CM9 / ICS)

## Estado CM9 (2026-06-06)

| Componente | Estado | Nota |
|---|---|---|
| Framebuffer / UI | **OK** | 320×480 60 Hz, 3 buffers |
| SurfaceFlinger compositor | **OK** | HW EGL con `debug.sf.hw=1`, Adreno 200 activo |
| GPU driver (Adreno200) | **OK** | `IOCTL_KGSL_TIMESTAMP_EVENT` portado; sin timeouts genlock en boot validado |
| Hardware GL en apps | **Parcial** | EGL Adreno inicializa; Gallery creó contexto EGL HW. Falta prueba amplia de apps 3D |
| hwcomposer | **No** | Desactivado: desalinea touch. Bug pendiente de investigar. |
| Ghosting / SWAP_RECTANGLE | **OK** | Fix `fb_setUpdateRect_noop` aplicado (ver sección CM7 abajo) |

### Fixes aplicados para GPU en ICS
- `libgsl.so` reemplazado por el del LG e400 CM9 (mismo Adreno200, compatible)
- `libsc-a2xx.so` añadido desde LG e400 CM9; sin este blob los drivers Adreno
  crashean al iniciar (`Library '/system/lib/libsc-a2xx.so' not found`)
- `/dev/pmem_gpu0/1` añadidos al kernel (`board-msm7x27a.c`, 4 MB cada uno)
- Permisos `pmem_gpu`: `0660 system graphics` en `ueventd.huawei.rc`
- `kgsl_devinfo`: `gmem_hostbaseaddr = gpurev (200)`, `chip_id &= 0xffff0000`
- ELF patches en blobs con `elf_add_needed.py` para añadir `libos_compat.so` como NEEDED

### Ruta HW actual

La ruta correcta para aceleración por hardware en este árbol es:

```properties
debug.sf.hw=1
persist.sys.nobootanimation=1
```

Requisitos de esa ruta:
- `private_handle_t` debe usar layout CAF con dos fds: `fd` + `genlockHandle`.
- `libgenlock` debe ser el real, no un stub; usa `/dev/genlock` y duplica el fd
  del lock dentro del native handle.
- El kernel debe implementar `IOCTL_KGSL_TIMESTAMP_EVENT` (`0x31`) para que el
  blob Adreno pueda registrar eventos de timestamp y liberar genlock cuando el
  render se retire. Sin ese ioctl, Adreno inicia pero loguea `errno 22 Invalid
  argument` y aparecen timeouts de genlock.

Validación realizada antes del parche KGSL:

```text
debug.sf.hw=1
sys.boot_completed=1
SurfaceFlinger vivo
renderer Adreno (TM) 200
sin GetBackBuffer / EGL_BAD_ALLOC / Fatal signal en la muestra
IOCTL_KGSL_TIMESTAMP_EVENT failed: errno 22
GENLOCK_IOC_LOCK timed out
```

Validación de build después del parche KGSL:

```text
docker exec cm9-builder bash -lc 'cd /home/builder/cm9 && source build/envsetup.sh && lunch cm_y210-eng && make -j$(nproc) bootimage'
Made boot image: out/target/product/y210/boot.img
sha256: cd183268b229b627ac579dd2715ac26bece4bb722ddeeb5cac38168b83766268
```

Validación final en dispositivo con ese `boot.img` flasheado:

```text
debug.sf.hw=1
sys.boot_completed=1
SurfaceFlinger renderer: Adreno (TM) 200
flags = 00200000
IOCTL_KGSL_TIMESTAMP_EVENT: 0 errores
GENLOCK_IOC_LOCK failed: 0
GetBackBuffer/EGL_BAD_ALLOC/GL_OUT_OF_MEMORY/Fatal signal: 0
```

### Rollback seguro

Si el kernel nuevo no está flasheado o hay regresión de boot, usar:

```properties
debug.sf.hw=0
persist.sys.nobootanimation=1
```

Ese modo mantiene SurfaceFlinger en PixelFlinger/software y evita que el backbuffer
Adreno sea crítico para el arranque. Fue validado el 2026-06-06 con boot completo,
`surfaceflinger/system_server/zygote` vivos y sin `EGL_BAD_ALLOC` ni SIGSEGV.

### Solución recomendada

La mejor solución ya no es quedarse en software: es mantener un stack coherente
CAF/Adreno para ICS y cerrar el contrato `gralloc` + `genlock` + `KGSL`.

No mezclar HALs gráficos por prueba rápida:
- Los blobs EGL Adreno del e400 requieren `libsc-a2xx.so`, pero eso no implica que
  el `gralloc`/`copybit` del e400 sean compatibles con Y210.
- El `private_handle_t` del e400 usa `sNumFds=2` y guarda `genlockHandle` como fd.
- El árbol Y210 fue corregido para usar `sNumFds=2` y conservar `genlockHandle`
  como segundo fd del native handle. `libgenlock` y `gralloc` deben compilarse
  siempre contra ese mismo layout.
- Si se renombra `gralloc.msm7k.so`/`copybit.msm7k.so` propietarios o del e400 a
  `gralloc.msm7x27a.so`, SurfaceFlinger puede arrancar parcialmente, pero termina
  en `libgenlock: invalid gralloc handle`, `GetBackBuffer() handle base address is
  NULL`, `EGL_BAD_ALLOC` o SIGSEGV.

La solución correcta a largo plazo no es copiar más blobs: es unificar el contrato
entre `gralloc`, `libgenlock`, `libui`/native handles y los blobs EGL Adreno.
Opciones defendibles:

1. Portar un `gralloc.msm7x27a` propio del Y210 que exponga exactamente el handle
   que espera Adreno 200 en ICS, incluyendo fds/genlock/base/offset válidos.
2. Mantener `sNumFds=1`, pero parchear el stack EGL/genlock para que no trate el
   handle como layout CAF/e400. Esto es más riesgoso porque toca blobs cerrados o
   wrappers alrededor de ellos.
3. Migrar todo el stack gráfico a una familia CAF coherente (`gralloc`, `copybit`,
   `libgenlock`, headers y blobs EGL del mismo árbol/base). Es más limpio, pero
   implica mayor superficie de regresión en framebuffer, cámara y copybit.

Con el kernel actualizado, `debug.sf.hw=1` es la ruta validada. `debug.sf.hw=0` queda solo como rollback seguro si aparece una regresión gráfica.

---

## Historial CM7 (aplica también a CM9 para los fixes de framebuffer)



## Hardware

- Display: 320×480 px, 32 bpp (RGBA_8888)
- GPU: Adreno 200 (MSM7225A)
- Framebuffer: double-buffered, 320×960 virtual (`yres_virtual`), page-flip via `FBIOPUT_VSCREENINFO` + `yoffset`
- Stride: 1280 bytes/row (320 px × 4 bytes)
- Framebuffer driver: `msmfb30_90000`

## Stack

```
SurfaceFlinger
  └─ libagl (PixelFlinger — SW GL compositor)
       └─ copybit.msm7x27a.so (MDP3 HW blit where usable)
  └─ gralloc.msm7x27a.so (framebuffer HAL)
       └─ /dev/graphics/fb0
            └─ MSMFB_BLIT ioctl (MDP DMA engine, also via /dev/fb0)
```

SurfaceFlinger must use **software GL** on the current tree. Letting SF use the
Adreno EGL path is unstable because the EGL blob and current gralloc/native handle
contract disagree about backbuffer/genlock state.

## Bootloop: Adreno `libsc-a2xx.so` + backbuffer failure (2026-06-06)

### Symptom 1 — missing shader compiler blob

Initial log:

```text
I/Adreno: os_lib_map error: Cannot load library:
Library '/system/lib/libsc-a2xx.so' not found
F/libc: Fatal signal 11 (SIGSEGV)
```

Fix applied:
- Copied `libsc-a2xx.so` from
  `/home/chijure/Documentos/e400/cm-9-20130929-NIGHTLY-e400/system/lib`.
- Registered it in `device/huawei/y210/proprietary-files.txt`.
- Registered it in `vendor/huawei/y210/y210-vendor-blobs.mk`.

### Symptom 2 — bootloop after adding `libsc-a2xx.so`

After the missing blob was fixed, SurfaceFlinger no longer died at library load.
The next failure moved to EGL/backbuffer allocation:

```text
E/Adreno200-EGLSUB: GetBackBuffer() handle base address is NULL
E/Adreno200-ES20: <gl2_surface_swap:41>: GL_OUT_OF_MEMORY
E/Adreno200-EGL: <qeglDrvAPI_eglSwapBuffers:3339>: EGL_BAD_ALLOC
E/SurfaceFlinger: eglSwapBuffers: EGL error 0x3003 (EGL_BAD_ALLOC)
```

The failure is not simple RAM pressure. It is a buffer-handle contract problem:
Adreno reaches `eglSwapBuffers`, asks for the backbuffer, and receives a gralloc
handle whose base/genlock metadata is not what the blob expects.

### Tests performed

1. **Only add `libsc-a2xx.so`**
   - Result: original missing-library SIGSEGV fixed.
   - New result: repeated `GetBackBuffer`, `GL_OUT_OF_MEMORY`, `EGL_BAD_ALLOC`.

2. **Push e400 `gralloc.msm7x27a.so` + `copybit.msm7x27a.so`**
   - Result: worse. SurfaceFlinger SIGSEGV during init.
   - Key log:
     ```text
     E/libgenlock: invalid gralloc handle
     E/msm7x27a.gralloc: alloc_impl: genlock_create_lock failed
     F/libc: Fatal signal 11
     ```

3. **Push Y210 proprietary `gralloc.msm7k.so`/`copybit.msm7k.so` renamed to msm7x27a**
   - Result: SurfaceFlinger initializes EGL, then dies when swapping.
   - Key log:
     ```text
     E/libgenlock: invalid gralloc handle
     E/Adreno200-EGLSUB: GetBackBuffer() handle base address is NULL
     E/Adreno200-EGL: EGL_BAD_ALLOC
     D/BootAnimation: SurfaceFlinger died, exiting
     ```

4. **Revert to locally built `gralloc.msm7x27a.so`/`copybit.msm7x27a.so`,
   force `debug.sf.hw=0`, disable bootanimation**
   - Result: boot stable.
   - Validation:
     ```text
     sys.boot_completed=1
     init.svc.bootanim=stopped
     no Fatal signal / SurfaceFlinger died / EGL_BAD_ALLOC / GL_OUT_OF_MEMORY
     ```

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

After push of the locally built `gralloc.msm7x27a.so` + `copybit.msm7x27a.so`
and `stop; start`:

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
