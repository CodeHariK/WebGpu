import { Vector2 } from './Vector2';

/**
 * Spline utility for generating smooth paths from control points.
 *
 * Supports the standard Catmull-Rom family via an `alpha` (knot) parameter:
 *   alpha = 0.0 -> uniform      (fast, but can overshoot / self-cross on sharp turns)
 *   alpha = 0.5 -> centripetal  (no cusps or self-intersections — the safe default)
 *   alpha = 1.0 -> chordal      (very taut)
 * The curve always passes through every control point.
 */
export class Spline {
    private points: Vector2[];
    private alpha: number;

    constructor(points: Vector2[], alpha: number = 0) {
        this.points = points;
        this.alpha = alpha;
    }

    /**
     * Get a point on the spline at parameter t (0.0 to 1.0).
     */
    public getPoint(t: number): Vector2 {
        if (this.points.length === 0) return new Vector2(0, 0);
        if (this.points.length === 1) return this.points[0];
        if (this.points.length === 2) {
            return new Vector2(
                this.points[0].x + (this.points[1].x - this.points[0].x) * t,
                this.points[0].y + (this.points[1].y - this.points[0].y) * t
            );
        }

        t = Math.max(0, Math.min(1, t));

        const segments = this.points.length - 1;
        const scaledT = t * segments;
        const i = Math.min(Math.floor(scaledT), segments - 1);
        const u = scaledT - i;

        const p0 = this.points[i === 0 ? 0 : i - 1];
        const p1 = this.points[i];
        const p2 = this.points[i + 1];
        const p3 = this.points[i + 1 === segments ? segments : i + 2];

        return this.alpha > 0
            ? this.nonUniform(p0, p1, p2, p3, u, this.alpha)
            : this.catmullRom(p0, p1, p2, p3, u);
    }

    /** Uniform Catmull-Rom (alpha = 0). */
    private catmullRom(p0: Vector2, p1: Vector2, p2: Vector2, p3: Vector2, t: number): Vector2 {
        const t2 = t * t;
        const t3 = t2 * t;
        const f1 = -0.5 * t3 + t2 - 0.5 * t;
        const f2 = 1.5 * t3 - 2.5 * t2 + 1.0;
        const f3 = -1.5 * t3 + 2.0 * t2 + 0.5 * t;
        const f4 = 0.5 * t3 - 0.5 * t2;
        return new Vector2(
            p0.x * f1 + p1.x * f2 + p2.x * f3 + p3.x * f4,
            p0.y * f1 + p1.y * f2 + p2.y * f3 + p3.y * f4
        );
    }

    /**
     * Non-uniform Catmull-Rom via the Barry-Goldman pyramid. Knot spacing is
     * |p_{i+1} - p_i|^alpha, so alpha=0.5 gives the centripetal spline (no cusps
     * or self-intersections even when consecutive points turn sharply — exactly
     * what smooths a jagged BFS road path). `u` is the local parameter in [0,1]
     * across the p1->p2 segment.
     */
    private nonUniform(p0: Vector2, p1: Vector2, p2: Vector2, p3: Vector2, u: number, alpha: number): Vector2 {
        const EPS = 1e-5;
        const knot = (ti: number, a: Vector2, b: Vector2): number => {
            const d = Math.hypot(b.x - a.x, b.y - a.y);
            return ti + Math.max(EPS, Math.pow(d, alpha));
        };
        const t0 = 0;
        const t1 = knot(t0, p0, p1);
        const t2 = knot(t1, p1, p2);
        const t3 = knot(t2, p2, p3);

        const t = t1 + u * (t2 - t1);

        const lerp = (a: Vector2, b: Vector2, ta: number, tb: number, tt: number): Vector2 => {
            const f = (tb - tt) / (tb - ta);
            const g = (tt - ta) / (tb - ta);
            return new Vector2(a.x * f + b.x * g, a.y * f + b.y * g);
        };

        const A1 = lerp(p0, p1, t0, t1, t);
        const A2 = lerp(p1, p2, t1, t2, t);
        const A3 = lerp(p2, p3, t2, t3, t);
        const B1 = lerp(A1, A2, t0, t2, t);
        const B2 = lerp(A2, A3, t1, t3, t);
        return lerp(B1, B2, t1, t2, t);
    }

    /**
     * Generate an array of points representing the smooth path at the given resolution.
     * @param resolution Number of steps per segment.
     */
    public getPath(resolution: number = 10): Vector2[] {
        if (this.points.length < 2) return this.points;
        const path: Vector2[] = [];
        const totalSteps = (this.points.length - 1) * resolution;
        for (let i = 0; i <= totalSteps; i++) {
            path.push(this.getPoint(i / totalSteps));
        }
        return path;
    }
}
