#!/usr/bin/env python3
"""Phase40 fail-closed startup readiness audit (diagnostic metadata only).

Reads exactly three known TSVs from a LOCAL R36S diagnostic ZIP or folder.
No APK/ELF is loaded, executed, copied, uploaded, or sent to GitHub.
The audit being PASS never means the game is ready to boot.
"""
import argparse
import csv
import io
from pathlib import Path
import re
import sys
import tempfile
import zipfile

NEEDED = ("lenneth-startup-gates.tsv", "lenneth-phase25-startup-plan.tsv",
          "lenneth-jni-demand31.tsv")
HEADERS = ("module", "gate", "status", "detail")
MAX_MEMBER = 262144
MAX_ROWS = 2000
MAX_ZIP = 10 * 1024 * 1024
PROVIDERS = {
    "libc.so": ("P0", "Bionic libc: layout/fortify/runtime ABI"),
    "libdl.so": ("P1", "Android dlfcn/linker namespaces"),
    "libm.so": ("P1", "libm symbol and ABI checks"),
    "libc++_shared.so": ("P0", "C++ runtime and constructor ordering"),
    "libandroid.so": ("P0", "NDK AAsset/native window Android APIs"),
    "liblog.so": ("P1", "Android logging shim"),
    "libOpenSLES.so": ("P1", "OpenSL ES queues -> ALSA"),
    "libGLESv1_CM.so": ("P1", "OpenGL ES1 fixed function"),
    "libGLESv2.so": ("P1", "OpenGL ES2/context interoperability"),
    "libEGL.so": ("P1", "EGL GBM/KMSDRM context lifecycle")
}

class AuditError(ValueError):
    pass

def load(path):
    p = Path(path)
    if p.is_dir():
        data = {}
        for name in NEEDED:
            f = p / name
            if not f.is_file() or f.stat().st_size > MAX_MEMBER:
                raise AuditError("required metadata missing or too large: " + name)
            data[name] = f.read_bytes()
    elif p.is_file() and p.suffix.lower() == ".zip":
        if p.stat().st_size > MAX_ZIP:
            raise AuditError("ZIP too large")
        data = {}
        with zipfile.ZipFile(p) as z:
            for name in NEEDED:
                matches = [i for i in z.infolist()
                           if i.filename == name and not i.is_dir()]
                if len(matches) != 1 or matches[0].file_size > MAX_MEMBER:
                    raise AuditError("missing/duplicate/oversized ZIP TSV: " + name)
                with z.open(matches[0]) as stream:
                    raw = stream.read(MAX_MEMBER + 1)
                if len(raw) != matches[0].file_size:
                    raise AuditError("truncated TSV: " + name)
                data[name] = raw
    else:
        raise AuditError("expected local diagnostics folder or ZIP")
    reports = {}
    for name, raw in data.items():
        if b"\0" in raw:
            raise AuditError("NUL in TSV: " + name)
        reader = csv.DictReader(io.StringIO(raw.decode("utf-8-sig")),
                                delimiter="\t", strict=True)
        if reader.fieldnames != list(HEADERS):
            raise AuditError("invalid TSV header: " + name)
        rows = []
        for row in reader:
            if len(rows) >= MAX_ROWS or None in row or any(
                    v is None or len(v) > 1024 or any(
                        c in v for c in ("\r", "\n", "\t"))
                    for v in row.values()):
                raise AuditError("invalid TSV row: " + name)
            rows.append(row)
        if not rows:
            raise AuditError("empty diagnostic TSV: " + name)
        reports[name] = rows
    return reports

def gate(rows, module, key):
    hits = [x for x in rows if x["module"] == module and x["gate"] == key]
    if len(hits) != 1:
        raise AuditError("required gate absent or repeated: %s/%s" % (module,key))
    return hits[0]

def count(detail, key):
    match = re.search(r"\b" + re.escape(key) + r"=(\d+)\b", detail)
    if not match:
        raise AuditError("missing metric " + key)
    return int(match.group(1))

def evidence(reports):
    startup, phase25, jni = [reports[name] for name in NEEDED]
    info = {}
    for module in ("libMyGame.so", "libc++_shared.so"):
        ctor = gate(startup,module,"CONSTRUCTOR_ORDER")
        reloc = gate(phase25,module,"INIT_ARRAY_RELOCATION")
        protect = gate(startup,module,"ELF_MEMORY_PROTECTION")
        if ctor["status"] != "REVIEW" or reloc["status"] != "PASS" or (
                protect["status"] != "REVIEW"):
            raise AuditError("unverified ctor/ELF state for " + module)
        total = count(ctor["detail"],"init_array_entries")
        matched = count(reloc["detail"],"reloc_matched")
        if total <= 0 or total != matched or count(reloc["detail"],"unknown") != 0:
            raise AuditError("constructor relocations incomplete for " + module)
        info[module] = total
        dependencies = [r["detail"] for r in startup if
                        r["module"] == module and r["gate"] == "DT_NEEDED"]
        dep_summary = gate(startup,module,"DEPENDENCIES")
        if dep_summary["status"] != "REVIEW" or (
                count(dep_summary["detail"],"needed") != len(dependencies)
                or len(set(dependencies)) != len(dependencies)):
            raise AuditError("inconsistent DT_NEEDED inventory: " + module)
        info[module + "_needed"] = dependencies
    for key in ("BIONIC_CTORS","JNI_JAVA_VM","COCOS_LIFECYCLE",
                "OPENSL_PCM","ASSET_HANDOFF","COMMERCIAL_ENGINE"):
        if gate(startup,"runtime",key)["status"] != "BLOCKED":
            raise AuditError("unexpected runtime gate: " + key)
    for key in ("RELRO_WX_TRANSITION","JNI_VM_CLASSLOADER",
                "GLES1_HOST_PROBE","COMMERCIAL_ENGINE"):
        if gate(phase25,"runtime",key)["status"] != "BLOCKED":
            raise AuditError("unexpected Phase25 runtime gate: " + key)
    j = gate(jni,"libMyGame.so","JNI_EXPORT_TOTAL")
    if j["status"] != "PASS":
        raise AuditError("JNI inventory incomplete")
    info["java_exports"] = count(j["detail"],"java")
    if info["java_exports"] < 1 or count(j["detail"],"truncated") != 0:
        raise AuditError("JNI export list truncated or empty")
    gl = gate(phase25,"libMyGame.so","GLES1_COMPATIBILITY")
    if gl["status"] not in ("REVIEW","PASS"):
        raise AuditError("GLES1 audit not completed")
    info["needs_gles1"] = count(gl["detail"],"needs_es1")
    info["gl_imports"] = count(gl["detail"],"total_gl_imports")
    return info

def make_plan(e):
    total_ctors = e["libMyGame.so"] + e["libc++_shared.so"]
    targets = [
      ("P0","JNI_ART_CLASSLOADER","BLOCKED","JNI_JAVA_VM",
       "Implement JNI/ART class and thread services",
       "The real JNI_OnLoad is not yet executed"),
      ("P0","BIONIC_CPP_CONSTRUCTORS","BLOCKED","BIONIC_CTORS",
       "Resolve Bionic/C++ ABI, then constructor ordering",
       "The %d original constructors have not run" % total_ctors),
      ("P0","COCOS_NATIVE_LIFECYCLE","BLOCKED","COCOS_LIFECYCLE",
       "Wire real JNI class/method and Cocos lifecycle contracts",
       "Only test-fixture nativeInit/nativeRender work"),
      ("P0","ASSET_APK_HANDOFF","BLOCKED","ASSET_HANDOFF",
       "Validate real Android APK assets/path handling",
       "Phase38 uses generated synthetic assets only"),
      ("P0","ELF_WX_RELRO","BLOCKED","RELRO_WX_TRANSITION",
       "Verify relocation/protection and executable transitions",
       "No original executable ELF mapping has run"),
      ("P1","OPENSL_PCM","BLOCKED","OPENSL_PCM",
       "Map OpenSL ES guest buffers to ALSA",
       "Audio engine still synthetic"),
      ("P1","GLES1_MALI_G31","REQUIRES_HARDWARE","GLES1_COMPATIBILITY",
       "Confirm native GLES1/EGL context and rendering on R36S",
       "Mesa cloud rendering does not benchmark Mali-G31")
    ]
    for needed in e["libMyGame.so_needed"]:
        priority, action = PROVIDERS.get(
            needed, ("P0","Unknown ELF dependency: inspect before startup"))
        targets.append((priority,"NEEDED:"+needed,"PROVIDER_ABI_UNVERIFIED",
                        "DT_NEEDED", action,
                        "Library name present; ABI/provider unresolved"))
    targets.append(("P2","REAL_FPS_RAM_CONTROLS","NOT_TESTED","DEVICE_ONLY",
                    "Benchmark actual gameplay, RAM and inputs on R36S",
                    "Cloud ARM64 CPUs do not emulate Cortex-A35 speed"))
    return targets

def emit(output, summary_path, e, actions, fixture):
    with open(output,"w",encoding="utf-8",newline="") as file:
        writer=csv.writer(file,delimiter="\t",lineterminator="\n")
        writer.writerow(("priority","blocker","status","evidence_gate",
                         "next_action","limitation"))
        writer.writerows(actions)
    with open(summary_path,"w",encoding="utf-8") as file:
        for key,val in (
            ("PHASE40_AUDIT","PASS"),("GAME_BOOT_READY","NO"),
            ("COMMERCIAL_ENGINE_EXECUTED","NO"),
            ("SOURCE","SYNTHETIC_FIXTURE" if fixture else "LOCAL_R36S_DIAGNOSTIC_METADATA"),
            ("GAME_CONSTRUCTORS",e["libMyGame.so"]),
            ("CPP_CONSTRUCTORS",e["libc++_shared.so"]),
            ("JNI_JAVA_EXPORTS",e["java_exports"]),
            ("GLES1_REQUIRED",e["needs_gles1"]),("GL_IMPORTS",e["gl_imports"]),
            ("LIBMYGAME_DT_NEEDED",len(e["libMyGame.so_needed"])),
            ("P0_ACTIONS",sum(row[0]=="P0" for row in actions)),
            ("P1_ACTIONS",sum(row[0]=="P1" for row in actions)),
            ("P2_ACTIONS",sum(row[0]=="P2" for row in actions)),
            ("GATE_REPORT","metadata_not_execution")):
            file.write("%s=%s\n"%(key,val))

def selftest(out,summ):
    a=[
       ("libMyGame.so","CONSTRUCTOR_ORDER","REVIEW","init_array_entries=3"),
       ("libMyGame.so","ELF_MEMORY_PROTECTION","REVIEW","relro=1 wx_segments=0"),
       ("libMyGame.so","DEPENDENCIES","REVIEW","needed=3"),
       ("libMyGame.so","DT_NEEDED","REVIEW_SHIM","libc++_shared.so"),
       ("libMyGame.so","DT_NEEDED","REVIEW_SHIM","libGLESv1_CM.so"),
       ("libMyGame.so","DT_NEEDED","REVIEW_SHIM","libandroid.so"),
       ("libc++_shared.so","CONSTRUCTOR_ORDER","REVIEW","init_array_entries=1"),
       ("libc++_shared.so","ELF_MEMORY_PROTECTION","REVIEW","relro=1 wx_segments=0"),
       ("libc++_shared.so","DEPENDENCIES","REVIEW","needed=1"),
       ("libc++_shared.so","DT_NEEDED","REVIEW_SHIM","libc.so"),
    ]+[( "runtime",key,"BLOCKED","synthetic") for key in
        ("BIONIC_CTORS","JNI_JAVA_VM","COCOS_LIFECYCLE","OPENSL_PCM",
         "ASSET_HANDOFF","COMMERCIAL_ENGINE")]
    b=[
       ("libMyGame.so","INIT_ARRAY_RELOCATION","PASS","reloc_matched=3 unknown=0"),
       ("libc++_shared.so","INIT_ARRAY_RELOCATION","PASS","reloc_matched=1 unknown=0"),
       ("libMyGame.so","GLES1_COMPATIBILITY","REVIEW",
        "needs_es1=1 total_gl_imports=9")
    ]+[("runtime",key,"BLOCKED","synthetic") for key in
        ("RELRO_WX_TRANSITION","JNI_VM_CLASSLOADER",
         "GLES1_HOST_PROBE","COMMERCIAL_ENGINE")]
    c=[("libMyGame.so","JNI_EXPORT_TOTAL","PASS",
        "exports=3 java=2 cocos=2 truncated=0")]
    def blob(rows):
        return ("\t".join(HEADERS)+"\n"+"".join("\t".join(row)+"\n"
                for row in rows)).encode()
    with tempfile.TemporaryDirectory() as d:
        zpath=Path(d)/"diagnostic.zip"
        def build(source):
            with zipfile.ZipFile(zpath,"w") as z:
                for name,rows in zip(NEEDED,source):
                    z.writestr(name,blob(rows))
        build((a,b,c))
        e=evidence(load(zpath))
        actions=make_plan(e)
        assert e["libMyGame.so"] == 3 and e["java_exports"] == 2
        assert all(row[2]!="BOOT_READY" for row in actions)
        emit(out,summ,e,actions,True)
        broken=list(b)
        broken[0]=("libMyGame.so","INIT_ARRAY_RELOCATION",
                   "PASS","reloc_matched=2 unknown=1")
        build((a,broken,c))
        try:
            evidence(load(zpath))
            raise AssertionError("missing ctor relocations incorrectly accepted")
        except AuditError:
            pass
        no_vm=[v for v in a if v[1]!="JNI_JAVA_VM"]
        build((no_vm,b,c))
        try:
            evidence(load(zpath))
            raise AssertionError("missing mandatory VM gate incorrectly accepted")
        except AuditError:
            pass
    print("PHASE40_STARTUP_TRIAGE=PASS constructor_counts=VERIFIED "
          "incomplete_relocations=DENIED missing_jni_gate=DENIED "
          "source=SYNTHETIC_FIXTURE game_executed=NO")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--diagnostics",help="local R36S diagnostics ZIP or folder")
    p.add_argument("--output",default="lenneth-startup-priorities40.tsv")
    p.add_argument("--summary",default="lenneth-startup-summary40.txt")
    p.add_argument("--selftest",action="store_true")
    args=p.parse_args()
    try:
        if args.selftest:
            if args.diagnostics:
                p.error("--selftest does not accept original metadata")
            selftest(args.output,args.summary)
        else:
            if not args.diagnostics:
                p.error("provide --diagnostics or --selftest")
            e=evidence(load(args.diagnostics))
            actions=make_plan(e)
            emit(args.output,args.summary,e,actions,False)
            print("PHASE40_STARTUP_TRIAGE=PASS blockers=%d game_boot_ready=NO "
                  "engine_executed=NO report=%s"%(len(actions),args.output))
    except (AuditError,OSError,UnicodeError,zipfile.BadZipFile,
            AssertionError,csv.Error) as exc:
        print("PHASE40_STARTUP_TRIAGE=FAIL %s"%exc,file=sys.stderr)
        return 40
    return 0

if __name__=="__main__":
    sys.exit(main())
