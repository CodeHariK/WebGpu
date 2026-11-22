// Colour Blocks — step 15: lots. The plots (Map26Plots) joined into lots as big as each zone's
// buildings need (Map26Zoning):
//   houses: one plot, one lot (as before)
//   downtown: runs of 2 to 4 plots side by side along the same road (same side, stretches in a
//     row): wider lots for bigger buildings
//   industry: half a block (split across its long way, through its middle), or the whole block if
//     it is small
//   mall, park: the whole block
// Plots never cross a road, so a lot stays in its block. Lots from whole or half blocks take the
// block's middle (the courtyard) too. Each lot faces the road it has most frontage on: its front
// is the middle of that frontage.
import type { Point } from './Map26Borders';
import { PLOT, type Plot, type Plots } from './Map26Plots';
import type { Zone, Zoning } from './Map26Zoning';

export type Lot = { zone: Zone; block: number; area: number; front: Point; along: Point; inward: Point; frontage: number }; // fine cells
export type Lots = { lotOf: Int32Array; lots: Lot[] }; // lotOf: per plot cell, its lot or −1

const SMALL_INDUSTRY = 2.2; // an industry block under this many plots' area stays one lot

// A lot facing its main road: of its plots, those on the (road, side) with the most frontage give
// its front, direction and way in.
function lotOf(p_plots: Plot[], p_zone: Zone, p_block: number, p_area: number): Lot {
    const byRoad = new Map<string, Plot[]>();
    for (const p of p_plots) {
        const key = `${p.road},${p.side}`;
        if (!byRoad.has(key)) byRoad.set(key, []);
        byRoad.get(key)!.push(p);
    }
    const sum = (ps: Plot[]) => ps.reduce((a, p) => a + p.frontage, 0);
    const main = [...byRoad.values()].sort((a, b) => sum(b) - sum(a))[0];
    const frontage = sum(main);
    const mean = (f: (p: Plot) => number) => main.reduce((a, p) => a + f(p) * p.frontage, 0) / frontage;
    const ax = mean((p) => p.along.x * Math.sign(p.along.x * main[0].along.x + p.along.y * main[0].along.y || 1));
    const ay = mean((p) => p.along.y * Math.sign(p.along.x * main[0].along.x + p.along.y * main[0].along.y || 1));
    const ix = mean((p) => p.inward.x);
    const iy = mean((p) => p.inward.y);
    const al = Math.hypot(ax, ay) || 1;
    const along = { x: ax / al, y: ay / al };
    // the way in: square to the road, on the side the frontage cells lie (cells by a road's end
    // point along it, so the average alone can be off)
    const sign = -along.y * ix + along.x * iy >= 0 ? 1 : -1;
    return { zone: p_zone, block: p_block, area: p_area, front: { x: mean((p) => p.front.x), y: mean((p) => p.front.y) }, along, inward: { x: -along.y * sign, y: along.x * sign }, frontage };
}

// Downtown: the block's plots in runs along their road (same road and side, stretches one after
// another), each run cut into pieces of 2 to 4.
function downtownRuns(p_plots: number[], p_all: Plot[], p_random: () => number): number[][] {
    const byRoad = new Map<string, number[]>();
    for (const k of p_plots) {
        const key = `${p_all[k].road},${p_all[k].side}`;
        if (!byRoad.has(key)) byRoad.set(key, []);
        byRoad.get(key)!.push(k);
    }
    const groups: number[][] = [];
    for (const ks of byRoad.values()) {
        ks.sort((a, b) => p_all[a].stretch - p_all[b].stretch);
        let run: number[] = [];
        let size = 2 + Math.floor(p_random() * 3);
        for (const k of ks) {
            const gap = run.length > 0 && p_all[k].stretch !== p_all[run[run.length - 1]].stretch + 1;
            if (gap || run.length >= size) {
                groups.push(run);
                run = [];
                size = 2 + Math.floor(p_random() * 3);
            }
            run.push(k);
        }
        if (run.length) groups.push(run);
    }
    return groups;
}

// Industry: the block's cells split in two across its long way (the side of the line through its
// middle, at right angles to its long axis, found from its cells' spread), or kept whole if small.
function industryHalves(p_cells: number[], p_n: number, p_area: number, p_plotArea: number): Int8Array {
    const side = new Int8Array(p_cells.length);
    if (p_area < p_plotArea * SMALL_INDUSTRY) return side;
    let mx = 0;
    let my = 0;
    for (const c of p_cells) {
        mx += c % p_n;
        my += Math.floor(c / p_n);
    }
    mx /= p_cells.length;
    my /= p_cells.length;
    let xx = 0;
    let xy = 0;
    let yy = 0;
    for (const c of p_cells) {
        const dx = (c % p_n) - mx;
        const dy = Math.floor(c / p_n) - my;
        xx += dx * dx;
        xy += dx * dy;
        yy += dy * dy;
    }
    const angle = 0.5 * Math.atan2(2 * xy, xx - yy); // the long axis
    const ux = Math.cos(angle);
    const uy = Math.sin(angle);
    p_cells.forEach((c, k) => (side[k] = ((c % p_n) - mx) * ux + (Math.floor(c / p_n) - my) * uy > 0 ? 1 : 0));
    return side;
}

// p_closedRoads: streets closed (step 18): no lot faces them.
export function makeLots(p_p: Plots, p_z: Zoning, p_plotArea: number, p_closedRoads: Set<number>, p_random: () => number): Lots {
    const n = p_p.n;
    const lotOfCell = new Int32Array(n * n).fill(-1);
    const lots: Lot[] = [];
    const plotsIn = p_z.blocks.map(() => new Set<number>());
    for (let c = 0; c < n * n; c++) if (p_p.id[c] >= 0 && p_z.blockOf[c] >= 0) plotsIn[p_z.blockOf[c]].add(p_p.id[c]);
    const areaOf = (ks: number[]) => ks.reduce((a, k) => a + p_p.plots[k].area, 0);
    p_z.blocks.forEach((block, b) => {
        const plots = [...plotsIn[b]];
        if (plots.length === 0) return;
        if (block.zone === 'houses' || block.zone === 'downtown') {
            const groups = block.zone === 'houses' ? plots.map((k) => [k]) : downtownRuns(plots, p_p.plots, p_random);
            const lotOfPlot = new Map<number, number>();
            for (const g of groups) {
                for (const k of g) lotOfPlot.set(k, lots.length);
                lots.push(lotOf(g.map((k) => p_p.plots[k]), block.zone, b, areaOf(g)));
            }
            for (const c of block.cells) if (p_p.id[c] >= 0) lotOfCell[c] = lotOfPlot.get(p_p.id[c]) ?? -1;
            return;
        }
        // whole block (mall, park) or its halves (industry), the courtyard with it
        const side = block.zone === 'industry' ? industryHalves(block.cells, n, block.area, p_plotArea) : new Int8Array(block.cells.length);
        for (const half of [0, 1]) {
            const cells = block.cells.filter((_, k) => side[k] === half);
            if (cells.length === 0) continue;
            const mine = new Set<number>();
            for (const c of cells) if (p_p.id[c] >= 0) mine.add(p_p.id[c]);
            const open = [...mine].filter((k) => !p_closedRoads.has(p_p.plots[k].road)); // facing a street still open
            const facing = open.length ? open : [...mine];
            if (facing.length === 0) continue;
            for (const c of cells) lotOfCell[c] = lots.length;
            lots.push(lotOf(facing.map((k) => p_p.plots[k]), block.zone, b, cells.length / (PLOT * PLOT)));
        }
    });
    return { lotOf: lotOfCell, lots };
}
