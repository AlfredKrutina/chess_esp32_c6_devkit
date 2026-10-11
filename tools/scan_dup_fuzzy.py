#!/usr/bin/env python3
import difflib
import os
import re
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIRS = ["main", "components", os.path.join("firmware", "stm32_hall_c031")]
SKIP = {"kos", "build", "build-hall-v2", ".git", "managed_components", "test", "tests"}
KW = {"if", "else", "for", "while", "switch", "do", "return", "case", "default"}


def strip(t: str) -> str:
    t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
    t = re.sub(r"//.*", "", t)
    return re.sub(r"\s+", " ", t).strip()


def funcs(path: str) -> list:
    try:
        src = open(path, encoding="utf-8", errors="replace").read()
    except OSError:
        return []
    out = []
    pat = re.compile(
        r"^(?:static\s+)?(?:inline\s+)?([\w\s\*]+?)\s+(\w+)\s*\([^;]*\)\s*\{",
        re.MULTILINE,
    )
    for m in pat.finditer(src):
        n = m.group(2)
        if n in KW:
            continue
        i = m.end() - 1
        b = 0
        while i < len(src):
            if src[i] == "{":
                b += 1
            elif src[i] == "}":
                b -= 1
                if b == 0:
                    body = src[m.start() : i + 1]
                    out.append((n, body, strip(body)))
                    break
            i += 1
    return out


def main() -> None:
    all_by_norm: dict[str, list] = defaultdict(list)
    for d in DIRS:
        base = os.path.join(ROOT, d)
        if not os.path.isdir(base):
            continue
        for root, dirs, files in os.walk(base):
            dirs[:] = [x for x in dirs if x not in SKIP and not x.startswith(".")]
            nr = root.replace("\\", "/")
            if "/kos/" in nr or nr.endswith("/kos"):
                continue
            for fn in files:
                if not fn.endswith(".c"):
                    continue
                p = os.path.join(root, fn)
                for n, raw, norm in funcs(p):
                    if len(norm) < 60:
                        continue
                    all_by_norm[norm].append((n, p, len(raw)))

    print("=== EXACT (len>=60) ===")
    for norm, items in sorted(all_by_norm.items(), key=lambda x: -len(x[1])):
        paths = {p for _, p, _ in items}
        if len(paths) < 2:
            continue
        names = {n for n, _, _ in items}
        print(names, "files:", len(paths))
        for n, p, ln in items:
            print(" ", os.path.relpath(p, ROOT), n, ln)
        print()

    by_name: dict[str, list] = defaultdict(list)
    for norm, items in all_by_norm.items():
        for n, p, ln in items:
            by_name[n].append((norm, p, ln))

    print("=== SIMILAR SAME NAME (ratio>=0.88, different file) ===")
    for name, locs in sorted(by_name.items()):
        if len(locs) < 2:
            continue
        for i in range(len(locs)):
            for j in range(i + 1, len(locs)):
                n1, p1, l1 = locs[i]
                n2, p2, l2 = locs[j]
                if p1 == p2:
                    continue
                r = difflib.SequenceMatcher(None, n1, n2).ratio()
                if r >= 0.88:
                    print(name, f"r={r:.3f}")
                    print(" ", os.path.relpath(p1, ROOT), l1)
                    print(" ", os.path.relpath(p2, ROOT), l2)
                    print()


if __name__ == "__main__":
    main()
