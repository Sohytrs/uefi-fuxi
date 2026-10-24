"""Extract the DXE drivers that fuxi's own UEFI firmware ships and lay them out as
EDK2 prebuilt modules, so fuxiPkg can link against fuxi's board-correct binaries
instead of the Xiaomi 13 Pro (nuwa) ones.

Input : the raw uefi_a partition image (5 MiB), i.e. what /dev/block/by-name/uefi_a
        holds on the phone.
Output: Binaries/fuxi/... trees mirroring the module paths the platform includes,
        each with <Name>.inf, <Name>.efi and <Name>.depex.

Usage: python extract-fuxi-drivers.py <uefi_a.bin> <uefi-fuxi-repo-root>
"""
import gzip
import os
import re
import struct
import sys
import uuid

GZIP_GUID = uuid.UUID('1d301fe9-be79-4353-91c2-d23bc959ae0c').bytes_le
SECTION_PE32 = 0x10
SECTION_UI = 0x15
FFS_TYPE_DXE_DRIVER = 0x07


def sections(d, start, end):
    """Yield (type, payload) for each section in [start, end)."""
    off = start
    while off + 4 <= end:
        stype = d[off + 3]
        size = d[off] | (d[off + 1] << 8) | (d[off + 2] << 16)
        hdr = 4
        if size == 0xFFFFFF:
            if off + 8 > end:
                break
            size = struct.unpack_from('<I', d, off + 4)[0]
            hdr = 8
        if size < hdr or off + size > end:
            break
        yield stype, d[off + hdr:off + size], off
        off = (off + size + 3) & ~3


def walk_fv(d, base, out):
    """Collect DXE drivers from one firmware volume."""
    sig = d.find(b'_FVH', base)
    if sig < 0:
        return
    hdr_start = sig - 0x28
    fv_len = struct.unpack_from('<Q', d, hdr_start + 0x20)[0]
    hdr_len = struct.unpack_from('<H', d, hdr_start + 0x30)[0]
    off = hdr_start + hdr_len
    end = min(hdr_start + fv_len, len(d))

    while off + 24 <= end:
        name = d[off:off + 16]
        if name == b'\xff' * 16:
            # FFS pad file: its size field is still valid, so step over it.
            pad = d[off + 0x14] | (d[off + 0x15] << 8) | (d[off + 0x16] << 16)
            pad_hdr = 24
            if pad == 0xFFFFFF:
                pad = struct.unpack_from('<Q', d, off + 24)[0]
                pad_hdr = 32
            if pad_hdr <= pad <= end - off:
                off = (off + pad + 7) & ~7
            else:
                off = (off + 8) & ~7
            continue
        if name == b'\x00' * 16:
            break
        size = d[off + 0x14] | (d[off + 0x15] << 8) | (d[off + 0x16] << 16)
        hdr = 24
        if size == 0xFFFFFF:
            size = struct.unpack_from('<Q', d, off + 24)[0]
            hdr = 32
        if size < hdr or off + size > end:
            off += 8
            continue
        ftype = d[off + 0x12]
        guid = str(uuid.UUID(bytes_le=bytes(name)))

        if ftype == FFS_TYPE_DXE_DRIVER:
            ui = None
            pe = None
            for stype, payload, _pos in sections(d, off + hdr, off + size):
                if stype == SECTION_UI and ui is None:
                    try:
                        ui = payload.decode('utf-16-le').rstrip('\x00')
                    except Exception:
                        pass
                elif stype == SECTION_PE32 and pe is None:
                    pe = payload
            if ui and pe and pe[:2] == b'MZ':
                out[ui] = (guid, pe)

        # Recurse into compressed / guided sections looking for more volumes.
        for stype, payload, sec_start in sections(d, off + hdr, off + size):
            if stype == 0x01 and len(payload) > 5 and payload[4] == 0:
                walk_fv(payload[5:], 0, out)
            elif stype == 0x02 and len(payload) > 20:
                guid = payload[:16]
                data_off, _ = struct.unpack_from('<HH', payload, 16)
                if guid == GZIP_GUID:
                    blob = d[sec_start + data_off:off + size]
                    if blob[:2] == b'\x1f\x8b':
                        try:
                            walk_fv(gzip.decompress(blob), 0, out)
                        except Exception:
                            pass

        off = (off + size + 7) & ~7


def keys(name):
    """Every spelling a module might appear under.

    nuwa and fuxi name the same driver differently (HWIODxe vs HWIODxeDriver,
    TLMMDxe vs DALTLMM, PmicDxeLa vs PmicDxe), so compare several trimmed forms.
    """
    out = {name.lower()}
    cur = name.lower()
    changed = True
    while changed:
        changed = False
        for prefix in ('dal', 'qcom'):
            if cur.startswith(prefix) and len(cur) > len(prefix) + 2:
                cur = cur[len(prefix):]
                out.add(cur)
                changed = True
        for suffix in ('driver', 'dxe', 'la', 'dxi'):
            if cur.endswith(suffix) and len(cur) > len(suffix) + 2:
                cur = cur[: -len(suffix)]
                out.add(cur)
                changed = True
    return out


def main():
    image = sys.argv[1]
    repo = sys.argv[2]
    d = open(image, 'rb').read()

    drivers = {}
    walk_fv(d, 0, drivers)
    print('extracted %d DXE drivers from %s' % (len(drivers), image))

    by_norm = {}
    for name, (guid, pe) in drivers.items():
        for key in keys(name):
            by_norm.setdefault(key, (name, guid, pe))

    # Modules the platform actually includes.
    wanted = set()
    inc_dir = os.path.join(repo, 'Platforms', 'Xiaomi', 'fuxiPkg', 'Include')
    for inc in ('DXE.inc', 'APRIORI.inc'):
        path = os.path.join(inc_dir, inc)
        if not os.path.exists(path):
            continue
        for line in open(path, encoding='utf-8', errors='replace'):
            m = re.search(r'Binaries/nuwa/(.+)/([A-Za-z0-9_]+)\.inf', line)
            if m:
                wanted.add((m.group(1), m.group(2)))

    made = 0
    missing = []
    for rel, base in sorted(wanted):
        hit = next((by_norm[k] for k in keys(base) if k in by_norm), None)
        if hit is None:
            missing.append('%s/%s' % (rel, base))
            continue
        src_name, guid, pe = hit
        out_dir = os.path.join(repo, 'Binaries', 'fuxi', rel)
        os.makedirs(out_dir, exist_ok=True)
        with open(os.path.join(out_dir, base + '.efi'), 'wb') as f:
            f.write(pe)
        with open(os.path.join(out_dir, base + '.depex'), 'wb') as f:
            f.write(b'\x08\x00')
        inf = (
            '[Defines]\n'
            '  INF_VERSION                    = 0x0001001B\n'
            '  BASE_NAME                      = %s\n'
            '  FILE_GUID                      = %s\n'
            '  MODULE_TYPE                    = DXE_DRIVER\n'
            '  VERSION_STRING                 = 1.0\n'
            '  ENTRY_POINT                    = EfiEntry\n'
            '\n'
            '[Binaries.AARCH64]\n'
            '  DXE_DEPEX|%s.depex|*\n'
            '  PE32|%s.efi|*\n'
            '\n'
            '[Depex]\n'
            '  TRUE\n'
        ) % (base, guid.upper(), base, base)
        with open(os.path.join(out_dir, base + '.inf'), 'w', encoding='utf-8', newline='\n') as f:
            f.write(inf)
        made += 1

    print('wrote %d modules under Binaries/fuxi' % made)
    if missing:
        print('not present in the dump (%d):' % len(missing))
        for m in missing:
            print('  ' + m)


if __name__ == '__main__':
    main()
