// Street Map — the small streets, worked out per pixel: how much of the pixel is street.
//
// A pixel's district is a side of its nearest boulevard. Measured from that boulevard it has:
//   along   how far along the boulevard (s of its nearest point)
//   out     how far from the boulevard
// Streets straight out from the boulevard are where `along` is a whole number of blocks; streets
// alongside it are where `out` is. Each district has its own block sizes and offsets (a hash of its
// number), so neighbouring districts don't line up, and some street pieces are left out (gaps).
// Plus: a street along every district border, winding roads along the hills' height lines, and a
// road round each lake.
import { zoneOf, type Field } from './Map24Field';
import { hash } from './Map24Random';
import type { Samples } from './Map24Samples';
import type { Settings } from './Map24Settings';
import { CELL, heightAt, heightSlope, type Terrain } from './Map24Terrain';

export const STREET_WIDTH = 1.15; // pixels
export const LAKE_PARK = 13; // pixels of park round a lake before the streets start
const SPREAD_STEP = 5; // pixels: how far apart `along` is compared to measure the spread

export type World = { settings: Settings; terrain: Terrain; samples: Samples; field: Field };

export type Here = {
    zone: number; // the district, -1 none
    out: number; // pixels from its boulevard
    along: number;
    spread: number; // how much further apart the streets out from the boulevard are here than at it
    point: number; // the nearest road point
};

const clamp01 = (p_v: number) => Math.min(1, Math.max(0, p_v));
const line = (p_distance: number) => clamp01(STREET_WIDTH * 0.5 + 0.5 - p_distance);

// From road point p_i, walk along its road (big steps, then small) while the points get nearer to
// (p_x, p_y): the nearest point of that road (a jump flood's answer is only nearly right).
export function nearestOnRoad(p_s: Samples, p_i: number, p_x: number, p_y: number): number {
    const road = p_s.road[p_i];
    let best = p_i;
    let bestD = (p_s.x[p_i] - p_x) ** 2 + (p_s.y[p_i] - p_y) ** 2;
    for (const jump of [16, 4, 1]) {
        for (let moved = true; moved; ) {
            moved = false;
            for (const k of [best - jump, best + jump]) {
                if (k < 0 || k >= p_s.count || p_s.road[k] !== road) continue;
                const d = (p_s.x[k] - p_x) ** 2 + (p_s.y[k] - p_y) ** 2;
                if (d < bestD) {
                    bestD = d;
                    best = k;
                    moved = true;
                }
            }
        }
    }
    return best;
}

// Where (p_x, p_y) is measured from its nearest boulevard (the jump flood's point, refined to the
// nearest point of that road).
export function locate(p_w: World, p_x: number, p_y: number, r_here: Here): void {
    const f = p_w.field;
    const s = p_w.samples;
    const cx = Math.min(f.cols - 1, Math.max(0, Math.floor(p_x / CELL)));
    const cy = Math.min(f.rows - 1, Math.max(0, Math.floor(p_y / CELL)));
    const start = f.near[cy * f.cols + cx];
    if (start < 0) {
        r_here.zone = -1;
        return;
    }
    const best = nearestOnRoad(s, start, p_x, p_y);
    const bestD = (s.x[best] - p_x) ** 2 + (s.y[best] - p_y) ** 2;
    r_here.zone = zoneOf(s, best, p_x, p_y);
    r_here.point = best;
    r_here.out = Math.sqrt(bestD);
    r_here.along = alongAt(s, best, p_x, p_y);
    // how far apart the streets out from the boulevard are here, compared with at the boulevard
    // (they close in on the inside of a bend, spread out on the outside): step SPREAD_STEP pixels
    // either way along the road's direction and see how much `along` changes
    const tx = s.tx[best];
    const ty = s.ty[best];
    const h = SPREAD_STEP;
    const ahead = alongAt(s, nearestOnRoad(s, best, p_x + tx * h, p_y + ty * h), p_x + tx * h, p_y + ty * h);
    const behind = alongAt(s, nearestOnRoad(s, best, p_x - tx * h, p_y - ty * h), p_x - tx * h, p_y - ty * h);
    // (`along` going backwards: round the end of a road, where streets would fan: none)
    r_here.spread = ahead - behind > 1e-3 ? (2 * h) / (ahead - behind) : 0;
}

// How far along road point p_i's road (p_x, p_y) is: the point's s plus the bit past it.
function alongAt(p_s: Samples, p_i: number, p_x: number, p_y: number): number {
    return p_s.s[p_i] + (p_x - p_s.x[p_i]) * p_s.tx[p_i] + (p_y - p_s.y[p_i]) * p_s.ty[p_i];
}

// The grid streets of the district: lines straight out from the boulevard and alongside it.
function gridStreets(p_w: World, p_here: Here): number {
    const z = p_here.zone;
    const salt = p_w.settings.seed;
    const block = p_w.settings.block;
    const alongBlock = block * (1.0 + 0.7 * hash(z, 1, salt)); // blocks are longer than deep
    const outBlock = block * (0.7 + 0.35 * hash(z, 2, salt));
    const alongPhase = alongBlock * hash(z, 3, salt);
    const outPhase = outBlock * (0.45 + 0.4 * hash(z, 4, salt));
    // where they close in, keep only every 2nd one, and none where they nearly meet
    const spread = p_here.spread;
    const keep = spread < 0.45 ? 0 : spread < 0.75 ? 2 : 1;
    const a = (p_here.along - alongPhase) / (alongBlock * Math.max(keep, 1));
    const o = (p_here.out - outPhase) / outBlock;
    const ka = Math.round(a);
    const ko = Math.round(o);
    const gaps = p_w.settings.gaps;
    let cover = 0;
    // a street out from the boulevard, unless this piece of it (between two streets alongside) is a gap
    if (keep > 0 && hash(ka * 131 + Math.floor(o), z, salt + 5) >= gaps) cover = line(Math.abs(a - ka) * alongBlock * keep * spread);
    if (ko >= 0 && hash(ko * 131 + Math.floor(a), z, salt + 6) >= gaps * 0.6) cover = Math.max(cover, line(Math.abs(o - ko) * outBlock));
    return cover;
}

// A street where two districts meet: halfway between this pixel's boulevard and the nearest other
// one (found through the nearest border cell, whose neighbour across the border knows it).
function borderStreet(p_w: World, p_x: number, p_y: number, p_here: Here): number {
    const f = p_w.field;
    const s = p_w.samples;
    const cx = Math.min(f.cols - 1, Math.max(0, Math.floor(p_x / CELL)));
    const cy = Math.min(f.rows - 1, Math.max(0, Math.floor(p_y / CELL)));
    const b = f.border[cy * f.cols + cx];
    if (b < 0 || p_here.zone < 0) return 0;
    if (p_here.out < p_w.settings.block * 1.5) return 0; // none near the boulevards: no streets along the bisector of every crossing
    const myRoad = p_here.zone >> 1;
    let other = -1;
    let otherD = Infinity;
    for (const n of [b, b + 1, b + f.cols, b - 1, b - f.cols]) {
        if (n < 0 || n >= f.near.length) continue;
        const i = f.near[n];
        if (i < 0 || s.road[i] === myRoad) continue;
        const k = nearestOnRoad(s, i, p_x, p_y);
        const d = Math.hypot(s.x[k] - p_x, s.y[k] - p_y);
        if (d < otherD) {
            otherD = d;
            other = k;
        }
    }
    if (other < 0) return 0;
    const t = p_w.terrain;
    if (s.x[other] < 0 || s.y[other] < 0 || s.x[other] > t.width || s.y[other] > t.height) return 0; // off-map roads make slivers at the edges
    // (otherD − out) grows by |u1 − u2| a pixel away from the border (u: unit vectors from each road)
    const mine = p_here.point;
    const ux = (p_x - s.x[mine]) / p_here.out - (p_x - s.x[other]) / otherD;
    const uy = (p_y - s.y[mine]) / p_here.out - (p_y - s.y[other]) / otherD;
    const slope = Math.hypot(ux, uy);
    if (slope < 0.3) return 0; // the two roads run side by side here: no clear border
    return line((otherD - p_here.out) / slope);
}

// Winding roads along a few of the hills' height lines, broken up by noise.
export function hillRoads(p_w: World, p_x: number, p_y: number, p_height: number): number {
    const t = p_w.terrain;
    const spacing = 0.045;
    const level = (p_height - t.hillLevel) / spacing;
    const k = Math.round(level);
    if (k < 1 || k > 3) return 0;
    if (t.noise(p_x / 70 + k * 3.1, p_y / 70 - k * 1.7) < 0.1) return 0; // broken: short winding pieces, not rings
    const distance = (Math.abs(level - k) * spacing) / heightSlope(t, p_x, p_y);
    return line(distance);
}

export type Layers = { grid: boolean; hillsAndLakes: boolean };

// How much of pixel (p_x, p_y) is street, 0..1: the district borders always, the grid streets and
// the hill and lake roads when p_layers says so.
export function streetCover(p_w: World, p_x: number, p_y: number, p_here: Here, p_lakeEdge: number, p_layers: Layers): number {
    const t = p_w.terrain;
    const height = heightAt(t, p_x, p_y);
    if (height > t.hillLevel) return p_layers.hillsAndLakes ? hillRoads(p_w, p_x, p_y, height) : 0;
    let cover = 0;
    if (p_layers.hillsAndLakes && t.lakes.length) cover = line(Math.abs(p_lakeEdge - LAKE_PARK));
    if (p_lakeEdge < LAKE_PARK) return cover;
    const edgeFade = clamp01((t.hillLevel - height) / 0.012); // streets thin out at the foot of a hill
    if (p_layers.grid && p_here.zone >= 0) cover = Math.max(cover, gridStreets(p_w, p_here) * edgeFade);
    cover = Math.max(cover, borderStreet(p_w, p_x, p_y, p_here) * edgeFade);
    return cover;
}
