import { Vector2 } from './Vector2';

/**
 * Spline utility for generating smooth paths from control points.
 */
export class Spline {
    private points: Vector2[];

    constructor(points: Vector2[]) {
        this.points = points;
    }

    /**
     * Get a point on the spline at parameter t (0.0 to 1.0).
     * This implementation uses Catmull-Rom interpolation to ensure the curve
     * passes directly through all control points.
     */
    public getPoint(t: number): Vector2 {
        if (this.points.length === 0) return new Vector2(0, 0);
        if (this.points.length === 1) return this.points[0];
        if (this.points.length === 2) {
            // Linear interpolation for just 2 points
            return new Vector2(
                this.points[0].x + (this.points[1].x - this.points[0].x) * t,
                this.points[0].y + (this.points[1].y - this.points[0].y) * t
            );
        }

        // Clamp t to [0, 1]
        t = Math.max(0, Math.min(1, t));

        // Determine which segment we are in
        const segments = this.points.length - 1;
        const scaledT = t * segments;
        const i = Math.min(Math.floor(scaledT), segments - 1);
        const u = scaledT - i;

        // Control points for the current segment [i, i+1]
        // We need 4 points: p[i-1], p[i], p[i+1], p[i+2]
        const p0 = this.points[i === 0 ? 0 : i - 1];
        const p1 = this.points[i];
        const p2 = this.points[i + 1];
        const p3 = this.points[i + 1 === segments ? segments : i + 2];

        return this.catmullRom(p0, p1, p2, p3, u);
    }

    /**
     * Standard Catmull-Rom spline formula.
     */
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
