// Colour Blocks — the spread itself, on any grid: a shortest-path search (Dijkstra) from all the
// seeds at once. Every cell has a cost to step into; a colour's speed divides its step costs. The
// queue holds only the frontier (cells next to reached ones, each with the time it would be
// reached); the earliest always goes next, and is final then: nothing still waiting can beat it.
// Wall cells (if given) are never entered: what grows on either side of a wall stays there.

export type Seed = { i: number; j: number; colour: number };
export type Growth = { owner: Int32Array; time: Float64Array; lastTime: number };

export const NEIGHBOURS: number[][] = [ // [di, dj, step length]: the 4 sides, then the 4 diagonals
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
    poppedTime = 0; // the time of the cell pop() last handed back
    pop(): number {
        const t = this.times;
        const c = this.cells;
        this.poppedTime = t[0];
        const top = c[0];
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

// The same as NEIGHBOURS, split into plain arrays: the hot loops read them by index (no
// destructuring, no garbage).
export const DI = Int8Array.from(NEIGHBOURS.map((n) => n[0]));
export const DJ = Int8Array.from(NEIGHBOURS.map((n) => n[1]));
export const STEP = Float64Array.from(NEIGHBOURS.map((n) => n[2]));

export function grow(p_n: number, p_cost: Float32Array, p_seeds: Seed[], p_speed: number[], p_walls: Uint8Array | null): Growth {
    const owner = new Int32Array(p_n * p_n).fill(-1);
    const time = new Float64Array(p_n * p_n).fill(Infinity); // full precision: a popped time must match the stored one exactly
    const queue = new Queue();
    for (const s of p_seeds) {
        const c = s.j * p_n + s.i;
        if (time[c] === 0 || p_walls?.[c]) continue; // two seeds on one cell: the first keeps it
        time[c] = 0;
        owner[c] = s.colour;
        queue.push(0, c);
    }
    let lastTime = 0;
    while (queue.size) {
        const c = queue.pop();
        const t = queue.poppedTime;
        if (t > time[c]) continue; // a later, slower visit: already done
        lastTime = Math.max(lastTime, t);
        const i = c % p_n;
        const j = (c - i) / p_n;
        const k = owner[c];
        for (let m = 0; m < 8; m++) {
            const di = DI[m];
            const dj = DJ[m];
            const step = STEP[m];
            const ni = i + di;
            const nj = j + dj;
            if (ni < 0 || nj < 0 || ni >= p_n || nj >= p_n) continue;
            const nc = nj * p_n + ni;
            if (p_walls && (p_walls[nc] || (di && dj && p_walls[j * p_n + ni] && p_walls[nj * p_n + i]))) continue; // a wall, or a diagonal squeeze between two
            const nt = t + (step * p_cost[nc]) / p_speed[k];
            if (nt < time[nc]) {
                time[nc] = nt;
                owner[nc] = k;
                queue.push(nt, nc);
            }
        }
    }
    return { owner, time, lastTime };
}
