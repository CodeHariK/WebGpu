// Street Map — the ground: a height map (hills where it is high) and lakes in low places.
import type { NoiseFunction2D } from 'simplex-noise';
import { fbm, makeNoise } from './Map24Random';
import type { Settings } from './Map24Settings';

export const CELL = 2; // pixels a grid cell (the height map, the districts)

export type Lake = {
    x: number;
    y: number;
    r: number;
    angle: number; // the oval's direction
    stretch: number; // how much longer than wide
    waves: { k: number; a: number; phase: number }[]; // bumps round the shore
};

export type Terrain = {
    width: number;
    height: number;
    cols: number; // grid corners: (cols + 1) x (rows + 1)
    rows: number;
    heights: Float32Array;
    hillLevel: number; // heights above this are hills
    lakes: Lake[];
    noise: NoiseFunction2D; // for anything else that wants soft randomness
};

const HEIGHT_SCALE = 1 / 230; // noise units a pixel: hills a few hundred pixels across

export function heightAt(p_t: Terrain, p_x: number, p_y: number): number {
    const gx = Math.min(Math.max(p_x / CELL, 0), p_t.cols - 0.001);
    const gy = Math.min(Math.max(p_y / CELL, 0), p_t.rows - 0.001);
    const i = Math.floor(gx);
    const j = Math.floor(gy);
    const fx = gx - i;
    const fy = gy - j;
    const w = p_t.cols + 1;
    const h = p_t.heights;
    const top = h[j * w + i] + (h[j * w + i + 1] - h[j * w + i]) * fx;
    const bottom = h[(j + 1) * w + i] + (h[(j + 1) * w + i + 1] - h[(j + 1) * w + i]) * fx;
    return top + (bottom - top) * fy;
}

// How fast the height changes a pixel (for turning a height difference into pixels).
export function heightSlope(p_t: Terrain, p_x: number, p_y: number): number {
    const dx = heightAt(p_t, p_x + CELL, p_y) - heightAt(p_t, p_x - CELL, p_y);
    const dy = heightAt(p_t, p_x, p_y + CELL) - heightAt(p_t, p_x, p_y - CELL);
    return Math.hypot(dx, dy) / (2 * CELL) + 1e-6;
}

// Pixels from the lake's shore: negative in the water.
export function lakeEdge(p_lake: Lake, p_x: number, p_y: number): number {
    const dx = p_x - p_lake.x;
    const dy = p_y - p_lake.y;
    const c = Math.cos(-p_lake.angle);
    const s = Math.sin(-p_lake.angle);
    const lx = dx * c - dy * s;
    const ly = (dx * s + dy * c) * p_lake.stretch;
    const theta = Math.atan2(ly, lx);
    let radius = p_lake.r;
    for (const w of p_lake.waves) radius += p_lake.r * w.a * Math.cos(w.k * theta + w.phase);
    return (Math.hypot(lx, ly) - radius) / Math.sqrt(p_lake.stretch);
}

export function nearestLakeEdge(p_t: Terrain, p_x: number, p_y: number): number {
    let edge = Infinity;
    for (const lake of p_t.lakes) edge = Math.min(edge, lakeEdge(lake, p_x, p_y));
    return edge;
}

// The height above which the given share of the map lies.
function quantile(p_values: Float32Array, p_share: number): number {
    const sorted = Float32Array.from(p_values).sort();
    return sorted[Math.min(sorted.length - 1, Math.floor(sorted.length * (1 - p_share)))];
}

// Lakes in low places, clear of the edges, the hills and each other.
function placeLakes(p_t: Terrain, p_settings: Settings, p_random: () => number): Lake[] {
    const lakes: Lake[] = [];
    const margin = 70;
    for (let n = 0; n < p_settings.lakes; n++) {
        let best: Lake | null = null;
        let bestHeight = Infinity;
        for (let tries = 0; tries < 160; tries++) {
            const r = p_settings.lakeSize * (0.75 + p_random() * 0.5);
            const x = margin + p_random() * (p_t.width - 2 * margin);
            const y = margin + p_random() * (p_t.height - 2 * margin);
            const height = heightAt(p_t, x, y);
            if (height > p_t.hillLevel - 0.06) continue;
            if (lakes.some((l) => Math.hypot(x - l.x, y - l.y) < l.r + r + 90)) continue;
            if (height < bestHeight) {
                bestHeight = height;
                best = {
                    x,
                    y,
                    r,
                    angle: p_random() * Math.PI,
                    stretch: 1.3 + p_random() * 0.6,
                    waves: [2, 3, 5].map((k) => ({ k, a: (0.12 / k) * (0.5 + p_random()), phase: p_random() * Math.PI * 2 })),
                };
            }
        }
        if (best) lakes.push(best);
    }
    return lakes;
}

export function makeTerrain(p_settings: Settings, p_width: number, p_height: number, p_random: () => number): Terrain {
    const noise = makeNoise(p_random);
    const cols = Math.ceil(p_width / CELL);
    const rows = Math.ceil(p_height / CELL);
    const heights = new Float32Array((cols + 1) * (rows + 1));
    for (let j = 0; j <= rows; j++) {
        for (let i = 0; i <= cols; i++) {
            heights[j * (cols + 1) + i] = fbm(noise, i * CELL * HEIGHT_SCALE, j * CELL * HEIGHT_SCALE, 4);
        }
    }
    const terrain: Terrain = { width: p_width, height: p_height, cols, rows, heights, hillLevel: quantile(heights, p_settings.hills), lakes: [], noise };
    terrain.lakes = placeLakes(terrain, p_settings, p_random);
    return terrain;
}
