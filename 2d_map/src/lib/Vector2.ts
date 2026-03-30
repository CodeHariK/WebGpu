/**
 * Represents a 2D vector and provides high-performance math operations.
 * Designed to minimize garbage collection by supporting in-place mutation.
 */
export class Vector2 {
    public x: number;
    public y: number;

    constructor(x: number = 0, y: number = 0) {
        this.x = x;
        this.y = y;
    }

    /**
     * Sets the x and y components of this vector.
     */
    public set(x: number, y: number): this {
        this.x = x;
        this.y = y;
        return this;
    }

    /**
     * Copies the values from another vector into this one.
     */
    public copy(v: Vector2): this {
        this.x = v.x;
        this.y = v.y;
        return this;
    }

    /**
     * Adds another vector to this vector in place.
     */
    public add(v: Vector2): this {
        this.x += v.x;
        this.y += v.y;
        return this;
    }

    /**
     * Subtracts another vector from this vector in place.
     */
    public sub(v: Vector2): this {
        this.x -= v.x;
        this.y -= v.y;
        return this;
    }

    /**
     * Multiplies this vector by a scalar value in place.
     */
    public mult(scalar: number): this {
        this.x *= scalar;
        this.y *= scalar;
        return this;
    }

    /**
     * Calculates the squared magnitude (length) of the vector.
     * Faster than magnitude() because it avoids the Math.sqrt() call.
     * Highly useful for distance comparisons.
     */
    public lengthSq(): number {
        return this.x * this.x + this.y * this.y;
    }

    /**
     * Calculates the magnitude (length) of the vector.
     */
    public mag(): number {
        return Math.sqrt(this.lengthSq());
    }

    /**
     * Normalizes the vector (makes its length exactly 1) in place.
     */
    public normalize(): this {
        const m = this.mag();
        if (m > 0) {
            this.x /= m;
            this.y /= m;
        }
        return this;
    }

    /**
     * Calculates the dot product of this vector and another.
     */
    public dot(v: Vector2): number {
        return this.x * v.x + this.y * v.y;
    }

    /**
     * Calculates the 2D cross product of this vector and another.
     * In 2D, the cross product is a scalar (a single number), representing
     * the area of the parallelogram formed by the two vectors.
     */
    public cross(v: Vector2): number {
        return this.x * v.y - this.y * v.x;
    }

    // --- STATIC METHODS (For use with object pooling) ---

    /**
     * Adds two vectors and stores the result in the 'out' vector.
     * This avoids creating a new Vector2 instance.
     */
    public static add(a: Vector2, b: Vector2, out: Vector2): Vector2 {
        out.x = a.x + b.x;
        out.y = a.y + b.y;
        return out;
    }

    /**
     * Calculates the distance between two vectors.
     */
    public static dist(a: Vector2, b: Vector2): number {
        const dx = a.x - b.x;
        const dy = a.y - b.y;
        return Math.sqrt(dx * dx + dy * dy);
    }

    /**
     * Subtracts vector b from vector a and stores the result in 'out'.
     */
    public static sub(a: Vector2, b: Vector2, out: Vector2): Vector2 {
        out.x = a.x - b.x;
        out.y = a.y - b.y;
        return out;
    }

    /**
     * Rotates this vector 90 degrees counter-clockwise (perpendicular) in place.
     */
    public perp(): this {
        const x = this.x;
        this.x = -this.y;
        this.y = x;
        return this;
    }

    public clone(): Vector2 {
        return new Vector2(this.x, this.y);
    }

    /**
     * Cross product (scalar x vector), returns a new vector.
     */
    public crossSv(scalar: number): Vector2 {
        return new Vector2(-this.y * scalar, this.x * scalar);
    }

    /**
     * Rotates this vector 90 degrees clockwise, returns a new vector.
     */
    public rotate90CW(): Vector2 {
        return new Vector2(this.y, -this.x);
    }

    /**
     * Returns a new vector containing the element-wise minimum of this and another vector.
     */
    public min(other: Vector2): Vector2 {
        return new Vector2(Math.min(this.x, other.x), Math.min(this.y, other.y));
    }

    /**
     * Returns a new vector containing the element-wise maximum of this and another vector.
     */
    public max(other: Vector2): Vector2 {
        return new Vector2(Math.max(this.x, other.x), Math.max(this.y, other.y));
    }
}
