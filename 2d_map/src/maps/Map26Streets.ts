// Colour Blocks — the third level: streets inside each district, lined up with the nearest road
// (the Street Map's idea: a city's grid follows the main road it grew from).
//
// Each district gets a straight grid of its own: turned to the direction of the road nearest its
// middle (a main road or a highway), with its own block size and offset, so neighbouring districts'
// streets don't line up (T-junctions at the roads between them). Every grid line is walked across
// the district in half-cell steps; the stretches inside it are streets. Each street is cut where the
// streets across it cross, and some of those pieces are left out (gaps): bigger, irregular blocks.
// The streets are kept as data (line pieces), not just drawn.
import type { Highway } from './Map26Borders';
import { DETAIL, type Districts } from './Map26Districts';
import type { Settings } from './Map26Settings';

export type Street = { ax: number; ay: number; bx: number; by: number }; // fine cells
export type Streets = { streets: Street[]; lines?: { x: number; y: number }[][] }; // lines: whole streets (step 12 on)

const STEP = 0.5; // fine cells between the points a grid line is checked at
const MIN_LENGTH = 1.5; // fine cells: shorter street pieces are dropped

// A random number 0..1 for three whole numbers.
function hash(p_a: number, p_b: number, p_c: number): number {
    let h = Math.imul(p_a | 0, 374761393) + Math.imul(p_b | 0, 668265263) + Math.imul(p_c | 0, 1442695041);
    h = Math.imul(h ^ (h >>> 13), 1274126177);
    return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
}

type Frame = { cx: number; cy: number; ax: number; ay: number; radius: number }; // middle, along-road direction, reach

// Each district's middle and how far its cells reach from it.
function middles(p_d: Districts): { cx: number; cy: number; radius: number }[] {
    const sums = p_d.block.map(() => ({ x: 0, y: 0, count: 0 }));
    for (let c = 0; c < p_d.owner.length; c++) {
        const k = p_d.owner[c];
        if (k < 0) continue;
        sums[k].x += (c % p_d.n) + 0.5;
        sums[k].y += Math.floor(c / p_d.n) + 0.5;
        sums[k].count++;
    }
    const out = sums.map((s) => ({ cx: s.x / Math.max(1, s.count), cy: s.y / Math.max(1, s.count), radius: 0 }));
    for (let c = 0; c < p_d.owner.length; c++) {
        const k = p_d.owner[c];
        if (k < 0) continue;
        const r = Math.hypot((c % p_d.n) + 0.5 - out[k].cx, Math.floor(c / p_d.n) + 0.5 - out[k].cy);
        out[k].radius = Math.max(out[k].radius, r);
    }
    return out;
}

// The direction of the road piece nearest (x, y), among all roads (fine cells).
function nearestRoadDirection(p_roads: Highway[], p_x: number, p_y: number): number {
    let best = Infinity;
    let angle = 0;
    for (const r of p_roads) {
        for (let k = 0; k < r.points.length - 1; k++) {
            const a = r.points[k];
            const b = r.points[k + 1];
            const dx = b.x - a.x;
            const dy = b.y - a.y;
            const t = Math.min(1, Math.max(0, ((p_x - a.x) * dx + (p_y - a.y) * dy) / (dx * dx + dy * dy || 1)));
            const d = Math.hypot(p_x - (a.x + dx * t), p_y - (a.y + dy * t));
            if (d < best) {
                best = d;
                angle = Math.atan2(dy, dx);
            }
        }
    }
    return angle;
}

// One grid line across district p_k: from p_from along (p_dx, p_dy), p_reach either way. Its stretches
// inside the district, cut where the cross streets are (every p_cross from p_crossPhase along it),
// with some pieces left out.
function walkLine(p_d: Districts, p_k: number, p_fx: number, p_fy: number, p_dx: number, p_dy: number, p_reach: number, p_cross: number, p_crossPhase: number, p_gaps: number, p_salt: number, r_out: Street[]): void {
    const inside = (t: number) => {
        const i = Math.floor(p_fx + p_dx * t);
        const j = Math.floor(p_fy + p_dy * t);
        return i >= 0 && j >= 0 && i < p_d.n && j < p_d.n && p_d.owner[j * p_d.n + i] === p_k;
    };
    let start: number | null = null;
    const emit = (t0: number, t1: number) => {
        // cut at the cross streets; keep each piece unless it is a gap; join kept neighbours back up
        let from = t0;
        let piece = Math.floor((t0 - p_crossPhase) / p_cross);
        let keepFrom: number | null = null;
        while (from < t1) {
            const to = Math.min(t1, p_crossPhase + (piece + 1) * p_cross);
            const keep = hash(piece, p_salt, p_k) >= p_gaps;
            if (keep && keepFrom === null) keepFrom = from;
            if (!keep && keepFrom !== null) {
                push(keepFrom, from);
                keepFrom = null;
            }
            from = to;
            piece++;
        }
        if (keepFrom !== null) push(keepFrom, t1);
    };
    const push = (t0: number, t1: number) => {
        if (t1 - t0 < MIN_LENGTH) return;
        r_out.push({ ax: p_fx + p_dx * t0, ay: p_fy + p_dy * t0, bx: p_fx + p_dx * t1, by: p_fy + p_dy * t1 });
    };
    for (let t = -p_reach; t <= p_reach + STEP; t += STEP) {
        const now = t <= p_reach && inside(t);
        if (now && start === null) start = t;
        if (!now && start !== null) {
            emit(start, t - STEP);
            start = null;
        }
    }
}

export function makeStreets(p_d: Districts, p_highways: Highway[], p_settings: Settings): Streets {
    const roads = [...p_d.roads.highways, ...p_highways.map((h) => ({ ...h, points: h.points.map((p) => ({ x: p.x * DETAIL, y: p.y * DETAIL })) }))];
    const streets: Street[] = [];
    const block = p_settings.streetBlock;
    middles(p_d).forEach((m, k) => {
        if (m.radius === 0) return;
        const angle = nearestRoadDirection(roads, m.cx, m.cy);
        const f: Frame = { cx: m.cx, cy: m.cy, ax: Math.cos(angle), ay: Math.sin(angle), radius: m.radius + 2 };
        const bx = -f.ay; // across the road
        const by = f.ax;
        const alongBlock = block * (1.3 + 0.6 * hash(k, 1, p_settings.seed)); // blocks longer than deep
        const acrossBlock = block * (0.8 + 0.3 * hash(k, 2, p_settings.seed));
        const alongPhase = alongBlock * hash(k, 3, p_settings.seed);
        const acrossPhase = acrossBlock * hash(k, 4, p_settings.seed);
        const lines = Math.ceil(f.radius / acrossBlock) + 1;
        for (let q = -lines; q <= lines; q++) { // streets alongside the road
            const off = acrossPhase + q * acrossBlock;
            walkLine(p_d, k, f.cx + bx * off, f.cy + by * off, f.ax, f.ay, f.radius, alongBlock, alongPhase, p_settings.streetGaps, q * 2 + 1, streets);
        }
        const cross = Math.ceil(f.radius / alongBlock) + 1;
        for (let q = -cross; q <= cross; q++) { // streets across it
            const off = alongPhase + q * alongBlock;
            walkLine(p_d, k, f.cx + f.ax * off, f.cy + f.ay * off, bx, by, f.radius, acrossBlock, acrossPhase, p_settings.streetGaps * 0.6, q * 2, streets);
        }
    });
    return { streets };
}
