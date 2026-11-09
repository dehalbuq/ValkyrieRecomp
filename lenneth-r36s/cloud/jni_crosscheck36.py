#!/usr/bin/env python3
"""Phase36: local JNI declaration-to-export audit, metadata only.

Joins an offline Phase35 DEX TSV to the read-only Phase31 JNI export TSV.
Input may be a local R36S diagnostic ZIP containing lenneth-jni-demand31.tsv.
No commercial libraries are loaded, run, shared or uploaded.

Matching is by JNI naming convention. Export presence does not prove callable
implementation or Android runtime compatibility. Missing export might mean
RegisterNatives, another ELF, dead code or a missing implementation.
"""
import argparse
import csv
import io
import re
import sys
import tempfile
import zipfile
from collections import Counter
from pathlib import Path

MAX_DEX_REPORT = 12 * 1024 * 1024
MAX_INVENTORY = 2 * 1024 * 1024
MAX_ROWS = 20000
INVENTORY_NAME = "lenneth-jni-demand31.tsv"
DEX_FIELDS = {"component", "class", "method", "signature", "status"}
INVENTORY_FIELDS = {"module", "gate", "status", "detail"}
NATIVE_STATUS = "DECLARED_NATIVE"
MODULE = "libMyGame.so"
SUMMARY_MARKER = "JNI_EXPORT_TOTAL"
EXPORT_MARKER = "JNI_NATIVE_EXPORT"


class AuditError(ValueError):
    pass


def safe_read(path, max_bytes):
    p = Path(path)
    if not p.is_file() or p.stat().st_size > max_bytes:
        raise AuditError("missing or oversized local file")
    raw = p.read_bytes()
    if b"\0" in raw:
        raise AuditError("NUL byte in TSV")
    return raw.decode("utf-8-sig")


def clean_cell(value):
    # Reports are UTF-8 TSV with no embedded row separators.
    value = str(value)
    return value.replace("\\", "\\\\").replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n")


def tsv_rows(text, required):
    rows = csv.DictReader(io.StringIO(text), delimiter="\t")
    if not rows.fieldnames or not required.issubset(rows.fieldnames):
        raise AuditError("incorrect TSV headers")
    for i, row in enumerate(rows):
        if i >= MAX_ROWS:
            raise AuditError("too many TSV records")
        if None in row or any(v is None for v in row.values()):
            raise AuditError("malformed TSV record")
        yield row


def read_inventory(path):
    p = Path(path)
    if p.suffix.lower() == ".zip":
        if not p.is_file() or not zipfile.is_zipfile(p):
            raise AuditError("invalid diagnostic ZIP")
        with zipfile.ZipFile(p) as z:
            matches = [x for x in z.infolist()
                       if x.filename == INVENTORY_NAME and not x.is_dir()]
            if len(matches) != 1 or matches[0].file_size > MAX_INVENTORY:
                raise AuditError("missing or oversized Phase31 inventory in ZIP")
            if matches[0].compress_size and (
                    matches[0].file_size > 250 * matches[0].compress_size):
                raise AuditError("suspicious inventory compression ratio")
            with z.open(matches[0]) as f:
                raw = f.read(MAX_INVENTORY + 1)
            if len(raw) != matches[0].file_size or b"\0" in raw:
                raise AuditError("invalid Phase31 TSV member")
            text = raw.decode("utf-8-sig")
    else:
        text = safe_read(p, MAX_INVENTORY)
    exports = set()
    count_report = None
    onload = False
    for row in tsv_rows(text, INVENTORY_FIELDS):
        if row["module"] != MODULE:
            continue
        gate, value = row["gate"], row["detail"]
        if gate == EXPORT_MARKER:
            if row["status"] != "INVENTORIED":
                raise AuditError("unexpected export status")
            if value.startswith("Java_"):
                exports.add(value)
            if value == "JNI_OnLoad":
                onload = True
        if gate == SUMMARY_MARKER:
            if count_report is not None:
                raise AuditError("ambiguous inventory total")
            if row["status"] != "PASS":
                raise AuditError("unverified inventory total")
            found = re.search(r"\bexports=(\d+) java=(\d+) cocos=(\d+) truncated=(\d+)", value)
            if not found:
                raise AuditError("missing export-count fields")
            count_report = tuple(int(x) for x in found.groups())
    if count_report is None:
        raise AuditError("missing Phase31 export summary")
    total, java_count, _, truncated = count_report
    if truncated:
        raise AuditError("truncated inventory: coverage cannot be established")
    if len(exports) != java_count or total != java_count + int(onload):
        raise AuditError("export-count mismatch")
    return exports, onload


def read_native_declarations(path):
    declarations = {}
    all_rows = 0
    for row in tsv_rows(safe_read(path, MAX_DEX_REPORT), DEX_FIELDS):
        if not re.fullmatch(r"classes(?:[2-9]|[1-9][0-9]+)?\.dex", row["component"]):
            continue
        all_rows += 1
        if row["status"] != NATIVE_STATUS:
            continue
        klass, name, sig = row["class"], row["method"], row["signature"]
        if not (klass.startswith("L") and klass.endswith(";")
                and "(" in sig and ")" in sig and name):
            raise AuditError("malformed declared-native signature")
        key = (klass, name, sig)
        if key in declarations and declarations[key] != row["component"]:
            raise AuditError("duplicate native declaration across DEX files")
        declarations[key] = row["component"]
    if not all_rows or not declarations:
        raise AuditError("no DEX method declarations found")
    return declarations


def mangle(s):
    """JNI spec's per UTF-16 code unit mangling (handles obfuscated names)."""
    b = s.encode("utf-16-be")
    out = []
    for i in range(0, len(b), 2):
        cp = (b[i] << 8) | b[i + 1]
        ch = chr(cp)
        if (0x30 <= cp <= 0x39 or 0x41 <= cp <= 0x5A or
                0x61 <= cp <= 0x7A):
            out.append(ch)
        elif ch == "/":
            out.append("_")
        elif ch == "_":
            out.append("_1")
        elif ch == ";":
            out.append("_2")
        elif ch == "[":
            out.append("_3")
        else:
            out.append("_0%04x" % cp)
    return "".join(out)


def jni_names(klass, name, signature):
    args = signature[signature.find("(") + 1:signature.find(")")]
    prefix = "Java_" + mangle(klass[1:-1]) + "_" + mangle(name)
    return prefix, prefix + "__" + mangle(args)


def reconcile(declarations, exports):
    counts = Counter()
    overloaded = Counter((klass, name) for klass, name, _ in declarations)
    matched_exports = set()
    report = []
    for (klass, method, signature), dex in sorted(declarations.items()):
        short, long = jni_names(klass, method, signature)
        if long in exports:
            state, exported = "MATCH_LONG", long
        elif short in exports and overloaded[(klass, method)] == 1:
            state, exported = "MATCH_SHORT", short
        elif short in exports:
            state, exported = "AMBIGUOUS_OVERLOAD", short
        else:
            state, exported = "NOT_EXPORTED_BY_LIBMYGAME", ""
        counts[state] += 1
        if state in ("MATCH_SHORT", "MATCH_LONG"):
            matched_exports.add(exported)
        report.append((dex, klass, method, signature, state, exported,
                       short, long))
    unpaired = exports - matched_exports
    return report, counts, unpaired


def write_report(path, report, counts, unpaired, exports, onload, synthetic):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("dex\tclass\tmethod\tsignature\tjoin_status\texport_matched\tjni_short_candidate\tjni_long_candidate\n")
        for row in report:
            f.write("\t".join(clean_cell(v) for v in row) + "\n")
        for symbol in sorted(unpaired):
            f.write("EXPORT_UNPAIRED\t\t\t\tEXPORT_WITHOUT_CONFIRMED_DECLARATION\t" +
                    clean_cell(symbol) + "\t\t\n")
    matched = counts["MATCH_SHORT"] + counts["MATCH_LONG"]
    return {
        "native_declarations": len(report),
        "jni_exports": len(exports),
        "matched": matched,
        "not_exported_here": counts["NOT_EXPORTED_BY_LIBMYGAME"],
        "ambiguous": counts["AMBIGUOUS_OVERLOAD"],
        "unpaired_exports": len(unpaired),
        "jni_onload": "YES" if onload else "NO",
        "engine_executed": "NO",
        "source": "SYNTHETIC_FIXTURE_ONLY" if synthetic else "OFFLINE_METADATA_ONLY"
    }


def selftest(output):
    declarations = {
        ("Lorg/cocos2dx/lib/Cocos2dxRenderer;", "nativeInit", "(II)V"): "classes.dex",
        ("Lorg/cocos2dx/lib/Cocos2dxHelper;", "nativeSetContext", "(I)V"): "classes.dex",
        ("Ltest/Foo;", "overload", "(I)V"): "classes2.dex",
        ("Ltest/Foo;", "overload", "(Ljava/lang/String;)V"): "classes2.dex"
    }
    exports = {"Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit",
               "Java_test_Foo_overload__I", "Java_test_Foo_overload"}
    report, counts, unpaired = reconcile(declarations, exports)
    assert counts == {"MATCH_SHORT": 1, "MATCH_LONG": 1,
                      "AMBIGUOUS_OVERLOAD": 1, "NOT_EXPORTED_BY_LIBMYGAME": 1}, counts
    assert len(unpaired) == 1 and "Java_test_Foo_overload" in unpaired
    assert mangle("foo_bar/a$") == "foo_1bar_a_00024"
    assert mangle("ۨ") == "_006e8"
    summary = write_report(output, report, counts, unpaired, exports, True, True)
    assert summary["matched"] == 2 and summary["ambiguous"] == 1
    # Additional parser roundtrip with no real binaries or game text.
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        dex = root / "dex.tsv"
        dex.write_text("component\tclass\tmethod\tsignature\tstatus\n" +
                       "classes.dex\tLx/Foo;\tnativeGo\t()V\tDECLARED_NATIVE\n" +
                       "CONTRACT\tLx/Foo;\tnativeGo\t()V\tDECLARED_NATIVE\n",
                       encoding="utf-8")
        inv = root / "inventory.tsv"
        inv.write_text("module\tgate\tstatus\tdetail\n" +
                       "libMyGame.so\tJNI_NATIVE_EXPORT\tINVENTORIED\tJava_x_Foo_nativeGo\n" +
                       "libMyGame.so\tJNI_NATIVE_EXPORT\tINVENTORIED\tJNI_OnLoad\n" +
                       "libMyGame.so\tJNI_EXPORT_TOTAL\tPASS\texports=2 java=1 cocos=0 truncated=0\n",
                       encoding="utf-8")
        parsed = read_native_declarations(dex)
        assert len(parsed) == 1
        sym, onload = read_inventory(inv)
        assert len(sym) == 1 and onload
        assert reconcile(parsed, sym)[1]["MATCH_SHORT"] == 1
        inv.write_text(inv.read_text().replace("truncated=0", "truncated=1"))
        try:
            read_inventory(inv)
            raise AssertionError("truncated inventory accepted")
        except AuditError:
            pass
    print("PHASE36_JNI_JOIN=PASS match_short=PASS match_long=PASS "
          "overloads=FAIL_CLOSED missing_exports=UNVERIFIED "
          "phase31_counts=VALIDATED fixture_only=YES commercial_engine=NO")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--dex", help="Phase35 TSV generated locally")
    p.add_argument("--inventory", help="Phase31 TSV or R36S diagnostics ZIP")
    p.add_argument("--output", default="lenneth-jni-join36.tsv")
    p.add_argument("--selftest", action="store_true")
    args = p.parse_args()
    try:
        if args.selftest:
            if args.dex or args.inventory:
                p.error("--selftest cannot use game metadata inputs")
            selftest(args.output)
        else:
            if not args.dex or not args.inventory:
                p.error("--dex and --inventory are both required")
            declarations = read_native_declarations(args.dex)
            exports, onload = read_inventory(args.inventory)
            report, counts, unpaired = reconcile(declarations, exports)
            summary = write_report(args.output, report, counts, unpaired,
                                   exports, onload, False)
            print("PHASE36_JNI_JOIN=PASS " +
                  " ".join("%s=%s" % pair for pair in summary.items()))
    except (AuditError, OSError, UnicodeError, csv.Error, zipfile.BadZipFile,
            AssertionError) as exc:
        print("PHASE36_JNI_JOIN=FAIL %s" % str(exc), file=sys.stderr)
        return 36
    return 0


if __name__ == "__main__":
    sys.exit(main())
