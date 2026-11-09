#!/usr/bin/env python3
"""Phase39: read-only AArch64 ELF dependency gap report, no ELF execution.

Use locally with your own libMyGame.so and libc++_shared.so. This tool
only reads ELF headers, SHT_DYNSYM and DT_NEEDED/DT_SONAME metadata.
It never loads, modifies or uploads private libraries. A matching symbol
is a *candidate*, not proof of ABI compatibility or relocation readiness.
External symbols without a matching supplied ELF are NOT automatically
considered missing from Android or the host.
"""
import argparse
import csv
import io
import os
from pathlib import Path
import re
import struct
import sys
import tempfile

MAX_BYTES = 128 * 1024 * 1024
MAX_SECTIONS = 4096
MAX_SYMBOLS = 400000
MAX_MODULES = 8
MAX_OUTPUT = 100000
SH_DYNSYM = 11
SH_DYNAMIC = 6
SH_STRTAB = 3
ET_DYN = 3
EM_AARCH64 = 183

class AuditError(ValueError):
    pass

def bounded(off, length, total):
    return off >= 0 and length >= 0 and off <= total and length <= total - off

def cstr(buf, pos, maximum=1024):
    if pos < 0 or pos >= len(buf):
        raise AuditError("invalid string-table index")
    end = buf.find(b"\0", pos, min(len(buf), pos + maximum + 1))
    if end < 0:
        raise AuditError("oversized or unterminated ELF name")
    name = buf[pos:end].decode("utf-8", "replace")
    if not name or any(ord(c) < 33 or ord(c) == 127 for c in name):
        raise AuditError("unsafe or empty ELF symbol/name")
    return name

def safe_token(value):
    return value.replace("\\", "\\\\").replace("\t", "\\t").replace(
        "\r", "\\r").replace("\n", "\\n")

def elf_info(filename):
    p = Path(filename)
    if not p.is_file() or p.stat().st_size > MAX_BYTES:
        raise AuditError("file absent or larger than audit limit")
    raw = p.read_bytes()
    if len(raw) < 64 or raw[:4] != b"\x7fELF" or raw[4:6] != b"\x02\x01":
        raise AuditError("not a little-endian ELF64")
    h = struct.unpack_from("<HHIQQQIHHHHHH", raw, 16)
    if h[0] != ET_DYN or h[1] != EM_AARCH64 or h[7] != 64:
        raise AuditError("not an ARM64 shared ELF")
    shoff, shentsize, shcount = h[5], h[10], h[11]
    if shentsize != 64 or not 0 < shcount <= MAX_SECTIONS or not bounded(
            shoff, shcount * shentsize, len(raw)):
        raise AuditError("invalid section header table")
    sections = []
    for i in range(shcount):
        s = struct.unpack_from("<IIQQQQIIQQ", raw, shoff + 64 * i)
        if s[1] != 8 and not bounded(s[4], s[5], len(raw)):
            raise AuditError("section outside ELF file")
        sections.append(s)
    def linked_strings(sec):
        link = sec[6]
        if link >= len(sections):
            raise AuditError("section has invalid string-table link")
        target = sections[link]
        if target[1] != SH_STRTAB or not bounded(
                target[4], target[5], len(raw)):
            raise AuditError("linked ELF string table is invalid")
        return raw[target[4]:target[4]+target[5]]

    imports, exports, needed, soname = {}, set(), [], None
    dynsym_found = False
    dynamic_found = False
    for s in sections:
        if s[1] != SH_DYNSYM:
            continue
        if dynsym_found or s[9] != 24 or s[5] % 24 or s[5]//24 > MAX_SYMBOLS:
            raise AuditError("invalid/multiple ELF dynamic symbol tables")
        dynsym_found = True
        names = linked_strings(s)
        for off in range(s[4], s[4]+s[5], 24):
            nameidx, info, other, section_index, value, sz = struct.unpack_from(
                "<IBBHQQ", raw, off)
            bind, symbol_type = info >> 4, info & 0xf
            if not nameidx or bind not in (1, 2) or symbol_type not in (0, 1, 2, 10):
                continue
            name = cstr(names, nameidx)
            if section_index == 0:
                if name in imports and imports[name] != (bind == 2):
                    # A single undefined binding cannot be both weak and strong.
                    raise AuditError("inconsistent duplicate dynamic symbol")
                imports[name] = bind == 2
            elif section_index != 0xfff2 and (other & 3) != 2:
                exports.add(name)
    if not dynsym_found:
        raise AuditError("ELF lacks SHT_DYNSYM; cannot establish coverage")
    for s in sections:
        if s[1] != SH_DYNAMIC:
            continue
        if dynamic_found or s[9] != 16 or s[5] % 16:
            raise AuditError("invalid ELF dynamic table")
        dynamic_found = True
        names = linked_strings(s)
        for off in range(s[4], s[4]+s[5], 16):
            tag, val = struct.unpack_from("<qQ", raw, off)
            if tag == 1:
                needed.append(cstr(names, val))
            elif tag == 14:
                if soname is not None:
                    raise AuditError("duplicate DT_SONAME")
                soname = cstr(names, val)
    if not dynamic_found:
        raise AuditError("ELF has no dynamic link metadata")
    return {"file": p.name, "soname": soname or p.name,
            "needed": sorted(set(needed)), "imports": imports,
            "exports": exports}

def scan(paths):
    if len(paths) < 1 or len(paths) > MAX_MODULES:
        raise AuditError("supply 1 to 8 shared libraries")
    modules = [elf_info(p) for p in paths]
    if len(set(m["soname"] for m in modules)) != len(modules):
        raise AuditError("duplicate SONAME/provider identity")
    result = []
    for module in modules:
        for needed in module["needed"]:
            included = any(m["soname"] == needed for m in modules)
            result.append((module["file"], "DT_NEEDED",
                           "SUPPLIED_LIBRARY" if included else "EXTERNAL_NOT_VERIFIED",
                           needed, "name_only_not_loader_proof"))
        for sym, weak in sorted(module["imports"].items()):
            providers = [m for m in modules
                         if m is not module and sym in m["exports"]]
            if weak:
                status, detail = "WEAK_OPTIONAL", "undefined_weak_symbol"
            elif len(providers) == 1:
                status = "CROSS_ELF_CANDIDATE"
                detail = providers[0]["soname"]
                if detail not in module["needed"]:
                    detail += "_not_direct_DT_NEEDED"
            elif len(providers) > 1:
                status, detail = "AMBIGUOUS_PROVIDER", "multiple_supplied_ELFs"
            else:
                status, detail = "PROVIDER_NOT_ESTABLISHED", "may_be_Android_Bionic_NDK_or_missing"
            result.append((module["file"], "IMPORT", status, sym, detail))
        result.append((module["file"], "SUMMARY", "INVENTORIED",
                       "imports=%d exports=%d needed=%d" % (
                           len(module["imports"]), len(module["exports"]), len(module["needed"])),
                       "ELF_metadata_only"))
    result.extend([
        ("runtime", "RELOCATION_AND_SYMBOL_VERSIONS", "UNVERIFIED",
         "version_binding_and_relocations", "symbol_name_match_not_ABI_proof"),
        ("runtime", "BIONIC_HOST_PROVIDER", "UNVERIFIED",
         "Android_glibc_compatibility", "needs_real_import_and_layout_validation"),
        ("runtime", "JNI_ART_COCOS2D", "BLOCKED",
         "Java_VM_and_real_Cocos_lifecycle", "synthetic_JNI_tests_only"),
        ("runtime", "COMMERCIAL_ENGINE", "NOT_EXECUTED",
         "libMyGame.so", "read_only_offline_ELF_scan")
    ])
    if len(result) > MAX_OUTPUT:
        raise AuditError("audit result too large")
    return result, modules

def report(output, rows):
    with open(output, "w", encoding="utf-8", newline="\n") as f:
        writer = csv.writer(f, delimiter="\t", lineterminator="\n")
        writer.writerow(("module","gate","status","symbol_or_dependency","detail"))
        for item in rows:
            writer.writerow([safe_token(x) for x in item])

def test_fixture(rows, modules):
    if len(modules) != 2:
        raise AuditError("test requires exactly two fixtures")
    consumer = next((m for m in modules if m["soname"] ==
                     "libphase39consumer.so"), None)
    provider = next((m for m in modules if m["soname"] ==
                     "libphase39provider.so"), None)
    if consumer is None or provider is None:
        raise AuditError("fixture SONAMEs not present")
    def has(gate, status, value):
        return any(r[0] == consumer["file"] and r[1] == gate and
                   r[2] == status and r[3] == value for r in rows)
    assert has("DT_NEEDED","SUPPLIED_LIBRARY","libphase39provider.so")
    assert has("IMPORT","CROSS_ELF_CANDIDATE","phase39_shared_api")
    assert has("IMPORT","PROVIDER_NOT_ESTABLISHED","phase39_missing_api")
    assert has("IMPORT","WEAK_OPTIONAL","phase39_weak_api")
    # Do not treat a supplied library as evidence that external imports work.
    assert any(r[1] == "RELOCATION_AND_SYMBOL_VERSIONS" and
               r[2] == "UNVERIFIED" for r in rows)
    with tempfile.TemporaryDirectory() as d:
        bad = Path(d) / "bad.so"
        bad.write_bytes(b"\x7fELF" + b"\x00"*60)
        try:
            elf_info(bad)
            raise AssertionError("invalid ELF accepted")
        except AuditError:
            pass
        bad.write_bytes(Path(next(m["file"] for m in [] )).read_bytes() if False else b"\x00")
        try:
            elf_info(bad)
            raise AssertionError("truncated ELF accepted")
        except AuditError:
            pass
    print("PHASE39_ELF_DEPENDENCIES=PASS cross_elf=PASS "
          "missing_provider=REPORTED weak_optional=PASS "
          "DT_NEEDED=PASS malformed=REJECTED "
          "private_libraries=NOT_PROVIDED game_executed=NO")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--modules", nargs="+", required=True,
                   help="local private SO paths (no upload)")
    p.add_argument("--output", default="lenneth-elf-dependencies39.tsv")
    p.add_argument("--fixture-check", action="store_true",
                   help="assert only against synthetic standalone Phase39 modules")
    args=p.parse_args()
    try:
        rows, modules = scan(args.modules)
        report(args.output, rows)
        if args.fixture_check:
            test_fixture(rows, modules)
        else:
            counts={}
            for row in rows:
                if row[1] == "IMPORT":
                    counts[row[2]]=counts.get(row[2],0)+1
            print("PHASE39_ELF_DEPENDENCIES=PASS modules=%d imports=%d "
                  "cross_candidates=%d provider_not_established=%d "
                  "output=%s uploaded=NO game_executed=NO" %
                  (len(modules),sum(counts.values()),
                   counts.get("CROSS_ELF_CANDIDATE",0),
                   counts.get("PROVIDER_NOT_ESTABLISHED",0),args.output))
    except (OSError, AuditError, struct.error, AssertionError, UnicodeError) as exc:
        print("PHASE39_ELF_DEPENDENCIES=FAIL %s" % exc,file=sys.stderr)
        return 39
    return 0

if __name__ == "__main__":
    sys.exit(main())
