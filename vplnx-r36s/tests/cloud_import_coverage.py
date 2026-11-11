#!/usr/bin/env python3
"""Read-only static provider-name coverage against Test 5 on R36S.

Does not need, download or execute any commercial libraries. Name matching
is not ABI proof or confirmation of runnable engine code.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re


def parse_group(text: str, heading: str) -> set[str]:
    fence = re.escape(chr(96) * 3)
    pattern = rf"## {re.escape(heading)}\s*\n\s*{fence}text\s*\n(.*?)\n{fence}"
    match = re.search(pattern, text, flags=re.S)
    if not match:
        raise ValueError(f"Historical group missing: {heading}")
    names = [line.strip() for line in match.group(1).splitlines()
             if line.strip()]
    if len(names) != len(set(names)):
        raise ValueError(f"Duplicate names in {heading}")
    return set(names)


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--evidence", type=Path, required=True)
    p.add_argument("--imports", type=Path, required=True)
    args = p.parse_args()
    evidence = args.evidence.read_text(encoding="utf-8")
    source = args.imports.read_text(encoding="utf-8")
    game = parse_group(evidence, "Pendentes no motor (57 nomes)")
    cxx = parse_group(evidence, "Pendentes na biblioteca C++ (62 nomes)")
    if len(game) != 57 or len(cxx) != 62 or len(game | cxx) != 99:
        raise ValueError("Test 5 evidence baseline corrupted")
    names = set(re.findall(r'FN\("([^"]+)"', source))
    names |= set(re.findall(r'FN_SELF\((\w+)\)', source))
    names |= set(re.findall(r'DATA\((\w+)\)', source))
    # Randomized Bionic canary uses special-case resolver, not static FN.
    if 'strcmp(symbol,"__stack_chk_guard")==0' in source:
        names.add("__stack_chk_guard")
    missing_game = game - names
    missing_cxx = cxx - names
    missing_all = missing_game | missing_cxx
    newly_covered = (game | cxx) & names
    print("CLOUD_IMPORT_BASELINE test=5 unique=99 game=57 cxx=62")
    print("CLOUD_IMPORT_AFTER_SOURCE "
          f"game_unresolved_names={len(missing_game)} "
          f"cxx_unresolved_names={len(missing_cxx)} "
          f"union_unresolved_names={len(missing_all)} "
          f"new_name_providers={len(newly_covered)}")
    print("CLOUD_IMPORT_RESOLVED_NAMES=" + ",".join(sorted(newly_covered)))
    print("CLOUD_IMPORT_STILL_MISSING=" + ",".join(sorted(missing_all)))
    if len(missing_all) >= 99:
        raise ValueError("Name-coverage regressed unexpectedly")
    print("CLOUD_IMPORT_COVERAGE=PASS name_match_only=1 "
          "commercial_ELF_executed=NO")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
