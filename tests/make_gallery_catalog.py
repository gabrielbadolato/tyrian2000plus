#!/usr/bin/env python3
# tests/make_gallery_catalog.py — make src/gallery_catalog.h, the list of the
# game's enemies the Gallery shows, from what the autopilot met.
#
#   tests/run_autoplay.sh-like runs with GALLERY_DUMP=<file.tsv> (see
#   tests/test_autoplay.c), then:
#   tests/make_gallery_catalog.py [--drop=<file>] [--frames=<file>]
#       [--fix=tests/gallery_catalog_fix.txt] <out.h> <e1-d2-r0.tsv> <more.tsv>...
#
# --frames: the enemies' animations (test_gallery <data> frames=<file>);
# --fix: what a look at every entry found (see that file).
#
# Each line of a .tsv is one thing the Gallery recorded in a level (an enemy,
# or the pieces of a boss or a building): episode, level, destroyed, then
# bank:sprite:size:x:y:enemyDat index:slot of each piece, and
# @frame:x:y:y on the map (when and where it came).  The list keeps one
# entry for each enemy: the same pieces seen in many runs are one; a part of
# a boss seen apart (it came first, or stayed) and a variant with a piece
# missing go into the whole; pieces drawn on top of each other once; a group
# of the same enemy (a formation, a row of tiles) is that enemy once.  The
# first file gives the order (the order the enemies are met).
#
# Copyright (C) 2026 Tyrian 2000+ contributors
# GPL 2 or later, as the rest of the program.
import sys
from collections import Counter, defaultdict

MAP_SLOT = 25  # slots 25-99: on the map (ground, top)


def dataset(episode):
    # episodes 1-3 share the enemy data of tyrian.hdt; 4 and 5 have their own
    return 1 if episode <= 3 else episode


def read(path, order_base):
    things = []
    with open(path) as f:
        for n, line in enumerate(f):
            cols = line.rstrip('\n').split('\t')
            made = None
            if cols[-1].startswith('@'):
                # the frame it came and where: things that came together
                v = [int(v) for v in cols.pop()[1:].split(':')]
                made = ((order_base, v[0]), v[1], v[2])
                # (and, since a later run, where on the map: v[3])
                map_dy = v[3] - v[2] if len(v) > 3 else None
            if len(cols) < 4:
                continue
            try:
                parts = [tuple(int(v) for v in c.split(':')) for c in cols[3:]]
            except ValueError:
                continue  # cut short (the run still going)
            if any(len(p) != 7 for p in parts):
                continue
            things.append({'episode': int(cols[0]), 'level': cols[1].strip(), 'parts': parts,
                           'order': order_base + n, 'made': made, 'map_dy': map_dy if made else None})
    return join_made_together(things)


def join_made_together(things):
    # things in the sky that came in the same frame are looked at together
    # (a boss and its arms, when the level does not link them): where they
    # are one from the other says if they are one
    out, frame = [], {}
    for t in things:
        if t['made'] is None:
            out.append(t)
            continue
        key, x, y = t['made']
        # (and which thing each piece was: see split)
        parts = [(p[0], p[1], p[2], p[3] + x, p[4] + y, p[5], p[6], t['order']) for p in t['parts']]
        if any(p[6] >= MAP_SLOT for p in t['parts']):
            out.append(dict(t, parts=parts))  # where it is, for join_pairs
            continue
        j = frame.get(key)
        if j is None:
            frame[key] = len(out)
            out.append(dict(t, parts=parts))
        else:
            out[j]['parts'] = out[j]['parts'] + parts
    return out


def normalize(parts):
    mx = min(p[3] for p in parts)
    my = min(p[4] for p in parts)
    out = [(p[0], p[1], p[2], p[3] - mx, p[4] - my, p[5], p[6]) for p in parts]
    out.sort(key=lambda p: (p[4], p[3], p[0], p[1], p[2]))
    return out


def look(parts):
    return tuple((p[0], p[1], p[2], p[3], p[4]) for p in parts)


NEAR = 4  # px: rows of a building that scrolled in on different frames


def origin(p):
    return p[7] if len(p) > 7 else None


def pair_counts(things):
    # how often each piece type is met, and each two types in the same thing
    seen_type = Counter()
    seen_pair = Counter()
    for t in things:
        for thing in {origin(p) for p in t['parts']}:
            types = sorted({p[5] for p in t['parts'] if origin(p) == thing})
            seen_type.update(types)
            for i, a in enumerate(types):
                for b in types[i + 1:]:
                    seen_pair[(a, b)] += 1
    return seen_type, seen_pair


def together(seen_type, seen_pair, ta, tb):
    # two pieces are one enemy when, of the times either is met, the other is
    # there at least half of them (where it is can change: a part that moves)
    if ta == tb:
        return False  # a row of the same enemy is that enemy, many times
    n = seen_pair[(min(ta, tb), max(ta, tb))]
    return 2 * n >= seen_type[ta] and 2 * n >= seen_type[tb]


def touch(a, b):
    # the pictures meet (or almost): pieces apart are not one picture
    w = lambda p: 24 if p[2] else 12
    h = lambda p: 28 if p[2] else 14
    return (a[3] < b[3] + w(b) + NEAR and b[3] < a[3] + w(a) + NEAR and
            a[4] < b[4] + h(b) + NEAR and b[4] < a[4] + h(a) + NEAR)


def sky_offsets(things):
    # pieces in the sky that meet, of two kinds: where one is from the other
    count = defaultdict(Counter)
    for t in things:
        ps = [p for p in t['parts'] if p[6] < MAP_SLOT]
        for i, a in enumerate(ps):
            for b in ps[i + 1:]:
                if a[5] != b[5]:
                    if a[5] > b[5]:
                        a, b = b, a
                    # (where they do not meet: None)
                    count[(a[5], b[5])][(b[3] - a[3], b[4] - a[4]) if touch(a, b) else None] += 1
    fixed = {}
    for pair, offs in count.items():
        off, n = offs.most_common(1)[0]
        if off is None:
            continue
        # always the same place: two parts of one picture (a boss's hull and
        # its arm), even when they came apart elsewhere
        if n >= 3 and 10 * n >= 9 * sum(offs.values()):
            fixed[pair] = off
    return fixed


SKY_FIXED = {}


def fixed_together(a, b):
    if a[6] >= MAP_SLOT or b[6] >= MAP_SLOT or a[5] == b[5]:
        return False
    if a[5] > b[5]:
        a, b = b, a
    return SKY_FIXED.get((a[5], b[5])) == (b[3] - a[3], b[4] - a[4])


def split(t, seen_type, seen_pair):
    # the thing in the enemies it is made of (pieces that are always together)
    ps = t['parts']
    group = list(range(len(ps)))

    def root(i):
        while group[i] != i:
            i = group[i]
        return i

    for i, a in enumerate(ps):
        for j in range(i + 1, len(ps)):
            b = ps[j]
            # (pieces of things that only came in the same frame: only if
            # always in the same place)
            if (origin(a) == origin(b) and together(seen_type, seen_pair, a[5], b[5]) and touch(a, b)) or \
                    fixed_together(a, b):
                group[root(j)] = root(i)
    out = defaultdict(list)
    for i, p in enumerate(ps):
        out[root(i)].append(p)
    return list(out.values())


RIGID_GAP = 32  # px: the most a boss's arm can be apart from its body


def gap(a, b):
    # px between two groups of pieces (0: they meet)
    w = lambda p: 24 if p[2] else 12
    h = lambda p: 28 if p[2] else 14
    return min(max(q[3] - (p[3] + w(p)), p[3] - (q[3] + w(q)), q[4] - (p[4] + h(p)), p[4] - (q[4] + h(q)), 0)
               for p in a for q in b)


def kind(group):
    return frozenset(p[5] for p in group)


def rigid_pairs(things, seen_type, seen_pair):
    # enemies that come together, always in the same place one from the other
    # (the arms of a boss, apart from its body), are one: of the things with
    # either one, at least half have both, the same distance apart
    n_kind = Counter()
    n_pair = Counter()
    for t in things:
        gs = split(t, seen_type, seen_pair)
        kinds = {kind(g) for g in gs}
        n_kind.update(kinds)
        pairs = set()
        for i, a in enumerate(gs):
            for b in gs[i + 1:]:
                ka, kb = kind(a), kind(b)
                if ka == kb or gap(a, b) > RIGID_GAP:
                    continue
                if sorted(ka) > sorted(kb):
                    a, b, ka, kb = b, a, kb, ka
                at = min((p[3], p[4]) for p in a)
                bt = min((p[3], p[4]) for p in b)
                pairs.add((ka, kb, bt[0] - at[0], bt[1] - at[1]))
        n_pair.update(pairs)
    best = {}
    for (ka, kb, dx, dy), n in n_pair.items():
        if 2 * n >= n_kind[ka] and 2 * n >= n_kind[kb]:
            best[(ka, kb)] = (dx, dy)
    return best


def join_map(ts, groups):
    # the pieces of a big thing on the map (the rows of a boss built into the
    # map) can come as things of their own, frames apart: where they are on
    # the map says if they meet.  Two kinds of piece that, every time they
    # are in the same level, meet always in the same place are one enemy.
    def layer(p):
        return 'top' if 50 <= p[6] < 75 else 'ground'

    visits = defaultdict(list)  # (run, level, layer): (group, its pieces on the map)
    for ti, (t, gs) in enumerate(zip(ts, groups)):
        if t.get('map_dy') is None or not t['parts'] or t['parts'][0][6] < MAP_SLOT:
            continue
        for gi, g in enumerate(gs):
            on_map = [(p, (p[3], p[4] + t['map_dy'])) for p in g]
            visits[(t['made'][0][0], t['level'], layer(g[0]))].append(((ti, gi), on_map))

    def meeting(items):
        # pieces of two groups that meet: (a, b, offset), by a grid
        grid = defaultdict(list)
        for k, (_, on_map) in enumerate(items):
            for p, (x, y) in on_map:
                grid[(x // 32, y // 32)].append((k, p, x, y))
        for (cx, cy), cell in grid.items():
            near = [q for dx in (-1, 0, 1) for dy in (-1, 0, 1) for q in grid.get((cx + dx, cy + dy), ())]
            for k, p, x, y in cell:
                for k2, p2, x2, y2 in near:
                    if k2 <= k or p[5] == p2[5]:
                        continue
                    a, b = (p[:3] + (x, y) + p[5:], p2[:3] + (x2, y2) + p2[5:])
                    if touch(a, b):
                        if a[5] < b[5]:
                            yield k, k2, a, b, (b[3] - a[3], b[4] - a[4])
                        else:
                            yield k2, k, b, a, (a[3] - b[3], a[4] - b[4])

    meet, together_n = defaultdict(Counter), Counter()
    for items in visits.values():
        for (a, b, off) in {(a[5], b[5], off) for _, _, a, b, off in meeting(items)}:
            meet[(a, b)][off] += 1
    for items in visits.values():
        n = Counter(p[5] for _, on_map in items for p, _ in on_map)
        for (a, b) in meet:
            together_n[(a, b)] += n[a] * n[b]
    fixed = {}
    for pair, offs in meet.items():
        off, k = offs.most_common(1)[0]
        if k >= 2 and 10 * k >= 9 * together_n[pair]:
            fixed[pair] = off

    out = []
    for items in visits.values():
        group = list(range(len(items)))

        def root(i):
            while group[i] != i:
                i = group[i]
            return i
        for k, k2, a, b, off in meeting(items):
            if fixed.get((a[5], b[5])) == off:
                group[root(k2)] = root(k)
        joined = defaultdict(list)
        for k, (_, on_map) in enumerate(items):
            joined[root(k)].append([p[:3] + xy + p[5:] for p, xy in on_map])
        out += [sum(gs, []) for gs in joined.values() if len(gs) > 1]
    return out


def size(group):
    w = lambda p: 24 if p[2] else 12
    h = lambda p: 28 if p[2] else 14
    return (max(p[3] + w(p) for p in group) - min(p[3] for p in group),
            max(p[4] + h(p) for p in group) - min(p[4] for p in group))


def corner(group):
    return min(p[3] for p in group), min(p[4] for p in group)


def join_pairs(ts, groups):
    # two halves (a boss's left and right arms) that come in the same frame,
    # always the same distance apart, however far: one enemy.  Halves: side by
    # side, as many pieces and as big, of two pieces at least.
    by_frame = defaultdict(list)
    for t, gs in zip(ts, groups):
        if t['made'] is not None:
            by_frame[t['made'][0]] += [g for g in gs if len(g) >= 2]
    n_kind = Counter(kind(g) for gs in groups for g in gs)
    n_pair = Counter()
    for gs in by_frame.values():
        for i, a in enumerate(gs):
            for b in gs[i + 1:]:
                if kind(a) != kind(b) and len(a) == len(b) and size(a) == size(b):
                    if sorted(kind(a)) > sorted(kind(b)):
                        a, b = b, a
                    (ax, ay), (bx, by) = corner(a), corner(b)
                    if abs(by - ay) <= 4:  # side by side
                        n_pair[(kind(a), kind(b), bx - ax, by - ay)] += 1
    pairs = {(ka, kb): (dx, dy) for (ka, kb, dx, dy), n in n_pair.items()
             if 10 * n >= 9 * n_kind[ka] and 10 * n >= 9 * n_kind[kb]}
    out, alias = [], set()
    for gs in by_frame.values():
        used = set()
        for i, a in enumerate(gs):
            for j in range(i + 1, len(gs)):
                b = gs[j]
                if i in used or j in used:
                    continue
                x, y = (a, b) if sorted(kind(a)) <= sorted(kind(b)) else (b, a)
                off = pairs.get((kind(x), kind(y)))
                (xx, xy), (yx, yy) = corner(x), corner(y)
                if off == (yx - xx, yy - xy):
                    used |= {i, j}
                    if size(a + b)[0] <= WIDEST:
                        out.append(a + b)
                    else:
                        alias.add((kind(x), kind(y)))  # too far apart to show: one of them
    return out, alias


WIDEST = 260  # px: wider does not fit the Gallery's picture


def split_rigid(t, seen_type, seen_pair, rigid):
    gs = split(t, seen_type, seen_pair)
    joined = True
    while joined:
        joined = False
        for i, a in enumerate(gs):
            for j in range(i + 1, len(gs)):
                b = gs[j]
                ka, kb = kind(a), kind(b)
                x, y = (a, b) if sorted(ka) <= sorted(kb) else (b, a)
                off = rigid.get((kind(x), kind(y)))
                if off is None or gap(a, b) > RIGID_GAP:
                    continue
                xt = min((p[3], p[4]) for p in x)
                yt = min((p[3], p[4]) for p in y)
                if (yt[0] - xt[0], yt[1] - xt[1]) == off:
                    gs[i] = a + b
                    del gs[j]
                    joined = True
                    break
            if joined:
                break
    return gs


def clean(parts):
    # the same picture drawn again (almost) on top of itself: once
    kept = []
    for p in parts:
        if any(q[0] == p[0] and q[1] == p[1] and q[2] == p[2] and abs(q[3] - p[3]) <= NEAR and abs(q[4] - p[4]) <= NEAR
               for q in kept):
            continue
        kept.append(p)
    return normalize(kept)


def main():
    args = sys.argv[1:]
    drop = set()
    if args[0].startswith('--drop='):
        # pictures that show nothing (or almost) in the Gallery, one per line
        with open(args.pop(0)[7:]) as f:
            for line in f:
                if line.strip():
                    drop.add(tuple(tuple(int(v) for v in c.split(':')) for c in line.split()))
    anim = {}
    if args[0].startswith('--frames='):
        # how each enemy is animated (test_gallery <data> frames=<file>):
        # dataset, enemyDat index, ani, animate, the 20 pictures
        with open(args.pop(0)[9:]) as f:
            for line in f:
                v = [int(x) for x in line.split()]
                if v[3] in (1, 2) and v[2] > 1:
                    anim[(v[0], v[1])] = (v[3], v[4:4 + min(v[2], 20)])
    fixes = []
    if args[0].startswith('--fix='):
        # what a look at every entry found (tests/gallery_catalog_fix.txt):
        # "hide 1:66" (not shown), "split 1:66" (each group of pieces that
        # touch is an enemy of its own), "rows", "into", "stack" (see below),
        # by the enemyDat index of a piece
        with open(args.pop(0)[6:]) as f:
            for line in f:
                words = line.split('#')[0].split()
                if words:
                    fixes.append((words[0], [tuple(int(v) for v in w.split(':')) for w in words[1:]]))
    out_path = args[0]
    things = []
    for i, path in enumerate(args[1:]):
        things += read(path, i * 1000000)

    by_dataset = defaultdict(list)
    for t in things:
        by_dataset[dataset(t['episode'])].append(t)

    entries = {}  # by look: an enemy of episodes 1-3 met again in 4 or 5 is one
    for ds, ts in sorted(by_dataset.items()):
        seen_type, seen_pair = pair_counts(ts)
        SKY_FIXED.clear()
        SKY_FIXED.update(sky_offsets(ts))
        rigid = rigid_pairs(ts, seen_type, seen_pair)

        # every enemy met, by its types: the look it has most often
        forms = defaultdict(Counter)
        sample = {}  # a look: its pieces, with their enemyDat index
        first = {}
        groups = [split_rigid(t, seen_type, seen_pair, rigid) for t in ts]
        joined, alias = join_pairs(ts, groups)
        joined += join_map(ts, groups)
        for t, gs in zip(ts + [None] * len(joined), groups + [[g] for g in joined]):
            if t is None:
                t = {'order': 1 << 40, 'episode': 9}
            for piece in gs:
                parts = clean(piece)
                types = frozenset(p[5] for p in parts)
                forms[types][look(parts)] += 1
                sample[look(parts)] = parts
                if types not in first or t['order'] < first[types][0]:
                    first[types] = (t['order'], t['episode'])

        # a part of another seen apart (the rest came later, or not at all):
        # into the whole
        sets = sorted(forms, key=lambda s: (-len(s), first[s][0]))
        whole = {}
        for s in sets:
            for big in sets:
                if len(big) <= len(s):
                    break
                if s < big and big not in whole:
                    whole[s] = big
                    break
        for s in sets:
            if s in whole:
                continue
            lk, n = forms[s].most_common(1)[0]
            e = entries.get(lk)  # another enemy that looks the same: once
            if e is None:
                entries[lk] = {'look': lk, 'types': {(ds, t) for t in s}, 'order': first[s][0], 'episode': first[s][1],
                               'parts': [(ds,) + p for p in sample[lk]]}
            else:
                e['types'] |= {(ds, t) for t in s}
                if first[s][0] < e['order']:
                    e['order'], e['episode'] = first[s]
        for s, big in whole.items():
            while big in whole:
                big = whole[big]
            for e in entries.values():
                if {(ds, t) for t in big} <= e['types']:
                    e['types'] |= {(ds, t) for t in s}
                    if first[s][0] < e['order']:
                        e['order'], e['episode'] = first[s]
                    break

        for ka, kb in alias:
            a = next((e for e in entries.values() if {(ds, t) for t in ka} <= e['types']), None)
            b = next((e for e in entries.values() if {(ds, t) for t in kb} <= e['types']), None)
            if a is not None and b is not None and a is not b and 'into' not in b:
                b['into'] = a
                a['types'] |= b['types']
                if b['order'] < a['order']:
                    a['order'], a['episode'] = b['order'], b['episode']

    # a piece of a bigger one (same enemy and the same pictures, mostly) that
    # came apart from it: into it
    pics = lambda e: {(p[0], p[1], p[2]) for p in e['look']}
    # (or all its pictures are in it: the same part, another enemyDat entry)
    same_set = lambda e, k: {ds for ds, _ in e['types']} & {ds for ds, _ in k['types']}
    big_first = sorted(entries.values(), key=lambda e: -len(e['look']))
    for i, e in enumerate(big_first):
        for k in big_first[:i]:
            if len(k['look']) > len(e['look']) and 'into' not in k and \
                    ((e['types'] & k['types'] and 2 * len(pics(e) & pics(k)) >= len(pics(e))) or
                     (same_set(e, k) and pics(e) <= pics(k))):
                e['into'] = k
                k['types'] |= e['types']
                if e['order'] < k['order']:
                    k['order'], k['episode'] = e['order'], e['episode']
                break

    entries = {k: e for k, e in entries.items() if 'into' not in e}
    for what, keys in fixes:
        e = next((e for e in entries.values() if keys[0] in e['types']), None)
        if e is None:
            print('fix: nothing has %d:%d' % keys[0])
            continue
        if what == 'stack':
            # rows that come one by one in the sky, one under the other, each
            # centred on the first: one enemy (the pieces never meet when made)
            rows = [next((r for r in entries.values() if k in r['types']), None) for k in keys]
            if None in rows or len({id(r) for r in rows}) < len(rows):
                print('fix: cannot stack %s' % ' '.join('%d:%d' % k for k in keys))
                continue
            width = lambda ps: max(p[4] + (24 if p[3] else 12) for p in ps)
            height = lambda ps: max(p[5] + (28 if p[3] else 14) for p in ps)
            mid, y, parts = width(rows[0]['parts']) // 2, 0, []
            for r in rows:
                dx = mid - width(r['parts']) // 2
                parts += [p[:4] + (p[4] + dx, p[5] + y) + p[6:] for p in r['parts']]
                y += height(r['parts'])
                del entries[r['look']]
            lk = look(clean([p[1:] for p in parts]))
            early = min(rows, key=lambda r: r['order'])
            entries[lk] = dict(e, look=lk, parts=parts, types=set().union(*(r['types'] for r in rows)),
                               order=early['order'], episode=early['episode'])
        elif what == 'into':
            # "into 1:66 1:70": the enemy with 1:70 is a part of the one with 1:66
            b = next((r for r in entries.values() if keys[1] in r['types']), None)
            if b is None or b is e:
                print('fix: cannot put %d:%d into %d:%d' % (keys[1] + keys[0]))
                continue
            del entries[b['look']]
            e['types'] |= b['types']
            if b['order'] < e['order']:
                e['order'], e['episode'] = b['order'], b['episode']
        elif what == 'hide':
            e['hidden'] = True
        elif what == 'once':
            # the same enemy twice, side by side: once
            del entries[e['look']]
            seen, parts = set(), []
            for p in e['parts']:
                if (p[1], p[2], p[3]) not in seen:
                    seen.add((p[1], p[2], p[3]))
                    parts.append(p)
            lk = look(clean([p[1:] for p in parts]))
            if lk in entries:
                entries[lk]['types'] |= e['types']
            else:
                entries[lk] = dict(e, look=lk, parts=parts)
        elif what in ('split', 'rows', 'banks'):
            # "split": each group of pieces that touch is an enemy of its own;
            # "rows": each row of them (things in a line, one over the other);
            # "banks": each group of pieces from the same sprite bank
            del entries[e['look']]
            ps = e['parts']
            group = list(range(len(ps)))

            def root(i):
                while group[i] != i:
                    i = group[i]
                return i
            for i, a in enumerate(ps):
                for j in range(i + 1, len(ps)):
                    if touch(a[1:], ps[j][1:]) and (what == 'split' or (a[5] == ps[j][5] if what == 'rows' else a[1] == ps[j][1])):
                        group[root(j)] = root(i)
            comps = defaultdict(list)
            for i, p in enumerate(ps):
                comps[root(i)].append(p)
            comps = sorted(comps.values(), key=len, reverse=True)
            # a group with only pictures of a bigger one is a piece of it
            gpics = lambda c: {(p[1], p[2], p[3]) for p in c}
            kept_comps = []
            for comp in comps:
                big = next((k for k in kept_comps if gpics(comp) <= gpics(k[0])), None)
                if big is None:
                    kept_comps.append([comp, {(p[0], p[6]) for p in comp}])
                else:
                    big[1] |= {(p[0], p[6]) for p in comp}
            kept_comps[0][1] |= set(e['types']) - {(p[0], p[6]) for p in ps}
            taken = set()
            for comp, types in kept_comps:  # each type once, in the biggest
                types -= taken
                taken |= types
                lk = look(clean([p[1:] for p in comp]))
                if lk in entries:
                    entries[lk]['types'] |= types
                else:
                    entries[lk] = dict(e, look=lk, types=types, parts=comp)

    kept = [e for e in entries.values() if e['look'] not in drop and not e.get('hidden')]
    hidden = {t for e in entries.values() if e.get('hidden') for t in e['types']} - \
        {t for e in kept for t in e['types']}
    kept.sort(key=lambda e: (e['episode'], e['order']))
    parts_out, enemies_out, types_out = [], [], []
    frames_out = [(0, [])]  # 0: not animated
    for i, e in enumerate(kept):
        enemies_out.append((e['episode'], len(e['look']), len(parts_out)))
        for lp in e['look']:
            # its animation: the pictures of its enemyDat entry, from the one it shows
            src = next((p for p in e['parts'] if (p[1], p[2], p[3]) == lp[:3]), None)
            a = anim.get((src[0], src[6])) if src is not None else None
            n = 0
            if a is not None and lp[1] in a[1]:
                k = a[1].index(lp[1])
                seq = (a[0], a[1][k:] + a[1][:k])
                if seq not in frames_out:
                    frames_out.append(seq)
                n = frames_out.index(seq)
            parts_out.append(lp + (n,))
        for ds, t in sorted(e['types']):
            types_out.append((ds, t, i))
    types_out += [(ds, t, 0xFFFF) for ds, t in hidden]
    types_out.sort()

    with open(out_path, 'w') as f:
        f.write('''/*
 * Tyrian 2000+: Tyrian 2000 with modern controls, based on OpenTyrian2000
 * Copyright (C) 2026 Tyrian 2000+ contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
// The list of the game's enemies for the Gallery (src/gallery.c), made by
// tests/make_gallery_catalog.py from the levels the autopilot played; do not
// edit by hand.  Only numbers: sprite banks and positions, and the enemyDat
// index of the enemies each entry is made of.
#ifndef GALLERY_CATALOG_H
#define GALLERY_CATALOG_H

#define CATALOG_HIDDEN 0xFFFF  // CatalogType.enemy of a piece the Gallery does not show

#include "SDL_types.h"

typedef struct
{
	Uint8 bank, size;
	Uint16 gr;
	Sint16 x, y;
	Uint16 anim;  // in catalogAnims (0: not animated)
} CatalogPart;

typedef struct
{
	Uint8 episode;  // where it is met first
	Uint8 parts;
	Uint16 first;   // in catalogParts
} CatalogEnemy;

typedef struct
{
	Uint8 dataset;  // 1: episodes 1-3; 4, 5
	Uint16 type;    // enemyDat index
	Uint16 enemy;   // in catalogEnemies
} CatalogType;

typedef struct
{
	Uint8 when;    // 1: always, 2: when it fires (as enemyDat animate)
	Uint8 count;
	Uint16 first;  // in catalogFrames: the pictures, one a frame (999: none)
} CatalogAnim;

''')
        f.write('static const CatalogPart catalogParts[] =\n{\n')
        for p in parts_out:
            f.write('\t{ %d, %d, %d, %d, %d, %d },\n' % (p[0], p[2], p[1], p[3], p[4], p[5]))
        f.write('};\n\nstatic const CatalogEnemy catalogEnemies[] =\n{\n')
        for e in enemies_out:
            f.write('\t{ %d, %d, %d },\n' % e)
        f.write('};\n\nstatic const CatalogType catalogTypes[] =\n{\n')
        for t in types_out:
            f.write('\t{ %d, %d, %d },\n' % t)
        f.write('};\n\nstatic const CatalogAnim catalogAnims[] =\n{\n')
        first = 0
        for when, seq in frames_out:
            f.write('\t{ %d, %d, %d },\n' % (when, len(seq), first))
            first += len(seq)
        f.write('};\n\nstatic const Uint16 catalogFrames[] =\n{\n')
        for when, seq in frames_out[1:]:
            f.write('\t%s,\n' % ', '.join(str(g) for g in seq))
        if len(frames_out) == 1:
            f.write('\t0\n')
        f.write('};\n\n#endif /* GALLERY_CATALOG_H */\n')

    print('%d things, %d enemies in the list, %d pieces' % (len(things), len(kept), len(parts_out)))


main()
