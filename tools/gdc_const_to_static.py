#!/usr/bin/env python3
"""BOX-ONLY prototype of the CORE 'player.consts' patcher (the shipped version would be C++ doing the
same thing in memory from the game's own res://game/player/player.gdc; nothing of the game is shipped).
Rewrites top-level `const NAME` -> `static var NAME` for the given names in a Godot 4.3+ binary-token
GDScript (GDSC v100/101). Token type ids are learned from the file itself (no engine enum baked in):
  CONST  = type of the token before the IDENTIFIER of a known top-level const
  STATIC/VAR = types of the two tokens before IDENTIFIER 'FEEL' (`static var FEEL`)
Fails closed (exit 2) if any pattern is not found exactly as expected.
usage: gdc_const_to_static.py in.gdc out.gdc NAME [NAME...]                     (static var, global)
       gdc_const_to_static.py --per-player in.gdc out.gdc NAME... --defunc FUNC... (instance var, per player)
       gdc_const_to_static.py --swap in.gdc out.gdc OWNER MEMBER REPL               (OWNER.MEMBER -> REPL.MEMBER)"""
import struct, sys, zstandard

def u32(b, o): return struct.unpack_from('<I', b, o)[0]
def die(m): print('FAIL-CLOSED:', m); sys.exit(2)

def parse(raw):
    if raw[:4] != b'GDSC': die('not GDSC')
    ver, dsize = u32(raw, 4), u32(raw, 8)
    if ver not in (100, 101): die('tokenizer version %d' % ver)
    c = zstandard.ZstdDecompressor().decompress(raw[12:], max_output_size=dsize) if dsize else raw[12:]
    if dsize and len(c) != dsize: die('size')
    nid, ncon, nlines, ntok = struct.unpack_from('<4I', c, 0); o = 16
    ids = []
    for _ in range(nid):
        n = u32(c, o); o += 4
        ids.append(''.join(chr(u32(bytes(x ^ 0xb6 for x in c[o + 4*j:o + 4*j + 4]), 0)) for j in range(n))); o += 4*n
    con_start = o


    for _ in range(ncon): o += vlen(c, o)
    consts_blob = c[con_start:o]
    lines = [struct.unpack_from('<2I', c, o + 8*i) for i in range(nlines)]; o += 8*nlines
    cols = [struct.unpack_from('<2I', c, o + 8*i) for i in range(nlines)]; o += 8*nlines
    toks = []
    for _ in range(ntok):
        if c[o] & 0x80: t = u32(c, o) & ~0x80; ln = u32(c, o + 4); o += 8; w = 8
        else: t = c[o]; ln = u32(c, o + 1); o += 5; w = 5
        toks.append([t, ln, w])
    if o != len(c): die('trailing bytes %d' % (len(c) - o))
    return ver, ids, ncon, consts_blob, lines, cols, toks

TOKEN_BITS = 8; TOKEN_MASK = (1 << (TOKEN_BITS - 1)) - 1
def vlen(c, o):
    t = u32(c, o) & 0xffff; flags = u32(c, o) >> 16
    wide = flags & 1



    f = 8 if wide else 4
    fixed = {0: 0, 1: 4, 2: 8 if wide else 4, 3: f, 5: 2*f, 6: 8, 7: 4*f, 8: 16, 9: 3*f, 10: 12, 11: 6*f, 12: 4*f,
             13: 16, 14: 4*f, 15: 4*f, 16: 6*f, 17: 9*f, 18: 12*f, 19: 16*f, 20: 16}
    if t in fixed: return 4 + fixed[t]
    if t in (4, 21):
        n = u32(c, o + 4); return 8 + n + ((4 - n % 4) % 4)
    die('constant of variant type %d not handled' % t)

def build(ver, ids, ncon, consts_blob, lines, cols, toks):
    c = bytearray(struct.pack('<4I', len(ids), ncon, len(lines), len(toks)))
    for s in ids:
        c += struct.pack('<I', len(s))
        for ch in s: c += bytes(x ^ 0xb6 for x in struct.pack('<I', ord(ch)))
    c += consts_blob
    for k, v in lines: c += struct.pack('<2I', k, v)
    for k, v in cols: c += struct.pack('<2I', k, v)
    for t, ln, w in toks:
        c += struct.pack('<II', t | 0x80, ln) if (w == 8 or t > 0x7f) else bytes([t]) + struct.pack('<I', ln)
    return b'GDSC' + struct.pack('<II', ver, len(c)) + zstandard.ZstdCompressor(level=19).compress(bytes(c))

def main():
    src, dst, names = sys.argv[1], sys.argv[2], sys.argv[3:]
    ver, ids, ncon, blob, lines, cols, toks = parse(open(src, 'rb').read())
    IDENT = None
    idx = {s: i for i, s in enumerate(ids)}
    if 'FEEL' not in idx: die('no FEEL identifier')

    def is_ident(tok, name): return (tok[0] >> TOKEN_BITS) == idx[name] and (tok[0] & TOKEN_MASK) == IDENT

    cand = [t[0] & TOKEN_MASK for t in toks if (t[0] >> TOKEN_BITS) == idx['FEEL'] and t[0] >> TOKEN_BITS]
    IDENT = max(set(cand), key=cand.count)
    feel = [i for i, t in enumerate(toks) if is_ident(t, 'FEEL') and i >= 2]
    STATIC, VAR = toks[feel[0] - 2][0], toks[feel[0] - 1][0]
    if not (STATIC <= TOKEN_MASK and VAR <= TOKEN_MASK): die('static/var not keywords')
    CONST = None; done = []
    out = []; shift = []
    pos_of = {}
    for n in names:
        if n not in idx: die('const %s not in script (game updated?)' % n)
        hits = [i for i, t in enumerate(toks) if is_ident(t, n) and i > 0 and toks[i-1][0] not in (STATIC, VAR)
                and (CONST is None or toks[i-1][0] == CONST)]
        decl = [i for i in hits if toks[i-1][0] & TOKEN_MASK == toks[i-1][0] and toks[i-1][0] not in (IDENT,)]

        decl = [i for i in decl if (i - 1) in dict(lines)]
        if len(decl) != 1: die('const %s: %d declarations' % (n, len(decl)))
        if CONST is None: CONST = toks[decl[0]-1][0]
        if toks[decl[0]-1][0] != CONST: die('const %s: unexpected keyword' % n)
        pos_of[decl[0]-1] = n
    if len(set(pos_of)) != len(names): die('dup')
    new = []; remap = {}
    for i, t in enumerate(toks):
        remap[i] = len(new)
        if i in pos_of:
            new.append([STATIC, t[1], toks[feel[0]-2][2]]); new.append([VAR, t[1], toks[feel[0]-1][2]])
        else: new.append(t)
    lines2 = [(remap[k], v) for k, v in lines]; cols2 = [(remap[k], v) for k, v in cols]
    open(dst, 'wb').write(build(ver, ids, ncon, blob, lines2, cols2, new))
    print('ok: %d consts -> static var; tokens %d -> %d; CONST=%d STATIC=%d VAR=%d IDENT=%d' % (len(names), len(toks), len(new), CONST, STATIC, VAR, IDENT))


def _learn(toks, ids):
    idx = {s: i for i, s in enumerate(ids)}
    return idx

def ident_type(toks, idx, name):
    cand = [t[0] & TOKEN_MASK for t in toks if (t[0] >> TOKEN_BITS) == idx[name]]
    return max(set(cand), key=cand.count)

def per_player(src, dst, names, defunc):
    """const NAME -> var NAME (instance member: per-player values; one token swapped, nothing inserted)
    and `static func F` -> `func F` for the static helpers that read those names (STATIC token removed).
    Token ids learned from the file: VAR from `static var FEEL`, STATIC likewise, CONST from the first decl."""
    ver, ids, ncon, blob, lines, cols, toks = parse(open(src, 'rb').read())
    idx = {s: i for i, s in enumerate(ids)}
    if 'FEEL' not in idx: die('no FEEL identifier')
    IDENT = ident_type(toks, idx, 'FEEL')
    isid = lambda t, n: (t[0] >> TOKEN_BITS) == idx[n] and (t[0] & TOKEN_MASK) == IDENT
    feel = [i for i, t in enumerate(toks) if isid(t, 'FEEL') and i >= 2][0]
    STATIC, VAR = toks[feel - 2][0], toks[feel - 1][0]
    ls = dict(lines); CONST = None; FUNC = None
    for n in names:
        if n not in idx: die('const %s missing (game updated?)' % n)
        d = [i for i, t in enumerate(toks) if isid(t, n) and i > 0 and (i - 1) in ls and toks[i-1][0] <= TOKEN_MASK and toks[i-1][0] not in (STATIC, VAR)]
        if len(d) != 1: die('const %s: %d declarations' % (n, len(d)))
        if CONST is None: CONST = toks[d[0]-1][0]
        if toks[d[0]-1][0] != CONST: die('const %s: unexpected keyword' % n)
        toks[d[0]-1][0] = VAR
    drop = set()
    for f in defunc:
        if f not in idx: die('func %s missing' % f)
        d = [i for i, t in enumerate(toks) if isid(t, f) and i > 1 and toks[i-2][0] == STATIC and (i - 2) in ls]
        if len(d) != 1: die('static func %s: %d declarations' % (f, len(d)))
        if FUNC is None: FUNC = toks[d[0]-1][0]
        if toks[d[0]-1][0] != FUNC: die('static func %s: unexpected keyword' % f)
        drop.add(d[0] - 2)
    new = []; remap = {}
    for i, t in enumerate(toks):
        if i in drop: remap[i] = len(new); continue
        remap[i] = len(new); new.append(t)
    lines2 = [(remap[k], v) for k, v in lines]; cols2 = [(remap[k], v) for k, v in cols]
    open(dst, 'wb').write(build(ver, ids, ncon, blob, lines2, cols2, new))
    print('per-player ok: %d consts -> var, %d static funcs -> func; tokens %d -> %d' % (len(names), len(defunc), len(toks), len(new)))

def swap_ident_after(src, dst, owner, member, repl):
    """`owner.member` -> `repl.member` (e.g. hitbox_view: Player.BLOCK_TAPE_SLACK -> body.BLOCK_TAPE_SLACK)."""
    ver, ids, ncon, blob, lines, cols, toks = parse(open(src, 'rb').read())
    idx = {s: i for i, s in enumerate(ids)}
    for n in (owner, member, repl):
        if n not in idx: die('identifier %s missing' % n)
    IDENT = ident_type(toks, idx, member)
    isid = lambda t, n: (t[0] >> TOKEN_BITS) == idx[n] and (t[0] & TOKEN_MASK) == IDENT
    hits = [i for i in range(len(toks) - 2) if isid(toks[i], owner) and isid(toks[i+2], member)]
    if not hits: die('%s.%s not found' % (owner, member))
    for i in hits: toks[i][0] = IDENT | (idx[repl] << TOKEN_BITS)
    open(dst, 'wb').write(build(ver, ids, ncon, blob, lines, cols, toks))
    print('swap ok: %d x %s.%s -> %s.%s' % (len(hits), owner, member, repl, member))

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--per-player':
        a = sys.argv[2:]; src, dst = a[0], a[1]; rest = a[2:]
        k = rest.index('--defunc') if '--defunc' in rest else len(rest)
        per_player(src, dst, rest[:k], rest[k+1:])
    elif len(sys.argv) > 1 and sys.argv[1] == '--swap':
        swap_ident_after(*sys.argv[2:7])
    else: main()
