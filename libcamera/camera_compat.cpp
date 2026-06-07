/*
 * camera_compat.cpp — GB→ICS stubs for libcamera.y210.so
 */

#include <stdint.h>
#include <string.h>

/* Simple sp-like struct to receive createMemory's hidden return value */
struct SpImem { void* m_ptr; };

/* ICS createMemory signature (hidden return convention) */
typedef void (*CreateMemFn)(SpImem* out, void* self,
                             unsigned offset, unsigned size);

/*
 * y210_mapMemory_impl — C++ helper for our mapMemory replacement.
 * r0 = MemoryHeapPmem* self (CORRECT from GB blob).
 * Returns sp<IMemory>.m_ptr in r0 (GB no-hidden-ret convention).
 */
extern "C" void* y210_mapMemory_impl(void* self)
{
    if (!self) return 0;

    void** vptr = *reinterpret_cast<void***>(self);
    /* vtable[11] = createMemory in ICS libbinder (verified) */
    CreateMemFn fn = reinterpret_cast<CreateMemFn>(vptr[11]);

    /* Read heap size from MemoryHeapBase embedded at start of object.
     * mSize is at offset 20 (from ICS MemoryHeapBase constructor analysis).
     * Fallback to 4 MB if value looks wrong. */
    unsigned size = *reinterpret_cast<unsigned*>(
                        reinterpret_cast<char*>(self) + 20);
    if (size < 4096 || size > 16u * 1024 * 1024)
        size = 4u * 1024 * 1024;

    SpImem result = { 0 };
    fn(&result, self, 0, size);
    return result.m_ptr;
}

/*
 * y210_fixed_mapMemory — vtable[7] replacement for mapMemory.
 * Uses a trampoline in camera_compat.S (ARM assembly) because
 * __attribute__((naked)) + extern "C" + asm is fragile with this toolchain.
 *
 * The .S file calls y210_mapMemory_impl(r0) and returns its result in r0.
 */

/* ISurface::BufferHeap stubs — blob needs them (never executed) */
extern "C" void
_ZN7android8ISurface10BufferHeapC1EjjiiijjRKNS_2spINS_11IMemoryHeapEEE() {}

extern "C" void
_ZN7android8ISurface10BufferHeapD1Ev() {}
