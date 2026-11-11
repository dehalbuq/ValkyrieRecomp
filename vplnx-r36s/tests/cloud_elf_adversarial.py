#!/usr/bin/env python3
"""Fuzz public synthetic AArch64 ELF headers and check clean rejection.

Only files synthesized from source-built CI fixtures are mutated. This is
not a commercial APK scanner and never executes any ELF input.
"""
import argparse
import pathlib
import random
import struct
import subprocess
import tempfile


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--elf-check", type=pathlib.Path, required=True)
    p.add_argument("--guest", type=pathlib.Path, required=True)
    p.add_argument("--partner", type=pathlib.Path, required=True)
    p.add_argument("--cases", type=int, default=128)
    args = p.parse_args()

    original = args.guest.read_bytes()
    if not original.startswith(b"\x7fELF") or len(original) < 128:
        raise SystemExit("fixture must be a valid synthetic ELF")
    rng = random.Random(0x364E4E36)
    variants = (
        ("magic", lambda b: b.__setitem__(slice(0, 4), b"BAD!")),
        ("class", lambda b: b.__setitem__(4, 1)),
        ("endian", lambda b: b.__setitem__(5, 2)),
        ("machine", lambda b: struct.pack_into("<H", b, 18, 62)),
        ("type", lambda b: struct.pack_into("<H", b, 16, 2)),
        ("phoff", lambda b: struct.pack_into("<Q", b, 32, (1 << 64) - 1)),
        ("shoff", lambda b: struct.pack_into("<Q", b, 40, (1 << 64) - 1)),
        ("phentsize", lambda b: struct.pack_into("<H", b, 54, 1)),
        ("shentsize", lambda b: struct.pack_into("<H", b, 58, 1)),
        ("phnum", lambda b: struct.pack_into("<H", b, 56, 0)),
        ("shnum", lambda b: struct.pack_into("<H", b, 60, 65535)),
    )
    accepted = 0
    clean_errors = 0
    timeout_errors = 0
    crashes = []
    with tempfile.TemporaryDirectory(prefix="vplnx-elf-fuzz-") as tmp:
        sample = pathlib.Path(tmp) / "test.so"
        for index in range(args.cases):
            data = bytearray(original)
            name, edit = variants[index % len(variants)]
            edit(data)
            # Randomize a few payload bytes outside tables for nontrivial
            # deterministic coverage; the HEADER is always invalid.
            for _ in range(index % 5):
                off = rng.randrange(128, min(len(data), 4096))
                data[off] ^= rng.randrange(1, 256)
            sample.write_bytes(data)
            try:
                r = subprocess.run(
                    [str(args.elf_check), str(sample), str(args.partner)],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    timeout=4,
                    check=False,
                )
            except subprocess.TimeoutExpired:
                timeout_errors += 1
                continue
            if r.returncode == 0:
                accepted += 1
            elif r.returncode < 0 or r.returncode >= 128:
                crashes.append((index, name, r.returncode, r.stderr[:120]))
            else:
                clean_errors += 1
    print(
        f"CLOUD_ELF_MUTATION cases={args.cases} clean_rejections={clean_errors} "
        f"accepted={accepted} crashes={len(crashes)} timeouts={timeout_errors}"
    )
    for issue in crashes[:5]:
        print(f"CRASH: {issue}")
    if not (clean_errors == args.cases and not accepted and
            not crashes and not timeout_errors):
        return 1
    print("CLOUD_ELF_MUTATION=PASS invalid_headers_rejected=1 commercial_engine=NO")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
