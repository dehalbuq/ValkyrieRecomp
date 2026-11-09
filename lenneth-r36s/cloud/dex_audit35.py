#!/usr/bin/env python3
"""Phase35 offline APK/DEX JNI signature audit (stdlib only).

Reads classes*.dex from a local APK without extracting game content.
Reports metadata, not executable code/assets. NEVER uploads data.
Only class_data declarations count as Java method declarations; a method_id
reference alone is NOT evidence that the target method is implemented.
"""
import argparse
import hashlib
import io
import os
import re
import struct
import sys
import tempfile
import zipfile
import zlib
from pathlib import Path

MAX_DEX = 64 * 1024 * 1024
MAX_STRING_UTF8 = 1024 * 1024  # A string literal is not a JNI identifier.
MAX_TOTAL = 128 * 1024 * 1024
MAX_DEX_FILES = 16
MAX_ROWS = 5000
TARGET_METHODS = {
    "getCocos2dxWritablePath", "getCurrentLanguage", "getSystemService",
    "getClassLoader", "getAssets", "loadClass", "nativeInit",
    "nativeRender", "nativeSetContext", "nativeSetAudioDeviceInfo"
}
CONTRACTS = [
    ("Lorg/cocos2dx/lib/Cocos2dxHelper;", "getCocos2dxWritablePath", "()Ljava/lang/String;"),
    ("Lorg/cocos2dx/lib/Cocos2dxHelper;", "getCurrentLanguage", "()Ljava/lang/String;"),
    ("Landroid/content/Context;", "getAssets", "()Landroid/content/res/AssetManager;"),
    ("Landroid/content/Context;", "getClassLoader", "()Ljava/lang/ClassLoader;"),
    ("Landroid/content/Context;", "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;"),
    ("Ljava/lang/ClassLoader;", "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;"),
]
DEX_NAME = re.compile(r"classes(?:[2-9]|[1-9][0-9]+)?\.dex\Z")

class DexError(ValueError):
    pass

def u32(data, pos):
    if pos < 0 or pos + 4 > len(data):
        raise DexError("u32 outside DEX")
    return struct.unpack_from("<I", data, pos)[0]

def u16(data, pos):
    if pos < 0 or pos + 2 > len(data):
        raise DexError("u16 outside DEX")
    return struct.unpack_from("<H", data, pos)[0]

def span(data, off, count, stride):
    if count > 1000000 or off < 0 or (count and (off == 0 or off > len(data))):
        raise DexError("invalid DEX section")
    if off + count * stride > len(data):
        raise DexError("DEX section truncated")

def uleb(data, pos):
    result = 0
    for shift in range(0, 35, 7):
        if pos >= len(data):
            raise DexError("truncated uleb128")
        value = data[pos]
        pos += 1
        if shift == 28 and value > 15:
            raise DexError("uleb128 overflow")
        result |= (value & 0x7F) << shift
        if value < 128:
            return result, pos
    raise DexError("invalid uleb128")

def encode_uleb(value):
    result = bytearray()
    while True:
        digit = value & 127
        value >>= 7
        result.append(digit | (128 if value else 0))
        if not value:
            return bytes(result)

def verify_dex(data):
    if len(data) < 112 or len(data) > MAX_DEX:
        raise DexError("invalid DEX size")
    if not (data.startswith(b"dex\n") and data[4:7] in
            (b"035", b"036", b"037", b"038", b"039", b"040") and data[7] == 0):
        raise DexError("unsupported DEX magic/version (compact DEX unsupported)")
    if u32(data, 32) != len(data) or u32(data, 36) != 112:
        raise DexError("incorrect DEX file_size/header_size")
    if u32(data, 40) != 0x12345678:
        raise DexError("unexpected endian encoding")
    if data[12:32] != hashlib.sha1(data[32:]).digest():
        raise DexError("DEX SHA1 signature mismatch")
    if u32(data, 8) != zlib.adler32(data[12:]) & 0xFFFFFFFF:
        raise DexError("DEX Adler32 checksum mismatch")

def parse_dex(data, filename):
    verify_dex(data)
    def section(count_off, offset_off, stride):
        count, offset = u32(data, count_off), u32(data, offset_off)
        span(data, offset, count, stride)
        return count, offset

    ns, so = section(56, 60, 4)
    nt, to = section(64, 68, 4)
    np, po = section(72, 76, 12)
    nm, mo = section(88, 92, 8)
    nc, co = section(96, 100, 32)
    if ns > 65536 or nt > 65536 or np > 65536 or nm > 65536 or nc > 65536:
        raise DexError("DEX table count exceeds safety limit")

    strings = []
    for i in range(ns):
        pos = u32(data, so + 4 * i)
        if pos >= len(data):
            raise DexError("invalid string_data offset")
        utf16_length, pos = uleb(data, pos)
        end = data.find(b"\0", pos, min(pos + MAX_STRING_UTF8 + 1, len(data)))
        if end == -1 or utf16_length > MAX_STRING_UTF8:
            raise DexError("oversized or unterminated DEX string")
        raw = data[pos:end]
        # Ordinary DEX string literals may contain tabs, CR/LF, and other
        # control characters. They are valid data, NOT a parsing error.
        # MUTF-8 encodes U+0000 as C0 80. Decode defensively and escape
        # only fields actually written to the TSV report (tsv_cell below).
        txt = raw.replace(b"\xc0\x80", b"\\0").decode("utf-8", "replace")
        strings.append(txt)

    types = []
    for i in range(nt):
        idx = u32(data, to + i * 4)
        if idx >= ns:
            raise DexError("invalid type string index")
        types.append(strings[idx])

    protos = []
    for i in range(np):
        off = po + i * 12
        result_idx = u32(data, off + 4)
        params_off = u32(data, off + 8)
        if result_idx >= nt:
            raise DexError("invalid return type index")
        params = []
        if params_off:
            n = u32(data, params_off)
            if n > 256:
                raise DexError("oversized method signature")
            span(data, params_off + 4, n, 2)
            for k in range(n):
                j = u16(data, params_off + 4 + k * 2)
                if j >= nt:
                    raise DexError("invalid argument type index")
                params.append(types[j])
        protos.append("(" + "".join(params) + ")" + types[result_idx])

    methods = []
    for i in range(nm):
        off = mo + i * 8
        cls, proto, name = u16(data, off), u16(data, off + 2), u32(data, off + 4)
        if cls >= nt or proto >= np or name >= ns:
            raise DexError("invalid method ID indices")
        methods.append((types[cls], strings[name], protos[proto]))

    declarations = {}
    for i in range(nc):
        off = co + 32 * i
        klass_id, class_data_off = u32(data, off), u32(data, off + 24)
        if klass_id >= nt:
            raise DexError("invalid class definition")
        if not class_data_off:
            continue
        pos = class_data_off
        counts = []
        for _ in range(4):
            n, pos = uleb(data, pos)
            if n > 65536:
                raise DexError("class_data entry count overflow")
            counts.append(n)
        for count in counts[:2]:
            prev = 0
            for _ in range(count):
                diff, pos = uleb(data, pos)
                _, pos = uleb(data, pos)
                prev += diff
        for count in counts[2:]:
            prev = 0
            for _ in range(count):
                diff, pos = uleb(data, pos)
                access, pos = uleb(data, pos)
                _, pos = uleb(data, pos)
                prev += diff
                if prev >= nm or methods[prev][0] != types[klass_id]:
                    raise DexError("method encoded outside defining class")
                if prev in declarations:
                    raise DexError("duplicate method declaration")
                declarations[prev] = access

    selected = []
    for idx, (klass, name, signature) in enumerate(methods):
        access = declarations.get(idx)
        if klass.startswith("Lorg/cocos2dx/lib/") or name in TARGET_METHODS or (
                access is not None and (access & 0x100)):
            status = ("DECLARED_NATIVE" if access is not None and access & 0x100
                      else "DECLARED" if access is not None else "REFERENCE_ONLY")
            selected.append((filename, klass, name, signature, status))
            if len(selected) > MAX_ROWS:
                raise DexError("too many output rows; audit requires narrower scope")
    return methods, declarations, selected

def audit(path):
    filename = Path(path)
    if not filename.is_file():
        raise DexError("local input file not found")
    results = []
    digest = []
    def process(data, name):
        if len(data) > MAX_DEX:
            raise DexError("DEX exceeds size limit")
        methods, declared, lines = parse_dex(data, name)
        digest.append((name, len(methods), len(declared),
                       hashlib.sha256(data).hexdigest()[:16]))
        results.extend(lines)

    if filename.suffix.lower() == ".dex":
        if filename.stat().st_size > MAX_DEX:
            raise DexError("DEX exceeds size limit")
        process(filename.read_bytes(), filename.name)
    elif filename.suffix.lower() in (".apk", ".zip"):
        if not zipfile.is_zipfile(filename):
            raise DexError("invalid APK/ZIP")
        total, found = 0, 0
        with zipfile.ZipFile(filename, "r") as z:
            for entry in z.infolist():
                if entry.filename.startswith("/") or "\\" in entry.filename or (
                        ".." in entry.filename.split("/")):
                    raise DexError("unsafe APK ZIP member path")
                if not DEX_NAME.fullmatch(entry.filename):
                    continue
                if entry.file_size > MAX_DEX or entry.file_size < 112:
                    raise DexError("DEX member too large/small")
                total += entry.file_size
                found += 1
                if found > MAX_DEX_FILES or total > MAX_TOTAL:
                    raise DexError("APK DEX limit exceeded")
                if entry.compress_size and entry.file_size > 200 * entry.compress_size:
                    raise DexError("suspicious APK compression ratio")
                with z.open(entry) as handle:
                    raw = handle.read(entry.file_size + 1)
                if len(raw) != entry.file_size:
                    raise DexError("truncated APK DEX member")
                process(raw, entry.filename)
        if not found:
            raise DexError("APK contains no root classes*.dex")
    else:
        raise DexError("expected local .dex or .apk")
    if len(results) > MAX_ROWS:
        raise DexError("total report exceeds row limit")
    evidence = {}
    for _, klass, name, sig, status in results:
        key = (klass, name, sig)
        if key not in evidence or status.startswith("DECLARED"):
            evidence[key] = status
    return digest, results, evidence

def tsv_cell(value):
    """Escape report-only cells without rejecting valid DEX string literals.

    Protects TSV row structure and prevents Excel interpreting a cell as
    a formula. No sanitization is applied to the source DEX itself.
    """
    text = str(value)
    escaped = []
    for ch in text:
        code = ord(ch)
        if ch == "\\":
            escaped.append("\\\\")
        elif ch == "\t":
            escaped.append("\\t")
        elif ch == "\n":
            escaped.append("\\n")
        elif ch == "\r":
            escaped.append("\\r")
        elif code < 32 or code == 127:
            escaped.append("\\x%02x" % code)
        else:
            escaped.append(ch)
    cell = "".join(escaped)
    return "'" + cell if cell.startswith(("=", "+", "-", "@")) else cell


def tsv_row(*cells):
    return "\t".join(tsv_cell(cell) for cell in cells) + "\n"


def save_report(path, digest, results, evidence, fixture=False):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("component\tclass\tmethod\tsignature\tstatus\n")
        for src, klass, name, sig, status in results:
            f.write(tsv_row(src, klass, name, sig, status))
        for klass, name, sig in CONTRACTS:
            f.write(tsv_row("CONTRACT", klass, name, sig,
                            evidence.get((klass, name, sig), "NOT_OBSERVED")))
        for name, total, declared, digest16 in digest:
            f.write(tsv_row("DEX_SUMMARY", name, str(total), str(declared), digest16))
        f.write("GATE\tEXTERNAL_APK\t\t\t" +
                ("NOT_PROVIDED" if fixture else "LOCAL_METADATA_ONLY") + "\n")
        f.write("GATE\tCOMMERCIAL_ENGINE\t\t\tNOT_EXECUTED\n")
        f.write("GATE\tACTUAL_JAVA_RUNTIME\t\t\tNOT_EMULATED\n")

def make_fixture():
    # An independently authored DEX with method IDs, native access flags and
    # real class_data uleb128 entries. No commercial data is present.
    strs = [
        "Lorg/cocos2dx/lib/Cocos2dxHelper;",
        "Lorg/cocos2dx/lib/Cocos2dxRenderer;",
        "Landroid/content/Context;",
        "Ljava/lang/String;", "Landroid/content/res/AssetManager;",
        "Ljava/lang/Object;", "I", "V",
        "getCocos2dxWritablePath", "nativeFixtureRender", "getAssets", "nativeInit",
        "VI", "L", "V",
        "Ordinary game string with tab:\t newline:\n carriage-return:\r",
        "A" * 6000,  # Valid larger string fixture (old 4096-byte cap failed).
    ]
    # Type index list must refer to strings, not descriptor IDs.
    td = [0, 1, 2, 3, 4, 5, 6, 7]
    proto = [(13, 3, []), (14, 7, []), (12, 7, [6, 6]), (13, 4, [])]
    # Method IDs are sorted by class_idx, name_idx, proto_idx.
    methods = [(0, 0, 8), (1, 1, 9), (1, 2, 11), (2, 3, 10)]
    data = bytearray(b"\0" * 112)
    data[:8] = b"dex\n035\0"
    def align(n=4):
        while len(data) % n: data.append(0)
    def alloc(size):
        align()
        pos = len(data)
        data.extend(b"\0" * size)
        return pos
    def put(off, fmt, *args):
        struct.pack_into("<" + fmt, data, off, *args)
    s_off = alloc(len(strs) * 4)
    t_off = alloc(len(td) * 4)
    p_off = alloc(len(proto) * 12)
    m_off = alloc(len(methods) * 8)
    c_off = alloc(2 * 32)
    data_start = len(data)
    for i, text in enumerate(strs):
        raw = text.encode("utf-8")
        pos = len(data)
        data.extend(encode_uleb(len(text)))
        data.extend(raw + b"\0")
        put(s_off + 4 * i, "I", pos)
    for i, idx in enumerate(td): put(t_off + i * 4, "I", idx)
    for i, (shorty, result, params) in enumerate(proto):
        params_pos = 0
        if params:
            align()
            params_pos = len(data)
            data.extend(struct.pack("<I", len(params)))
            for n in params: data.extend(struct.pack("<H", n))
        put(p_off + 12 * i, "III", shorty, result, params_pos)
    for i, (klass, signature, name) in enumerate(methods):
        put(m_off + 8 * i, "HHI", klass, signature, name)
    # class_def helper -> method 0, renderer -> methods 1 and 2 (both native)
    for class_no, entries in ((0, [(0, 0x9)]), (1, [(1, 0x109), (2, 0x109)])):
        pos = len(data)
        data.extend(encode_uleb(0) + encode_uleb(0) +
                    encode_uleb(len(entries)) + encode_uleb(0))
        prev = 0
        for i, (method_id, flags) in enumerate(entries):
            data.extend(encode_uleb(method_id - prev))
            data.extend(encode_uleb(flags))
            data.extend(encode_uleb(0))
            prev = method_id
        put(c_off + class_no * 32, "IIIIIIII", class_no, 1, 5, 0, 0, 0, pos, 0)
    align()
    for a, b, c in ((56, 60, (len(strs), s_off)), (64, 68, (len(td), t_off)),
                    (72, 76, (len(proto), p_off)), (88, 92, (len(methods), m_off)),
                    (96, 100, (2, c_off))):
        put(a, "I", c[0]); put(b, "I", c[1])
    put(32, "I", len(data))
    put(36, "I", 112)
    put(40, "I", 0x12345678)
    put(104, "II", len(data) - data_start, data_start)
    rehash(data)
    return bytes(data)

def rehash(b):
    b[12:32] = hashlib.sha1(b[32:]).digest()
    struct.pack_into("<I", b, 8, zlib.adler32(b[12:]) & 0xFFFFFFFF)

def selftest(output):
    raw = make_fixture()
    assert b"with tab:\t newline:\n carriage-return:\r" in raw
    assert tsv_cell("a\tb\nc\rd\\e") == "a\\tb\\nc\\rd\\\\e"
    assert tsv_cell("=1+1") == "'=1+1"
    with tempfile.TemporaryDirectory() as d:
        root = Path(d)
        dex = root / "classes.dex"
        dex.write_bytes(raw)
        digest, results, evidence = audit(dex)
        expected = ("Lorg/cocos2dx/lib/Cocos2dxHelper;", "getCocos2dxWritablePath",
                    "()Ljava/lang/String;")
        native = ("Lorg/cocos2dx/lib/Cocos2dxRenderer;", "nativeFixtureRender", "()V")
        assert evidence.get(expected) == "DECLARED", evidence
        assert evidence.get(native) == "DECLARED_NATIVE", evidence
        assert evidence.get(("Landroid/content/Context;", "getAssets",
                             "()Landroid/content/res/AssetManager;")) == "REFERENCE_ONLY"
        save_report(output, digest, results, evidence, fixture=True)
        apk = root / "synthetic.apk"
        with zipfile.ZipFile(apk, "w", compression=zipfile.ZIP_DEFLATED) as z:
            z.writestr("classes.dex", raw)
        assert audit(apk)[2] == evidence
        corrupt = bytearray(raw)
        corrupt[-1] ^= 0x55
        dex.write_bytes(corrupt)
        try: audit(dex); raise AssertionError("corrupt checksum accepted")
        except DexError: pass
        dex.write_bytes(raw[:80])
        try: audit(dex); raise AssertionError("truncated DEX accepted")
        except DexError: pass
        corrupt = bytearray(raw)
        struct.pack_into("<H", corrupt, u32(corrupt, 92), 65535)
        rehash(corrupt)
        dex.write_bytes(corrupt)
        try: audit(dex); raise AssertionError("invalid method index accepted")
        except DexError: pass
        with zipfile.ZipFile(apk, "w") as z:
            z.writestr("../classes.dex", raw)
        try: audit(apk); raise AssertionError("unsafe ZIP path accepted")
        except DexError: pass
        with zipfile.ZipFile(apk, "w") as z:
            z.writestr("readme.txt", "no DEX")
        try: audit(apk); raise AssertionError("APK without DEX accepted")
        except DexError: pass
    print("PHASE35_DEX_AUDIT=PASS fixture_methods=4 native_declared=2 "
          "control_chars=ACCEPTED large_strings=ACCEPTED tsv_escape=PASS "
          "checksum=REJECTED truncated=REJECTED indices=REJECTED "
          "zip_traversal=REJECTED missing_dex=REJECTED "
          "external_apk=NOT_PROVIDED engine_executed=NO")

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("source", nargs="?", help="local original APK or .dex (never uploaded)")
    p.add_argument("--output", default="lenneth-dex-contract35.tsv")
    p.add_argument("--selftest", action="store_true")
    a = p.parse_args()
    try:
        if a.selftest:
            if a.source: p.error("--selftest does not accept real game files")
            selftest(a.output)
        else:
            if not a.source: p.error("provide a local APK or .dex")
            digest, rows, evidence = audit(a.source)
            save_report(a.output, digest, rows, evidence)
            print("PHASE35_DEX_AUDIT=PASS dex_files=%d selected=%d "
                  "report=%s uploaded=NO commercial_engine=NO" %
                  (len(digest), len(rows), a.output))
    except (DexError, OSError, zipfile.BadZipFile, AssertionError) as exc:
        print("PHASE35_DEX_AUDIT=FAIL %s" % str(exc), file=sys.stderr)
        return 35
    return 0

if __name__ == "__main__":
    sys.exit(main())
