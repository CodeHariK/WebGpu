// Islands — the painted layer, from the coast distance alone: grass getting darker inland, a sand
// rim, a dark coast line, shallow water near the shore turning deep, a foam line off the coast.
import { coastAt, type Shape } from './Map25Shape';

type Rgb = [number, number, number];

const GRASS: Rgb = [163, 209, 114];
const GRASS_DEEP: Rgb = [122, 177, 88];
const SAND: Rgb = [245, 228, 172];
const COAST: Rgb = [78, 104, 60];
const SHALLOW: Rgb = [146, 214, 226];
const DEEP: Rgb = [74, 148, 196];
const FOAM: Rgb = [255, 255, 255];
const SAND_WIDTH = 4; // pixels of beach
const FOAM_OUT = 7; // the foam line, this far off the coast

const clamp01 = (p_v: number) => Math.min(1, Math.max(0, p_v));
const line = (p_distance: number, p_width: number) => clamp01(p_width * 0.5 + 0.5 - Math.abs(p_distance));

function set(r_out: Rgb, p_c: Rgb): void {
    r_out[0] = p_c[0];
    r_out[1] = p_c[1];
    r_out[2] = p_c[2];
}

function mix(r_out: Rgb, p_c: Rgb, p_t: number): void {
    r_out[0] += (p_c[0] - r_out[0]) * p_t;
    r_out[1] += (p_c[1] - r_out[1]) * p_t;
    r_out[2] += (p_c[2] - r_out[2]) * p_t;
}

function shade(p_coast: number, r_out: Rgb): void {
    if (p_coast < 0) {
        set(r_out, GRASS);
        mix(r_out, GRASS_DEEP, clamp01((-p_coast - 10) / 50));
        mix(r_out, SAND, clamp01(p_coast + SAND_WIDTH + 0.5)); // the beach, soft-edged
    } else {
        set(r_out, SHALLOW);
        mix(r_out, DEEP, clamp01(p_coast / 45));
        mix(r_out, FOAM, line(p_coast - FOAM_OUT, 1.2) * 0.55);
    }
    mix(r_out, COAST, line(p_coast, 1.5));
}

export function paintIslands(p_image: ImageData, p_shape: Shape): void {
    const colour: Rgb = [0, 0, 0];
    const data = p_image.data;
    for (let y = 0; y < p_image.height; y++) {
        for (let x = 0; x < p_image.width; x++) {
            shade(coastAt(p_shape, x + 0.5, y + 0.5), colour);
            const i = (y * p_image.width + x) * 4;
            data[i] = colour[0];
            data[i + 1] = colour[1];
            data[i + 2] = colour[2];
            data[i + 3] = 255;
        }
    }
}
