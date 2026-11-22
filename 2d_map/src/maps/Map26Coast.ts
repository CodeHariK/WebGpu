// Colour Blocks — the coastlines as closed lines (for the JSON export): marching squares on the
// island's coast distance grid (Map25Shape: negative on land, GRID pixels apart).
//
// Each grid square whose 4 corners are not all land or all sea has the coast crossing two (or, at
// a saddle, four) of its sides; where along a side comes from the two corners' distances (linear).
// The crossings are joined square to square into closed loops (the map's edge is sea, so every
// loop closes), pulled straight a little (Ramer–Douglas–Peucker), and turned so land is always on
// the left (walking a loop, x right and y down): an island's coast runs one way round, a lake's
// shore the other.
import { straighten, type Point } from './Map26Borders';
import { GRID } from './Map25Shape';
import { coastDistance, type Island } from './Map26Island';

const TOLERANCE = 0.6; // pixels a coastline may be pulled straight

export function coastlines(p_island: Island): Point[][] {
    const { cols, rows, field } = p_island.shape;
    const w = cols + 1;
    const at = (i: number, j: number) => field[j * w + i];
    // a crossing's id: the side it is on (h: from (i, j) to (i + 1, j); v: from (i, j) to (i, j + 1))
    const hId = (i: number, j: number) => (j * w + i) * 2;
    const vId = (i: number, j: number) => (j * w + i) * 2 + 1;
    const where = (id: number): Point => {
        const c = id >> 1;
        const i = c % w;
        const j = (c - i) / w;
        const a = at(i, j);
        const b = id & 1 ? at(i, j + 1) : at(i + 1, j);
        const t = a / (a - b || 1);
        return id & 1 ? { x: i * GRID, y: (j + t) * GRID } : { x: (i + t) * GRID, y: j * GRID };
    };
    const links = new Map<number, number[]>();
    const join = (a: number, b: number) => {
        if (!links.has(a)) links.set(a, []);
        if (!links.has(b)) links.set(b, []);
        links.get(a)!.push(b);
        links.get(b)!.push(a);
    };
    for (let j = 0; j < rows; j++) {
        for (let i = 0; i < cols; i++) {
            const land = [at(i, j) < 0, at(i + 1, j) < 0, at(i + 1, j + 1) < 0, at(i, j + 1) < 0]; // corners clockwise from top left
            const sides = [hId(i, j), vId(i + 1, j), hId(i, j + 1), vId(i, j)]; // top, right, bottom, left
            const cut = [0, 1, 2, 3].filter((s) => land[s] !== land[(s + 1) % 4]);
            if (cut.length === 2) join(sides[cut[0]], sides[cut[1]]);
            else if (cut.length === 4) { // a saddle: the middle decides which corners join up
                const middleLand = at(i, j) + at(i + 1, j) + at(i + 1, j + 1) + at(i, j + 1) < 0;
                if (middleLand === land[0]) {
                    join(sides[0], sides[1]);
                    join(sides[2], sides[3]);
                } else {
                    join(sides[3], sides[0]);
                    join(sides[1], sides[2]);
                }
            }
        }
    }
    const used = new Set<number>();
    const loops: Point[][] = [];
    for (const start of links.keys()) {
        if (used.has(start)) continue;
        const ids = [start];
        used.add(start);
        let prev = -1;
        let cur = start;
        for (;;) {
            const next = links.get(cur)!.find((n) => n !== prev && !used.has(n));
            if (next === undefined) break;
            ids.push(next);
            used.add(next);
            prev = cur;
            cur = next;
        }
        if (ids.length < 4) continue;
        let loop = straighten({ points: [...ids, ids[0]].map(where), closed: true }, TOLERANCE).points;
        // land on the left: a point just left of the first stretch must be land
        const a = loop[0];
        const b = loop[1];
        const length = Math.hypot(b.x - a.x, b.y - a.y) || 1;
        const left = { x: (a.x + b.x) / 2 + ((b.y - a.y) / length) * 0.8, y: (a.y + b.y) / 2 - ((b.x - a.x) / length) * 0.8 };
        if (coastDistance(p_island, left.x, left.y) >= 0) loop = loop.reverse();
        loops.push(loop);
    }
    return loops;
}
