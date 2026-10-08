// Enemy Arena — the level: walls (axis-aligned boxes), hand-placed markers, raycasts and
// circle-vs-box collision. Units are metres; the arena is SIZE × SIZE.
import { add, dist, scale, sub, v, type V } from './Map20Math';

export const SIZE = 40; // metres

export type Box = { x: number; y: number; w: number; h: number };
export type Hit = { hit: boolean; point: V; t: number };

/** Hand-placed points: the designer decides where enemies hide, patrol and live. */
export type Markers = {
    cover: V[]; // spots behind something solid (ranged enemies hide here)
    camps: { home: V; patrol: V[] }[]; // one per enemy: its home and its patrol loop
};

export type World = { walls: Box[]; markers: Markers };

/** Segment from → to against every wall (slab test). The nearest hit, or { hit: false }. */
export function raycast(world: World, from: V, to: V): Hit {
    const d = sub(to, from);
    let best = 1;
    for (const b of world.walls) {
        const t = rayBox(from, d, b);
        if (t !== null && t < best) best = t;
    }
    return { hit: best < 1, point: add(from, scale(d, best)), t: best };
}

/** True if nothing blocks the straight line between a and b. */
export const clear = (world: World, a: V, b: V): boolean => !raycast(world, a, b).hit;

function rayBox(o: V, d: V, b: Box): number | null {
    let t0 = 0;
    let t1 = 1;
    const axes: [number, number, number, number][] = [
        [o.x, d.x, b.x, b.x + b.w],
        [o.y, d.y, b.y, b.y + b.h],
    ];
    for (const [p, dp, lo, hi] of axes) {
        if (Math.abs(dp) < 1e-9) {
            if (p < lo || p > hi) return null;
            continue;
        }
        let a = (lo - p) / dp;
        let c = (hi - p) / dp;
        if (a > c) [a, c] = [c, a];
        t0 = Math.max(t0, a);
        t1 = Math.min(t1, c);
        if (t0 > t1) return null;
    }
    return t0;
}

/** Push a circle out of every wall it overlaps (and keep it in the arena). */
export function pushOut(world: World, p: V, radius: number): V {
    let q = p;
    for (const b of world.walls) {
        const cx = Math.max(b.x, Math.min(q.x, b.x + b.w));
        const cy = Math.max(b.y, Math.min(q.y, b.y + b.h));
        const dx = q.x - cx;
        const dy = q.y - cy;
        const d = Math.hypot(dx, dy);
        if (d >= radius) continue;
        if (d > 1e-6) q = { x: cx + (dx / d) * radius, y: cy + (dy / d) * radius };
        else q = { x: q.x, y: b.y - radius }; // centre inside the box: pop out the top
    }
    return { x: Math.max(radius, Math.min(SIZE - radius, q.x)), y: Math.max(radius, Math.min(SIZE - radius, q.y)) };
}

/** True if a circle at p overlaps a wall or leaves the arena. */
export function blocked(world: World, p: V, radius: number): boolean {
    if (p.x < radius || p.y < radius || p.x > SIZE - radius || p.y > SIZE - radius) return true;
    return world.walls.some((b) => {
        const cx = Math.max(b.x, Math.min(p.x, b.x + b.w));
        const cy = Math.max(b.y, Math.min(p.y, b.y + b.h));
        return dist(p, v(cx, cy)) < radius;
    });
}
