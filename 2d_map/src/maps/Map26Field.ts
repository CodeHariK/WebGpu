// Colour Blocks — step 12: the street field, a direction at every point, taken from the roads.
//
// Every road (highways, the ring road, the main roads between districts) is sampled every fine
// cell; each sample carries its direction. At a point, each sample's direction counts with a weight
// that falls off fast with distance (1 / (1 + (distance / fieldReach)⁴)): next to a road its own
// direction wins, between two roads the direction turns smoothly from one to the other. A
// direction has no front or back (a road east is a road west), so they are added as a tensor: the
// angle doubled (cos 2θ, sin 2θ), summed, and halved back. Kept on the block grid (every block
// cell) and read anywhere by blending the 4 nearest.
import type { Highway, Point } from './Map26Borders';
import { DETAIL } from './Map26Districts';

export type Field = { n: number; c: Float32Array; s: Float32Array }; // per block cell: the summed cos 2θ, sin 2θ

type Sample = { x: number; y: number; c: number; s: number }; // block cells

// Every road sampled about every fine cell (in block cells), each sample with its doubled angle.
function samplesOf(p_roads: Point[][]): Sample[] {
    const out: Sample[] = [];
    const step = 1 / DETAIL;
    for (const pts of p_roads) {
        for (let k = 0; k < pts.length - 1; k++) {
            const a = pts[k];
            const b = pts[k + 1];
            const length = Math.hypot(b.x - a.x, b.y - a.y);
            if (length === 0) continue;
            const angle = 2 * Math.atan2(b.y - a.y, b.x - a.x);
            const c = Math.cos(angle);
            const s = Math.sin(angle);
            const count = Math.max(1, Math.round(length / step));
            for (let m = 0; m < count; m++) out.push({ x: a.x + ((b.x - a.x) * (m + 0.5)) / count, y: a.y + ((b.y - a.y) * (m + 0.5)) / count, c, s });
        }
    }
    return out;
}

// p_highways in block cells, p_mainRoads in fine cells; p_inside: per block cell, 1 where it is
// needed (the city and the cells round it).
export function makeField(p_n: number, p_highways: Highway[], p_mainRoads: Highway[], p_inside: Uint8Array, p_reach: number): Field {
    const roads = [...p_highways.map((h) => h.points), ...p_mainRoads.map((h) => h.points.map((p) => ({ x: p.x / DETAIL, y: p.y / DETAIL })))];
    const samples = samplesOf(roads);
    const c = new Float32Array(p_n * p_n);
    const s = new Float32Array(p_n * p_n);
    for (let cell = 0; cell < p_n * p_n; cell++) {
        if (!p_inside[cell]) continue;
        const x = (cell % p_n) + 0.5;
        const y = Math.floor(cell / p_n) + 0.5;
        let sc = 0;
        let ss = 0;
        for (const q of samples) {
            const d = Math.hypot(q.x - x, q.y - y) / p_reach;
            const w = 1 / (1 + d * d * d * d);
            sc += q.c * w;
            ss += q.s * w;
        }
        c[cell] = sc;
        s[cell] = ss;
    }
    return { n: p_n, c, s };
}

// The field's direction (radians, 0..π) at (x, y) fine cells: the 4 nearest block cells blended.
export function angleAt(p_f: Field, p_x: number, p_y: number): number {
    const gx = Math.min(p_f.n - 1.001, Math.max(0, p_x / DETAIL - 0.5));
    const gy = Math.min(p_f.n - 1.001, Math.max(0, p_y / DETAIL - 0.5));
    const i = Math.floor(gx);
    const j = Math.floor(gy);
    const fx = gx - i;
    const fy = gy - j;
    const at = (a: Float32Array) => {
        const k = j * p_f.n + i;
        const top = a[k] + (a[k + 1] - a[k]) * fx;
        const bottom = a[k + p_f.n] + (a[k + p_f.n + 1] - a[k + p_f.n]) * fx;
        return top + (bottom - top) * fy;
    };
    return Math.atan2(at(p_f.s), at(p_f.c)) / 2;
}
