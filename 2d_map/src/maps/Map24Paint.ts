// Street Map — the raster layer: paper, hills, lakes and parks, district tints (step 3) and the
// small streets (Map24Streets), pixel by pixel. The boulevards and everything drawn on top are
// Map24Draw's.
import { hash } from './Map24Random';
import { LAKE_PARK, locate, streetCover, type Here, type World } from './Map24Streets';
import { heightAt, nearestLakeEdge } from './Map24Terrain';

type Rgb = [number, number, number];

const PAPER: Rgb = [244, 236, 216];
const HILL: Rgb = [214, 228, 186];
const HILL_DARK: Rgb = [196, 216, 166];
const PARK: Rgb = [222, 233, 196];
const WATER: Rgb = [163, 203, 224];
const SHORE: Rgb = [118, 165, 194];
const STREET: Rgb = [128, 118, 106];

const clamp01 = (p_v: number) => Math.min(1, Math.max(0, p_v));

function mixInto(r_out: Rgb, p_to: Rgb, p_amount: number): void {
    r_out[0] += (p_to[0] - r_out[0]) * p_amount;
    r_out[1] += (p_to[1] - r_out[1]) * p_amount;
    r_out[2] += (p_to[2] - r_out[2]) * p_amount;
}

// A soft pastel for a district (step 3 only).
function tint(p_zone: number): Rgb {
    const h = hash(p_zone, 9, 1) * Math.PI * 2;
    return [200 + 40 * Math.cos(h), 200 + 40 * Math.cos(h + 2.1), 200 + 40 * Math.cos(h + 4.2)];
}

function shade(p_w: World, p_x: number, p_y: number, p_here: Here, r_out: Rgb): void {
    const t = p_w.terrain;
    const step = p_w.settings.step;
    r_out[0] = PAPER[0];
    r_out[1] = PAPER[1];
    r_out[2] = PAPER[2];
    const height = heightAt(t, p_x, p_y);
    if (height > t.hillLevel) {
        mixInto(r_out, HILL, clamp01((height - t.hillLevel) / 0.006));
        mixInto(r_out, HILL_DARK, clamp01(t.noise(p_x / 40, p_y / 40) * 0.8));
    }
    const lake = t.lakes.length ? nearestLakeEdge(t, p_x, p_y) : Infinity;
    if (lake < 0) {
        r_out[0] = WATER[0];
        r_out[1] = WATER[1];
        r_out[2] = WATER[2];
        mixInto(r_out, SHORE, clamp01(1 + lake * 0.6));
        return;
    }
    if (step >= 5 && lake < LAKE_PARK) mixInto(r_out, PARK, 1);
    if (step < 3) return;
    locate(p_w, p_x, p_y, p_here);
    if (step === 3 && p_here.zone >= 0 && height <= t.hillLevel) mixInto(r_out, tint(p_here.zone), 0.55);
    const layers = { grid: step >= 4, hillsAndLakes: step >= 5 };
    mixInto(r_out, STREET, streetCover(p_w, p_x, p_y, p_here, step >= 4 ? lake : Infinity, layers));
}

export function paintMap(p_image: ImageData, p_w: World): void {
    const here: Here = { zone: -1, out: 0, along: 0, spread: 1, point: 0 };
    const colour: Rgb = [0, 0, 0];
    const data = p_image.data;
    for (let y = 0; y < p_image.height; y++) {
        for (let x = 0; x < p_image.width; x++) {
            shade(p_w, x + 0.5, y + 0.5, here, colour);
            const i = (y * p_image.width + x) * 4;
            data[i] = colour[0];
            data[i + 1] = colour[1];
            data[i + 2] = colour[2];
            data[i + 3] = 255;
        }
    }
}
