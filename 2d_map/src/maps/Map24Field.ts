// Street Map — the districts. Every grid cell finds its nearest road point (a jump flood: every
// cell asks cells 2^k away which point they found, for k going down, and keeps the nearest). The
// side of that road it is on makes its district (road x 2 + side). A second jump flood, from the
// cells where two districts meet, gives every cell its distance to a district border (a street).
import type { Samples } from './Map24Samples';
import { CELL, type Terrain } from './Map24Terrain';

export type Field = {
    cols: number;
    rows: number;
    near: Int32Array; // per cell: its nearest road point (index into Samples), -1 none
    zone: Int32Array; // per cell: its district
    border: Int32Array; // per cell: the nearest border cell, -1 none
};

// p_seed[c] is a seed id or -1; p_sx[id], p_sy[id] the seed's position. Returns per cell the nearest seed.
function jumpFlood(p_cols: number, p_rows: number, p_seed: Int32Array, p_sx: Float32Array, p_sy: Float32Array): Int32Array {
    let a = Int32Array.from(p_seed);
    let b = new Int32Array(a.length);
    let step = 1;
    while (step < Math.max(p_cols, p_rows)) step *= 2;
    const steps: number[] = [];
    for (step /= 2; step >= 1; step /= 2) steps.push(step);
    steps.push(1); // one more pass at 1 cleans up the few cells the jumps got wrong
    for (const k of steps) {
        for (let cy = 0; cy < p_rows; cy++) {
            for (let cx = 0; cx < p_cols; cx++) {
                const px = (cx + 0.5) * CELL;
                const py = (cy + 0.5) * CELL;
                let best = a[cy * p_cols + cx];
                let bestD = best >= 0 ? (p_sx[best] - px) ** 2 + (p_sy[best] - py) ** 2 : Infinity;
                for (let dy = -1; dy <= 1; dy++) {
                    const ny = cy + dy * k;
                    if (ny < 0 || ny >= p_rows) continue;
                    for (let dx = -1; dx <= 1; dx++) {
                        const nx = cx + dx * k;
                        if (nx < 0 || nx >= p_cols || (dx === 0 && dy === 0)) continue;
                        const cand = a[ny * p_cols + nx];
                        if (cand < 0 || cand === best) continue;
                        const d = (p_sx[cand] - px) ** 2 + (p_sy[cand] - py) ** 2;
                        if (d < bestD) {
                            bestD = d;
                            best = cand;
                        }
                    }
                }
                b[cy * p_cols + cx] = best;
            }
        }
        [a, b] = [b, a];
    }
    return a;
}

// The district of point (p_x, p_y) given its nearest road point p_i: road x 2 + which side.
export function zoneOf(p_samples: Samples, p_i: number, p_x: number, p_y: number): number {
    const side = p_samples.tx[p_i] * (p_y - p_samples.y[p_i]) - p_samples.ty[p_i] * (p_x - p_samples.x[p_i]) >= 0 ? 1 : 0;
    return p_samples.road[p_i] * 2 + side;
}

export function makeField(p_samples: Samples, p_t: Terrain): Field {
    const cols = p_t.cols;
    const rows = p_t.rows;
    const seeds = new Int32Array(cols * rows).fill(-1);
    for (let i = 0; i < p_samples.count; i++) {
        const cx = Math.floor(p_samples.x[i] / CELL);
        const cy = Math.floor(p_samples.y[i] / CELL);
        if (cx >= 0 && cy >= 0 && cx < cols && cy < rows) seeds[cy * cols + cx] = i;
    }
    const near = jumpFlood(cols, rows, seeds, p_samples.x, p_samples.y);
    const zone = new Int32Array(cols * rows).fill(-1);
    for (let c = 0; c < cols * rows; c++) {
        if (near[c] >= 0) zone[c] = zoneOf(p_samples, near[c], ((c % cols) + 0.5) * CELL, (Math.floor(c / cols) + 0.5) * CELL);
    }
    // border cells: next to a cell of another district (not just the other side of the same road)
    const borderSeeds = new Int32Array(cols * rows).fill(-1);
    for (let cy = 0; cy < rows - 1; cy++) {
        for (let cx = 0; cx < cols - 1; cx++) {
            const c = cy * cols + cx;
            const z = zone[c];
            for (const n of [c + 1, c + cols]) {
                const other = zone[n];
                if (other !== z && other >> 1 !== z >> 1) borderSeeds[c] = c;
            }
        }
    }
    const cellX = new Float32Array(cols * rows); // border seeds are cells: their centres
    const cellY = new Float32Array(cols * rows);
    for (let c = 0; c < cols * rows; c++) {
        cellX[c] = ((c % cols) + 0.5) * CELL;
        cellY[c] = (Math.floor(c / cols) + 0.5) * CELL;
    }
    const border = jumpFlood(cols, rows, borderSeeds, cellX, cellY);
    return { cols, rows, near, zone, border };
}
