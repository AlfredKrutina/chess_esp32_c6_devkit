#!/usr/bin/env python3
"""Scan for duplicate C function bodies in firmware tree."""
import os
import re
import hashlib
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIRS = ["main", "components", os.path.join("firmware", "stm32_hall_c031")]
SKIP_DIRS = {"kos", "build", "build-hall-v2", ".git", "managed_components"}
KEYWORDS = {
    "if",
    "else",
    "for",
    "while",
    "switch",
    "do",
    "return",
    "case",
    "default",
}
MIN_BODY = 80


def strip_c_body(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*", "", text)
    text = re.sub(r"\s+", " ", text).strip()
    return text


def extract_functions(path: str) -> list:
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            src = f.read()
    except OSError:
        return []
    funcs = []
    pat = re.compile(
        r"^(?:static\s+)?(?:inline\s+)?([\w\s\*]+?)\s+(\w+)\s*\([^;]*\)\s*\{",
        re.MULTILINE,
    )
    for m in pat.finditer(src):
        name = m.group(2)
        if name in KEYWORDS:
            continue
        brace = 0
        i = m.end() - 1
        while i < len(src):
            if src[i] == "{":
                brace += 1
            elif src[i] == "}":
                brace -= 1
                if brace == 0:
                    body = src[m.start() : i + 1]
                    funcs.append((name, body, path))
                    break
            i += 1
    return funcs


def main() -> None:
    all_funcs = []
    for d in DIRS:
        base = os.path.join(ROOT, d)
        if not os.path.isdir(base):
            continue
        for root, dirs, files in os.walk(base):
            dirs[:] = [x for x in dirs if x not in SKIP_DIRS and not x.startswith(".")]
            norm_root = root.replace("\\", "/")
            if "/kos/" in norm_root or norm_root.endswith("/kos"):
                continue
            for fn in files:
                if fn.endswith(".c"):
                    all_funcs.extend(extract_functions(os.path.join(root, fn)))

    by_hash: dict[str, list] = defaultdict(list)
    by_name: dict[str, list] = defaultdict(list)
    for name, body, path in all_funcs:
        norm = strip_c_body(body)
        h = hashlib.md5(norm.encode()).hexdigest()[:16]
        by_hash[h].append((name, path, len(body)))
        by_name[name].append((path, len(body)))

    print("=== EXACT BODY DUPLICATES (2+ files, body>=%d) ===" % MIN_BODY)
    for h, items in sorted(by_hash.items(), key=lambda x: -len(x[1])):
        paths = {p for _, p, _ in items}
        if len(paths) < 2:
            continue
        if max(ln for _, _, ln in items) < MIN_BODY:
            continue
        names = {n for n, _, _ in items}
        print(f"hash={h} names={names}")
        for n, p, ln in items:
            print(f"  {n} @ {os.path.relpath(p, ROOT)} ({ln})")
        print()

    print("=== SAME NAME, similar size, multiple files ===")
    for name, locs in sorted(by_name.items()):
        paths = {p for p, _ in locs}
        if len(paths) < 2:
            continue
        sizes = [s for _, s in locs]
        if max(sizes) - min(sizes) > 200:
            continue
        print(f"{name}: {len(paths)} files sizes={sizes}")
        for p, s in locs:
            print(f"  {os.path.relpath(p, ROOT)} ({s})")
        print()


if __name__ == "__main__":
    main()
