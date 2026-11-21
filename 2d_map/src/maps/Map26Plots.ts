// Colour Blocks — step 13: plots. Every block between the roads is cut into plots, each a strip of
// ground running back from one road (its frontage), as in a real street: houses face the road,
// gardens meet back to back.
//
// On a grid twice as fine as the districts' (PLOT cells to a fine cell, each way) the roads are
// drawn in, each road cell remembering which road it is, how far along that road it is, and the
// road's direction there (from the nearest point of the nearest road's line). A block is a piece of
// ground between road cells (a flood fill). Its cells next to a road are its frontage; each is
// keyed by that road, the side of it, and which stretch of plotWidth along it (each road its own
// random start). Then every block cell takes the key of the frontage nearest it (a search out
// from all the frontage at once, Dijkstra), up to plotDepth back: cells sharing a key are a plot.
// Further back than that is the block's middle (no plot: a courtyard). Corners split themselves:
// a corner cell goes to the road it is nearest. Slivers (where stretches, roads or corners meet:
// under a third of a full plot) join the plot beside them they share most edge with.
import type { Point } from './Map26Borders';
import { DI, DJ, Queue, STEP } from './Map26Grow';
import type { Districts } from './Map26Districts';

export const PLOT = 2; // plot cells to a fine cell, each way

export type Road = { points: Point[]; halfWidth: number }; // fine cells
// fine cells: the middle of its frontage, the road's direction, the way back from it, its frontage
// length and its area
export type Plot = { front: Point; along: Point; inward: Point; frontage: number; area: number };
export type Plots = { n: number; id: Int32Array; plots: Plot[] }; // id per plot cell: its plot, ROAD or MIDDLE

export const ROAD = -1; // a road (or outside the city)
export const MIDDLE = -2; // a block's middle: further from the roads than plotDepth

// A random number 0..1 for a whole number.
function hash(p_k: number): number {
    let h = Math.imul(p_k + 1, 2654435761);
    h ^= h >>> 15;
    h = Math.imul(h, 2246822519);
    return ((h ^ (h >>> 13)) >>> 0) / 4294967296;
}

type Raster = { road: Int32Array; along: Float32Array; dist: Float32Array; dx: Float32Array; dy: Float32Array; cx: Float32Array; cy: Float32Array };

// The roads drawn in: per plot cell, the road it is under (or −1), how far along it, its direction,
// and its line's nearest point (plot cells).
function drawRoads(p_n: number, p_roads: Road[]): Raster {
    const r: Raster = { road: new Int32Array(p_n * p_n).fill(-1), along: new Float32Array(p_n * p_n), dist: new Float32Array(p_n * p_n).fill(Infinity), dx: new Float32Array(p_n * p_n), dy: new Float32Array(p_n * p_n), cx: new Float32Array(p_n * p_n), cy: new Float32Array(p_n * p_n) };
    p_roads.forEach((road, id) => {
        const half = road.halfWidth * PLOT;
        const reach = Math.ceil(half);
        let walked = 0;
        const pts = road.points.map((p) => ({ x: p.x * PLOT, y: p.y * PLOT }));
        for (let k = 0; k < pts.length - 1; k++) {
            const a = pts[k];
            const b = pts[k + 1];
            const length = Math.hypot(b.x - a.x, b.y - a.y);
            if (length === 0) continue;
            const ux = (b.x - a.x) / length;
            const uy = (b.y - a.y) / length;
            for (let t = 0; t <= length; t += 0.5) {
                const x = a.x + ux * t;
                const y = a.y + uy * t;
                for (let j = Math.floor(y - reach); j <= Math.ceil(y + reach); j++) {
                    for (let i = Math.floor(x - reach); i <= Math.ceil(x + reach); i++) {
                        if (i < 0 || j < 0 || i >= p_n || j >= p_n) continue;
                        const d = Math.hypot(i + 0.5 - x, j + 0.5 - y);
                        const c = j * p_n + i;
                        if (d > half || d >= r.dist[c]) continue;
                        r.road[c] = id;
                        r.dist[c] = d;
                        r.along[c] = (walked + t) / PLOT;
                        r.dx[c] = ux;
                        r.dy[c] = uy;
                        r.cx[c] = x;
                        r.cy[c] = y;
                    }
                }
            }
            walked += length;
        }
    });
    return r;
}

// Plots under p_least cells join the neighbouring plot they share most edge with, smallest first
// (so a sliver beside a sliver ends up in a real plot). Returns per plot the plot it ended up in;
// the cells are relabelled.
function joinSlivers(r_id: Int32Array, p_n: number, p_count: number, p_least: number): Int32Array {
    const root = Int32Array.from({ length: p_count }, (_, k) => k);
    const find = (k: number): number => {
        while (root[k] !== k) k = root[k] = root[root[k]];
        return k;
    };
    const area = new Int32Array(p_count);
    for (const k of r_id) if (k >= 0) area[k]++;
    const shared = new Map<number, Map<number, number>>(); // sliver: neighbour: shared edges
    for (let c = 0; c < p_n * p_n; c++) {
        const k = r_id[c];
        if (k < 0 || area[k] >= p_least) continue;
        const i = c % p_n;
        for (const nc of [i + 1 < p_n ? c + 1 : -1, i > 0 ? c - 1 : -1, c + p_n < p_n * p_n ? c + p_n : -1, c - p_n]) {
            if (nc < 0 || r_id[nc] < 0 || r_id[nc] === k) continue;
            if (!shared.has(k)) shared.set(k, new Map());
            const v = shared.get(k)!;
            v.set(r_id[nc], (v.get(r_id[nc]) ?? 0) + 1);
        }
    }
    const slivers = [...shared.keys()].sort((a, b) => area[a] - area[b]);
    for (const k of slivers) {
        const totals = new Map<number, number>(); // by the plot each neighbour has ended up in
        for (const [m, edges] of shared.get(k)!) {
            const r = find(m);
            if (r !== find(k)) totals.set(r, (totals.get(r) ?? 0) + edges);
        }
        if (totals.size === 0) continue;
        const into = [...totals.entries()].sort((a, b) => b[1] - a[1])[0][0];
        root[find(k)] = into;
    }
    for (let k = 0; k < p_count; k++) root[k] = find(k);
    for (let c = 0; c < p_n * p_n; c++) if (r_id[c] >= 0) r_id[c] = root[r_id[c]];
    return root;
}

// p_roads: every road (streets, main roads, highways), fine cells. p_width, p_depth: a plot's
// frontage and how far back it reaches (fine cells).
export function makePlots(p_d: Districts, p_roads: Road[], p_width: number, p_depth: number): Plots {
    const n = p_d.n * PLOT;
    const r = drawRoads(n, p_roads);
    const id = new Int32Array(n * n).fill(MIDDLE);
    for (let c = 0; c < n * n; c++) {
        const i = c % n;
        const j = (c - i) / n;
        if (r.road[c] >= 0 || p_d.owner[Math.floor(j / PLOT) * p_d.n + Math.floor(i / PLOT)] < 0) id[c] = ROAD;
    }
    // the frontage: ground cells beside a road cell, keyed by road, side and stretch
    const keyOf = new Map<string, number>();
    const time = new Float64Array(n * n).fill(Infinity);
    const queue = new Queue();
    const sums: { x: number; y: number; ax: number; ay: number; ix: number; iy: number; count: number }[] = [];
    for (let c = 0; c < n * n; c++) {
        if (id[c] === ROAD) continue;
        const i = c % n;
        const j = (c - i) / n;
        let best = -1;
        for (let m = 0; m < 4; m++) {
            const ni = i + DI[m];
            const nj = j + DJ[m];
            if (ni < 0 || nj < 0 || ni >= n || nj >= n) continue;
            const nc = nj * n + ni;
            if (r.road[nc] >= 0 && (best < 0 || r.dist[nc] < r.dist[best])) best = nc;
        }
        if (best < 0) continue;
        const road = r.road[best];
        const ix = i + 0.5 - r.cx[best];
        const iy = j + 0.5 - r.cy[best];
        const side = r.dx[best] * iy - r.dy[best] * ix > 0 ? 1 : 0;
        const stretch = Math.floor(r.along[best] / p_width + hash(road));
        const key = `${road},${side},${stretch}`;
        let plot = keyOf.get(key);
        if (plot === undefined) {
            plot = sums.length;
            keyOf.set(key, plot);
            sums.push({ x: 0, y: 0, ax: 0, ay: 0, ix: 0, iy: 0, count: 0 });
        }
        const s = sums[plot];
        const flip = s.count > 0 && s.ax * r.dx[best] + s.ay * r.dy[best] < 0 ? -1 : 1;
        s.x += i + 0.5;
        s.y += j + 0.5;
        s.ax += r.dx[best] * flip;
        s.ay += r.dy[best] * flip;
        const length = Math.hypot(ix, iy) || 1;
        s.ix += ix / length;
        s.iy += iy / length;
        s.count++;
        id[c] = plot;
        time[c] = 0;
        queue.push(0, c);
    }
    // every ground cell to the plot of the frontage nearest it, up to the depth
    const deepest = p_depth * PLOT;
    while (queue.size) {
        const c = queue.pop();
        const t = queue.poppedTime;
        if (t > time[c]) continue;
        const i = c % n;
        const j = (c - i) / n;
        for (let m = 0; m < 8; m++) {
            const ni = i + DI[m];
            const nj = j + DJ[m];
            if (ni < 0 || nj < 0 || ni >= n || nj >= n) continue;
            const nc = nj * n + ni;
            if (id[nc] === ROAD) continue;
            const nt = t + STEP[m];
            if (nt > deepest || nt >= time[nc]) continue;
            time[nc] = nt;
            id[nc] = id[c];
            queue.push(nt, nc);
        }
    }
    const root = joinSlivers(id, n, sums.length, (p_width * p_depth * PLOT * PLOT) / 3);
    // the plots left, numbered again; a joined sliver's frontage counts in its plot's
    const index = new Int32Array(sums.length).fill(-1);
    const merged: typeof sums = [];
    sums.forEach((_, k) => {
        if (root[k] === k) {
            index[k] = merged.length;
            merged.push({ ...sums[k] });
        }
    });
    sums.forEach((s, k) => {
        if (root[k] === k) return;
        const m = merged[index[root[k]]];
        const flip = m.ax * s.ax + m.ay * s.ay < 0 ? -1 : 1;
        m.x += s.x;
        m.y += s.y;
        m.ax += s.ax * flip;
        m.ay += s.ay * flip;
        m.ix += s.ix;
        m.iy += s.iy;
        m.count += s.count;
    });
    const area = new Float32Array(merged.length);
    for (let c = 0; c < n * n; c++) {
        if (id[c] < 0) continue;
        id[c] = index[id[c]];
        area[id[c]] += 1 / (PLOT * PLOT);
    }
    const plots = merged.map((s, k) => {
        const al = Math.hypot(s.ax, s.ay) || 1;
        const il = Math.hypot(s.ix, s.iy) || 1;
        return { front: { x: s.x / s.count / PLOT, y: s.y / s.count / PLOT }, along: { x: s.ax / al, y: s.ay / al }, inward: { x: s.ix / il, y: s.iy / il }, frontage: s.count / PLOT, area: area[k] };
    });
    return { n, id, plots };
}
