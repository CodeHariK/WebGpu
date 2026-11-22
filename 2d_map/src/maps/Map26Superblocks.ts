// Colour Blocks — step 18: superblocks. A mall or a park is too long and thin on one street block,
// so it takes in the blocks round it, with the streets between them closed.
//
// Two blocks are neighbours across a street if, from a cell of one, a straight run (4 ways, up to
// ACROSS cells) over street cells only reaches a cell of the other; those street cells lie between
// them. Each mall grows to mallBlocks blocks, each park to parkBlocks: it takes, one at a time, the
// neighbour whose middle is nearest its own (so it grows round, not long), from the blocks that are
// houses or park (a mall never swallows downtown, industry or another mall). A block taken in joins
// it with its cells and the street cells between them; those streets are closed (cut out of the
// street lines, so they are no longer drawn, and no lot faces them).
import type { Point } from './Map26Borders';
import { STREET_HALF } from './Map26Dimensions';
import { DI, DJ } from './Map26Grow';
import { PLOT, type Plots } from './Map26Plots';
import type { Settings } from './Map26Settings';
import type { Streets, Street } from './Map26Streets';
import type { Zoning } from './Map26Zoning';

const ACROSS = Math.ceil(STREET_HALF * 2 * PLOT) + 2; // plot cells: the widest street crossed between neighbours

export type Superblocks = { closed: Uint8Array; closedRoads: Set<number> }; // closed: per plot cell, 1 on a closed street

// Neighbours across a street: per pair of blocks ("a,b", a < b), the street cells between them.
function acrossStreets(p_p: Plots, p_z: Zoning): Map<string, Set<number>> {
    const n = p_p.n;
    const pairs = new Map<string, Set<number>>();
    for (let c = 0; c < n * n; c++) {
        const a = p_z.blockOf[c];
        if (a < 0) continue;
        const i = c % n;
        const j = (c - i) / n;
        for (let m = 0; m < 4; m++) {
            const passed: number[] = [];
            for (let k = 1; k <= ACROSS + 1; k++) {
                const ni = i + DI[m] * k;
                const nj = j + DJ[m] * k;
                if (ni < 0 || nj < 0 || ni >= n || nj >= n) break;
                const nc = nj * n + ni;
                const b = p_z.blockOf[nc];
                if (b === a) break;
                if (b >= 0) {
                    if (passed.length === 0) break;
                    const key = a < b ? `${a},${b}` : `${b},${a}`;
                    if (!pairs.has(key)) pairs.set(key, new Set());
                    for (const s of passed) pairs.get(key)!.add(s);
                    break;
                }
                const road = p_p.road[nc];
                if (road < 0 || p_p.roads[road].kind !== 'street') break; // only across a street
                passed.push(nc);
            }
        }
    }
    return pairs;
}

export function makeSuperblocks(p_p: Plots, r_z: Zoning, p_settings: Settings): Superblocks {
    const n = p_p.n;
    const closed = new Uint8Array(n * n);
    const closedRoads = new Set<number>();
    const pairs = acrossStreets(p_p, r_z);
    const neighbours = r_z.blocks.map(() => [] as number[]);
    for (const key of pairs.keys()) {
        const [a, b] = key.split(',').map(Number);
        neighbours[a].push(b);
        neighbours[b].push(a);
    }
    const order = r_z.blocks.map((_, k) => k).filter((k) => r_z.blocks[k].zone === 'mall' || r_z.blocks[k].zone === 'park');
    order.sort((a, b) => (r_z.blocks[a].zone === 'mall' ? 0 : 1) - (r_z.blocks[b].zone === 'mall' ? 0 : 1)); // malls first
    const owner = r_z.blocks.map((_, k) => k); // which block each block now belongs to
    for (const seed of order) {
        const block = r_z.blocks[seed];
        if (owner[seed] !== seed) continue; // taken in by a mall already
        const want = block.zone === 'mall' ? p_settings.mallBlocks : p_settings.parkBlocks;
        const members = [seed];
        while (members.length < want) {
            let best = -1;
            let bestD = Infinity;
            for (const m of members) {
                for (const b of neighbours[m]) {
                    if (owner[b] !== b || members.includes(b)) continue;
                    const zone = r_z.blocks[b].zone;
                    if (zone !== 'houses' && !(zone === 'park' && block.zone === 'park')) continue;
                    const d = Math.hypot(r_z.blocks[b].middle.x - block.middle.x, r_z.blocks[b].middle.y - block.middle.y);
                    if (d < bestD) [bestD, best] = [d, b];
                }
            }
            if (best < 0) break;
            // take it in, with the street cells between it and the members
            const taken = r_z.blocks[best];
            const between: number[] = [];
            for (const m of members) {
                const cells = pairs.get(m < best ? `${m},${best}` : `${best},${m}`);
                if (cells) between.push(...cells);
            }
            for (const c of between) {
                if (r_z.blockOf[c] >= 0) continue;
                closed[c] = 1;
                closedRoads.add(p_p.road[c]);
                r_z.blockOf[c] = seed;
                block.cells.push(c);
            }
            for (const c of taken.cells) r_z.blockOf[c] = seed;
            block.cells.push(...taken.cells);
            const area = block.area + taken.area;
            block.middle = { x: (block.middle.x * block.area + taken.middle.x * taken.area) / area, y: (block.middle.y * block.area + taken.middle.y * taken.area) / area };
            block.area = area + between.length / (PLOT * PLOT);
            taken.cells = [];
            taken.area = 0;
            taken.zone = block.zone;
            owner[best] = seed;
            members.push(best);
        }
    }
    return { closed, closedRoads };
}

// The streets with the closed stretches cut out (each street walked every half fine cell; the
// pieces off the closed cells kept).
export function openStreets(p_s: Streets, p_closed: Uint8Array, p_n: number): Streets {
    const isClosed = (p: Point) => {
        const i = Math.floor(p.x * PLOT);
        const j = Math.floor(p.y * PLOT);
        return i >= 0 && j >= 0 && i < p_n && j < p_n && p_closed[j * p_n + i] === 1;
    };
    const streets: Street[] = [];
    const lines: Point[][] = [];
    const source = p_s.lines ?? p_s.streets.map((t) => [{ x: t.ax, y: t.ay }, { x: t.bx, y: t.by }]);
    for (const line of source) {
        let run: Point[] = [];
        const flush = () => {
            if (run.length > 1) {
                lines.push(run);
                for (let k = 0; k < run.length - 1; k++) streets.push({ ax: run[k].x, ay: run[k].y, bx: run[k + 1].x, by: run[k + 1].y });
            }
            run = [];
        };
        for (let k = 0; k < line.length - 1; k++) {
            const a = line[k];
            const b = line[k + 1];
            const steps = Math.max(1, Math.ceil(Math.hypot(b.x - a.x, b.y - a.y) * 2));
            for (let s = 0; s < steps; s++) {
                const p = { x: a.x + ((b.x - a.x) * s) / steps, y: a.y + ((b.y - a.y) * s) / steps };
                if (isClosed(p)) flush();
                else if (s === 0 || run.length === 0) run.push(p);
            }
            if (!isClosed(b) && run.length) run.push(b);
        }
        flush();
    }
    return { streets, lines };
}
