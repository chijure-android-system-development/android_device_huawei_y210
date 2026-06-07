#!/usr/bin/env python3
"""
patch_elf_needed.py - Add DT_NEEDED entry to a 32-bit ARM ELF shared library.

Works without patchelf by finding spare space in .dynstr (trailing nulls)
and free DT_NULL slots in .dynamic.
"""

import struct
import sys
import os

def u32le(data, off):
    return struct.unpack_from('<I', data, off)[0]

def p32le(val):
    return struct.pack('<I', val)

def patch_needed(src, dst, lib_name):
    data = bytearray(open(src, 'rb').read())
    lib_bytes = lib_name.encode() + b'\x00'

    # --- Parse ELF header ---
    assert data[:4] == b'\x7fELF', "Not an ELF file"
    e_shoff  = u32le(data, 0x20)
    e_shentsize = struct.unpack_from('<H', data, 0x2e)[0]
    e_shnum  = struct.unpack_from('<H', data, 0x30)[0]
    e_shstrndx = struct.unpack_from('<H', data, 0x32)[0]

    # --- Read section headers ---
    sections = []
    for i in range(e_shnum):
        base = e_shoff + i * e_shentsize
        sh = {
            'name_idx': u32le(data, base + 0x00),
            'type':     u32le(data, base + 0x04),
            'flags':    u32le(data, base + 0x08),
            'addr':     u32le(data, base + 0x0c),
            'offset':   u32le(data, base + 0x10),
            'size':     u32le(data, base + 0x14),
            'base':     base,
        }
        sections.append(sh)

    # Get section name strings
    shstr_sec = sections[e_shstrndx]
    shstr_off = shstr_sec['offset']

    def secname(sh):
        off = shstr_off + sh['name_idx']
        end = data.index(b'\x00', off)
        return data[off:end].decode()

    dynstr = next(s for s in sections if secname(s) == '.dynstr')
    dynamic = next(s for s in sections if secname(s) == '.dynamic')

    ds_off  = dynstr['offset']
    ds_size = dynstr['size']
    dyn_off = dynamic['offset']
    dyn_sz  = dynamic['size']

    # --- Check for trailing nulls in .dynstr ---
    print(f".dynstr: file offset=0x{ds_off:x}, size=0x{ds_size:x}")
    # Count trailing nulls
    end = ds_off + ds_size
    trailing = 0
    while trailing < ds_size and data[end - 1 - trailing] == 0:
        trailing += 1
    print(f"  trailing null bytes: {trailing}")

    # Try to fit lib_name in trailing space
    if trailing >= len(lib_bytes):
        # Use the LAST (len(lib_bytes)) bytes of .dynstr for our string
        str_offset_in_dynstr = ds_size - len(lib_bytes)
        str_file_off = ds_off + str_offset_in_dynstr
        # Verify it's all zeros
        if data[str_file_off : str_file_off + len(lib_bytes)] == b'\x00' * len(lib_bytes):
            print(f"  Using trailing nulls at dynstr+0x{str_offset_in_dynstr:x}")
        else:
            print("  WARNING: trailing bytes not zero, using anyway (overwrite)")
    else:
        # Not enough trailing nulls — look for zeros anywhere in the mapping near .dynstr
        # Strategy: scan .dynstr for a run of zeros long enough
        found_off = -1
        for i in range(ds_size - len(lib_bytes), -1, -1):
            region = data[ds_off + i : ds_off + i + len(lib_bytes)]
            if region == b'\x00' * len(lib_bytes):
                found_off = i
                break
        if found_off >= 0:
            str_offset_in_dynstr = found_off
            str_file_off = ds_off + found_off
            print(f"  Found {len(lib_bytes)}-byte zero run at dynstr+0x{found_off:x}")
        else:
            print(f"ERROR: no room for '{lib_name}' in .dynstr")
            print("       Need at least {len(lib_bytes)} trailing null bytes.")
            print("       Run 'readelf -d' to see the blob's .dynamic content,")
            print("       then use --force-data-append if you want to extend the file.")
            sys.exit(1)

    # --- Find DT_STRTAB address (to compute DT_NEEDED value) ---
    # DT_NEEDED value = offset of lib name FROM DT_STRTAB address
    DT_STRTAB = 5
    DT_NEEDED = 1
    DT_NULL   = 0

    strtab_addr = None
    entry_count = dyn_sz // 8
    for i in range(entry_count):
        tag = u32le(data, dyn_off + i * 8)
        val = u32le(data, dyn_off + i * 8 + 4)
        if tag == DT_STRTAB:
            strtab_addr = val
            break

    assert strtab_addr is not None, "DT_STRTAB not found in .dynamic"
    dynstr_addr = dynstr['addr']
    print(f"  DT_STRTAB=0x{strtab_addr:x}, .dynstr addr=0x{dynstr_addr:x} (delta={strtab_addr - dynstr_addr})")

    # DT_NEEDED value = (dynstr_addr - strtab_addr) + offset_in_dynstr
    # (usually strtab_addr == dynstr_addr, so just offset_in_dynstr)
    needed_val = (dynstr_addr - strtab_addr) + str_offset_in_dynstr

    # --- Find last DT_NULL in .dynamic and use the slot before it ---
    null_idx = None
    for i in range(entry_count):
        tag = u32le(data, dyn_off + i * 8)
        if tag == DT_NULL:
            null_idx = i
            break  # first DT_NULL is the end

    assert null_idx is not None, "DT_NULL not found in .dynamic"
    print(f".dynamic: {entry_count} slots, DT_NULL at index {null_idx}")

    # Check we have a free slot after DT_NULL
    if null_idx + 1 >= entry_count:
        print("ERROR: no free slot after DT_NULL in .dynamic")
        sys.exit(1)

    free_slot = null_idx  # we'll insert before DT_NULL, shifting DT_NULL one forward
    # Check that free_slot+1 is also DT_NULL (padding)
    next_tag = u32le(data, dyn_off + (null_idx + 1) * 8)
    if next_tag != DT_NULL:
        print(f"ERROR: slot after DT_NULL contains tag 0x{next_tag:x}, expected DT_NULL")
        sys.exit(1)

    # --- Check if already patched ---
    for i in range(entry_count):
        tag = u32le(data, dyn_off + i * 8)
        val = u32le(data, dyn_off + i * 8 + 4)
        if tag == DT_NEEDED and val == needed_val:
            print(f"Already patched (DT_NEEDED 0x{needed_val:x} exists)")
            import shutil; shutil.copy(src, dst)
            return

    # --- Apply patch ---
    # 1. Write the library name into .dynstr
    data[str_file_off : str_file_off + len(lib_bytes)] = lib_bytes

    # 2. Insert DT_NEEDED at free_slot, move DT_NULL to free_slot+1
    slot_off = dyn_off + free_slot * 8
    data[slot_off : slot_off + 4] = p32le(DT_NEEDED)
    data[slot_off + 4 : slot_off + 8] = p32le(needed_val)
    # slot_off+8 already has DT_NULL (was the original DT_NULL)

    # --- Verify the dynstr entry (sanity) ---
    found_name = bytes(data[str_file_off : str_file_off + len(lib_bytes)])
    assert found_name == lib_bytes, f"Verification failed: {found_name!r}"

    open(dst, 'wb').write(data)
    print(f"Patched: {src} -> {dst}")
    print(f"  Added DT_NEEDED '{lib_name}' (dynstr offset=0x{str_offset_in_dynstr:x}, val=0x{needed_val:x})")

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} input.so output.so libname.so")
        sys.exit(1)
    patch_needed(sys.argv[1], sys.argv[2], sys.argv[3])
