#!/usr/bin/env python3
# tests/make_gallery_catalog.py — make src/gallery_catalog.h, the list of the
# game's enemies the Gallery shows, from what the autopilot met.
#
#   tests/run_autoplay.sh-like runs with GALLERY_DUMP=<file.tsv> (see
#   tests/test_autoplay.c), then:
#   tests/make_gallery_catalog.py <out.h> <e1-d2-r0.tsv> <more.tsv>...
#
# Each line of a .tsv is one thing the Gallery recorded in a level (an enemy,
# or the pieces of a boss or a building): episode, level, destroyed, then
# bank:sprite:size:x:y:enemyDat index:slot of each piece.  The list keeps one
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
            if len(cols) < 4:
                continue
            try:
                parts = [tuple(int(v) for v in c.split(':')) for c in cols[3:]]
            except ValueError:
                continue  # cut short (the run still going)
            if any(len(p) != 7 for p in parts):
                continue
            things.append({'episode': int(cols[0]), 'level': cols[1].strip(), 'parts': parts,
                           'order': order_base + n})
    return things


def normalize(parts):
    mx = min(p[3] for p in parts)
    my = min(p[4] for p in parts)
    out = [(p[0], p[1], p[2], p[3] - mx, p[4] - my, p[5], p[6]) for p in parts]
    out.sort(key=lambda p: (p[4], p[3], p[0], p[1], p[2]))
    return out


def look(parts):
    return tuple((p[0], p[1], p[2], p[3], p[4]) for p in parts)


NEAR = 4  # px: rows of a building that scrolled in on different frames


def pair_counts(things):
    # how often each piece type is met, and each two types in the same thing
    seen_type = Counter()
    seen_pair = Counter()
    for t in things:
        types = sorted({p[5] for p in t['parts']})
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
            if together(seen_type, seen_pair, a[5], b[5]) and touch(a, b):
                group[root(j)] = root(i)
    out = defaultdict(list)
    for i, p in enumerate(ps):
        out[root(i)].append(p)
    return list(out.values())


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

        # every enemy met, by its types: the look it has most often
        forms = defaultdict(Counter)
        first = {}
        for t in ts:
            for piece in split(t, seen_type, seen_pair):
                parts = clean(piece)
                types = frozenset(p[5] for p in parts)
                forms[types][look(parts)] += 1
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
                entries[lk] = {'look': lk, 'types': {(ds, t) for t in s}, 'order': first[s][0], 'episode': first[s][1]}
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

    # a piece of a bigger one (same enemy and the same pictures, mostly) that
    # came apart from it: into it
    pics = lambda e: {(p[0], p[1], p[2]) for p in e['look']}
    big_first = sorted(entries.values(), key=lambda e: -len(e['look']))
    for i, e in enumerate(big_first):
        for k in big_first[:i]:
            if len(k['look']) > len(e['look']) and 'into' not in k and e['types'] & k['types'] and \
                    2 * len(pics(e) & pics(k)) >= len(pics(e)):
                e['into'] = k
                k['types'] |= e['types']
                if e['order'] < k['order']:
                    k['order'], k['episode'] = e['order'], e['episode']
                break

    kept = [e for e in entries.values() if e['look'] not in drop and 'into' not in e]
    kept.sort(key=lambda e: (e['episode'], e['order']))
    parts_out, enemies_out, types_out = [], [], []
    for i, e in enumerate(kept):
        enemies_out.append((e['episode'], len(e['look']), len(parts_out)))
        parts_out += e['look']
        for ds, t in sorted(e['types']):
            types_out.append((ds, t, i))
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

#include "SDL_types.h"

typedef struct
{
	Uint8 bank, size;
	Uint16 gr;
	Sint16 x, y;
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

''')
        f.write('static const CatalogPart catalogParts[] =\n{\n')
        for p in parts_out:
            f.write('\t{ %d, %d, %d, %d, %d },\n' % (p[0], p[2], p[1], p[3], p[4]))
        f.write('};\n\nstatic const CatalogEnemy catalogEnemies[] =\n{\n')
        for e in enemies_out:
            f.write('\t{ %d, %d, %d },\n' % e)
        f.write('};\n\nstatic const CatalogType catalogTypes[] =\n{\n')
        for t in types_out:
            f.write('\t{ %d, %d, %d },\n' % t)
        f.write('};\n\n#endif /* GALLERY_CATALOG_H */\n')

    print('%d things, %d enemies in the list, %d pieces' % (len(things), len(kept), len(parts_out)))


main()
