// Enemy Arena — tiny 2D vector helpers. Plain {x, y} objects and pure functions (no classes,
// no mutation) so the AI code ports one-to-one to GDScript / C++ (Vector2 there).

export type V = { x: number; y: number };

export const v = (x: number, y: number): V => ({ x, y });
export const add = (a: V, b: V): V => ({ x: a.x + b.x, y: a.y + b.y });
export const sub = (a: V, b: V): V => ({ x: a.x - b.x, y: a.y - b.y });
export const scale = (a: V, s: number): V => ({ x: a.x * s, y: a.y * s });
export const dot = (a: V, b: V): number => a.x * b.x + a.y * b.y;
export const len = (a: V): number => Math.hypot(a.x, a.y);
export const dist = (a: V, b: V): number => Math.hypot(a.x - b.x, a.y - b.y);
export const lerp = (a: V, b: V, t: number): V => ({ x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t });

export const norm = (a: V): V => {
    const l = len(a);
    return l > 1e-6 ? { x: a.x / l, y: a.y / l } : { x: 0, y: 0 };
};

/** Unit vector at `angle` radians (0 = +x, counter-clockwise in maths, clockwise on screen). */
export const fromAngle = (angle: number): V => ({ x: Math.cos(angle), y: Math.sin(angle) });
export const angleOf = (a: V): number => Math.atan2(a.y, a.x);

/** Shortest signed difference b − a between two angles, in −π..π. */
export const angleDiff = (a: number, b: number): number => {
    let d = (b - a) % (Math.PI * 2);
    if (d > Math.PI) d -= Math.PI * 2;
    if (d < -Math.PI) d += Math.PI * 2;
    return d;
};

/** Turn angle `from` toward `to` by at most `step` radians. */
export const turnToward = (from: number, to: number, step: number): number => {
    const d = angleDiff(from, to);
    return from + Math.max(-step, Math.min(step, d));
};

export const clamp = (x: number, lo: number, hi: number): number => Math.max(lo, Math.min(hi, x));

/** Seeded random (mulberry32): same seed → same arena and patrol pauses. */
export const makeRandom = (seed: number) => {
    let s = seed >>> 0;
    return () => {
        s = (s + 0x6d2b79f5) >>> 0;
        let t = s;
        t = Math.imul(t ^ (t >>> 15), t | 1);
        t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
};
