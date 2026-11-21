// Street Map — the roads as points every SPACING pixels, each knowing its road, its distance along
// the road (s) and the road's direction there: what the districts and streets are measured from.
import type { Road } from './Map24Roads';

export const SPACING = 2;

export type Samples = {
    count: number;
    x: Float32Array;
    y: Float32Array;
    tx: Float32Array; // the road's direction (unit)
    ty: Float32Array;
    s: Float32Array; // pixels along the road from its start
    bend: Float32Array; // curvature: how fast the direction turns a pixel (+ turning left)
    road: Int32Array;
};

export function sampleRoads(p_roads: Road[]): Samples {
    const xs: number[] = [];
    const ys: number[] = [];
    const ss: number[] = [];
    const ids: number[] = [];
    for (const road of p_roads) {
        let s = 0;
        const pts = road.points;
        for (let i = 0; i < pts.length - 1; i++) {
            const a = pts[i];
            const b = pts[i + 1];
            const len = Math.hypot(b.x - a.x, b.y - a.y);
            const n = Math.max(1, Math.ceil(len / SPACING));
            for (let k = 0; k < n; k++) {
                const f = k / n;
                xs.push(a.x + (b.x - a.x) * f);
                ys.push(a.y + (b.y - a.y) * f);
                ss.push(s + len * f);
                ids.push(road.id);
            }
            s += len;
        }
    }
    const count = xs.length;
    const samples: Samples = {
        count,
        x: Float32Array.from(xs),
        y: Float32Array.from(ys),
        tx: new Float32Array(count),
        ty: new Float32Array(count),
        s: Float32Array.from(ss),
        bend: new Float32Array(count),
        road: Int32Array.from(ids),
    };
    for (let i = 0; i < count; i++) {
        const a = i > 0 && samples.road[i - 1] === samples.road[i] ? i - 1 : i;
        const b = i < count - 1 && samples.road[i + 1] === samples.road[i] ? i + 1 : i;
        const dx = samples.x[b] - samples.x[a];
        const dy = samples.y[b] - samples.y[a];
        const len = Math.hypot(dx, dy) || 1;
        samples.tx[i] = dx / len;
        samples.ty[i] = dy / len;
    }
    for (let i = 0; i < count; i++) {
        const a = Math.max(0, i - 8);
        const b = Math.min(count - 1, i + 8);
        if (samples.road[a] !== samples.road[i] || samples.road[b] !== samples.road[i]) continue;
        const cross = samples.tx[a] * samples.ty[b] - samples.ty[a] * samples.tx[b];
        samples.bend[i] = cross / ((b - a) * SPACING);
    }
    return samples;
}
