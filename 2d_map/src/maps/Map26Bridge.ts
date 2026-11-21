// Colour Blocks — step 11: the motorway as a bridge, joining the two islands' cities.
//
// It runs from the ring road of one city to the ring road of the other, and nowhere else: it never
// enters a city (no crossing over the other roads) and never goes round an island. The cheapest
// path (Dijkstra) from every cell just outside the first city to any cell just outside the second,
// over the cells outside both cities: a step on land costs 1, a step over water BRIDGE (so it walks
// out to where the strait is narrowest and crosses there, straight). Each end is then carried on to
// the nearest point of a highway (the ring road), where it joins. Pulled straight and rounded like
// the highways; the pieces over water are drawn as a bridge deck.
import { smooth, straighten, type Borders, type Highway, type Point } from './Map26Borders';
import { piecesOf } from './Map26City';
import { DI, DJ, Queue, STEP } from './Map26Grow';
import { coastDistance, type Island } from './Map26Island';
import type { Settings } from './Map26Settings';
import type { Spread } from './Map26Spread';

const BRIDGE = 8; // a step over water costs this many on land
const SHORTEST_BRIDGE = 1.5; // cells: shorter stretches over water (a corner of a bay) are not drawn as bridges
const ABUTMENT = 0.6; // cells: a bridge reaches this far onto land at each end

export type Motorway = { road: Highway; bridges: Point[][] }; // bridges: the pieces over water (drawn as a deck)

// Per cell: 1 or 2 for the cells of the two biggest cities, 0 elsewhere.
function cities(p_spread: Spread): Uint8Array {
    const n = p_spread.n;
    const inside = Uint8Array.from(p_spread.owner, (k) => (k >= 0 ? 1 : 0));
    const which = new Uint8Array(n * n);
    piecesOf(inside, n)
        .sort((a, b) => b.length - a.length)
        .slice(0, 2)
        .forEach((cells, k) => cells.forEach((c) => (which[c] = k + 1)));
    return which;
}

// Per cell outside the cities: the city (1 or 2) it touches (8 neighbours), or 0.
function touching(p_which: Uint8Array, p_n: number): Uint8Array {
    const next = new Uint8Array(p_n * p_n);
    for (let c = 0; c < p_n * p_n; c++) {
        if (p_which[c]) continue;
        const i = c % p_n;
        const j = (c - i) / p_n;
        for (let m = 0; m < 8; m++) {
            const ni = i + DI[m];
            const nj = j + DJ[m];
            if (ni >= 0 && nj >= 0 && ni < p_n && nj < p_n && p_which[nj * p_n + ni]) next[c] = p_which[nj * p_n + ni];
        }
    }
    return next;
}

// The cheapest path (cells, first city's end first) from a cell next to city 1 to a cell next to
// city 2, never through a city; [] if there is none.
function crossing(p_n: number, p_which: Uint8Array, p_next: Uint8Array, p_water: Uint8Array): number[] {
    const time = new Float64Array(p_n * p_n).fill(Infinity);
    const from = new Int32Array(p_n * p_n).fill(-1);
    const queue = new Queue();
    for (let c = 0; c < p_n * p_n; c++) {
        if (p_next[c] !== 1) continue;
        time[c] = 0;
        queue.push(0, c);
    }
    let end = -1;
    while (queue.size) {
        const c = queue.pop();
        const t = queue.poppedTime;
        if (t > time[c]) continue;
        if (p_next[c] === 2) {
            end = c;
            break;
        }
        const i = c % p_n;
        const j = (c - i) / p_n;
        for (let m = 0; m < 8; m++) {
            const ni = i + DI[m];
            const nj = j + DJ[m];
            if (ni < 0 || nj < 0 || ni >= p_n || nj >= p_n) continue;
            const nc = nj * p_n + ni;
            if (p_which[nc]) continue; // never into a city
            const nt = t + STEP[m] * (p_water[nc] ? BRIDGE : 1);
            if (nt < time[nc]) {
                time[nc] = nt;
                from[nc] = c;
                queue.push(nt, nc);
            }
        }
    }
    if (end < 0) return [];
    const path = [end];
    while (from[path[path.length - 1]] >= 0) path.push(from[path[path.length - 1]]);
    return path.reverse();
}

// The point of the highways nearest p.
function nearestOnRoads(p_highways: Highway[], p: Point): Point {
    let best = p;
    let bestD = Infinity;
    for (const h of p_highways) {
        for (let k = 0; k < h.points.length - 1; k++) {
            const a = h.points[k];
            const b = h.points[k + 1];
            const dx = b.x - a.x;
            const dy = b.y - a.y;
            const t = Math.min(1, Math.max(0, ((p.x - a.x) * dx + (p.y - a.y) * dy) / (dx * dx + dy * dy || 1)));
            const q = { x: a.x + dx * t, y: a.y + dy * t };
            const d = Math.hypot(p.x - q.x, p.y - q.y);
            if (d < bestD) [bestD, best] = [d, q];
        }
    }
    return best;
}

// The pieces of the road over water (bridges): its points checked every quarter cell. Each reaches
// ABUTMENT cells onto land at both ends; stretches shorter than SHORTEST_BRIDGE are left out.
function bridgesOf(p_road: Highway, p_island: Island, p_n: number, p_size: number): Point[][] {
    const cell = p_size / p_n;
    const dense: Point[] = []; // the road every quarter cell
    const pts = p_road.points;
    for (let k = 0; k < pts.length - 1; k++) {
        const a = pts[k];
        const b = pts[k + 1];
        const steps = Math.max(1, Math.ceil(Math.hypot(b.x - a.x, b.y - a.y) * 4));
        for (let s = 0; s < steps; s++) dense.push({ x: a.x + ((b.x - a.x) * s) / steps, y: a.y + ((b.y - a.y) * s) / steps });
    }
    dense.push(pts[pts.length - 1]);
    const water = dense.map((p) => coastDistance(p_island, p.x * cell, p.y * cell) > 0);
    const reach = Math.round(ABUTMENT * 4);
    const bridges: Point[][] = [];
    for (let k = 0; k < dense.length; k++) {
        if (!water[k]) continue;
        let end = k;
        while (end + 1 < dense.length && water[end + 1]) end++;
        if ((end - k) / 4 >= SHORTEST_BRIDGE) bridges.push(dense.slice(Math.max(0, k - reach), Math.min(dense.length, end + 1 + reach)));
        k = end;
    }
    return bridges;
}

export function makeBridge(p_spread: Spread, p_borders: Borders, p_island: Island, p_size: number, p_settings: Settings): Motorway | null {
    const n = p_spread.n;
    const cell = p_size / n;
    const which = cities(p_spread);
    const water = new Uint8Array(n * n);
    for (let c = 0; c < n * n; c++) water[c] = coastDistance(p_island, ((c % n) + 0.5) * cell, (Math.floor(c / n) + 0.5) * cell) > 0 ? 1 : 0;
    const path = crossing(n, which, touching(which, n), water);
    if (path.length < 2) return null;
    const middle = path.map((c) => ({ x: (c % n) + 0.5, y: Math.floor(c / n) + 0.5 }));
    const points = [nearestOnRoads(p_borders.highways, middle[0]), ...middle, nearestOnRoads(p_borders.highways, middle[middle.length - 1])];
    const road = smooth(straighten({ points, closed: false }, p_settings.straighten * 2), p_settings.smoothing + 1);
    return { road, bridges: bridgesOf(road, p_island, n, p_size) };
}
