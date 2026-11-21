// Islands — the shape: for every point, a signed distance to the coast (negative on land), worked
// out once on a grid and read back smoothly (bilinear) by each pixel.
//
//   each island      its hill: noise plus a falloff, land above sea level (Map25Noise)
//   its headlands    fat lines out to its bridges, melted in with a smooth minimum
//   its territory    and never past half a channel from its border: islands can't touch
//   the map          the nearest island wins (only islands whose box is near are measured)
//   then             the real distance to that coast, measured on the grid (measureDistance)
import type { Island, Layout } from './Map25Layout';
import type { NoiseFunction2D } from 'simplex-noise';
import { hillDistance } from './Map25Noise';
import type { Headland, Parts } from './Map25Parts';
import type { Settings } from './Map25Settings';

export const GRID = 2; // pixels a grid cell
const FAR = 50; // pixels out from the coast where the water is fully deep: no need to measure further

export type Shape = { cols: number; rows: number; field: Float32Array };

function capsule(p_x: number, p_y: number, p_b: Headland): number {
    const dx = p_b.bx - p_b.ax;
    const dy = p_b.by - p_b.ay;
    const t = Math.min(1, Math.max(0, ((p_x - p_b.ax) * dx + (p_y - p_b.ay) * dy) / (dx * dx + dy * dy || 1)));
    return Math.hypot(p_x - (p_b.ax + dx * t), p_y - (p_b.ay + dy * t)) - (p_b.ar + (p_b.br - p_b.ar) * t);
}

// A minimum that rounds off the corner where a and b meet, over a width of k.
function smoothMin(p_a: number, p_b: number, p_k: number): number {
    const h = Math.max(p_k - Math.abs(p_a - p_b), 0) / p_k;
    return Math.min(p_a, p_b) - h * h * p_k * 0.25;
}

// Pixels from (x, y) to the nearest border of island p_i's territory.
function toBorder(p_islands: Island[], p_i: Island, p_x: number, p_y: number): number {
    let nearest = Infinity;
    const mine = (p_x - p_i.x) ** 2 + (p_y - p_i.y) ** 2 - p_i.size * p_i.size;
    for (const j of p_islands) {
        if (j === p_i) continue;
        const theirs = (p_x - j.x) ** 2 + (p_y - j.y) ** 2 - j.size * j.size;
        nearest = Math.min(nearest, (theirs - mine) / (2 * Math.hypot(j.x - p_i.x, j.y - p_i.y)));
    }
    return nearest;
}

// Pixels to the nearest coast (negative on land), roughly: each island near enough is measured,
// the nearest wins. Far from every island it is just FAR (open water).
function distanceAt(p_x: number, p_y: number, p_islands: Island[], p_boxes: Box[], p_parts: Parts[], p_settings: Settings, p_noise: NoiseFunction2D): number {
    let nearest = FAR;
    for (const island of p_islands) {
        const box = p_boxes[island.id];
        if (p_x < box.x0 || p_x > box.x1 || p_y < box.y0 || p_y > box.y1) continue;
        const parts = p_parts[island.id];
        let d = hillDistance(p_x, p_y, parts.hill, p_settings, p_noise);
        for (const h of parts.headlands) d = smoothMin(d, capsule(p_x, p_y, h), p_settings.blend);
        const room = toBorder(p_islands, island, p_x, p_y) - p_settings.channel / 2;
        nearest = Math.min(nearest, -smoothMin(-d, room, 6)); // the larger of the two, softly: no land past the border
    }
    return nearest;
}

type Box = { x0: number; y0: number; x1: number; y1: number };

// How far out from an island anything is measured: its falloff's reach, plus as far as the water
// shading looks.
function boxOf(p_parts: Parts): Box {
    const reach = p_parts.hill.size + FAR;
    return { x0: p_parts.hill.x - reach, y0: p_parts.hill.y - reach, x1: p_parts.hill.x + reach, y1: p_parts.hill.y + reach };
}

// The shape's field says where the coast is (where it crosses zero), but further out it is only a
// rough distance (a height difference, not pixels). So the real distance to the coast is measured on the grid: start at the grid points right by the coast
// (their own value, under a cell), then sweep the grid twice (down-right, then up-left), each point
// taking the nearest of its neighbours' distances plus the step to them (a chamfer distance).
export function measureDistance(p_field: Float32Array, p_w: number, p_h: number): Float32Array {
    const dist = new Float32Array(p_field.length).fill(FAR);
    for (let j = 0; j < p_h; j++) {
        for (let i = 0; i < p_w; i++) {
            const c = j * p_w + i;
            const land = p_field[c] < 0;
            const coastal =
                (i > 0 && p_field[c - 1] < 0 !== land) ||
                (i < p_w - 1 && p_field[c + 1] < 0 !== land) ||
                (j > 0 && p_field[c - p_w] < 0 !== land) ||
                (j < p_h - 1 && p_field[c + p_w] < 0 !== land);
            if (coastal) dist[c] = Math.min(Math.abs(p_field[c]), GRID);
        }
    }
    const straight = GRID;
    const diagonal = GRID * Math.SQRT2;
    const relax = (c: number, n: number, step: number) => {
        if (dist[n] + step < dist[c]) dist[c] = dist[n] + step;
    };
    for (let j = 0; j < p_h; j++) {
        for (let i = 0; i < p_w; i++) {
            const c = j * p_w + i;
            if (i > 0) relax(c, c - 1, straight);
            if (j > 0) relax(c, c - p_w, straight);
            if (i > 0 && j > 0) relax(c, c - p_w - 1, diagonal);
            if (i < p_w - 1 && j > 0) relax(c, c - p_w + 1, diagonal);
        }
    }
    for (let j = p_h - 1; j >= 0; j--) {
        for (let i = p_w - 1; i >= 0; i--) {
            const c = j * p_w + i;
            if (i < p_w - 1) relax(c, c + 1, straight);
            if (j < p_h - 1) relax(c, c + p_w, straight);
            if (i < p_w - 1 && j < p_h - 1) relax(c, c + p_w + 1, diagonal);
            if (i > 0 && j < p_h - 1) relax(c, c + p_w - 1, diagonal);
        }
    }
    for (let c = 0; c < dist.length; c++) if (p_field[c] < 0) dist[c] = -dist[c];
    return dist;
}

export function makeShape(p_width: number, p_height: number, p_layout: Layout, p_parts: Parts[], p_settings: Settings, p_noise: NoiseFunction2D): Shape {
    const cols = Math.ceil(p_width / GRID);
    const rows = Math.ceil(p_height / GRID);
    const field = new Float32Array((cols + 1) * (rows + 1));
    const boxes = p_parts.map(boxOf);
    for (let j = 0; j <= rows; j++) {
        for (let i = 0; i <= cols; i++) field[j * (cols + 1) + i] = distanceAt(i * GRID, j * GRID, p_layout.islands, boxes, p_parts, p_settings, p_noise);
    }
    return { cols, rows, field: measureDistance(field, cols + 1, rows + 1) };
}

// The coast distance at any pixel, blended from the four grid corners round it.
export function coastAt(p_shape: Shape, p_x: number, p_y: number): number {
    const gx = Math.min(Math.max(p_x / GRID, 0), p_shape.cols - 0.001);
    const gy = Math.min(Math.max(p_y / GRID, 0), p_shape.rows - 0.001);
    const i = Math.floor(gx);
    const j = Math.floor(gy);
    const fx = gx - i;
    const fy = gy - j;
    const w = p_shape.cols + 1;
    const f = p_shape.field;
    const top = f[j * w + i] + (f[j * w + i + 1] - f[j * w + i]) * fx;
    const bottom = f[(j + 1) * w + i] + (f[(j + 1) * w + i + 1] - f[(j + 1) * w + i]) * fx;
    return top + (bottom - top) * fy;
}
