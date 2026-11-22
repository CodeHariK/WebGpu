// Colour Blocks — the map as JSON, for building it in another engine (Godot): everything the 3D
// preview shows, in metres.
//
// Units: metres (one map pixel is metresPerPixel). Axes: x to the right, z down the map (the 2D
// map's y), y up; the origin is the map's middle. Angles: radians, from x towards z (in Godot,
// rotation.y = −angle). A line is a list of [x, z] points. Contents:
//   coastlines: closed loops, land on the left walking along them (islands one way round, lakes
//     the other)
//   roads: each with its kind (street, main, highway, ring, arterial, motorway), its width and its
//     line; the motorway also has a height at each point (on its bridge over the water)
//   junctions: where highways meet
//   buildings: each a box (x, z, angle, width, depth) or a cylinder (x, z, radius), with floors and
//     height; kinds: house, downtown, tower, industry, mall, warehouse, ship. A house's, downtown
//     block's, industry shed's or mall's front (towards its road) is its −depth side: the way
//     (sin angle, −cos angle); in Godot (rotation.y = −angle) its local −z
//   trees, ponds (circles), carParks (boxes), dockYards (polygons), piers (boxes)
import { onRing } from './Map26Arterials';
import type { Point } from './Map26Borders';
import { coastlines } from './Map26Coast';
import { ARTERIAL_HALF, deckHeights, densify, FLOOR, HIGHWAY_HALF, houseFloors, MAIN_HALF, MOTORWAY_HALF, STREET_HALF } from './Map26Dimensions';
import type { Rect } from './Map26Fit';
import type { Blocks } from './Map26Render';

export const FLOOR_METRES = 5.0; // a floor's height in the export (cartoon-tall, for the big chunky houses and blocks)
const ROUND = 100; // metres kept to the centimetre

type Box = { x: number; z: number; angle: number; width: number; depth: number };

export function exportMap(p_b: Blocks, p_size: number, p_metresPerPixel: number): object {
    const m = p_metresPerPixel;
    const half = p_size / 2;
    const r = (v: number) => Math.round(v * ROUND) / ROUND;
    const xz = (p: Point, p_scale: number): [number, number] => [r((p.x * p_scale - half) * m), r((p.y * p_scale - half) * m)];
    const block = p_size / p_b.spread.n; // pixels a block cell
    const fine = p_b.districts ? p_size / p_b.districts.n : block / 3; // pixels a fine cell
    const box = (p_r: Rect, p_scale: number): Box => {
        const [x, z] = xz({ x: p_r.x, y: p_r.y }, p_scale);
        return { x, z, angle: Math.round(p_r.angle * 10000) / 10000, width: r(p_r.width * p_scale * m), depth: r(p_r.depth * p_scale * m) };
    };
    const floorsToHeight = (floors: number) => r(floors * FLOOR_METRES);

    const roads: object[] = [];
    const road = (kind: string, p_half: number, points: Point[], p_scale: number) => roads.push({ kind, width: r(p_half * 2 * fine * m), points: points.map((p) => xz(p, p_scale)) });
    for (const line of p_b.streets?.lines ?? p_b.streets?.streets.map((s) => [{ x: s.ax, y: s.ay }, { x: s.bx, y: s.by }]) ?? []) road('street', STREET_HALF, line, fine);
    for (const h of p_b.districts?.roads.highways ?? []) road('main', MAIN_HALF, h.points, fine);
    p_b.borders.highways.forEach((h, k) => {
        if (p_b.arterials?.pieces[k]) return; // in an arterial below
        road(onRing(h.points, p_b.spread) && p_b.island ? 'ring' : 'highway', HIGHWAY_HALF, h.points, block);
    });
    for (const line of p_b.arterials?.lines ?? []) road('arterial', ARTERIAL_HALF, line, block);
    if (p_b.motorway && p_b.island) {
        const pts = densify(p_b.motorway.road.points.map((p) => ({ x: p.x * block, y: p.y * block })), block / 4);
        const heights = deckHeights(pts, p_b.island);
        roads.push({ kind: 'motorway', width: r(MOTORWAY_HALF * 2 * fine * m), points: pts.map((p) => xz(p, 1)), heights: heights.map((h) => r(h * m)) });
    }

    const buildings: object[] = [];
    p_b.houses?.houses.forEach((h, k) => {
        const floors = houseFloors(k);
        buildings.push({ kind: 'house', shape: 'box', ...box(h, fine), floors, height: floorsToHeight(floors), roof: h.roof });
    });
    for (const b of p_b.buildings?.buildings ?? []) {
        if (b.rect) buildings.push({ kind: b.kind, shape: 'box', ...box(b.rect, fine), floors: b.floors, height: floorsToHeight(b.floors) });
        else if (b.circle) {
            const [x, z] = xz(b.circle, fine);
            buildings.push({ kind: b.kind, shape: 'cylinder', x, z, radius: r(b.circle.radius * fine * m), floors: b.floors, height: floorsToHeight(b.floors) });
        }
    }
    for (const w of p_b.docks?.warehouses ?? []) buildings.push({ kind: 'warehouse', shape: 'box', ...box(w, fine), floors: 2, height: floorsToHeight(2) });
    for (const s of p_b.docks?.ships ?? []) buildings.push({ kind: 'ship', shape: 'box', ...box(s, fine), floors: 1, height: r(1.8 * m) });

    const circle = (c: { x: number; y: number; radius: number }) => {
        const [x, z] = xz(c, fine);
        return { x, z, radius: r(c.radius * fine * m) };
    };
    return {
        format: 'map26',
        version: 2,
        seed: p_b.settings.seed,
        step: p_b.settings.step,
        units: 'metres',
        metresPerPixel: m,
        size: r(p_size * m),
        floorHeight: FLOOR_METRES,
        axes: 'x right, z down the map, y up; origin the map middle; angle from x towards z (Godot rotation.y = -angle)',
        coastlines: p_b.island ? coastlines(p_b.island).map((loop) => loop.map((p) => xz(p, 1))) : [],
        roads,
        junctions: p_b.borders.junctions.map((p) => xz(p, block)),
        buildings,
        trees: (p_b.buildings?.trees ?? []).map(circle),
        ponds: (p_b.buildings?.ponds ?? []).map(circle),
        carParks: (p_b.buildings?.carParks ?? []).map((c) => box(c, fine)),
        dockYards: (p_b.docks?.yards ?? []).map((y) => y.map((p) => xz(p, fine))),
        piers: (p_b.docks?.piers ?? []).map((p) => box(p, fine)),
        settings: p_b.settings,
        pixelsPerFloorIn3D: FLOOR,
    };
}

// Hands the JSON to the browser to save as a file.
export function downloadJson(p_data: object, p_name: string): void {
    const url = URL.createObjectURL(new Blob([JSON.stringify(p_data)], { type: 'application/json' }));
    const a = document.createElement('a');
    a.href = url;
    a.download = p_name;
    a.click();
    URL.revokeObjectURL(url);
}
