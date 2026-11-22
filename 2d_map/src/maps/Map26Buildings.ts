// Colour Blocks — step 16: placeholder buildings on the lots (Map26Lots), each with a height (in
// floors) for the 3D map later.
//   houses: the houses as before (one per plot), 1 to 2 floors
//   downtown: a block filling most of its lot, little setback; 3 floors at the city's edge up to
//     about 20 at its middle (centrality), give or take; of each island's downtown lots, the one
//     best for a round tower (central, with room for a wide circle) gets one instead
//   industry: a long low shed (warehouse) filling most of its lot, 1 to 2 floors
//   mall: a big low block at the back of its lot, a car park in front of it, on the road
//   park: no building: trees (small circles, apart) and, in a big park, a pond (a circle)
// A rectangle is the biggest of its shape that fits on the lot, turned to the lot's road (its −depth
// side towards it: faceRoad), its front a setback from the lot's frontage; if none fits there (a lot
// round a bend), centred on the lot's widest point instead (Map26Fit).
import { biggestCircle, faceRoad, fitRect, fitRectAround, rectOnLot, type Circle, type Rect } from './Map26Fit';
import type { Lots } from './Map26Lots';
import { PLOT } from './Map26Plots';
import type { Zoning } from './Map26Zoning';

export type Building = { kind: 'downtown' | 'tower' | 'industry' | 'mall'; rect: Rect | null; circle: Circle | null; floors: number };
export type Buildings = { buildings: Building[]; carParks: Rect[]; trees: Circle[]; ponds: Circle[] };

const MIN_POND = 2.4; // fine cells: a park whose biggest circle is smaller gets no pond
const TREE_GAP = 1.1; // fine cells between trees (more than their radii)

// Every lot's cells.
function cellsOf(p_l: Lots): number[][] {
    const cells = p_l.lots.map(() => [] as number[]);
    for (let c = 0; c < p_l.lotOf.length; c++) if (p_l.lotOf[c] >= 0) cells[p_l.lotOf[c]].push(c);
    return cells;
}

// Trees on park lot p_k: tried at random cells, kept if the whole tree is on the lot, clear of the
// pond and TREE_GAP from every other tree.
function plantTrees(p_cells: number[], p_k: number, p_l: Lots, p_n: number, p_pond: Circle | null, p_random: () => number, r_trees: Circle[]): void {
    const mine: Circle[] = [];
    const area = p_cells.length / (PLOT * PLOT);
    const onLot = (x: number, y: number) => {
        const i = Math.floor(x * PLOT);
        const j = Math.floor(y * PLOT);
        return i >= 0 && j >= 0 && i < p_n && j < p_n && p_l.lotOf[j * p_n + i] === p_k;
    };
    for (let tries = 0; tries < area * 1.5; tries++) {
        const c = p_cells[Math.floor(p_random() * p_cells.length)];
        const t: Circle = { x: ((c % p_n) + p_random()) / PLOT, y: (Math.floor(c / p_n) + p_random()) / PLOT, radius: 0.55 + 0.45 * p_random() };
        if (![[1, 0], [-1, 0], [0, 1], [0, -1]].every(([dx, dy]) => onLot(t.x + dx * t.radius, t.y + dy * t.radius))) continue;
        if (p_pond && Math.hypot(t.x - p_pond.x, t.y - p_pond.y) < p_pond.radius + t.radius + 0.3) continue;
        if (mine.some((o) => Math.hypot(o.x - t.x, o.y - t.y) < o.radius + t.radius + TREE_GAP - 1)) continue;
        mine.push(t);
    }
    r_trees.push(...mine);
}

// The biggest p_width x p_depth rectangle on lot p_k: set back from its frontage, else centred on
// its widest point.
function fitOnLot(p_l: Lots, p_k: number, p_cells: number[], p_n: number, p_width: number, p_depth: number, p_setback: number): Rect | null {
    const lot = p_l.lots[p_k];
    const facing = (p_r: Rect | null) => (p_r ? { ...p_r, angle: faceRoad(p_r.angle, lot.inward) } : null);
    const r = fitRect(lot, p_k, p_l.lotOf, p_n, p_width, p_depth, p_setback);
    if (r && r.width >= p_width * 0.5) return facing(r);
    const c = biggestCircle(p_cells, p_l.lotOf, p_n, p_k);
    const around = fitRectAround(lot, p_k, p_l.lotOf, p_n, p_width, p_depth, c);
    return facing(around && (!r || around.width > r.width) ? around : r);
}

// The mall's car park: in front of its building (towards the road), as deep as fits, up to p_depth.
function carParkFor(p_l: Lots, p_k: number, p_n: number, p_b: Rect, p_depth: number): Rect | null {
    const lot = p_l.lots[p_k];
    for (const f of [1, 0.75, 0.5, 0.3]) {
        const d = p_depth * f;
        const back = p_b.depth / 2 + 0.3 + d / 2;
        const r = { x: p_b.x - lot.inward.x * back, y: p_b.y - lot.inward.y * back, angle: p_b.angle, width: p_b.width, depth: d };
        if (rectOnLot(r, p_l.lotOf, p_n, p_k)) return r;
    }
    return null;
}

export function makeBuildings(p_l: Lots, p_z: Zoning, p_n: number, p_random: () => number): Buildings {
    const out: Buildings = { buildings: [], carParks: [], trees: [], ponds: [] };
    const cells = cellsOf(p_l);
    // each island's round tower: on the downtown lot scoring best for centrality and room (the
    // radius of the biggest circle on it), among its 8 most central
    const towers = new Map<number, Circle>();
    const islands = new Set(p_z.blocks.map((b) => b.island));
    for (const island of islands) {
        const candidates = p_l.lots
            .map((lot, k) => ({ lot, k }))
            .filter((o) => o.lot.zone === 'downtown' && p_z.blocks[o.lot.block].island === island && cells[o.k].length > 0)
            .sort((a, b) => p_z.blocks[b.lot.block].central - p_z.blocks[a.lot.block].central)
            .slice(0, 8);
        let best: { k: number; c: Circle; score: number } | null = null;
        for (const o of candidates) {
            const c = biggestCircle(cells[o.k], p_l.lotOf, p_n, o.k);
            const score = p_z.blocks[o.lot.block].central + c.radius / 4;
            if (!best || score > best.score) best = { k: o.k, c, score };
        }
        if (best) towers.set(best.k, best.c);
    }
    p_l.lots.forEach((lot, k) => {
        if (cells[k].length === 0) return;
        const deep = lot.area / Math.max(1, lot.frontage); // how far back the lot reaches, roughly
        const central = p_z.blocks[lot.block].central;
        if (lot.zone === 'downtown' && towers.has(k)) {
            const c = towers.get(k)!;
            out.buildings.push({ kind: 'tower', rect: null, circle: { ...c, radius: c.radius * 0.85 }, floors: 22 + Math.round(6 * p_random()) });
        } else if (lot.zone === 'downtown') {
            const rect = fitOnLot(p_l, k, cells[k], p_n, lot.frontage * 0.92, deep * 0.85, 0.25);
            if (rect) out.buildings.push({ kind: 'downtown', rect, circle: null, floors: Math.round(3 + 14 * central + 4 * p_random()) });
        } else if (lot.zone === 'industry') {
            const rect = fitOnLot(p_l, k, cells[k], p_n, lot.frontage * 0.85, deep * 0.7, 0.6);
            if (rect) out.buildings.push({ kind: 'industry', rect, circle: null, floors: 1 + Math.round(p_random()) });
        } else if (lot.zone === 'mall') {
            const park = deep * 0.3; // the car park in front
            const rect = fitOnLot(p_l, k, cells[k], p_n, Math.min(lot.frontage * 0.85, deep * 1.6), deep * 0.55, park);
            if (rect) {
                out.buildings.push({ kind: 'mall', rect, circle: null, floors: 2 });
                const carPark = carParkFor(p_l, k, p_n, rect, park);
                if (carPark) out.carParks.push(carPark);
            }
        } else if (lot.zone === 'park') {
            const c = biggestCircle(cells[k], p_l.lotOf, p_n, k);
            const pond = c.radius >= MIN_POND && p_random() < 0.75 ? { ...c, radius: c.radius * 0.6 } : null;
            if (pond) out.ponds.push(pond);
            plantTrees(cells[k], k, p_l, p_n, pond, p_random, out.trees);
        }
    });
    return out;
}
