#!/usr/bin/env python3
"""Verify (and with --fix, fill in) the game references in src/api/capabilities.def.

Every ref is "path:line symbol". The check passes when that line of the recovered script
mentions the symbol. With --fix, refs written as "path:? symbol" (or whose line moved after
a game update) are resolved to the symbol's declaration (class_name / signal / var / func /
const), falling back to the first line that mentions it.
    tools/check_api_refs.py [--fix] [pck_out dir]     (box only; needs the recovered scripts)
Exit code 1 if any ref is unresolved or wrong.
"""
import os, re, sys

here = os.path.dirname(os.path.abspath(__file__))
DEF = os.path.join(here, "..", "src", "api", "capabilities.def")
fix = "--fix" in sys.argv
args = [a for a in sys.argv[1:] if a != "--fix"]
root = args[0] if args else None
if not root:
    sys.exit("usage: check_api_refs.py <game-scripts-folder>")
cache = {}

def lines_of(p):
    if p not in cache:
        fp = os.path.join(root, p)
        cache[p] = open(fp, encoding="utf-8", errors="replace").read().splitlines() if os.path.exists(fp) else None
    return cache[p]

def resolve(p, sym):
    ls = lines_of(p)
    if ls is None: return None
    decl = re.compile(r"^\s*(class_name\s+%s\b|signal\s+%s\b|(static\s+)?(@onready\s+|@export\w*(\([^)]*\))?\s+)?var\s+%s\b|(static\s+)?func\s+%s\b|const\s+%s\b)" % ((re.escape(sym),) * 5))
    for i, l in enumerate(ls, 1):
        if decl.search(l): return i
    word = re.compile(r"\b%s\b" % re.escape(sym))
    for i, l in enumerate(ls, 1):
        if word.search(l): return i
    return None

src = open(DEF).read()
bad, total = 0, 0
def fix_refs(m):
    global bad, total
    out = []
    for ref in m.group(1).split("; "):
        if ref == "-": out.append(ref); continue
        mm = re.match(r"^(\S+):(\?|\d+) (\w+)$", ref)
        if not mm: print("MALFORMED", ref); bad += 1; out.append(ref); continue
        p, ln, sym = mm.groups(); total += 1
        ls = lines_of(p)
        ok = ls is not None and ln != "?" and int(ln) <= len(ls) and re.search(r"\b%s\b" % re.escape(sym), ls[int(ln) - 1])
        if not ok:
            r = resolve(p, sym)
            if fix and r: ln = str(r)
            else:
                print("BAD %s:%s %s%s" % (p, ln, sym, " (found at %s)" % r if r else " (not found)")); bad += 1
        out.append("%s:%s %s" % (p, ln, sym))
    return '"%s"' % "; ".join(out)


new = re.sub(r'"((?:game/|-)[^"]*?)"(?=, ")', fix_refs, src)
if fix and new != src:
    open(DEF, "w").write(new)
print("refs checked: %d, bad: %d" % (total, bad))
sys.exit(1 if bad else 0)
