// Colour Blocks — the fourth level: houses along the streets and main roads.
//
// Every street and main road is walked along both its sides. A house (a small rectangle, its
// front to the road, a random width and depth near houseSize) goes a little back from the road;
// then the walk moves on by the house's width and a small gap. A house is only placed if all of
// it is inside one district and clear of every road and every house placed before; checked on an
// occupancy grid twice as fine as the district grid, where the roads were drawn in first. Some lots
// are left empty (houseFill). Houses are kept as data.
import type { Highway } from './Map26Borders';
import { DETAIL, type Districts } from './Map26Districts';
import type { Settings } from './Map26Settings';
import type { Streets } from './Map26Streets';

export type House = { x: number; y: number; angle: number; width: number; depth: number; roof: number }; // fine cells
export type Houses = { houses: House[] };

const OCC = 2; // occupancy cells to a fine cell, each way
const ROOFS = 6; // roof colours (Map26Draw has them)
const INSET = 0.2; // fine cells: a house's edges are tested this far inside
const STREET_SETBACK = 0.95; // fine cells from a street's line to the houses' fronts
const MAIN_SETBACK = 1.45; // ... from a main road's

type Grid = { n: number; occupied: Uint8Array };

function mark(p_g: Grid, p_x: number, p_y: number, p_radius: number): void {
    const r = Math.ceil(p_radius * OCC);
    const ci = Math.floor(p_x * OCC);
    const cj = Math.floor(p_y * OCC);
    for (let j = cj - r; j <= cj + r; j++) {
        for (let i = ci - r; i <= ci + r; i++) {
            if (i < 0 || j < 0 || i >= p_g.n || j >= p_g.n) continue;
            if (Math.hypot(i + 0.5 - p_x * OCC, j + 0.5 - p_y * OCC) <= p_radius * OCC) p_g.occupied[j * p_g.n + i] = 1;
        }
    }
}

// A road drawn into the occupancy grid, p_halfWidth (fine cells) either side of its line.
function markRoad(p_g: Grid, p_points: { x: number; y: number }[], p_halfWidth: number): void {
    for (let k = 0; k < p_points.length - 1; k++) {
        const a = p_points[k];
        const b = p_points[k + 1];
        const steps = Math.max(1, Math.ceil(Math.hypot(b.x - a.x, b.y - a.y) * 4));
        for (let s = 0; s <= steps; s++) mark(p_g, a.x + ((b.x - a.x) * s) / steps, a.y + ((b.y - a.y) * s) / steps, p_halfWidth);
    }
}

// The points a house is checked at: its corners, the middle of each side, its middle.
function footprint(p_h: House): { x: number; y: number }[] {
    const ux = Math.cos(p_h.angle);
    const uy = Math.sin(p_h.angle);
    const pts: { x: number; y: number }[] = [];
    for (const a of [-0.5, 0, 0.5]) {
        for (const b of [-0.5, 0, 0.5]) {
            pts.push({ x: p_h.x + ux * a * p_h.width - uy * b * p_h.depth, y: p_h.y + uy * a * p_h.width + ux * b * p_h.depth });
        }
    }
    return pts;
}

// All of p_h inside district p_k and on free ground? (Checked at its 9 points and in the occupancy
// cells under it.)
function fits(p_h: House, p_d: Districts, p_k: number, p_g: Grid): boolean {
    for (const p of footprint(p_h)) {
        const i = Math.floor(p.x);
        const j = Math.floor(p.y);
        if (i < 0 || j < 0 || i >= p_d.n || j >= p_d.n || p_d.owner[j * p_d.n + i] !== p_k) return false;
    }
    const ux = Math.cos(p_h.angle);
    const uy = Math.sin(p_h.angle);
    const edge = INSET / Math.min(p_h.width, p_h.depth); // test a little inside the edges
    for (let a = -0.5 + edge; a <= 0.5 - edge; a += 0.5 / (p_h.width * OCC)) {
        for (let b = -0.5 + edge; b <= 0.5 - edge; b += 0.5 / (p_h.depth * OCC)) {
            const x = p_h.x + ux * a * p_h.width - uy * b * p_h.depth;
            const y = p_h.y + uy * a * p_h.width + ux * b * p_h.depth;
            const i = Math.floor(x * OCC);
            const j = Math.floor(y * OCC);
            if (i < 0 || j < 0 || i >= p_g.n || j >= p_g.n || p_g.occupied[j * p_g.n + i]) return false;
        }
    }
    return true;
}

function claim(p_h: House, p_g: Grid): void {
    const ux = Math.cos(p_h.angle);
    const uy = Math.sin(p_h.angle);
    for (let a = -0.5; a <= 0.5; a += 0.5 / (p_h.width * OCC)) {
        for (let b = -0.5; b <= 0.5; b += 0.5 / (p_h.depth * OCC)) {
            mark(p_g, p_h.x + ux * a * p_h.width - uy * b * p_h.depth, p_h.y + uy * a * p_h.width + ux * b * p_h.depth, 0.25); // a little round each: a gap to the next
        }
    }
}

// Houses along one road piece (a to b), both sides, p_setback fine cells from its line to the
// houses' fronts.
function lineUp(p_ax: number, p_ay: number, p_bx: number, p_by: number, p_setback: number, p_d: Districts, p_g: Grid, p_settings: Settings, p_random: () => number, r_out: House[]): void {
    const length = Math.hypot(p_bx - p_ax, p_by - p_ay);
    if (length < 1) return;
    const ux = (p_bx - p_ax) / length;
    const uy = (p_by - p_ay) / length;
    const angle = Math.atan2(uy, ux);
    const size = p_settings.houseSize;
    for (const side of [1, -1]) {
        let t = p_random() * size * 0.5;
        while (t < length) {
            const width = size * (0.8 + 0.45 * p_random());
            const depth = size * (0.85 + 0.45 * p_random());
            const along = t + width / 2;
            const out = side * (p_setback + depth / 2);
            const h: House = { x: p_ax + ux * along - uy * out, y: p_ay + uy * along + ux * out, angle, width, depth, roof: Math.floor(p_random() * ROOFS) };
            const i = Math.floor(h.x);
            const j = Math.floor(h.y);
            const k = i >= 0 && j >= 0 && i < p_d.n && j < p_d.n ? p_d.owner[j * p_d.n + i] : -1;
            if (k >= 0 && p_random() < p_settings.houseFill && fits(h, p_d, k, p_g)) {
                r_out.push(h);
                claim(h, p_g);
            }
            t += width + size * (0.15 + 0.25 * p_random());
        }
    }
}

export function placeHouses(p_d: Districts, p_streets: Streets, p_highways: Highway[], p_settings: Settings, p_random: () => number): Houses {
    const g: Grid = { n: p_d.n * OCC, occupied: new Uint8Array(p_d.n * OCC * p_d.n * OCC) };
    const highways = p_highways.map((h) => h.points.map((p) => ({ x: p.x * DETAIL, y: p.y * DETAIL })));
    for (const h of highways) markRoad(g, h, 1.6); // the roads as drawn, a little wider
    for (const r of p_d.roads.highways) markRoad(g, r.points, 1.0);
    for (const s of p_streets.streets) markRoad(g, [{ x: s.ax, y: s.ay }, { x: s.bx, y: s.by }], 0.55);
    const houses: House[] = [];
    for (const s of p_streets.streets) lineUp(s.ax, s.ay, s.bx, s.by, STREET_SETBACK, p_d, g, p_settings, p_random, houses);
    for (const r of p_d.roads.highways) {
        for (let k = 0; k < r.points.length - 1; k++) lineUp(r.points[k].x, r.points[k].y, r.points[k + 1].x, r.points[k + 1].y, MAIN_SETBACK, p_d, g, p_settings, p_random, houses);
    }
    return { houses };
}
