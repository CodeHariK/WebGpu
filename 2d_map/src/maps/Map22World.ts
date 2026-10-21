// Wick — the ground: sampling the level's spline paths into short pieces, what is walkable (on a
// path or in a clearing, not behind a fog wall), pushing a circle back onto the ground, and how
// close a point is to the path (the shadow things rise from the water just off it).
import type { LevelDef } from './Map22Level';
import type { Clearing, Seg, Vec } from './Map22State';

// Catmull-Rom through the control points, `per` samples between each pair.
export function samplePaths(level: LevelDef, per = 8): Seg[] {
    const segs: Seg[] = [];
    for (const path of level.paths) {
        const pts = path.pts;
        const at = (i: number) => pts[Math.max(0, Math.min(pts.length - 1, i))];
        let prev: (Vec & { w: number }) | null = null;
        for (let i = 0; i < pts.length - 1; i++) {
            const p0 = at(i - 1), p1 = at(i), p2 = at(i + 1), p3 = at(i + 2);
            for (let k = 0; k <= per; k++) {
                if (i > 0 && k === 0) continue;
                const t = k / per;
                const t2 = t * t, t3 = t2 * t;
                const cr = (a: number, b: number, c: number, d: number) => 0.5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3);
                const q = { x: cr(p0.x, p1.x, p2.x, p3.x), y: cr(p0.y, p1.y, p2.y, p3.y), w: p1.w + (p2.w - p1.w) * t };
                if (prev) segs.push({ a: { x: prev.x, y: prev.y }, b: { x: q.x, y: q.y }, wa: prev.w, wb: q.w });
                prev = q;
            }
        }
    }
    return segs;
}

// The nearest point on a piece and the path's half-width there.
function onSeg(s: Seg, p: Vec): { q: Vec; w: number; d: number } {
    const dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
    const len2 = dx * dx + dy * dy || 1;
    const k = Math.max(0, Math.min(1, ((p.x - s.a.x) * dx + (p.y - s.a.y) * dy) / len2));
    const q = { x: s.a.x + dx * k, y: s.a.y + dy * k };
    return { q, w: s.wa + (s.wb - s.wa) * k, d: Math.hypot(p.x - q.x, p.y - q.y) };
}

/** How far inside the ground p is (positive: inside, by that many metres; negative: out over the water). */
export function groundDepth(segs: Seg[], clearings: Clearing[], p: Vec): number {
    let best = -Infinity;
    for (const s of segs) {
        const o = onSeg(s, p);
        best = Math.max(best, o.w - o.d);
    }
    for (const c of clearings) best = Math.max(best, c.def.r - Math.hypot(p.x - c.def.pos.x, p.y - c.def.pos.y));
    return best;
}

/** Is a fog wall standing at p? (a clearing not yet lit blocks the way out) */
export function inFogWall(clearings: Clearing[], p: Vec, r: number): (Vec & { r: number }) | null {
    for (const c of clearings) {
        if (c.done) continue;
        for (const g of c.def.gates) if (Math.hypot(p.x - g.x, p.y - g.y) < g.r + r) return g;
    }
    return null;
}

/** Keep a circle of radius r on the ground (and out of standing fog walls). */
export function keepOnGround(segs: Seg[], clearings: Clearing[], pos: Vec, r: number): void {
    // Fog walls first: push straight out.
    const g = inFogWall(clearings, pos, r);
    if (g) {
        const d = Math.hypot(pos.x - g.x, pos.y - g.y) || 1e-3;
        pos.x = g.x + ((pos.x - g.x) / d) * (g.r + r);
        pos.y = g.y + ((pos.y - g.y) / d) * (g.r + r);
    }
    // Then onto the ground: the deepest place nearby wins, so you slide along the edge.
    let bestDepth = -Infinity;
    let fix: Vec | null = null;
    for (const s of segs) {
        const o = onSeg(s, pos);
        const depth = o.w - r - o.d;
        if (depth >= 0) return; // already fine
        if (depth > bestDepth) {
            bestDepth = depth;
            const k = o.d > 1e-4 ? (o.w - r) / o.d : 0;
            fix = { x: o.q.x + (pos.x - o.q.x) * Math.max(0, k), y: o.q.y + (pos.y - o.q.y) * Math.max(0, k) };
        }
    }
    for (const c of clearings) {
        const d = Math.hypot(pos.x - c.def.pos.x, pos.y - c.def.pos.y);
        const depth = c.def.r - r - d;
        if (depth >= 0) return;
        if (depth > bestDepth) {
            bestDepth = depth;
            const k = (c.def.r - r) / (d || 1);
            fix = { x: c.def.pos.x + (pos.x - c.def.pos.x) * k, y: c.def.pos.y + (pos.y - c.def.pos.y) * k };
        }
    }
    if (fix) { pos.x = fix.x; pos.y = fix.y; }
}

/** A point out on the water near p, `off` metres beyond the ground's edge (where shapes rise). */
export function waterNear(segs: Seg[], clearings: Clearing[], p: Vec, off: number): Vec {
    for (let tries = 0; tries < 24; tries++) {
        const a = Math.random() * Math.PI * 2;
        for (let r = 2; r < 14; r += 0.5) {
            const q = { x: p.x + Math.cos(a) * r, y: p.y + Math.sin(a) * r };
            if (groundDepth(segs, clearings, q) < -off) return q;
        }
    }
    return { x: p.x + off + 4, y: p.y };
}
