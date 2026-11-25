#!/usr/bin/env python3
"""One-shot migration helper (kept for the record / future libraries): rewrites direct game
script access in a library source file into CORE game IO calls.

  RECV->get("name")                   -> IO.get("cap", RECV, "name")
  RECV->set("name", v)                -> IO.set("cap", RECV, "name", v)
  RECV->call("name", ...)             -> IO.call("cap", RECV, "name", ...)
  RECV->connect("sig", ...)           -> IO.connect("cap", RECV, "sig", ...)
  RECV->disconnect("sig", ...)        -> IO.disconnect("cap", RECV, "sig", ...)
  RECV->get_node_or_null(NodePath("P")) -> IO.node("cap", RECV, "P")

Only names listed in the mapping (name -> capability) are rewritten; engine built-ins stay.
Usage: io_rewrite.py <file> <IO-expr> <mapping.json>
"""
import json, re, sys

OPS = ("get", "set", "call", "connect", "disconnect", "get_node_or_null")

def match_back(s, i, open_c, close_c):
    depth = 0
    while i >= 0:
        if s[i] == close_c: depth += 1
        elif s[i] == open_c:
            depth -= 1
            if depth == 0: return i
        i -= 1
    raise ValueError("unbalanced")

def receiver_start(s, end):
    """s[end] is the '-' of '->'. Return start index of the receiver expression."""
    i = end - 1
    while i >= 0 and s[i] == ' ': i -= 1
    while True:
        if s[i] in ')]':
            i = match_back(s, i, '(' if s[i] == ')' else '[', s[i]) - 1
            if i >= 0 and s[i] == '>':
                i = match_back(s, i, '<', '>') - 1
        j = i
        while j >= 0 and (s[j].isalnum() or s[j] == '_'): j -= 1
        i = j

        if i >= 1 and s[i-1:i+1] == '->': i -= 2; continue
        if i >= 0 and s[i] == '.' : i -= 1; continue
        if i >= 1 and s[i-1:i+1] == '::': i -= 2; continue
        return i + 1

def match_fwd(s, i):
    depth = 0
    instr = False
    while i < len(s):
        c = s[i]
        if instr:
            if c == '\\': i += 2; continue
            if c == '"': instr = False
        elif c == '"': instr = True
        elif c == '(': depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0: return i
        i += 1
    raise ValueError("unbalanced fwd")

def rewrite(src, io, mapping):
    pat = re.compile(r'->(%s)\((NodePath\()?"([^"]+)"' % "|".join(OPS))
    out = []; pos = 0; n = 0; skipped = {}
    for m in pat.finditer(src):
        if m.start() < pos: continue
        op, wrapped, name = m.group(1), m.group(2), m.group(3)
        cap = mapping.get(name)
        if cap is None and name.startswith("/root/"): cap = None
        if cap is None:
            skipped[name] = skipped.get(name, 0) + 1
            continue
        rs = receiver_start(src, m.start())
        recv = src[rs:m.start()]
        open_paren = m.start() + 2 + len(op)
        close = match_fwd(src, open_paren)
        inner = src[open_paren + 1:close]

        if wrapped:
            lit_end = inner.index('")') + 2
        else:
            lit_end = inner.index('"', 1) + 1
        rest = inner[lit_end:]
        fn = "node" if op == "get_node_or_null" else op
        new = '%s.%s("%s", %s, "%s"%s)' % (io, fn, cap, recv.strip(), name, rest)
        out.append(src[pos:rs]); out.append(new); pos = close + 1; n += 1
    out.append(src[pos:])
    return "".join(out), n, skipped

if __name__ == "__main__":
    f, io, mp = sys.argv[1], sys.argv[2], json.load(open(sys.argv[3]))
    s = open(f).read()
    s2, n, sk = rewrite(s, io, mp)
    open(f, "w").write(s2)
    print(f"{f}: {n} rewritten; untouched names: {sk}")
