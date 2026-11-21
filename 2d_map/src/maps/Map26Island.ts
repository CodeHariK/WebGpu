// Colour Blocks — step 8: one island, a noise map with a circular falloff (as Map 25's islands).
//
// A height for every point: noise (4 sizes added, each half the size and half as strong) mixed
// with a hill that is 1 in the map's middle and falls to 0 at islandRadius away
// (1 − (distance / radius)²). Land is where the height is above sea level (0.5): near the middle
// the hill keeps it land, near the coast only the noise's high spots poke out (headlands, islets)
// and its low spots sink (bays). islandNoise is how much the noise counts against the hill. The
// colours come from the real distance to the coast, measured and painted as in Map 25. Its parts can
// be looked at on their own (falloff, noise, height, in grey; the sea level in red), and the
// falloff's rings drawn over any view. The city (step 9) reads the coast distance (coastDistance).
import type { NoiseFunction2D } from 'simplex-noise';
import { paintIslands } from './Map25Paint';
import { GRID, measureDistance, type Shape } from './Map25Shape';
import type { Settings } from './Map26Settings';

const SEA_LEVEL = 0.5;

export type IslandView = 'map' | 'falloff' | 'noise' | 'height';

// shape: the coast distance (for the map; negative on land); falloff, noise, height: each part on
// the same grid (for the other views); land: the share of the map that is land
export type Island = { shape: Shape; falloff: Float32Array; noise: Float32Array; height: Float32Array; radius: number; land: number };

// 4 sizes of noise added, −1..1.
function fbm(p_noise: NoiseFunction2D, p_x: number, p_y: number): number {
    let sum = 0;
    let strength = 0.5;
    let scale = 1;
    for (let i = 0; i < 4; i++) {
        sum += p_noise(p_x * scale, p_y * scale) * strength;
        strength *= 0.5;
        scale *= 2;
    }
    return sum / 0.9375;
}

// The two parts of the height at (x, y) pixels: the falloff (1 in the middle, 0 at the radius,
// negative beyond) and the noise (0..1).
function parts(p_x: number, p_y: number, p_size: number, p_settings: Settings, p_noise: NoiseFunction2D): [number, number] {
    const radius = p_settings.islandRadius * p_size;
    const r = Math.hypot(p_x - p_size / 2, p_y - p_size / 2) / radius;
    const noise = 0.5 + 0.5 * fbm(p_noise, p_x / p_settings.islandNoiseSize + 71.3, p_y / p_settings.islandNoiseSize - 23.9);
    return [1 - r * r, noise];
}

export function makeIsland(p_size: number, p_settings: Settings, p_noise: NoiseFunction2D): Island {
    const cols = Math.ceil(p_size / GRID);
    const rows = cols;
    const count = (cols + 1) * (rows + 1);
    const field = new Float32Array(count);
    const falloff = new Float32Array(count);
    const noise = new Float32Array(count);
    const height = new Float32Array(count);
    const radius = p_settings.islandRadius * p_size;
    let land = 0;
    for (let j = 0; j <= rows; j++) {
        for (let i = 0; i <= cols; i++) {
            const c = j * (cols + 1) + i;
            [falloff[c], noise[c]] = parts(i * GRID, j * GRID, p_size, p_settings, p_noise);
            height[c] = falloff[c] * (1 - p_settings.islandNoise) + noise[c] * p_settings.islandNoise;
            field[c] = (SEA_LEVEL - height[c]) * radius; // only its sign matters: measured next
            if (height[c] > SEA_LEVEL) land++;
        }
    }
    return { shape: { cols, rows, field: measureDistance(field, cols + 1, rows + 1) }, falloff, noise, height, radius, land: land / count };
}

// How far (pixels) the point (x, y) is from the coast: negative on land (the nearest grid point).
export function coastDistance(p_island: Island, p_x: number, p_y: number): number {
    const w = p_island.shape.cols + 1;
    const i = Math.min(w - 1, Math.max(0, Math.round(p_x / GRID)));
    const j = Math.min(w - 1, Math.max(0, Math.round(p_y / GRID)));
    return p_island.shape.field[j * w + i];
}

// One part (falloff, noise or height) in grey: black at 0, white at 1; where it is above sea level
// a faint green tint, and the sea level itself as a red line (sharp: read from the grid).
function drawPart(p_ctx: CanvasRenderingContext2D, p_values: Float32Array, p_island: Island): void {
    const w = p_island.shape.cols + 1;
    const canvas = document.createElement('canvas');
    canvas.width = w;
    canvas.height = w;
    const small = canvas.getContext('2d')!;
    const image = small.createImageData(w, w);
    for (let c = 0; c < p_values.length; c++) {
        const v = Math.min(1, Math.max(0, p_values[c]));
        const grey = Math.round(v * 255);
        const above = p_values[c] > SEA_LEVEL;
        image.data[c * 4] = above ? grey * 0.85 : grey;
        image.data[c * 4 + 1] = grey;
        image.data[c * 4 + 2] = above ? grey * 0.8 : grey;
        image.data[c * 4 + 3] = 255;
        const right = c % w < w - 1 ? p_values[c + 1] > SEA_LEVEL : above;
        const below = c + w < p_values.length ? p_values[c + w] > SEA_LEVEL : above;
        if (right !== above || below !== above) {
            image.data[c * 4] = 220;
            image.data[c * 4 + 1] = 50;
            image.data[c * 4 + 2] = 40;
        }
    }
    small.putImageData(image, 0, 0);
    p_ctx.save();
    p_ctx.imageSmoothingEnabled = false;
    p_ctx.drawImage(canvas, 0, 0, p_ctx.canvas.width, p_ctx.canvas.height);
    p_ctx.restore();
}

// The falloff's rings: where it reaches 0 (dashed: the radius), and where it is 0.5 (dotted: the
// coast if the noise were everywhere average).
export function drawRings(p_ctx: CanvasRenderingContext2D, p_island: Island): void {
    const size = p_ctx.canvas.width;
    const rings: [number, number[], string][] = [
        [p_island.radius, [8, 6], 'falloff 0 (radius)'],
        [p_island.radius * Math.SQRT1_2, [2, 5], 'falloff 0.5'],
    ];
    p_ctx.save();
    p_ctx.strokeStyle = 'rgba(40, 30, 20, 0.75)';
    p_ctx.fillStyle = 'rgba(40, 30, 20, 0.85)';
    p_ctx.lineWidth = 1.5;
    p_ctx.font = '600 12px Inter, sans-serif';
    p_ctx.textAlign = 'center';
    for (const [r, dash, label] of rings) {
        p_ctx.setLineDash(dash);
        p_ctx.beginPath();
        p_ctx.arc(size / 2, size / 2, r, 0, Math.PI * 2);
        p_ctx.stroke();
        p_ctx.fillText(label, size / 2, size / 2 - r - 5);
    }
    p_ctx.setLineDash([]);
    p_ctx.beginPath(); // the middle
    p_ctx.arc(size / 2, size / 2, 3, 0, Math.PI * 2);
    p_ctx.fill();
    p_ctx.restore();
}

// The island on its own: the map, or one of its parts in grey; the falloff's rings over it if asked.
export function drawIsland(p_ctx: CanvasRenderingContext2D, p_island: Island, p_view: IslandView, p_rings: boolean): void {
    if (p_view === 'map') {
        const image = p_ctx.createImageData(p_ctx.canvas.width, p_ctx.canvas.height);
        paintIslands(image, p_island.shape);
        p_ctx.putImageData(image, 0, 0);
    } else {
        const values = { falloff: p_island.falloff, noise: p_island.noise, height: p_island.height }[p_view];
        drawPart(p_ctx, values, p_island);
    }
    if (p_rings) drawRings(p_ctx, p_island);
}
