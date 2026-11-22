// Colour Blocks — fitting shapes into a lot (Map26Lots), for the placeholder buildings.
//
// A lot is a set of cells on the plot grid (lotOf[c] === k). fitRect: the biggest rectangle of a
// given shape, turned to the lot's road, standing a given setback back from its frontage, that is
// all on the lot (tried at full size, then smaller by halving the gap, 8 times). fitRectAround: the
// same, but centred on a given point (for a lot whose frontage is too bent for its front to lie
// on it: a whole block round a curve). biggestCircle: the
// largest circle on the lot (the lot cell farthest from any cell not on it: a distance search out
// from the lot's edge, Dijkstra-like on 8 neighbours).
import type { Point } from './Map26Borders';
import { DI, DJ, Queue, STEP } from './Map26Grow';
import type { Lot } from './Map26Lots';
import { PLOT } from './Map26Plots';

export type Rect = { x: number; y: number; angle: number; width: number; depth: number }; // fine cells
export type Circle = { x: number; y: number; radius: number }; // fine cells

const INSET = 0.2; // fine cells: a rectangle's edges are tested this far inside

// p_angle turned (by half a turn if need be) so that its rectangle's road side is its −depth side:
// p_inward (the way from the road into the lot) is then (−sin, cos) of the angle, and the side
// facing the road (sin, −cos). In Godot (rotation.y = −angle) that side is the local −z, forward.
export function faceRoad(p_angle: number, p_inward: Point): number {
    if (-Math.sin(p_angle) * p_inward.x + Math.cos(p_angle) * p_inward.y >= 0) return p_angle;
    return p_angle > 0 ? p_angle - Math.PI : p_angle + Math.PI;
}

// All of rectangle p_r on lot p_k?
export function rectOnLot(p_r: Rect, p_lotOf: Int32Array, p_n: number, p_k: number): boolean {
    const ux = Math.cos(p_r.angle);
    const uy = Math.sin(p_r.angle);
    const ea = INSET / p_r.width;
    const eb = INSET / p_r.depth;
    for (let a = -0.5 + ea; a <= 0.5 - ea + 1e-9; a += 0.4 / (p_r.width * PLOT)) {
        for (let b = -0.5 + eb; b <= 0.5 - eb + 1e-9; b += 0.4 / (p_r.depth * PLOT)) {
            const i = Math.floor((p_r.x + ux * a * p_r.width - uy * b * p_r.depth) * PLOT);
            const j = Math.floor((p_r.y + uy * a * p_r.width + ux * b * p_r.depth) * PLOT);
            if (i < 0 || j < 0 || i >= p_n || j >= p_n || p_lotOf[j * p_n + i] !== p_k) return false;
        }
    }
    return true;
}

// A rectangle p_width x p_depth (scaled by p_scale), turned to lot p_lot's road, its front p_setback
// back from the lot's frontage (the setback scales too).
function placed(p_lot: Lot, p_width: number, p_depth: number, p_setback: number, p_scale: number): Rect {
    const w = p_width * p_scale;
    const d = p_depth * p_scale;
    const back = p_setback * p_scale + d / 2;
    return { x: p_lot.front.x + p_lot.inward.x * back, y: p_lot.front.y + p_lot.inward.y * back, angle: Math.atan2(p_lot.along.y, p_lot.along.x), width: w, depth: d };
}

// The biggest p_place(scale) (scale 0.15 .. 1) that is all on lot p_k, or null.
function biggest(p_place: (s: number) => Rect, p_lotOf: Int32Array, p_n: number, p_k: number): Rect | null {
    if (rectOnLot(p_place(1), p_lotOf, p_n, p_k)) return p_place(1);
    let lo = 0.15;
    let hi = 1;
    if (!rectOnLot(p_place(lo), p_lotOf, p_n, p_k)) return null;
    for (let m = 0; m < 8; m++) {
        const mid = (lo + hi) / 2;
        if (rectOnLot(p_place(mid), p_lotOf, p_n, p_k)) lo = mid;
        else hi = mid;
    }
    return p_place(lo);
}

// The biggest rectangle of p_width x p_depth's shape (no bigger) that fits on lot p_k, or null.
export function fitRect(p_lot: Lot, p_k: number, p_lotOf: Int32Array, p_n: number, p_width: number, p_depth: number, p_setback: number): Rect | null {
    if (p_width <= 0 || p_depth <= 0) return null;
    return biggest((s) => placed(p_lot, p_width, p_depth, p_setback, s), p_lotOf, p_n, p_k);
}

// The same shape, turned to the lot's road but centred on p_centre.
export function fitRectAround(p_lot: Lot, p_k: number, p_lotOf: Int32Array, p_n: number, p_width: number, p_depth: number, p_centre: Point): Rect | null {
    if (p_width <= 0 || p_depth <= 0) return null;
    const angle = Math.atan2(p_lot.along.y, p_lot.along.x);
    return biggest((s) => ({ x: p_centre.x, y: p_centre.y, angle, width: p_width * s, depth: p_depth * s }), p_lotOf, p_n, p_k);
}

// The largest circle on lot p_k (its cells p_cells), fine cells.
export function biggestCircle(p_cells: number[], p_lotOf: Int32Array, p_n: number, p_k: number): Circle {
    const dist = new Map<number, number>();
    const queue = new Queue();
    for (const c of p_cells) { // the edge: lot cells beside a cell not on the lot
        const i = c % p_n;
        const j = (c - i) / p_n;
        let edge = false;
        for (let m = 0; m < 8 && !edge; m++) {
            const ni = i + DI[m];
            const nj = j + DJ[m];
            edge = ni < 0 || nj < 0 || ni >= p_n || nj >= p_n || p_lotOf[nj * p_n + ni] !== p_k;
        }
        if (!edge) continue;
        dist.set(c, 0.5);
        queue.push(0.5, c);
    }
    let best = p_cells[0];
    let bestD = 0;
    while (queue.size) {
        const c = queue.pop();
        const t = queue.poppedTime;
        if (t > (dist.get(c) ?? Infinity)) continue;
        if (t > bestD) [bestD, best] = [t, c];
        const i = c % p_n;
        const j = (c - i) / p_n;
        for (let m = 0; m < 8; m++) {
            const nc = (j + DJ[m]) * p_n + i + DI[m];
            if (p_lotOf[nc] !== p_k) continue;
            const nt = t + STEP[m];
            if (nt < (dist.get(nc) ?? Infinity)) {
                dist.set(nc, nt);
                queue.push(nt, nc);
            }
        }
    }
    const centre: Point = { x: ((best % p_n) + 0.5) / PLOT, y: (Math.floor(best / p_n) + 0.5) / PLOT };
    return { x: centre.x, y: centre.y, radius: bestD / PLOT };
}
