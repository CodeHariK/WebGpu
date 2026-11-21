// Colour Blocks — the second level: districts inside each block, the same way the blocks were made.
//
// On a finer grid (DETAIL cells to a block cell, each way) the highways are drawn in as walls, two
// cells thick. The walls cut the map into regions, one a block (a flood fill finds them). Each
// region gets a few seeds (one for every districtSize² of area, 1 to 6), and they spread exactly as
// the blocks did (Map26Grow), but never into a wall: a district can't cross a highway. Tidied the
// same way (Map26Tidy). Where two districts meet runs a main road (Map26Borders); it ends where
// it meets a highway. On the island (step 9) the outside of the city is a wall as well (every fine
// cell of a block cell outside it), and the slivers cut off between the ring road and the block
// cells' steps (regions under MIN_DISTRICT) go outside too: the city ends at the ring road.
import type { NoiseFunction2D } from 'simplex-noise';
import { findBorders, type Borders, type Highway } from './Map26Borders';
import { grow, NEIGHBOURS, type Seed } from './Map26Grow';
import type { Settings } from './Map26Settings';
import type { Spread } from './Map26Spread';
import { tidy } from './Map26Tidy';

export const DETAIL = 3; // fine cells to a block cell, each way
const MIN_DISTRICT = 120; // fine cells: smaller districts are dropped
const TRIM = 3; // fine cells: strands and notches narrower than 2 x TRIM + 1 are trimmed
const MAX_SEEDS = 6; // districts in one block at most
const SIDES = NEIGHBOURS.slice(0, 4);

export type Districts = {
    n: number; // fine cells along each side
    owner: Int32Array; // per fine cell: its district (−1: under a highway, or outside the city)
    outside: Uint8Array | null; // per fine cell: 1 outside the city (step 9), else null
    block: number[]; // per district: its block's colour
    seeds: Seed[];
    roads: Borders; // the main roads, in fine cells
};

// The highways as walls on the fine grid: every point along them stamped 2 x 2 cells.
function wallsFrom(p_highways: Highway[], p_n: number): Uint8Array {
    const walls = new Uint8Array(p_n * p_n);
    const stamp = (x: number, y: number) => {
        const i0 = Math.floor(x - 0.5);
        const j0 = Math.floor(y - 0.5);
        for (let j = j0; j <= j0 + 1; j++) for (let i = i0; i <= i0 + 1; i++) if (i >= 0 && j >= 0 && i < p_n && j < p_n) walls[j * p_n + i] = 1;
    };
    for (const h of p_highways) {
        for (let k = 0; k < h.points.length - 1; k++) {
            const ax = h.points[k].x * DETAIL;
            const ay = h.points[k].y * DETAIL;
            const bx = h.points[k + 1].x * DETAIL;
            const by = h.points[k + 1].y * DETAIL;
            const steps = Math.max(1, Math.ceil(Math.hypot(bx - ax, by - ay) * 2)); // every half cell
            for (let s = 0; s <= steps; s++) stamp(ax + ((bx - ax) * s) / steps, ay + ((by - ay) * s) / steps);
        }
    }
    return walls;
}

// The fine cells under a block cell outside the city.
function outsideFrom(p_spread: Spread, p_n: number): Uint8Array {
    const outside = new Uint8Array(p_n * p_n);
    for (let j = 0; j < p_n; j++) {
        for (let i = 0; i < p_n; i++) outside[j * p_n + i] = p_spread.owner[Math.floor(j / DETAIL) * p_spread.n + Math.floor(i / DETAIL)] < 0 ? 1 : 0;
    }
    return outside;
}

// The regions between the walls (side-to-side flood fills): the cells of each.
function regionsOf(p_walls: Uint8Array, p_n: number): number[][] {
    const seen = new Uint8Array(p_n * p_n);
    const regions: number[][] = [];
    for (let start = 0; start < p_n * p_n; start++) {
        if (seen[start] || p_walls[start]) continue;
        const cells = [start];
        seen[start] = 1;
        for (let q = 0; q < cells.length; q++) {
            const c = cells[q];
            const i = c % p_n;
            const j = (c - i) / p_n;
            for (const [di, dj] of SIDES) {
                const ni = i + di;
                const nj = j + dj;
                if (ni < 0 || nj < 0 || ni >= p_n || nj >= p_n) continue;
                const nc = nj * p_n + ni;
                if (seen[nc] || p_walls[nc]) continue;
                seen[nc] = 1;
                cells.push(nc);
            }
        }
        regions.push(cells);
    }
    return regions;
}

// A region's seeds: one per districtSize² of area (1 .. MAX_SEEDS), spread apart (each the farthest
// of a few random tries from those placed), on easy ground where possible.
function seedRegion(p_cells: number[], p_n: number, p_count: number, p_firstColour: number, p_cost: Float32Array, p_easy: number, p_random: () => number): Seed[] {
    const seeds: Seed[] = [];
    for (let k = 0; k < p_count; k++) {
        let best = -1;
        let bestGap = -1;
        for (let tries = 0; tries < 30; tries++) {
            const c = p_cells[Math.floor(p_random() * p_cells.length)];
            if (p_cost[c] > p_easy && tries < 20) continue;
            const i = c % p_n;
            const j = (c - i) / p_n;
            const gap = Math.min(Infinity, ...seeds.map((s) => Math.hypot(s.i - i, s.j - j)));
            if (gap > bestGap) {
                bestGap = gap;
                best = c;
            }
        }
        if (best >= 0) seeds.push({ i: best % p_n, j: Math.floor(best / p_n), colour: p_firstColour + k });
    }
    return seeds;
}

// p_city: on the island (step 9): the outside of the city is a wall too.
export function makeDistricts(p_spread: Spread, p_highways: Highway[], p_settings: Settings, p_random: () => number, p_noise: NoiseFunction2D, p_city: boolean): Districts {
    const n = p_spread.n * DETAIL;
    const cost = new Float32Array(n * n);
    for (let j = 0; j < n; j++) {
        for (let i = 0; i < n; i++) { // the same ground as the blocks, read finer
            const v = 0.5 + 0.5 * p_noise(i / DETAIL / p_settings.noiseSize, j / DETAIL / p_settings.noiseSize);
            cost[j * n + i] = 1 + p_settings.roughness * v * v;
        }
    }
    const walls = wallsFrom(p_highways, n);
    const outside = p_city ? outsideFrom(p_spread, n) : null;
    if (outside) for (let c = 0; c < n * n; c++) walls[c] |= outside[c];
    const area = (p_settings.districtSize * DETAIL) ** 2;
    const seeds: Seed[] = [];
    for (const cells of regionsOf(walls, n)) {
        if (outside && cells.length < MIN_DISTRICT) { // a sliver past the ring road: outside
            for (const c of cells) walls[c] = outside[c] = 1;
            continue;
        }
        const count = Math.max(1, Math.min(MAX_SEEDS, Math.round(cells.length / area)));
        seeds.push(...seedRegion(cells, n, count, seeds.length, cost, 1 + p_settings.roughness * 0.2, p_random));
    }
    const speed = seeds.map(() => 1 + (p_random() * 2 - 1) * p_settings.speedVariety * 0.35);
    const { owner } = grow(n, cost, seeds, speed, walls);
    for (let c = 0; c < n * n; c++) if (walls[c]) owner[c] = -1;
    tidy(n, owner, seeds, MIN_DISTRICT, TRIM);
    const block = seeds.map((s) => p_spread.owner[Math.floor(s.j / DETAIL) * p_spread.n + Math.floor(s.i / DETAIL)]);
    const roads = findBorders(n, owner, p_settings.straighten * DETAIL, p_settings.smoothing);
    return { n, owner, outside, block, seeds, roads };
}
