/*
 * libcamera_compat.cpp — Minimal GB→ICS symbol stubs for libcamera.y210.so
 *
 * Loaded with RTLD_GLOBAL before the blob so the dynamic linker finds these
 * symbols when relocating libcamera.y210.so.
 *
 * These code paths are NEVER executed:
 *  - ISurface::BufferHeap: removed in ICS; blob uses it for registerBuffers
 *    which our wrapper never calls (we use CAMERA_MSG_PREVIEW_FRAME callbacks).
 *  - Overlay::*: removed in ICS; blob calls them only when useOverlay()=true,
 *    which our wrapper always returns false.
 *
 * No C++ runtime dependencies — pure C symbol definitions.
 */

/* ISurface::BufferHeap constructor (8 args) */
extern "C" void
_ZN7android8ISurface10BufferHeapC1EjjiiijjRKNS_2spINS_11IMemoryHeapEEE() {}

/* ISurface::BufferHeap destructor */
extern "C" void
_ZN7android8ISurface10BufferHeapD1Ev() {}

/* Overlay::queueBuffer(void*) */
extern "C" int
_ZN7android7Overlay11queueBufferEPv() { return -1; }

/* Overlay::setFd(int) */
extern "C" void
_ZN7android7Overlay5setFdEi() {}

/* Overlay::setCrop(uint32_t, uint32_t, uint32_t, uint32_t) */
extern "C" void
_ZN7android7Overlay7setCropEjjjj() {}
