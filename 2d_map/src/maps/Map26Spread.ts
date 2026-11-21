// Colour Blocks — the first level: the grid, the seeds and the spread into blocks.
//
// Every cell has a cost to step into: 1 plus noise (roughness). All seeds start at time 0 and
// spread at once (Map26Grow): every cell goes to the colour that reaches it first. A colour's speed
// divides its step costs, so faster colours win more cells. The arrival times also give the
// animation: a cell shows its colour once the clock passes its time. Afterwards the blocks are
// tidied (Map26Tidy).
import type { NoiseFunction2D } from 'simplex-noise';
import type { Settings } from './Map26Settings';

import { grow, type Seed } from './Map26Grow';
import { tidy } from './Map26Tidy';

const MIN_BLOCK = 40; // cells: a colour left with less than this (even its seed's piece) is dropped
const TRIM = 2; // cells: strands, tips and notches narrower than 2 x TRIM + 1 are trimmed

export type { Seed } from './Map26Grow';

export type Spread = {
    n: number; // cells along each side
    owner: Int32Array; // per cell: its block's colour (after tidying)
    reached: Int32Array; // per cell: the colour that reached it first (before tidying: what the spread shows)
    time: Float64Array; // per cell: when it got there
    lastTime: number; // when the last cell was reached
    seeds: Seed[];
    cost: Float32Array; // per cell: how long it takes to step into (1 + roughness x noise²)
    speed: number[]; // per colour: its speed (step times are divided by it)
};

// Each colour's first seed: spread apart and on easy ground, each the best of a few random tries
// (the one farthest from those placed), and only tries where stepping is cheap (else a seed in a
// slow patch barely spreads before its neighbours close round it). Then some colours get extra
// seeds (0 .. seedsPerColour - 1 more) in a chain: each about chainStep cells on from the one
// before, in roughly the same direction, and clear of every other seed (far apart enough to
// shape the block: long and bent, not round).
function placeSeeds(p_n: number, p_settings: Settings, p_cost: Float32Array, p_easy: number, p_random: () => number): Seed[] {
    const seeds: Seed[] = [];
    const margin = Math.floor(p_n * 0.06); // not right at the map's edge
    const inside = (v: number) => Math.min(p_n - 1 - margin, Math.max(margin, v));
    for (let k = 0; k < p_settings.seeds; k++) {
        let best: Seed = { i: 0, j: 0, colour: k };
        let bestGap = -1;
        for (let tries = 0; tries < 40; tries++) {
            const p: Seed = { i: margin + Math.floor(p_random() * (p_n - 2 * margin)), j: margin + Math.floor(p_random() * (p_n - 2 * margin)), colour: k };
            if (p_cost[p.j * p_n + p.i] > p_easy && tries < 30) continue; // the last tries take any ground
            const gap = Math.min(Infinity, ...seeds.map((s) => Math.hypot(s.i - p.i, s.j - p.j)));
            if (gap > bestGap) {
                bestGap = gap;
                best = p;
            }
        }
        seeds.push(best);
    }
    const firsts = [...seeds];
    for (const first of firsts) {
        const extra = Math.floor(p_random() * p_settings.seedsPerColour); // 0 .. seedsPerColour - 1 more
        let from = first;
        let heading = p_random() * Math.PI * 2; // the chain's direction, bending a little each link
        for (let e = 0; e < extra; e++) {
            for (let tries = 0; tries < 16; tries++) {
                const angle = heading + (p_random() - 0.5) * 1.2;
                const distance = p_settings.chainStep * (0.8 + 0.4 * p_random());
                const p: Seed = { i: inside(Math.round(from.i + Math.cos(angle) * distance)), j: inside(Math.round(from.j + Math.sin(angle) * distance)), colour: first.colour };
                const clear = seeds.every((s) => Math.hypot(s.i - p.i, s.j - p.j) >= (s.colour === p.colour ? p_settings.chainStep * 0.6 : p_settings.chainStep * 0.55));
                if (!clear) {
                    heading = p_random() * Math.PI * 2; // blocked: try another way
                    continue;
                }
                if (p_cost[p.j * p_n + p.i] > p_easy && tries < 12) continue;
                seeds.push(p);
                from = p;
                heading = angle;
                break;
            }
        }
    }
    return seeds;
}

export function spread(p_settings: Settings, p_random: () => number, p_noise: NoiseFunction2D): Spread {
    const n = p_settings.cells;
    const cost = new Float32Array(n * n);
    for (let j = 0; j < n; j++) {
        for (let i = 0; i < n; i++) {
            const v = 0.5 + 0.5 * p_noise(i / p_settings.noiseSize, j / p_settings.noiseSize); // 0..1
            cost[j * n + i] = 1 + p_settings.roughness * v * v;
        }
    }
    const seeds = placeSeeds(n, p_settings, cost, 1 + p_settings.roughness * 0.2, p_random);
    const speed = Array.from({ length: p_settings.seeds }, () => 1 + (p_random() * 2 - 1) * p_settings.speedVariety * 0.35);
    const { owner, time, lastTime } = grow(n, cost, seeds, speed, null);
    const reached = Int32Array.from(owner);
    tidy(n, owner, seeds, MIN_BLOCK, TRIM);
    return { n, owner, reached, time, lastTime, seeds, cost, speed };
}
