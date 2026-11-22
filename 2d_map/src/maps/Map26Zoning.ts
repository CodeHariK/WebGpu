// Colour Blocks — step 14: zoning. Each block (the ground between the streets) is given a use:
// a mall, downtown, industry, a park, or houses (the rest).
//
// A block is a piece of ground between road cells on the plot grid (a flood fill). For each:
// its area, its middle, which island it is on (the nearest island middle), how central it is
// (1 at its island city's middle, 0 at the city's farthest block), which kinds of road it touches,
// how close it comes to the coast, and how near it is to an end of the bridge. Then the uses are
// handed out in turn, each taking the best blocks left for it:
//   mall: one per island, the biggest block touching an arterial or near the bridge (else the
//     biggest touching a highway)
//   downtown: the most central blocks (touching an arterial counts extra), downtownShare of the area
//   industry: blocks nearest the coast (touching a highway counts extra), industryShare of the area
//   park: mid-sized blocks, at random, parkShare of the area
//   houses: everything left
import type { Point } from './Map26Borders';
import { DETAIL } from './Map26Districts';
import { DI, DJ } from './Map26Grow';
import { coastDistance, type Island } from './Map26Island';
import { PLOT, ROAD, type Plots, type RoadKind } from './Map26Plots';
import type { Settings } from './Map26Settings';

export type Zone = 'houses' | 'downtown' | 'mall' | 'park' | 'industry';

export type Block = {
    cells: number[]; // plot cells
    area: number; // fine cells²
    middle: Point; // fine cells
    island: number;
    central: number; // 0..1
    touches: Set<RoadKind>;
    coast: number; // pixels from the coast to its nearest cell
    bridge: number; // fine cells from its middle to the nearest end of the bridge
    zone: Zone;
};

export type Zoning = { blocks: Block[]; blockOf: Int32Array }; // blockOf: per plot cell, its block or −1

const NEAR_BRIDGE = 25; // fine cells: a block this near an end of the bridge counts as by it
const MIN_MALL = 0.5; // a mall's block is at least this share of the island's average block area x 4

// A random number 0..1 for a whole number.
function hash(p_k: number): number {
    let h = Math.imul(p_k + 7, 2654435761);
    h ^= h >>> 15;
    h = Math.imul(h, 2246822519);
    return ((h ^ (h >>> 13)) >>> 0) / 4294967296;
}

// The blocks: side-to-side joined ground cells of the plot grid.
function findBlocks(p_p: Plots): { blocks: number[][]; blockOf: Int32Array } {
    const n = p_p.n;
    const blockOf = new Int32Array(n * n).fill(-1);
    const blocks: number[][] = [];
    for (let start = 0; start < n * n; start++) {
        if (p_p.id[start] === ROAD || blockOf[start] >= 0) continue;
        const cells = [start];
        blockOf[start] = blocks.length;
        for (let q = 0; q < cells.length; q++) {
            const i = cells[q] % n;
            const j = (cells[q] - i) / n;
            for (let m = 0; m < 4; m++) {
                const ni = i + DI[m];
                const nj = j + DJ[m];
                if (ni < 0 || nj < 0 || ni >= n || nj >= n) continue;
                const nc = nj * n + ni;
                if (p_p.id[nc] === ROAD || blockOf[nc] >= 0) continue;
                blockOf[nc] = blocks.length;
                cells.push(nc);
            }
        }
        blocks.push(cells);
    }
    return { blocks, blockOf };
}

// What is known about one block (its zone still to be decided).
function describe(p_cells: number[], p_p: Plots, p_island: Island, p_size: number, p_bridgeEnds: Point[]): Block {
    const n = p_p.n;
    const px = p_size / n; // pixels a plot cell
    let x = 0;
    let y = 0;
    let coast = Infinity;
    const touches = new Set<RoadKind>();
    p_cells.forEach((c, k) => {
        const i = c % n;
        const j = (c - i) / n;
        x += i + 0.5;
        y += j + 0.5;
        if (k % 3 === 0) coast = Math.min(coast, -coastDistance(p_island, (i + 0.5) * px, (j + 0.5) * px));
        for (let m = 0; m < 4; m++) {
            const ni = i + DI[m];
            const nj = j + DJ[m];
            if (ni < 0 || nj < 0 || ni >= n || nj >= n) continue;
            const road = p_p.road[nj * n + ni];
            if (road >= 0) touches.add(p_p.roads[road].kind);
        }
    });
    const middle = { x: x / p_cells.length / PLOT, y: y / p_cells.length / PLOT };
    const fineToPx = p_size / (n / PLOT);
    let island = 0;
    let best = Infinity;
    p_island.centres.forEach((c, k) => {
        const d = Math.hypot(middle.x * fineToPx - c.x, middle.y * fineToPx - c.y);
        if (d < best) [best, island] = [d, k];
    });
    const bridge = Math.min(Infinity, ...p_bridgeEnds.map((e) => Math.hypot(e.x - middle.x, e.y - middle.y)));
    return { cells: p_cells, area: p_cells.length / (PLOT * PLOT), middle, island, central: 0, touches, coast, bridge, zone: 'houses' };
}

// Each block's centrality: 1 at its island city's middle (the blocks' area-weighted middle), 0 at
// the farthest block.
function centrality(r_blocks: Block[], p_islands: number): void {
    for (let k = 0; k < p_islands; k++) {
        const mine = r_blocks.filter((b) => b.island === k);
        const total = mine.reduce((a, b) => a + b.area, 0) || 1;
        const cx = mine.reduce((a, b) => a + b.middle.x * b.area, 0) / total;
        const cy = mine.reduce((a, b) => a + b.middle.y * b.area, 0) / total;
        const far = Math.max(1, ...mine.map((b) => Math.hypot(b.middle.x - cx, b.middle.y - cy)));
        for (const b of mine) b.central = 1 - Math.hypot(b.middle.x - cx, b.middle.y - cy) / far;
    }
}

// Hands zone p_zone to the best-scoring blocks still houses, until they cover p_share of each
// island's area (or p_count blocks, if given). p_score < 0: never. A block over half the goal again
// is passed over (a share is a few blocks, not one huge one).
function handOut(r_blocks: Block[], p_islands: number, p_zone: Zone, p_score: (b: Block) => number, p_share: number, p_count = Infinity): void {
    for (let k = 0; k < p_islands; k++) {
        const mine = r_blocks.filter((b) => b.island === k);
        const goal = mine.reduce((a, b) => a + b.area, 0) * p_share;
        const free = mine.filter((b) => b.zone === 'houses' && p_score(b) >= 0).sort((a, b) => p_score(b) - p_score(a));
        let got = 0;
        let count = 0;
        for (const b of free) {
            if (got >= goal || count >= p_count) break;
            if (p_count === Infinity && b.area > goal * 1.5) continue;
            b.zone = p_zone;
            got += b.area;
            count++;
        }
    }
}

export function makeZoning(p_p: Plots, p_island: Island, p_size: number, p_bridgeEnds: Point[], p_settings: Settings): Zoning {
    const found = findBlocks(p_p);
    const blocks = found.blocks.map((cells) => describe(cells, p_p, p_island, p_size, p_bridgeEnds));
    const islands = p_island.centres.length;
    centrality(blocks, islands);
    const big = (b: Block) => {
        const mine = blocks.filter((o) => o.island === b.island);
        return b.area / (mine.reduce((a, o) => a + o.area, 0) / mine.length);
    };
    const shore = p_settings.cityMargin + 14; // pixels: a block coming this near the coast is on the shore
    const mallRoad = (b: Block) => b.touches.has('arterial') || b.touches.has('highway') || b.bridge < NEAR_BRIDGE;
    handOut(blocks, islands, 'mall', (b) => (big(b) < MIN_MALL * 4 || !mallRoad(b) ? -1 : b.area + (b.touches.has('arterial') || b.bridge < NEAR_BRIDGE ? 1e6 : 0)), 1, 1);
    handOut(blocks, islands, 'downtown', (b) => b.central + (b.touches.has('arterial') ? 0.25 : 0) + 0.1 * hash(b.cells[0]), p_settings.downtownShare);
    handOut(blocks, islands, 'industry', (b) => (b.coast > shore ? -1 : 1 - b.coast / shore + (b.touches.has('highway') ? 0.3 : 0) + 0.1 * hash(b.cells[1] ?? 0)), p_settings.industryShare);
    handOut(blocks, islands, 'park', (b) => (big(b) < 0.4 || big(b) > 2.5 ? -1 : hash(b.cells[0] * 31)), p_settings.parkShare);
    return { blocks, blockOf: found.blockOf };
}

// The ends of the bridge, in fine cells (from its line in block cells).
export function bridgeEnds(p_points: Point[] | null): Point[] {
    if (!p_points || p_points.length < 2) return [];
    const a = p_points[0];
    const b = p_points[p_points.length - 1];
    return [{ x: a.x * DETAIL, y: a.y * DETAIL }, { x: b.x * DETAIL, y: b.y * DETAIL }];
}
