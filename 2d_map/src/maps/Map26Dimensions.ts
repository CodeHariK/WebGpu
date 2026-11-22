// Colour Blocks — sizes shared by the 2D map, the 3D preview (Map26Scene) and the JSON export
// (Map26Export), so all three agree: each road kind's half width, a floor's height, how many floors
// a house has, and how the motorway climbs onto its bridge.
import { coastDistance, type Island } from './Map26Island';

// Half widths (fine cells) either side of a road's line: as drawn, and as the plots and houses keep
// clear of it. At the export's 4 m a map pixel (a fine cell is 10.7 m): a street about 23 m, a
// district's main road 33, a highway (and the ring road) 65, an arterial 72, the motorway 85.
export const STREET_HALF = 1.08;
export const MAIN_HALF = 1.56;
export const HIGHWAY_HALF = 3.04;
export const ARTERIAL_HALF = 3.36;
export const MOTORWAY_HALF = 4.0;

export const FLOOR = 1.25; // map pixels a floor (in the 3D preview: 5 m at 4 m a pixel)
export const BRIDGE_HEIGHT = 4.5; // map pixels: the bridge's deck above the water
const RAMP = 6; // points (each a quarter block cell) the motorway takes to climb onto its bridge
const ROAD_HEIGHT = 0.3; // map pixels: the motorway's top off the bridge

// A random number 0..1 for a whole number.
function hash(p_k: number): number {
    let h = Math.imul(p_k + 3, 2654435761);
    h ^= h >>> 15;
    return ((h >>> 0) % 1000) / 1000;
}

// House p_k's floors: 1, or 2 for about a third of them.
export function houseFloors(p_k: number): number {
    return 1 + (hash(p_k) < 0.35 ? 1 : 0);
}

// The motorway's height (map pixels) at each of its points (map pixels): ROAD_HEIGHT on land,
// BRIDGE_HEIGHT over water, eased up and down over the RAMP points either side of the water.
export function deckHeights(p_points: { x: number; y: number }[], p_island: Island): number[] {
    const water = p_points.map((p) => coastDistance(p_island, p.x, p.y) > 0);
    return p_points.map((_, k) => {
        let near = RAMP + 1;
        for (let d = 0; d <= RAMP && near > RAMP; d++) if (water[k - d] || water[k + d]) near = d;
        const t = Math.max(0, 1 - near / (RAMP + 1));
        return ROAD_HEIGHT + (BRIDGE_HEIGHT - ROAD_HEIGHT) * t * t * (3 - 2 * t);
    });
}

// A line (in any units) as points every p_step along it.
export function densify(p_points: { x: number; y: number }[], p_step: number): { x: number; y: number }[] {
    const out: { x: number; y: number }[] = [];
    for (let k = 0; k < p_points.length - 1; k++) {
        const a = p_points[k];
        const b = p_points[k + 1];
        const steps = Math.max(1, Math.ceil(Math.hypot(b.x - a.x, b.y - a.y) / p_step));
        for (let s = 0; s < steps; s++) out.push({ x: a.x + ((b.x - a.x) * s) / steps, y: a.y + ((b.y - a.y) * s) / steps });
    }
    if (p_points.length) out.push(p_points[p_points.length - 1]);
    return out;
}
