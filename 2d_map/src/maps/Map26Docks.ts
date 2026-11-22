// Colour Blocks — step 17: docks, one stretch of each island's coast, in the strip between the
// ring road and the sea (outside the city, which stops cityMargin short of the coast).
//
// The site: the ring road (the highways along the city's edge) is walked a fine cell at a time;
// every stretch dockLength long is scored by how near the sea it runs (the mean coast distance of
// its points); stretches near an end of the bridge are left out. The nearest wins, one per
// island. Along it, at every point, the yard reaches out from the ring road's edge (square to the
// road, away from the city) to the coast. On the yard: warehouses (rectangles along the road, as
// deep as the yard allows, in 3 lengths). From the coast: piers straight out into the water, every
// PIER_GAP, each kept only if its whole length is over water; beside some piers, a ship.
import type { Borders, Point } from './Map26Borders';
import { onRing } from './Map26Arterials';
import { DETAIL } from './Map26Districts';
import type { Rect } from './Map26Fit';
import { coastDistance, type Island } from './Map26Island';
import type { Settings } from './Map26Settings';
import type { Spread } from './Map26Spread';

export type Docks = { yards: Point[][]; warehouses: Rect[]; piers: Rect[]; ships: Rect[] }; // fine cells

const ROAD_EDGE = 1.8; // fine cells from the ring road's line to the yard
const NEAR_BRIDGE = 30; // fine cells: no docks this near an end of the bridge
const PIER_GAP = 7; // fine cells between piers
const PIER_LENGTH = 7;
const WAREHOUSES = 3;

type Sample = { p: Point; out: Point; inner: Point; coast: Point }; // a point on the ring road, the way out (to the sea), the yard's ends

// p_points (fine cells) every fine cell.
function resample(p_points: Point[]): Point[] {
    const out: Point[] = [p_points[0]];
    let carry = 0;
    for (let k = 1; k < p_points.length; k++) {
        const a = p_points[k - 1];
        const b = p_points[k];
        const length = Math.hypot(b.x - a.x, b.y - a.y);
        for (let t = 1 - carry; t <= length; t += 1) out.push({ x: a.x + ((b.x - a.x) * t) / length, y: a.y + ((b.y - a.y) * t) / length });
        carry = (carry + length) % 1;
    }
    return out;
}

export function makeDocks(p_spread: Spread, p_borders: Borders, p_island: Island, p_size: number, p_bridgeEnds: Point[], p_settings: Settings, p_random: () => number): Docks {
    const px = p_size / (p_spread.n * DETAIL); // pixels a fine cell
    const coast = (p: Point) => coastDistance(p_island, p.x * px, p.y * px); // pixels, negative on land
    const docks: Docks = { yards: [], warehouses: [], piers: [], ships: [] };
    const rings = p_borders.highways.filter((h) => onRing(h.points, p_spread)).map((h) => resample(h.points.map((p) => ({ x: p.x * DETAIL, y: p.y * DETAIL }))));
    const length = Math.round(p_settings.dockLength);
    p_island.centres.forEach((_, island) => {
        let best: { ring: Point[]; at: number; score: number } | null = null;
        for (const ring of rings) {
            const mid = ring[Math.floor(ring.length / 2)];
            const nearest = p_island.centres.reduce((a, c, k) => (Math.hypot(c.x - mid.x * px, c.y - mid.y * px) < Math.hypot(p_island.centres[a].x - mid.x * px, p_island.centres[a].y - mid.y * px) ? k : a), 0);
            if (nearest !== island || ring.length <= length + 4) continue;
            for (let at = 2; at + length < ring.length - 2; at += 2) {
                const stretch = ring.slice(at, at + length);
                if (stretch.some((p) => p_bridgeEnds.some((e) => Math.hypot(e.x - p.x, e.y - p.y) < NEAR_BRIDGE))) continue;
                const score = stretch.reduce((a, p) => a - coast(p), 0) / length; // pixels inland, on average
                if (!best || score < best.score) best = { ring, at, score };
            }
        }
        if (!best) return;
        const samples = sampleYard(best.ring, best.at, length, coast);
        if (samples.length < length / 2) return;
        docks.yards.push([...samples.map((s) => s.inner), ...samples.map((s) => s.coast).reverse()]);
        addWarehouses(samples, docks.warehouses);
        addPiers(samples, coast, p_random, docks);
    });
    return docks;
}

// Each point of the stretch: the way out (square to the road, towards the sea), the yard's inner
// edge (the road's edge) and its outer one (the last point on land walking out).
function sampleYard(p_ring: Point[], p_at: number, p_length: number, p_coast: (p: Point) => number): Sample[] {
    const out: Sample[] = [];
    for (let k = p_at; k < p_at + p_length; k++) {
        const a = p_ring[k - 1];
        const b = p_ring[k + 1];
        const tl = Math.hypot(b.x - a.x, b.y - a.y) || 1;
        let n = { x: -(b.y - a.y) / tl, y: (b.x - a.x) / tl };
        const p = p_ring[k];
        if (p_coast({ x: p.x + n.x * 3, y: p.y + n.y * 3 }) < p_coast({ x: p.x - n.x * 3, y: p.y - n.y * 3 })) n = { x: -n.x, y: -n.y }; // towards the sea
        const inner = { x: p.x + n.x * ROAD_EDGE, y: p.y + n.y * ROAD_EDGE };
        let last = inner;
        for (let t = ROAD_EDGE; t < 40; t += 0.5) {
            const q = { x: p.x + n.x * t, y: p.y + n.y * t };
            if (p_coast(q) > 0) break;
            last = q;
        }
        if (Math.hypot(last.x - inner.x, last.y - inner.y) < 1) continue; // no yard here
        out.push({ p, out: n, inner, coast: last });
    }
    return out;
}

// Warehouses: the stretch cut in WAREHOUSES lengths, each a rectangle along the road, as deep as
// the yard is at its shallowest there (less a margin each side).
function addWarehouses(p_s: Sample[], r_out: Rect[]): void {
    const per = Math.floor(p_s.length / WAREHOUSES);
    for (let w = 0; w < WAREHOUSES; w++) {
        const part = p_s.slice(w * per + 1, (w + 1) * per - 1);
        if (part.length < 3) continue;
        const depth = Math.min(...part.map((s) => Math.hypot(s.coast.x - s.inner.x, s.coast.y - s.inner.y))) - 1.4;
        if (depth < 1.5) continue;
        const a = part[0];
        const b = part[part.length - 1];
        const mid = part[Math.floor(part.length / 2)];
        const back = 0.6 + depth / 2;
        r_out.push({
            x: (a.inner.x + b.inner.x) / 2 + mid.out.x * back,
            y: (a.inner.y + b.inner.y) / 2 + mid.out.y * back,
            angle: Math.atan2(b.p.y - a.p.y, b.p.x - a.p.x),
            width: Math.hypot(b.inner.x - a.inner.x, b.inner.y - a.inner.y),
            depth,
        });
    }
}

// Piers out from the coast every PIER_GAP, kept only where all of one is over water; a ship beside
// some of them (on water too).
function addPiers(p_s: Sample[], p_coast: (p: Point) => number, p_random: () => number, r_docks: Docks): void {
    const water = (r: Rect) => {
        const ux = Math.cos(r.angle);
        const uy = Math.sin(r.angle);
        for (const a of [-0.5, -0.25, 0, 0.25, 0.5]) for (const b of [-0.5, 0, 0.5]) if (p_coast({ x: r.x + ux * a * r.width - uy * b * r.depth, y: r.y + uy * a * r.width + ux * b * r.depth }) <= 0) return false;
        return true;
    };
    for (let k = Math.floor(PIER_GAP / 2); k < p_s.length; k += PIER_GAP) {
        const s = p_s[k];
        const angle = Math.atan2(s.out.y, s.out.x);
        const pier: Rect = { x: s.coast.x + s.out.x * (PIER_LENGTH / 2 + 0.3), y: s.coast.y + s.out.y * (PIER_LENGTH / 2 + 0.3), angle, width: PIER_LENGTH, depth: 1.1 };
        const reach: Rect = { ...pier, x: pier.x + s.out.x * 0.8, y: pier.y + s.out.y * 0.8, width: PIER_LENGTH - 1.6 };
        if (!water(reach)) continue;
        r_docks.piers.push(pier);
        if (p_random() > 0.65) continue;
        const side = p_random() < 0.5 ? 1 : -1; // a ship along one side of it
        const ship: Rect = { x: pier.x - s.out.y * side * 1.9 + s.out.x * 0.6, y: pier.y + s.out.x * side * 1.9 + s.out.y * 0.6, angle, width: PIER_LENGTH * 0.8, depth: 1.8 };
        if (water(ship)) r_docks.ships.push(ship);
    }
}
