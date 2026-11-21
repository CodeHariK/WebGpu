// Colour Blocks — the grid, the seeds and the spread.
//
// Every cell has a cost to step into: 1 plus noise (roughness). All seeds start at time 0 and
// spread at once; a shortest-path search (Dijkstra) from all of them finds, for every cell, the
// earliest time any colour can reach it and which colour that is. A colour's speed divides its
// step costs, so faster colours win more cells. The arrival times also give the animation: a cell
// shows its colour once the clock passes its time. Afterwards the blocks are tidied (tidy), and
// every block made one piece (onePiece).
import type { NoiseFunction2D } from 'simplex-noise';
import type { Settings } from './Map26Settings';

const MIN_BLOCK = 40; // cells: a colour left with less than this (even its seed's piece) is dropped
const TRIM = 2; // cells: strands, tips and notches narrower than 2 x TRIM + 1 are trimmed

export type Seed = { i: number; j: number; colour: number };

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

export const NEIGHBOURS = [ // [di, dj, step length]: the 4 sides, then the 4 diagonals
    [1, 0, 1],
    [-1, 0, 1],
    [0, 1, 1],
    [0, -1, 1],
    [1, 1, Math.SQRT2],
    [1, -1, Math.SQRT2],
    [-1, 1, Math.SQRT2],
    [-1, -1, Math.SQRT2],
];

// A min-heap of (time, cell): always hands back the earliest.
class Queue {
    private times: number[] = [];
    private cells: number[] = [];
    get size() {
        return this.times.length;
    }
    push(p_time: number, p_cell: number): void {
        const t = this.times;
        const c = this.cells;
        t.push(p_time);
        c.push(p_cell);
        for (let i = t.length - 1; i > 0; ) {
            const up = (i - 1) >> 1;
            if (t[up] <= t[i]) break;
            [t[up], t[i]] = [t[i], t[up]];
            [c[up], c[i]] = [c[i], c[up]];
            i = up;
        }
    }
    pop(): [number, number] {
        const t = this.times;
        const c = this.cells;
        const top: [number, number] = [t[0], c[0]];
        const lastT = t.pop()!;
        const lastC = c.pop()!;
        if (t.length) {
            t[0] = lastT;
            c[0] = lastC;
            for (let i = 0; ; ) {
                const l = 2 * i + 1;
                const r = l + 1;
                let s = i;
                if (l < t.length && t[l] < t[s]) s = l;
                if (r < t.length && t[r] < t[s]) s = r;
                if (s === i) break;
                [t[s], t[i]] = [t[i], t[s]];
                [c[s], c[i]] = [c[i], c[s]];
                i = s;
            }
        }
        return top;
    }
}

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
    const owner = new Int32Array(n * n).fill(-1);
    const time = new Float64Array(n * n).fill(Infinity); // full precision: a popped time must match the stored one exactly
    const queue = new Queue();
    for (const s of seeds) {
        const c = s.j * n + s.i;
        if (time[c] === 0) continue; // two seeds on one cell: the first keeps it
        time[c] = 0;
        owner[c] = s.colour;
        queue.push(0, c);
    }
    let lastTime = 0;
    while (queue.size) {
        const [t, c] = queue.pop();
        if (t > time[c]) continue; // a later, slower visit: already done
        lastTime = Math.max(lastTime, t);
        const i = c % n;
        const j = (c - i) / n;
        const k = owner[c];
        for (const [di, dj, step] of NEIGHBOURS) {
            const ni = i + di;
            const nj = j + dj;
            if (ni < 0 || nj < 0 || ni >= n || nj >= n) continue;
            const nc = nj * n + ni;
            const nt = t + (step * cost[nc]) / speed[k];
            if (nt < time[nc]) {
                time[nc] = nt;
                owner[nc] = k;
                queue.push(nt, nc);
            }
        }
    }
    const reached = Int32Array.from(owner);
    tidy(n, owner, seeds);
    return { n, owner, reached, time, lastTime, seeds, cost, speed };
}

// Shaves single-cell spikes (twice: each cell takes the colour 5 or more of its 8 neighbours share;
// seeds keep theirs), makes every block one piece (onePiece), and trims strands, tips and notches
// thinner than 5 cells (trimThin).
function tidy(p_n: number, r_owner: Int32Array, p_seeds: Seed[]): void {
    const isSeed = new Uint8Array(p_n * p_n);
    for (const s of p_seeds) isSeed[s.j * p_n + s.i] = 1;
    for (let pass = 0; pass < 2; pass++) {
        const before = Int32Array.from(r_owner);
        for (let j = 1; j < p_n - 1; j++) {
            for (let i = 1; i < p_n - 1; i++) {
                if (isSeed[j * p_n + i]) continue;
                const counts = new Map<number, number>();
                for (const [di, dj] of NEIGHBOURS) {
                    const k = before[(j + dj) * p_n + i + di];
                    counts.set(k, (counts.get(k) ?? 0) + 1);
                }
                for (const [k, count] of counts) if (count >= 5) r_owner[j * p_n + i] = k;
            }
        }
    }
    onePiece(p_n, r_owner, p_seeds);
    trimThin(p_n, r_owner, isSeed);
    onePiece(p_n, r_owner, p_seeds); // trimming a strand can cut a block in two
}

// Thin bits (strands, tips and notches under 2 x TRIM + 1 cells wide) go to the colour beside them.
// An opening: a cell survives shrinking if every cell within TRIM of it (a square) shares its colour
// (the map's edge counts as the same); a cell is solid if a survivor of its colour is within TRIM.
// What isn't solid is thin.
function trimThin(p_n: number, r_owner: Int32Array, p_isSeed: Uint8Array): void {
    const at = (i: number, j: number, p_k: number) => (i < 0 || j < 0 || i >= p_n || j >= p_n ? p_k : r_owner[j * p_n + i]);
    const core = new Uint8Array(p_n * p_n);
    for (let j = 0; j < p_n; j++) {
        for (let i = 0; i < p_n; i++) {
            const k = r_owner[j * p_n + i];
            let all = true;
            for (let dj = -TRIM; dj <= TRIM && all; dj++) for (let di = -TRIM; di <= TRIM && all; di++) all = at(i + di, j + dj, k) === k;
            core[j * p_n + i] = all ? 1 : 0;
        }
    }
    const before = Int32Array.from(r_owner);
    for (let j = 0; j < p_n; j++) {
        for (let i = 0; i < p_n; i++) {
            const c = j * p_n + i;
            const k = before[c];
            if (core[c] || p_isSeed[c]) continue;
            let solid = false;
            for (let dj = -TRIM; dj <= TRIM && !solid; dj++) {
                for (let di = -TRIM; di <= TRIM && !solid; di++) {
                    const ni = i + di;
                    const nj = j + dj;
                    solid = ni >= 0 && nj >= 0 && ni < p_n && nj < p_n && core[nj * p_n + ni] === 1 && before[nj * p_n + ni] === k;
                }
            }
            if (solid) continue;
            const counts = new Map<number, number>();
            for (const [di, dj] of NEIGHBOURS) {
                const other = at(i + di, j + dj, k);
                if (other !== k) counts.set(other, (counts.get(other) ?? 0) + 1);
            }
            if (counts.size) r_owner[c] = [...counts.entries()].sort((a, b) => b[1] - a[1])[0][0];
        }
    }
}

// Every colour keeps only the pieces joined (side to side, not just at a corner) to its own seeds,
// and only those with MIN_BLOCK cells or more. Any other piece of it, whatever its size,
// goes to the colour it touches most. Repeated until
// nothing moves (a piece can be wrapped in another piece that is itself moving).
function onePiece(p_n: number, r_owner: Int32Array, p_seeds: Seed[]): void {
    const SIDES = NEIGHBOURS.slice(0, 4);
    for (let round = 0; round < 50; round++) {
        const kept = new Uint8Array(p_n * p_n);
        p_seeds.forEach((s) => { // flood each colour from each of its seeds
            const k = s.colour;
            const start = s.j * p_n + s.i;
            if (r_owner[start] !== k || kept[start]) return;
            const queue = [start];
            kept[start] = 1;
            for (let q = 0; q < queue.length; q++) {
                const c = queue[q];
                const i = c % p_n;
                const j = (c - i) / p_n;
                for (const [di, dj] of SIDES) {
                    const ni = i + di;
                    const nj = j + dj;
                    if (ni < 0 || nj < 0 || ni >= p_n || nj >= p_n) continue;
                    const nc = nj * p_n + ni;
                    if (kept[nc] || r_owner[nc] !== k) continue;
                    kept[nc] = 1;
                    queue.push(nc);
                }
            }
            if (queue.length < MIN_BLOCK) for (const c of queue) kept[c] = 0; // too small to be a block: it goes too
        });
        let moved = false;
        for (let c = 0; c < p_n * p_n; c++) {
            if (kept[c]) continue;
            const i = c % p_n;
            const j = (c - i) / p_n;
            const counts = new Map<number, number>();
            for (const [di, dj] of SIDES) { // only kept neighbours count: they are where it belongs
                const ni = i + di;
                const nj = j + dj;
                if (ni < 0 || nj < 0 || ni >= p_n || nj >= p_n) continue;
                const nc = nj * p_n + ni;
                if (kept[nc]) counts.set(r_owner[nc], (counts.get(r_owner[nc]) ?? 0) + 1);
            }
            if (counts.size === 0) continue; // inside the stray piece: its edge cells go first
            r_owner[c] = [...counts.entries()].sort((a, b) => b[1] - a[1])[0][0];
            kept[c] = 1;
            moved = true;
        }
        if (!moved) break;
    }
}
