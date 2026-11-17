#!/usr/bin/env python3
"""
Convert the display POKEs and the display activation CALL of a Sharp pocket computer BASIC program
from one pocket computer to another.  Only that: the rest of the program is not touched or checked
(no keyword check, no screen check).

Usage:
    python3 sharp_convert.py input --to PC-1475 [output] [--from PC-126x] [--listing FILE.bas]

    input           BASIC listing (.bas) or tokenized image (.bin, also a .tap tape file)
    --to MODEL      target pocket computer (required)
    --from MODEL    source pocket computer; for an image it is detected if left out, for a listing
                    it is required
    --listing FILE  for an image: also write the BASIC listing that was converted

Only the pocket computers whose display addresses are known can be used (see DISPLAYS below):
PC-1401/02, PC-140x/1460 (1403, 1403H, 1460), PC-1475, PC-125x (1250 ... 1255) and PC-126x
(1260 ... 1262).  A model with a two line display (PC-126x, PC-1475) can only be converted to
another two line model, one line models only to one line models.

What happens
    * The program is searched for POKEs into the display memory of the source and for the CALL that
      activates its display.  If there is none of them, only a message is shown and no file is written.
    * Otherwise they are converted and the report shows, for every changed statement, the source and
      the target statement:
        - POKE into the display memory: moved to the address of the same line, display position and
          column on the target.  The models have different address blocks (the PC-1403 has six, the
          PC-125x two, the PC-1475 four, the PC-126x two): where the bytes of one POKE are not
          continuous on the target, the POKE is split into several POKE statements.
        - CALL that activates the display: changed to the call of the target; removed if the target
          needs none (PC-1475, PC-126x); if the target needs one (PC-1401/02, PC-1403, PC-125x) and the
          program has none, a line with the call is added in front of the first display POKE (in the
          same line if there is no free line number before it, or if the line is jumped to).
    * A POKE with the address in a variable or an expression (POKE Z,X   POKE &2040+I,X) is followed
      through the program: a variable that is only used as POKE address gets its assignments changed
      (Z=&2878 becomes Z=&2A78); a variable that is set once and used elsewhere is replaced by the new
      address in the POKE; &2040+I gets a new first term (&2A00+I) if the area is a plain shift on
      the target.  If the address cannot be determined (the variable changes in loops, is read with
      INPUT, is set in several places and used elsewhere ...) the POKE is listed with the reason.
    * Everything else that could not be changed (a POKE inside IF ... THEN, bytes that the target has no
      place for, CALL with an address in a variable) is listed.
    * The result has to be reviewed manually; the report says so.

The output has the type of the input: a listing gives a listing (NAME_pc<model>.bas), an image gives an
image (NAME_pc<model>.bin; the program is read with the tokens of the source and written with the
tokens of the target).
"""

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))


def _tok():
    import sharp_tok
    return sharp_tok


def pretty(model):
    """'1403' / 'pc-1403' / 'pc-125x' -> menu name of the model or family"""
    key = _tok().family_key(model)
    if key:
        for name in _tok().FAMILY_MENU:
            if name.upper() == "PC-" + key:
                return name
    text = str(model).strip().upper()
    return text if text.startswith("PC-") else "PC-" + text


# ---------------------------------------------------------------------------
# Display memory (POKE) and the call that activates the display
# ---------------------------------------------------------------------------
# Data given by the user.  Per model group: the address ranges of the display memory (first, last,
# which display positions), the call that activates the display (None: the model needs none) and the
# display position that is used for graphics.  Only these models can be converted.
# PC-1475 line 2 positions 1-12 is &2840-&287B (corrected by the user).  The PC-126x ranges are kept as
# given (&2000-&201F, &2800-&28BF) for the detection of display POKEs; its character layout is in BLOCKS.
DISPLAYS = {
    "PC-1401/1402": {
        "numbers": (1401, 1402), "call": 0x05A2, "graphic": None,
        "ranges": [(0x6400, 0x6427, "positions 1-8"), (0x6440, 0x6467, "positions 16-9")]},
    "PC-1403/1403H/1460": {
        "numbers": (1403, 1460), "call": 0x04B8, "graphic": "position 24",
        "ranges": [(0x3000, 0x301D, "positions 1-6"), (0x301E, 0x302C, "positions 10-12"),
                   (0x302D, 0x303B, "positions 7-9"), (0x3040, 0x305D, "positions 24-19"),
                   (0x305E, 0x306C, "positions 15-13"), (0x306D, 0x307B, "positions 18-16")]},
    "PC-1475": {
        "numbers": (1475,), "call": None, "graphic": None,
        "ranges": [(0x2800, 0x283B, "line 1, positions 1-12"), (0x2A00, 0x2A3B, "line 1, positions 13-24"),
                   (0x2840, 0x287B, "line 2, positions 1-12"), (0x2A40, 0x2A7B, "line 2, positions 13-24")]},
    "PC-125x": {
        "numbers": (1250, 1251, 1252, 1253, 1254, 1255), "call": 0x11E0, "graphic": "position 1",
        "ranges": [(0xF800, 0xF83B, "positions 1-12"), (0xF840, 0xF87B, "positions 13-24 (backwards)")]},
    "PC-126x": {
        "numbers": (1260, 1261, 1262), "call": None, "graphic": None,
        "ranges": [(0x2000, 0x201F, "line 1, positions 1-24"), (0x2800, 0x28BF, "line 2, positions 1-24")]},
}


# Character cells of the display memory.  From the table the program builds, for every model, a map
# "address -> (line, display position, column)" and its reverse; a POKE into the display memory is moved
# to the address of the same line / position / column on the other model.  Rules:
#   * a block is (first address, last address as written, line, first position, last position);
#     the number of positions decides its size (5 bytes = 5 columns per position), the written last
#     address is only a check (the PC-1475 line 2 and the PC-126x lines are written shorter or longer);
#   * positions that run against the addresses (PC-1401 "16-9", PC-1403 "24-19", the PC-125x range
#     written &F87B-&F840) mean a block that is stored mirrored, its columns run backwards too;
#   * bytes that the target has no place for (other line count, other number of positions) are not
#     changed and reported.
# Format: (start, end, line, first position, last position)
BLOCKS = {
    "PC-1401/1402": [(0x6400, 0x6427, 1, 1, 8), (0x6440, 0x6467, 1, 16, 9)],
    "PC-1403/1403H/1460": [(0x3000, 0x301D, 1, 1, 6), (0x301E, 0x302C, 1, 10, 12),
                           (0x302D, 0x303B, 1, 7, 9), (0x3040, 0x305D, 1, 24, 19),
                           (0x305E, 0x306C, 1, 15, 13), (0x306D, 0x307B, 1, 18, 16)],
    "PC-1475": [(0x2800, 0x283B, 1, 1, 12), (0x2A00, 0x2A3B, 1, 13, 24),
                (0x2840, 0x287B, 2, 1, 12), (0x2A40, 0x2A7B, 2, 13, 24)],
    "PC-125x": [(0xF800, 0xF83B, 1, 1, 12), (0xF87B, 0xF840, 1, 13, 24)],
    # The table gives two ranges (&2000-&201F, &2800-&28BF).  Programs for the PC-126x use &2040, &204A and
    # &2878-&287B, which a 24 x 5 byte line starting at &2000 does not explain; they fit the layout of the
    # PC-1475 (two blocks of 12 positions = 60 bytes per line), so the PC-126x is taken like that.
    "PC-126x": [(0x2000, 0x203B, 1, 1, 12), (0x2040, 0x207B, 1, 13, 24),
                (0x2800, 0x283B, 2, 1, 12), (0x2840, 0x287B, 2, 13, 24)],
}
CELL = 5                                          # bytes (columns) of one display position
for _name, _blocks in BLOCKS.items():
    DISPLAYS[_name]["blocks"] = _blocks


def _block_cells(block):
    """[(address, (line, position, column))] of one block."""
    start, end, line, first, last = block
    count = abs(last - first) + 1
    pos_step = 1 if last >= first else -1
    addr_step = 1 if end >= start else -1
    # the size comes from the positions; a block written from its high address ends at `start`
    low = start - (count * CELL - 1) if addr_step < 0 else start
    mirrored = count > 1 and pos_step != addr_step
    cells = []
    for off in range(count * CELL):
        k = off // CELL
        if addr_step > 0:
            pos = first + pos_step * k
        else:                                     # written from the high address: the lowest holds `last`
            pos = last - pos_step * k
        col = off % CELL
        cells.append((low + off, (line, pos, CELL - 1 - col if mirrored else col)))
    return cells


def display_maps(d):
    """(address -> cell, cell -> address) of a display entry; empty maps without blocks."""
    cache = d.setdefault("_maps", None)
    if cache is None:
        fwd, rev = {}, {}
        for block in d.get("blocks", []):
            for addr, cell in _block_cells(block):
                fwd[addr] = cell
                rev[cell] = addr
        d["_maps"] = cache = (fwd, rev)
    return cache


def _all_ranges(d):
    """Display ranges of the table plus the memory of the blocks (some ranges are written shorter or
    longer than their positions need)."""
    extra = []
    for block in d.get("blocks", []):
        addrs = [a for a, _c in _block_cells(block)]
        extra.append((min(addrs), max(addrs), "positions %d-%d" % (block[3], block[4])))
    return d["ranges"] + [e for e in extra if e not in d["ranges"]]


def _split_args(text):
    """Arguments of a statement, split at commas outside of strings and brackets."""
    args, cur, inq, depth = [], [], False, 0
    for c in text:
        if c == '"':
            inq = not inq
        elif not inq:
            if c in "([":
                depth += 1
            elif c in ")]":
                depth -= 1
            elif c == "," and depth == 0:
                args.append("".join(cur))
                cur = []
                continue
        cur.append(c)
    args.append("".join(cur))
    return [a.strip() for a in args]


# ---------------------------------------------------------------------------
# Addresses that are given by a variable or an expression:  POKE Z,X   POKE &2040+I,X
# ---------------------------------------------------------------------------
_TOKEN = re.compile(r"&[0-9A-Fa-f]+|\d+\.?\d*|[A-Za-z][A-Za-z0-9]?\$?|[-+*/()]|\S")
_NAME = re.compile(r"(?<![A-Za-z0-9&$])([A-Za-z][A-Za-z0-9]?)(?![A-Za-z0-9$(])")
_ASSIGN = re.compile(r'^(\s*(?:"[^"]*"\s*)?(?:LET\s+)?)([A-Za-z][A-Za-z0-9]?)(\s*=\s*)(.+?)\s*$', re.I | re.S)
_POKE_VAR = re.compile(r'^\s*(?:"[^"]*"\s*)?POKE\s*([A-Za-z][A-Za-z0-9]?)\s*,(.+)$', re.I | re.S)


def _strip_text(st):
    """Statement without strings; '' for REM / DATA statements."""
    plain = re.sub(r'"[^"]*"?', '""', st).strip()
    return "" if re.match(r"(REM|DATA|')", plain, re.I) else plain


def _calc(expr, lookup):
    """Value of a numeric expression (+ - * / and brackets) with constants and variables; the
    variables are asked from lookup(name).  None if anything is unknown."""
    tokens = _TOKEN.findall(expr)
    pos = [0]

    def peek():
        return tokens[pos[0]] if pos[0] < len(tokens) else None

    def take():
        pos[0] += 1
        return tokens[pos[0] - 1]

    def factor():
        t = peek()
        if t is None:
            return None
        take()
        if t == "-":
            v = factor()
            return None if v is None else -v
        if t == "(":
            v = add()
            if peek() != ")":
                return None
            take()
            return v
        if t.startswith("&"):
            return int(t[1:], 16)
        if t[0].isdigit():
            return float(t) if "." in t else int(t)
        if t[0].isalpha() and not t.endswith("$"):
            return lookup(t.upper())
        return None

    def mul():
        v = factor()
        while v is not None and peek() in ("*", "/"):
            op = take()
            w = factor()
            if w is None or (op == "/" and w == 0):
                return None
            v = v * w if op == "*" else v / w
        return v

    def add():
        v = mul()
        while v is not None and peek() in ("+", "-"):
            op = take()
            w = mul()
            if w is None:
                return None
            v = v + w if op == "+" else v - w
        return v

    value = add()
    if value is None or pos[0] != len(tokens):
        return None
    return int(value) if float(value).is_integer() else None


def scan_variables(text):
    """The assignments of the program.  Returns a dict: assigns {name: [(line, statement index, expression)]},
    unknown {names that are changed in a way that is not followed: FOR, INPUT, READ, IF ... THEN V=,
    V=V+1 ...}, occurrences {name: count}, pokes {name: [(line, statement index, number of values)]}."""
    assigns, unknown, occ, pokes = {}, set(), {}, {}
    for raw in text.replace("\x1a", "").replace("\r\n", "\n").replace("\r", "\n").split("\n"):
        m = re.match(r"\s*(\d+)\s?(.*)$", raw)
        if not m:
            continue
        no = int(m.group(1))
        for k, st in enumerate(_statements(m.group(2))):
            plain = _strip_text(st)
            if not plain:
                continue
            for name in _NAME.findall(plain):
                occ[name.upper()] = occ.get(name.upper(), 0) + 1
            up = plain.upper()
            am = _ASSIGN.match(st)
            if am and not re.match(r"\s*(IF|FOR|ON)\b", up):
                name, expr = am.group(2).upper(), am.group(4)
                if '"' in expr or name in {n.upper() for n in _NAME.findall(_strip_text(expr))}:
                    unknown.add(name)                 # a text, or V=V+1: the value is not followed
                else:
                    assigns.setdefault(name, []).append((no, k, expr))
                continue
            pm = _POKE_VAR.match(st)
            if pm:
                pokes.setdefault(pm.group(1).upper(), []).append((no, k, len(_split_args(pm.group(2)))))
            for fm in re.finditer(r"\bFOR\s*([A-Z][A-Z0-9]?)\s*=", up):
                unknown.add(fm.group(1))
            if re.match(r"(INPUT|READ|GET|LINPUT)\b", up):
                unknown.update(n.upper() for n in _NAME.findall(up))
            for tm in re.finditer(r"(?:THEN|ELSE)\s*(?:LET\s+)?([A-Z][A-Z0-9]?)\s*=", up):
                unknown.add(tm.group(1))
    return {"assigns": assigns, "unknown": unknown, "occ": occ, "pokes": pokes}


def _var_value(info, name, depth=0):
    """Value of a variable that is set exactly once in the program to a value that can be calculated."""
    if depth > 6 or name in info["unknown"]:
        return None
    found = info["assigns"].get(name, [])
    if len(found) != 1:
        return None
    return _calc(found[0][2], lambda n: _var_value(info, n, depth + 1))


def _terms(expr):
    """The terms of a sum, split at + outside of brackets."""
    terms, cur, depth = [], [], 0
    for c in expr:
        depth += c == "("
        depth -= c == ")"
        if c == "+" and depth == 0 and "".join(cur).strip():
            terms.append("".join(cur))
            cur = []
        else:
            cur.append(c)
    terms.append("".join(cur))
    return terms


def plan_address_variables(info, src_d, dst_d):
    """Variables that are only used as the address of POKE statements and only get constant display
    addresses: their assignments can simply be changed.  Returns ({(line, index): new expression},
    [(name, reason)] for the variables that cannot be handled this way)."""
    src_fwd, _r = display_maps(src_d)
    _f, dst_rev = display_maps(dst_d)
    plan, why = {}, []
    for name, found in info["assigns"].items():
        if name in info["unknown"] or name not in info["pokes"]:
            continue
        values = [_calc(expr, lambda n: None) for _l, _k, expr in found]
        if None in values or not any(src_fwd.get(v) for v in values):
            continue                                  # not constants, or not display addresses
        if info["occ"].get(name, 0) != len(found) + len(info["pokes"][name]):
            why.append((name, "the variable is also used somewhere else"))
            continue
        longest = max(n for _l, _k, n in info["pokes"][name])
        new_values, bad = [], None
        for v in values:
            targets = [dst_rev.get(src_fwd.get(v + i)) if src_fwd.get(v + i) else None for i in range(longest)]
            if None in targets or any(targets[i + 1] != targets[i] + 1 for i in range(len(targets) - 1)):
                bad = f"{_hex(v)} (and the following {longest - 1} bytes) is not one continuous area on the target"
                break
            new_values.append(targets[0])
        if bad:
            why.append((name, bad))
            continue
        for (l, k, _e), nv in zip(found, new_values):
            plan[(l, k)] = _hex(nv)
    return plan, why


def _resolve_address(arg, info, src_fwd, dst_rev, nvalues):
    """Address of a POKE that is not a plain constant.  Returns (kind, value, note):
    ('const', address, note) when the address can be calculated, ('base', (new first term, rest), note)
    for constant + unknown (POKE &2040+I,...), or ('none', None, reason)."""
    value = _calc(arg, lambda n: _var_value(info, n))
    if value is not None:
        names = sorted({n.upper() for n in _NAME.findall(arg)})
        shown = ", ".join(f"{n} = {_hex(_var_value(info, n))}" for n in names if _var_value(info, n) is not None)
        return "const", value, (f"address from the program: {shown}" if shown else "")
    terms = _terms(arg)
    if len(terms) > 1:
        known = [_calc(t, lambda n: _var_value(info, n)) for t in terms]
        if known.count(None) == 1 and all(isinstance(v, int) for v in known if v is not None):
            base = sum(v for v in known if v is not None)
            rest = terms[known.index(None)].strip()
            cell = src_fwd.get(base)
            if cell is None:
                return "other", None, ""              # not the display memory: not our business
            # the area from the start to the end of its block must be a plain shift on the target
            n, shift_ok = 0, True
            target0 = dst_rev.get(cell)
            while src_fwd.get(base + n) is not None:
                t = dst_rev.get(src_fwd[base + n])
                if t is None or target0 is None or t != target0 + n:
                    shift_ok = False
                    break
                n += 1
            if shift_ok and target0 is not None:
                return "base", (target0, rest), (f"{_hex(base)} + {rest}: moved to {_hex(target0)} + {rest} "
                                                 f"(the area of {n} bytes is a plain shift; {rest} must stay inside it)")
            return "none", None, (f"{_hex(base)} + {rest}: the display area is not a plain shift on the target "
                                  "(it is split or mirrored): convert by hand")
    names = sorted({n.upper() for n in _NAME.findall(_strip_text(arg))})
    where = []
    for n in names:
        found = info["assigns"].get(n, [])
        if n in info["unknown"]:
            where.append(f"{n} changes in the program (FOR / INPUT / {n}={n}+...)")
        elif len(found) > 1:
            where.append(f"{n} is set in {len(found)} places (lines " + ", ".join(str(l) for l, _k, _e in found[:5]) + ")")
        elif not found:
            where.append(f"{n} is never set")
        else:
            where.append(f"{n} = {found[0][2]} (line {found[0][0]}) cannot be calculated")
    return "none", None, "; ".join(where) if where else arg


def remap_display_pokes(text, src_d, dst_d):
    """Move the POKEs into the display memory of the source to the display memory of the target.
    A POKE is split where the target memory is not continuous (one block of the source, several of
    the target); POKE statements that follow each other and become continuous on the target are joined
    (several blocks of the source, one of the target).  An address given by a variable or an
    expression is calculated from the program; a variable that is only used as POKE address gets
    its assignments changed.
    Returns (new text, set of lines that are completely handled,
             [(line, source statement, target statements)], [(line, source statement, reason)],
             [(line, source statement, reason)] for POKEs whose address cannot be determined)."""
    src_fwd, _r = display_maps(src_d)
    _f, dst_rev = display_maps(dst_d)
    info = scan_variables(text)
    plan, plan_failed = plan_address_variables(info, src_d, dst_d)
    handled_vars = {name for name, found in info["assigns"].items()
                    if found and all((l, k) in plan for l, k, _e in found)}
    why_var = dict(plan_failed)
    done, bad_lines, pairs, failed, unresolved, out = set(), set(), [], [], [], []
    for raw in text.replace("\r\n", "\n").replace("\r", "\n").split("\n"):
        m = re.match(r"\s*(\d+)\s?(.*)$", raw)
        if not m:
            out.append(raw)
            continue
        no, parts = int(m.group(1)), _statements(m.group(2))
        conv, raw_new, edited = {}, {}, False
        for k, st in enumerate(parts):
            if (no, k) in plan:                       # V=&2040  ->  V=&2A00
                am = _ASSIGN.match(st)
                new_st = am.group(1) + am.group(2) + am.group(3) + plan[(no, k)]
                pairs.append((no, st.strip(), new_st.strip() + "   (the variable is only used as POKE address)"))
                parts[k] = new_st
                edited = True
                continue
            vm = _POKE_VAR.match(st)
            if vm and vm.group(1).upper() in handled_vars:
                done.add(no)
                continue
            pm = re.match(r'(\s*(?:"[^"]*"\s*)?)POKE\s*(.*)$', st, re.I | re.S)   # a label "T" may stand in front
            if not pm:
                continue
            args = _split_args(pm.group(2))
            values = args[1:]
            if not args[0] or not values:
                continue
            addr, note = _const(args[0]), ""
            if addr is None:
                kind, value, note = _resolve_address(args[0], info, src_fwd, dst_rev, len(values))
                if kind == "base":                    # POKE &2040+I,...
                    first, rest = value
                    new_st = pm.group(1) + "POKE " + _hex(first) + "+" + rest + "," + ",".join(values)
                    pairs.append((no, st.strip(), new_st.strip() + "   (" + note + ")"))
                    raw_new[k] = new_st
                    continue
                if kind == "other":
                    continue
                if kind == "none":                    # could be a display POKE: say so
                    names = [n.upper() for n in _NAME.findall(_strip_text(args[0]))]
                    why = next((why_var[n] for n in names if n in why_var), None)
                    unresolved.append((no, st.strip(), (why and f"the variable {why}") or note))
                    continue
                addr = value
            last = addr + len(values) - 1
            if not any(addr <= b and last >= a for a, b, _t in _all_ranges(src_d)):
                continue
            targets, missing = [], []
            for n in range(len(values)):
                cell = src_fwd.get(addr + n)
                target = dst_rev.get(cell) if cell else None
                targets.append(target)
                if target is None:
                    missing.append(addr + n)
            if missing:
                failed.append((no, st.strip(), "not changed: " + ", ".join(_hex(a) for a in missing[:6])
                               + (" ..." if len(missing) > 6 else "")
                               + " has no place in the display memory of the target"))
                bad_lines.add(no)
                continue
            runs, start = [], 0
            for n in range(1, len(values) + 1):
                if n == len(values) or targets[n] != targets[n - 1] + 1:
                    runs.append([targets[start], values[start:n]])
                    start = n
            conv[k] = [pm.group(1), st.strip() + (f"   [{note}]" if note else ""), runs]
        # join POKE statements that follow each other when the target addresses are continuous
        anchor, last = None, None
        for k in sorted(conv):
            if (anchor is not None and last == k - 1 and not conv[k][0].strip()
                    and conv[anchor][2][-1][0] + len(conv[anchor][2][-1][1]) == conv[k][2][0][0]):
                conv[anchor][2][-1][1] = conv[anchor][2][-1][1] + conv[k][2][0][1]
                conv[k][2].pop(0)
                if conv[k][2]:
                    anchor = k
            else:
                anchor = k
            last = k
        for k, (prefix, src_st, runs) in conv.items():
            new_st = ":".join("POKE " + _hex(a) + "," + ",".join(v) for a, v in runs)
            pairs.append((no, src_st, new_st if runs else "(joined with the POKE before it)"))
        if (conv or raw_new) and no not in bad_lines:
            done.add(no)
        if conv or raw_new or edited:
            keep = []
            for k, st in enumerate(parts):
                if k in raw_new:
                    st = raw_new[k]
                elif k in conv:
                    prefix, _src, runs = conv[k]
                    if not runs:
                        continue
                    st = prefix + ":".join("POKE " + _hex(a) + "," + ",".join(v) for a, v in runs)
                keep.append(st)
            raw = raw[:m.start(2)] + ":".join(keep)
        out.append(raw)
    return "\n".join(out), done, pairs, failed, unresolved


def display_of(model):
    """(name, entry) of the display data of a model, or (None, None)."""
    number = _tok().parse_model(model)
    for name, entry in DISPLAYS.items():
        if number in entry["numbers"]:
            return name, entry
    return None, None


def _hex(value):
    return f"&{value:04X}"


def _const(text):
    """Value of a numeric constant (&F800 or 1234), else None."""
    t = text.strip()
    try:
        if t.startswith("&"):
            return int(t[1:], 16)
        if t.isdigit():
            return int(t)
    except ValueError:
        pass
    return None


def _const_args(parts, idx):
    """Arguments after the keyword at parts[idx] up to the end of the statement: list of int,
    or None where the argument is not a plain constant."""
    args, cur = [], []
    for kind, text in parts[idx + 1:]:
        if kind == "sep" or (kind == "kw" and text.upper() in ("THEN", "ELSE")):
            break
        if kind == "comma" and text == ",":
            args.append(cur)
            cur = []
        else:
            cur.append((kind, text))
    args.append(cur)
    return [_const(a[0][1]) if len(a) == 1 and a[0][0] == "num" else None for a in args if a]


def display_scan(text, src_keywords):
    """POKE and CALL statements of the listing.  -> list of (line, 'poke', first, last),
    (line, 'call', address) and (line, 'dynamic', keyword)."""
    import sharp_rem
    events = []
    for raw in text.replace("\x1a", "").replace("\r\n", "\n").replace("\r", "\n").split("\n"):
        m = re.match(r"\s*(\d+)\s?(.*)$", raw)
        if not m:
            continue
        no = int(m.group(1))
        parts = sharp_rem.pieces(m.group(2), src_keywords, 0)
        for idx, (kind, word) in enumerate(parts):
            if kind != "kw" or word.upper() not in ("POKE", "CALL"):
                continue
            args = _const_args(parts, idx)
            if not args or args[0] is None:
                events.append((no, "dynamic", word.upper()))
            elif word.upper() == "CALL":
                events.append((no, "call", args[0]))
            else:
                events.append((no, "poke", args[0], args[0] + max(1, len(args) - 1) - 1))
    return events


def _replace_call(body, old, new_text):
    """Change CALL <old> to CALL <new_text> outside of strings, comments and DATA."""
    out, i, n, inq, changed = [], 0, len(body), False, False
    while i < n:
        c = body[i]
        if c == '"':
            inq = not inq
        elif not inq:
            if c == "'":
                out.append(body[i:])
                break
            word_start = i == 0 or not body[i - 1].isalpha()
            if word_start and body[i:i + 3].upper() == "REM":
                out.append(body[i:])
                break
            if word_start and body[i:i + 4].upper() == "DATA":
                j, q = i + 4, False
                while j < n and (q or body[j] != ":"):
                    if body[j] == '"':
                        q = not q
                    j += 1
                out.append(body[i:j])
                i = j
                continue
            if word_start and body[i:i + 4].upper() == "CALL":
                m = re.match(r"(CALL\s*)(&[0-9A-Fa-f]+|\d+)", body[i:], re.I)
                if m and _const(m.group(2)) == old:
                    out.append(m.group(1) + new_text)
                    i += m.end()
                    changed = True
                    continue
        out.append(c)
        i += 1
    return "".join(out), changed


def _statements(body):
    """Split a line body at ':' outside of strings.  A comment (REM, ') or DATA statement keeps
    the rest of the line."""
    parts, cur, inq, i, n = [], [], False, 0, len(body)
    while i < n:
        c = body[i]
        if c == '"':
            inq = not inq
        elif not inq:
            head = "".join(cur).strip().upper()
            if c == ":":
                parts.append("".join(cur))
                cur = []
                i += 1
                continue
            if not head and (body[i:i + 3].upper() == "REM" or c == "'" or body[i:i + 4].upper() == "DATA"):
                if body[i:i + 4].upper() == "DATA":
                    j, q = i, False
                    while j < n and (q or body[j] != ":"):
                        if body[j] == '"':
                            q = not q
                        j += 1
                    cur.append(body[i:j])
                    i = j
                    continue
                cur.append(body[i:])
                break
        cur.append(c)
        i += 1
    parts.append("".join(cur))
    return parts


def _is_call(statement, address):
    m = re.fullmatch(r"\s*CALL\s*(&[0-9A-Fa-f]+|\d+)\s*", statement, re.I)
    return bool(m) and _const(m.group(1)) == address


def _poke_statement(statements):
    """Index of the first statement that contains POKE (outside of strings)."""
    for k, st in enumerate(statements):
        plain = re.sub(r'"[^"]*"?', '""', st).upper()
        if re.search(r"(?<![A-Z])POKE", plain):
            return k
    return None


def _jump_targets(text, src_keywords):
    """Line numbers that a GOTO / GOSUB / ... of the program refers to."""
    import sharp_info
    found = set()
    for raw in text.replace("\r\n", "\n").replace("\r", "\n").split("\n"):
        m = re.match(r"\s*(\d+)\s?(.*)$", raw)
        if m:
            try:
                for _kw, target in sharp_info.scan_body(m.group(2), src_keywords, 0)[0]:
                    if target is not None:
                        found.add(target)
            except Exception:
                pass
    return found


def _display_lines(d):
    return max(block[2] for block in d["blocks"])


# Menu names of the models that can be converted, and the entry of DISPLAYS of each
MODEL_ORDER = ["PC-1401/02", "PC-140x/1460", "PC-1475", "PC-125x", "PC-126x"]
_MENU_ENTRY = {"PC-1401/02": "PC-1401/1402", "PC-140x/1460": "PC-1403/1403H/1460", "PC-1475": "PC-1475",
               "PC-125x": "PC-125x", "PC-126x": "PC-126x"}


def model_names():
    return list(MODEL_ORDER)


def supported_text():
    return ", ".join(MODEL_ORDER)


def menu_name(model):
    """Menu name of a model of the table, or None if its display addresses are not known."""
    try:
        name, entry = display_of(model)
    except ValueError:
        return None
    if entry is None:
        return None
    return next(m for m in MODEL_ORDER if _MENU_ENTRY[m] == name)


def targets_for(model):
    """Menu names the model can be converted to: the other models with the same number of display
    lines (two line displays only to two line displays)."""
    name = menu_name(model)
    if name is None:
        return list(MODEL_ORDER)
    lines = _display_lines(DISPLAYS[_MENU_ENTRY[name]])
    return [m for m in MODEL_ORDER if m != name and _display_lines(DISPLAYS[_MENU_ENTRY[m]]) == lines]


def check_models(src_model, dst_model):
    """Raise ValueError if the pair cannot be converted."""
    src, dst = menu_name(src_model), menu_name(dst_model)
    for given, found in ((src_model, src), (dst_model, dst)):
        if found is None:
            raise ValueError(f"{given}: the display addresses of this model are not known; Convert model "
                             f"works with {supported_text()}")
    if src == dst:
        raise ValueError(f"{src_model} and {dst_model} have the same display; nothing to convert")
    if dst not in targets_for(src):
        raise ValueError(f"{src} has a {_display_lines(DISPLAYS[_MENU_ENTRY[src]])} line display and "
                         f"{dst} a {_display_lines(DISPLAYS[_MENU_ENTRY[dst]])} line display: a program "
                         f"can only be converted between displays with the same number of lines "
                         f"(from {src} to {', '.join(targets_for(src))})")
    return src, dst


def _line_body(text, no):
    for raw in text.replace("\r\n", "\n").replace("\r", "\n").split("\n"):
        m = re.match(r"\s*(\d+)\s?(.*)$", raw)
        if m and int(m.group(1)) == no:
            return m.group(2).strip()
    return ""


def display_changes(text, src_model, dst_model, src_keywords):
    """Convert the display POKEs and CALLs of a listing.  Returns a dict:
    text, found (anything of the display detected), pokes / calls [(line, source, target)],
    added [(line, statement, comment)], unchanged [(line, source, reason)], dynamic [lines], notes."""
    src_name, src_d = display_of(src_model)
    dst_name, dst_d = display_of(dst_model)
    res = {"text": text, "found": False, "pokes": [], "calls": [], "added": [], "unchanged": [],
           "dynamic": [], "notes": [], "src": src_name, "dst": dst_name}
    new_text, remapped, pairs, failed, unresolved = remap_display_pokes(text, src_d, dst_d)
    res["pokes"] = pairs
    res["unresolved"] = unresolved
    res["unchanged"] += [(no, st, why) for no, st, why in failed]
    failed_lines = {no for no, _st, _why in failed}
    calls, removes = {}, {}
    has_dst_call, first_poke, touched = False, None, bool(pairs or failed)
    ranges = _all_ranges(src_d)
    for ev in display_scan(text, src_keywords):
        no, kind = ev[0], ev[1]
        if kind == "dynamic":
            if ev[2] == "CALL" and no not in res["dynamic"]:
                res["dynamic"].append(no)             # a POKE with a variable address: see "unresolved"
        elif kind == "poke":
            if any(ev[2] <= b and ev[3] >= a for a, b, _l in ranges):
                touched = True
                if first_poke is None:
                    first_poke = no
                if no not in remapped and no not in failed_lines:
                    res["unchanged"].append((no, _line_body(text, no)[:70],
                                             "not changed: the POKE is not a plain POKE statement "
                                             "(for example inside IF ... THEN / ELSE)"))
        elif kind == "call":
            if src_d["call"] is not None and ev[2] == src_d["call"]:
                touched = True
                if dst_d["call"] is not None:
                    calls[no] = src_d["call"]
                else:
                    removes[no] = src_d["call"]
            if dst_d["call"] is not None and ev[2] == dst_d["call"]:
                has_dst_call = True
    res["found"] = touched
    if not touched:
        return res
    add_call = dst_d["call"] is not None and first_poke is not None and not has_dst_call and not calls
    text = new_text
    lines = text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    targets = _jump_targets(text, src_keywords) if (removes or add_call) else set()
    numbers = sorted(int(m.group(1)) for m in (re.match(r"\s*(\d+)", l) for l in lines) if m)
    out = []
    for raw in lines:
        m = re.match(r"\s*(\d+)\s?(.*)$", raw)
        no = int(m.group(1)) if m else None
        if m and no in calls:
            new_call = _hex(dst_d["call"])
            body, changed = _replace_call(m.group(2), calls[no], new_call)
            if changed:
                raw = raw[:m.start(2)] + body
                res["calls"].append((no, f"CALL {_hex(calls[no])}", f"CALL {new_call}"))
        elif m and no in removes:
            parts = _statements(m.group(2))
            kept = [st for st in parts if not _is_call(st, removes[no])]
            src_st = f"CALL {_hex(removes[no])}"
            if len(kept) == len(parts):
                res["unchanged"].append((no, src_st, f"not removed: it is part of IF ... THEN / ELSE; "
                                         f"{dst_name} needs no call - remove it by hand"))
            else:
                body = ":".join(kept).strip()
                if not body and no in targets:
                    body = "REM"
                    res["calls"].append((no, src_st, "removed (the line is jumped to, so it stays as REM)"))
                elif not body:
                    res["calls"].append((no, src_st, "removed (the line held only this call: deleted)"))
                    continue
                else:
                    res["calls"].append((no, src_st, "removed"))
                raw = raw[:m.start(2)] + body
        elif m and add_call and no == first_poke:
            call_text = f"CALL {_hex(dst_d['call'])}"
            before = [n for n in numbers if n < no]
            low = before[-1] if before else 0
            if no not in targets and no - low > 1:
                new_no = low + (no - low) // 2
                out.append(f"{new_no} {call_text}")
                res["added"].append((new_no, call_text, f"new line in front of the first display POKE "
                                     f"(line {no}); {dst_name} needs it to activate the display"))
            else:
                parts = _statements(m.group(2))
                k = _poke_statement(parts)
                parts.insert(k or 0, call_text)
                raw = raw[:m.start(2)] + ":".join(parts)
                res["added"].append((no, call_text, "added in front of the first display POKE in the same "
                                     "line (" + ("the line is jumped to" if no in targets
                                                 else "no free line number before it")
                                     + f"); {dst_name} needs it to activate the display"))
        out.append(raw)
    res["text"] = "\n".join(out)
    if src_d["graphic"] != dst_d["graphic"]:
        def _g(name, g):
            return f"{name}: display {g}" if g else f"{name}: no position reserved for graphics"
        res["notes"].append("graphics differ (" + _g(src_name, src_d["graphic"]) + "; "
                            + _g(dst_name, dst_d["graphic"]) + ") - text or graphics there may need to move")
    return res


def readback(source_text, image, model):
    """Compare the new image (read with the target model) with the program."""
    import sharp_info
    import sharp_rem
    try:
        text, _used = sharp_info.detokenize_image(image, model)
    except ValueError as exc:
        return [f"the new image could not be read back: {exc}"]

    def lines(t):
        out = {}
        for raw in t.replace("\x1a", "").replace("\r\n", "\n").replace("\r", "\n").split("\n"):
            m = re.match(r"\s*(\d+)\s?(.*)$", raw)
            if m:
                out[int(m.group(1))] = sharp_rem.loose(m.group(2))
        return out
    a, b = lines(source_text), lines(text)
    return [f"line {no} reads differently: {a.get(no)!r} -> {b.get(no)!r}"
            for no in sorted(set(a) | set(b)) if a.get(no) != b.get(no)]


def tokenize_for(text, model):
    """-> (image bytes or None, message)"""
    import sharp_info
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        src, dst = Path(tmp) / "p.bas", Path(tmp) / "p.bin"
        src.write_text(text, encoding="utf-8")
        rc, msg = sharp_info._quiet_tok(["tok", "--model", model, str(src), str(dst)])
        if rc != 0 or not dst.exists():
            lines = [ln for ln in msg.strip().splitlines() if ln.strip()]
            return None, (lines[-1] if lines else "tokenizing failed")
        notes = [ln[6:].strip() for ln in msg.splitlines() if ln.startswith("note:")]
        return dst.read_bytes(), " ".join(notes)


def load_source(path, model=None):
    """-> (listing text, source model name, image bytes or None, notes)"""
    import sharp_info
    data = Path(path).read_bytes()
    notes = []
    if is_image_file(path, data):
        import sharp_tap
        if sharp_tap.looks_like_tape(data):
            data = sharp_tap.unframe(data)["payload"]
            notes.append("tape file: header and checksums removed")
        text, used = sharp_info.detokenize_image(data, model)
        if not model:
            notes.append(f"source model detected as {used} (use --from if it is another one)")
        return text, used, data, notes
    if not model:
        raise ValueError("a listing needs --from (the pocket computer it was written for)")
    return data.decode("utf-8", errors="replace"), model, None, notes


def is_image_file(path, data=None):
    """True for a tokenized file (by its name or, for other names, by its bytes)."""
    if Path(path).suffix.lower() in (".bin", ".img", ".tap"):
        return True
    try:
        data = Path(path).read_bytes() if data is None else data
    except OSError:
        return False
    return b"\x00" in data


def convert(text, src_model, dst_model, source_size=None):
    """Convert the display POKEs / CALLs of a listing.  Returns a dict: report (lines), text (the
    converted listing), image (bytes or None), found (bool), written (True if there is a result)."""
    import sharp_info
    src, dst = check_models(src_model, dst_model)
    src_model, dst_model = src, dst
    src_group, src_num = sharp_info.model_group(src_model)
    src_keywords = sharp_info.keyword_names(src_group, src_num)
    res = display_changes(text, src_model, dst_model, src_keywords)
    rep = [f"Convert {src_model} -> {dst_model}  (display POKE and CALL only)"]
    if not res["found"]:
        rep.append(f"No POKE into the display memory of {res['src']} and no CALL that activates its "
                   "display was found in the program: nothing to convert, no file written.")
        if res["dynamic"]:
            rep.append("CALL with an address in a variable is not checked (line "
                       + ", ".join(str(n) for n in res["dynamic"][:10]) + ").")
        for no, st, why in res["unresolved"]:
            rep.append(f"  line {no}  {st}")
            rep.append(f"  {' ' * len(str(no))}       address not determined: {why}")
        return {"report": rep, "text": text, "image": None, "found": False, "written": False}
    rep.append(f"Found: {len(res['pokes']) + sum(1 for u in res['unchanged'] if 'POKE' in u[1].upper())} "
               f"POKE statement(s) into the display memory and {len(res['calls'])} display activation CALL(s)"
               f"; {src_model} display addresses -> {dst_model} display addresses.")
    rep.append("")
    rep.append("Source and target statements:")
    for no, a, b in res["pokes"]:
        rep.append(f"  line {no}  source: {a}")
        rep.append(f"  {' ' * len(str(no))}       target: {b}")
    for no, a, b in res["calls"]:
        rep.append(f"  line {no}  source: {a}")
        rep.append(f"  {' ' * len(str(no))}       target: {b}")
    for no, st, why in res["added"]:
        rep.append(f"  line {no}  added:  {st}   ({why})")
    for no, st, why in res["unchanged"]:
        rep.append(f"  line {no}  source: {st}")
        rep.append(f"  {' ' * len(str(no))}       {why}")
    for no, st, why in res["unresolved"]:
        rep.append(f"  line {no}  source: {st}")
        rep.append(f"  {' ' * len(str(no))}       not changed, address not determined: {why}")
    if res["dynamic"]:
        rep.append("  CALL with an address in a variable is not checked (line "
                   + ", ".join(str(n) for n in res["dynamic"][:10]) + ").")
    for note in res["notes"]:
        rep.append("Note: " + note)
    rep.append("")
    rep.append("REVIEW MANUALLY: the display POKEs and CALLs were converted automatically from the table of "
               "display addresses; check every line above on the target (display position, graphics, "
               "program flow) before using the program.")
    image = None
    if source_size is not None:                       # an image comes in: an image goes out
        image, msg = tokenize_for(res["text"], dst_model)
        if image is None:
            raise ValueError("the converted program could not be tokenized for " + dst_model + ": " + msg)
        rep.append(f"Image: read with the tokens of {src_model}, written with the tokens of {dst_model} "
                   f"({source_size} -> {len(image)} bytes); the rest of the program is not checked.")
        for diff in readback(res["text"], image, dst_model)[:5]:
            rep.append("Warning: " + diff)
    return {"report": rep, "text": res["text"], "image": image, "found": True, "written": True}


def default_output(path, dst_model, image=True):
    """next to the input, same type as the input: NAME_pc125x.bin for an image,
    NAME_pc125x.bas (the extension of the input) for a listing"""
    tag = re.sub(r"[^a-z0-9]+", "_", re.sub(r"^PC-?", "", str(dst_model).strip().upper()).lower()).strip("_")
    p = Path(path)
    ext = ".bin" if image else (p.suffix if p.suffix.lower() not in ("", ".bin", ".img", ".tap") else ".bas")
    return str(p.with_name(f"{p.stem}_pc{tag}{ext}"))


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Convert the display POKEs and the display activation CALL of a Sharp BASIC program "
                    "to another pocket computer (" + supported_text() + ").",
        usage="sharp_convert.py input --to MODEL [output] [--from MODEL] [--listing FILE.bas]")
    ap.add_argument("input", help="listing (.bas) or image (.bin / .tap)")
    ap.add_argument("output", nargs="?", help="default: NAME_pc<model>.bas for a listing, .bin for an image")
    ap.add_argument("--to", dest="to", required=True, help="target pocket computer: " + supported_text())
    ap.add_argument("--from", dest="src", default=None, help="source pocket computer")
    ap.add_argument("--listing", default=None, help="for an image: also write the converted listing here")
    args = ap.parse_args(argv)
    try:
        text, src_model, image, notes = load_source(args.input, args.src)
        result = convert(text, src_model, args.to, len(image) if image is not None else None)
    except (OSError, ValueError, SystemExit) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    for note in notes:
        print("Note: " + note)
    print("\n".join(result["report"]))
    if not result["written"]:
        return 0
    is_image = image is not None
    out = args.output or default_output(args.input, args.to, is_image)
    if Path(out).resolve() == Path(args.input).resolve():
        print("error: the output file would overwrite the input file", file=sys.stderr)
        return 1
    if is_image:
        Path(out).write_bytes(result["image"])
    else:
        listing = result["text"]
        if "\r\n" in text and "\r\n" not in listing:
            listing = listing.replace("\n", "\r\n")
        Path(out).write_bytes(listing.encode("utf-8"))
    print(f"Written: {out}")
    if args.listing and is_image:
        Path(args.listing).write_text(result["text"], encoding="utf-8")
        print(f"Listing: {args.listing}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
