#!/usr/bin/env python3
"""Read-only ELF64/AArch64 compatibility audit for the vpl_nx hook offsets.

Does NOT load, execute, alter or upload the commercial libMyGame.so.
No external Python modules or Linux readelf required (works on Windows).
"""
import argparse
import bisect
import csv
import pathlib
import struct
import sys

# Original address constants from delsonazevedo/vpl_nx source/main.c.
# Valid for its exact game build only, NOT necessarily other Android APKs.
HOOKS = {
    "CR_CHECKNATIVECALLBACK": 0x3F6E18,
    "MV_ISPREPARING": 0x30FF9C,
    "MV_ISPLAY": 0x30FF30,
    "MV_ISSTOP": 0x310010,
    "MV_PREPARE": 0x30FD80,
    "MV_PLAY": 0x30FDEC,
    "LVLCHECK": 0x309538,
    "GETRESULTCODE": 0x30B868,
    "ISCONNECTEND": 0x30B854,
    "EXEPURCHASEPROC": 0x310998,
    "SETOBBMOUNT": 0x307CD8,
    "INITFIELDDATA": 0x466AC8,
    "LOADIMAGE2": 0x41A654,
    "CAMPCHARFIELDDATAREAD": 0x4340A0,
    "ALLOCANDDECODEN": 0x422B4C,
    "CRIATOMEXPLAYER_SETDATA": 0x691500,
    "OPENREADMALLOC": 0x41E568,
    "GETPADSTATUS": 0x40E4D0,
    "GETPADSTOCKSTATUS": 0x40E4E4,
    "GETPADSINGLESTATUS": 0x40E4F8,
    "GETPADREPEATSTATUS": 0x40E50C,
    "GETANALOGPOSITION": 0x40E2B8,
    "CJOYPAD_ISPUSH": 0x306F80,
}
EXPECTED_OWNERS = {
    "CR_CHECKNATIVECALLBACK": ("checkNativeCallBack",),
    "MV_ISPREPARING": ("CRIMovieManager",),
    "MV_ISPLAY": ("CRIMovieManager",),
    "MV_ISSTOP": ("CRIMovieManager",),
    "MV_PREPARE": ("CRIMovieManager",),
    "MV_PLAY": ("CRIMovieManager",),
    "INITFIELDDATA": ("InitFieldData",),
}
REQUIRED_JNI = (
    "JNI_OnLoad",
    "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit",
    "Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeRender",
    "Java_org_cocos2dx_lib_Cocos2dxHelper_nativeSetContext",
)

def audit(path):
    if path.stat().st_size > 256 * 1024 * 1024:
        raise ValueError("SO_UNUSUALLY_LARGE")
    data = path.read_bytes()
    if len(data) < 64 or data[:4] != b"\x7fELF":
        raise ValueError("NOT_ELF")
    if data[4] != 2 or data[5] != 1:
        raise ValueError("REQUIRES_LITTLE_ENDIAN_ELF64")
    h = struct.unpack_from("<HHIQQQIHHHHHH", data, 16)
    if h[0] != 3 or h[1] != 183:
        raise ValueError("REQUIRES_AARCH64_SHARED_OBJECT")
    phoff, shoff = h[4], h[5]
    phsize, phnum, shsize, shnum = h[8], h[9], h[10], h[11]
    if phsize < 56 or shsize < 64 or phnum > 4096 or shnum > 65535:
        raise ValueError("BAD_ELF_TABLE_SIZES")
    if phoff + phsize * phnum > len(data) or shoff + shsize * shnum > len(data):
        raise ValueError("ELF_TABLE_OUT_OF_RANGE")
    executable = []
    for i in range(phnum):
        p = struct.unpack_from("<IIQQQQQQ", data, phoff + i * phsize)
        if p[0] == 1 and (p[1] & 1):
            executable.append((p[3], p[3] + p[6]))
    sections = [
        struct.unpack_from("<IIQQQQIIQQ", data, shoff + i * shsize)
        for i in range(shnum)
    ]
    funcs = []
    for sh in sections:
        if sh[1] != 11:
            continue
        off, size, link, entry = sh[4], sh[5], sh[6], sh[9]
        if link >= shnum or entry < 24 or size % entry:
            raise ValueError("INVALID_DYNSYM")
        strings = sections[link]
        st_off, st_size = strings[4], strings[5]
        if off + size > len(data) or st_off + st_size > len(data):
            raise ValueError("DYNSYM_OUT_OF_RANGE")
        st = data[st_off:st_off + st_size]
        for j in range(size // entry):
            noff, info, other, index, value, length = struct.unpack_from(
                "<IBBHQQ", data, off + j * entry
            )
            if noff >= len(st) or not index or (info & 15) != 2:
                continue
            terminator = st.find(b"\0", noff)
            if terminator < 0:
                continue
            name = st[noff:terminator].decode("utf8", "replace")
            if name:
                funcs.append((value, length, name))
    funcs.sort()
    starts = [value for value, length, name in funcs]
    exported = {name for value, length, name in funcs}
    rows = []
    collisions = 0
    for label, address in HOOKS.items():
        executable_addr = any(lo <= address < hi for lo, hi in executable)
        j = bisect.bisect_right(starts, address) - 1
        prev = funcs[j] if j >= 0 else (0, 0, "")
        base, length, name = prev
        covered = bool(length and base <= address < base + length)
        actual = name if covered else "NO_EXPORTED_SYMBOL_COVERAGE"
        if not executable_addr:
            status = "OUTSIDE_EXECUTABLE"
            collisions += 1
        elif covered and label in EXPECTED_OWNERS and not any(
            marker.lower() in name.lower() for marker in EXPECTED_OWNERS[label]
        ):
            status = "COLLIDES_WITH_UNRELATED_SYMBOL"
            collisions += 1
        else:
            status = "UNVERIFIED_NO_SIGNATURE_MATCH"
        rows.append((label, f"0x{address:x}", status, actual,
                     f"0x{base:x}" if covered else ""))
    missing = sorted(set(REQUIRED_JNI) - exported)
    return rows, missing, collisions, len(funcs)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("library", type=pathlib.Path, help="local private libMyGame.so")
    ap.add_argument("--report", type=pathlib.Path, help="sanitized TSV output")
    args = ap.parse_args()
    try:
        rows, missing, collisions, count = audit(args.library)
    except (ValueError, OSError, struct.error) as e:
        print("ELF_AUDIT=ERROR detail=" + str(e), file=sys.stderr)
        return 2
    print(f"ELF_AUDIT=PASS symbols={count} collisions={collisions} missing_jni={len(missing)}")
    print("GATE=" + ("NOT_COMPATIBLE_WITH_HARDCODED_HOOKS" if collisions else "UNVERIFIED"))
    print("COMMERCIAL_CODE_EXECUTED=NO")
    for row in rows:
        print("\t".join(row))
    if missing:
        print("MISSING_JNI=" + ",".join(missing))
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        with args.report.open("w", encoding="utf-8", newline="") as out:
            writer = csv.writer(out, delimiter="\t")
            writer.writerow(("hook", "upstream_offset", "status", "actual_owner", "actual_owner_offset"))
            writer.writerows(rows)
            writer.writerow(("FINAL", "", "NOT_EXECUTED", "", ""))
    return 0

if __name__ == "__main__":
    sys.exit(main())
