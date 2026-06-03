#!/usr/bin/env python3
"""
elf_add_needed.py — Add a DT_NEEDED entry to an ARM32 ELF shared library.

Two strategies for extending the string table:
  1. Direct: if there are zero-padding bytes immediately after .dynstr, use them.
  2. Distant: find any zero run in the first LOAD segment, write the string there,
     and use a large DT_NEEDED offset from DT_STRTAB (the dynamic linker only reads
     the specific offset; it does not scan the whole range).
"""

import struct, sys, os, shutil

DT_NULL = 0; DT_NEEDED = 1; DT_STRTAB = 5; DT_STRSZ = 10
PT_LOAD = 1; PT_DYNAMIC = 2


def parse(data):
    if data[:4] != b'\x7fELF': raise ValueError("Not ELF")
    if data[4] != 1: raise ValueError("Not 32-bit")
    if data[5] != 1: raise ValueError("Not little-endian")

    e_phoff     = struct.unpack_from('<I', data, 28)[0]
    e_phentsize = struct.unpack_from('<H', data, 42)[0]
    e_phnum     = struct.unpack_from('<H', data, 44)[0]

    load_segs, dyn = [], None
    for i in range(e_phnum):
        b = e_phoff + i * e_phentsize
        pt = struct.unpack_from('<I', data, b)[0]
        po, pv, pf = (struct.unpack_from('<I', data, b+o)[0] for o in (4,8,16))
        if pt == PT_LOAD:    load_segs.append((pv, po, pf))
        elif pt == PT_DYNAMIC: dyn = (po, pf, b)   # (file_off, size, ph_base)

    if dyn is None: raise ValueError("No PT_DYNAMIC")

    def va2off(va):
        for sv, so, sf in load_segs:
            if sv <= va < sv+sf: return so + (va - sv)
        return None

    dyn_off, dyn_size, dyn_ph = dyn
    n = dyn_size // 8
    strtab_va = strsz = None
    nulls, neededs = [], []
    for i in range(n):
        b = dyn_off + i*8
        tag, val = struct.unpack_from('<II', data, b)
        if tag == DT_STRTAB: strtab_va = val
        elif tag == DT_STRSZ: strsz = val
        elif tag == DT_NULL:  nulls.append(b)
        elif tag == DT_NEEDED: neededs.append((b, val))

    if strtab_va is None or strsz is None: raise ValueError("DT_STRTAB/DT_STRSZ missing")
    if not nulls: raise ValueError("No DT_NULL to replace")

    strtab_off = va2off(strtab_va)
    return dict(load_segs=load_segs, dyn_off=dyn_off, dyn_size=dyn_size,
                dyn_ph=dyn_ph, strtab_va=strtab_va, strtab_off=strtab_off,
                strsz=strsz, nulls=nulls, neededs=neededs, va2off=va2off)


def patch(filename, lib_to_add, dry_run=False):
    with open(filename, 'rb') as f:
        data = bytearray(f.read())

    p = parse(data)
    strtab_off = p['strtab_off']
    strsz      = p['strsz']
    neededs    = p['neededs']
    nulls      = p['nulls']
    dyn_off    = p['dyn_off']
    va2off     = p['va2off']

    # Check if already present
    for _, idx in neededs:
        ns = strtab_off + idx
        ne = data.index(0, ns)
        if data[ns:ne].decode('ascii') == lib_to_add:
            print(f"[INFO] '{lib_to_add}' already in NEEDED — nothing to do.")
            return False

    new_str = lib_to_add.encode('ascii') + b'\x00'
    needed_len = len(new_str)   # 16 for "libos_compat.so\0"

    # ── Strategy 1: zero-padding immediately after .dynstr ─────────────────
    after_strtab = strtab_off + strsz
    if all(data[after_strtab + j] == 0 for j in range(needed_len)):
        str_file_off = after_strtab
        str_va_off   = strsz          # offset from DT_STRTAB base
        new_strsz    = strsz + needed_len
        strategy     = "direct (padding after dynstr)"
    else:
        # ── Strategy 2: find a zero run in the first LOAD segment ───────────
        # Only use runs that are inside a LOAD seg so they have a valid vaddr.
        found = None
        for seg_va, seg_off, seg_fsz in p['load_segs']:
            # Scan within this segment for a sufficient zero run.
            # Skip the strtab range itself to avoid corrupting existing strings.
            seg_end = seg_off + seg_fsz
            strtab_range = (strtab_off, strtab_off + strsz)
            i = seg_off
            while i + needed_len <= seg_end:
                # Skip over the strtab itself
                if strtab_range[0] <= i < strtab_range[1]:
                    i = strtab_range[1]
                    continue
                if all(data[i + j] == 0 for j in range(needed_len)):
                    candidate_vaddr = seg_va + (i - seg_off)
                    if candidate_vaddr >= p['strtab_va']:  # ensure non-negative offset
                        found = (i, candidate_vaddr)
                        break
                # advance past non-zero byte
                nxt = next((k for k in range(i, min(i + needed_len, seg_end)) if data[k] != 0), None)
                i = (nxt + 1) if nxt is not None else (i + needed_len)
            if found:
                break

        if found is None:
            # Strategy 2b: extend first LOAD segment to cover gap after it.
            # The gap between LOAD segments is typically zero-padded.
            # Extend p_filesz/p_memsz by needed_len bytes.
            if p['load_segs']:
                # Find the LOAD with the smallest end vaddr (first LOAD)
                sv0, so0, sf0 = min(p['load_segs'], key=lambda x: x[0])
                gap_file_off  = so0 + sf0
                gap_vaddr     = sv0 + sf0
                gap_avail     = needed_len
                # Check the gap bytes are zero in the file
                if (gap_file_off + needed_len <= len(data) and
                        all(data[gap_file_off + j] == 0 for j in range(needed_len))):
                    # Find program header of this LOAD and extend it
                    e_phoff2     = struct.unpack_from('<I', data, 28)[0]
                    e_phentsize2 = struct.unpack_from('<H', data, 42)[0]
                    e_phnum2     = struct.unpack_from('<H', data, 44)[0]
                    for i in range(e_phnum2):
                        b = e_phoff2 + i * e_phentsize2
                        if (struct.unpack_from('<I', data, b)[0] == PT_LOAD and
                                struct.unpack_from('<I', data, b+8)[0] == sv0):
                            old_fs = struct.unpack_from('<I', data, b+16)[0]
                            old_ms = struct.unpack_from('<I', data, b+20)[0]
                            struct.pack_into('<I', data, b+16, old_fs + needed_len)
                            struct.pack_into('<I', data, b+20, old_ms + needed_len)
                            break
                    found = (gap_file_off, gap_vaddr)
                    strategy_suffix = "LOAD-extended gap"
                else:
                    raise ValueError(f"Cannot find {needed_len} zero bytes in any LOAD segment")
            else:
                raise ValueError(f"Cannot find {needed_len} zero bytes in any LOAD segment")

        str_file_off, str_vaddr = found
        strtab_va    = p['strtab_va']
        str_va_off   = str_vaddr - strtab_va   # may be large, that's fine
        # DT_STRSZ must cover at least up to the new string
        new_strsz    = max(strsz, str_va_off + needed_len)
        sfx = locals().get('strategy_suffix', f"file 0x{str_file_off:x}, va-off 0x{str_va_off:x}")
        strategy     = f"distant ({sfx})"

    if dry_run:
        print(f"[DRY-RUN] Strategy: {strategy}")
        print(f"  Write '{lib_to_add}' at file 0x{str_file_off:x}")
        print(f"  DT_NEEDED offset = 0x{str_va_off:x}")
        print(f"  DT_STRSZ: {strsz} → {new_strsz}")
        print(f"  Replace DT_NULL @ file 0x{nulls[0]:x} with DT_NEEDED")
        return True

    # ── Apply ───────────────────────────────────────────────────────────────
    # 1. Write string
    for j, b in enumerate(new_str):
        data[str_file_off + j] = b

    # 2. Update DT_STRSZ
    n = p['dyn_size'] // 8
    for i in range(n):
        b = dyn_off + i*8
        if struct.unpack_from('<I', data, b)[0] == DT_STRSZ:
            struct.pack_into('<I', data, b + 4, new_strsz)
            break

    # 3. Replace first DT_NULL with DT_NEEDED
    fn = nulls[0]
    struct.pack_into('<I', data, fn,     DT_NEEDED)
    struct.pack_into('<I', data, fn + 4, str_va_off)

    with open(filename, 'wb') as f:
        f.write(data)

    print(f"[OK] Added DT_NEEDED '{lib_to_add}' — {strategy}")
    return True


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <elf> <lib_to_add> [--dry-run]")
        sys.exit(1)
    elf, lib, dry = sys.argv[1], sys.argv[2], '--dry-run' in sys.argv
    if not dry and not os.path.exists(elf + '.orig'):
        shutil.copy2(elf, elf + '.orig')
        print(f"[INFO] Backup → {elf}.orig")
    try:
        patch(elf, lib, dry_run=dry)
    except Exception as e:
        print(f"[ERROR] {e}", file=sys.stderr); sys.exit(1)
