// Colour Blocks — makes the spread, the borders and (from step 5) the districts and (step 6) the
// streets and (step 7) the houses from the settings, or (step 8) the island on its own, or (step 9)
// the city on the island, kept cityMargin off the coast, a ring road round it, with (step 10)
// arterials, or (step 11) all that on two islands, a motorway bridge between their ring roads, with
// (step 12) the streets along a street field, (step 13) the blocks cut into plots and (step 14)
// each block given a use, (step 15) the plots joined into lots by use; and draws a step (at a point in the spread's animation: p_progress 0..1).
import { findArterials, type Arterials } from './Map26Arterials';
import { findBorders, type Borders, type Highway, type Point } from './Map26Borders';
import { DETAIL, makeDistricts, type Districts } from './Map26Districts';
import { drawArterials, drawCells, drawField, drawLots, drawPlots, drawZones, drawDistrictCells, drawGrid, drawHighways, drawHouses, drawMainRoads, drawMotorway, drawQueue, drawSeeds, drawStreets } from './Map26Draw';
import { placeHouses, type Houses } from './Map26Houses';
import { housesOnPlots } from './Map26PlotHouses';
import { makePlots, type Plots, type Road } from './Map26Plots';
import { bridgeEnds, makeZoning, type Zoning } from './Map26Zoning';
import { makeLots, type Lots } from './Map26Lots';
import { cityOutside, withEdge } from './Map26City';
import { drawIsland, drawRings, islandLayout, makeIsland, type Island, type IslandView } from './Map26Island';
import { makeBridge, type Motorway } from './Map26Bridge';
import { makeField, type Field } from './Map26Field';
import { makeFieldStreets } from './Map26FieldStreets';
import { makeStreets, type Streets } from './Map26Streets';
import { queueAt, type QueueView } from './Map26Queue';
import { makeNoise, makeRandom } from './Map24Random';
import type { Settings } from './Map26Settings';
import { spread, type Spread } from './Map26Spread';

export type Blocks = {
    settings: Settings;
    spread: Spread;
    borders: Borders;
    districts: Districts | null;
    streets: Streets | null;
    houses: Houses | null;
    island: Island | null;
    arterials: Arterials | null; // step 10 on
    motorway: Motorway | null; // step 11 on: the bridge between the islands
    field: Field | null; // step 12 on: the street field
    plots: Plots | null; // step 13 on
    zoning: Zoning | null; // step 14 on
    lots: Lots | null; // step 15 on
};
export type Show = { queue: boolean; ground: boolean; island: IslandView; rings: boolean; field: boolean }; // steps 1 - 2, 8 and 12 extras

const DISTRICTS_FROM = 5; // the step the districts appear
const STREETS_FROM = 6;
const HOUSES_FROM = 7;
const ISLAND_FROM = 8; // the island, on its own
const CITY_ON_ISLAND = 9; // the city again, on the island, kept off the coast, a ring road round it
const ARTERIALS_FROM = 10;
const TWO_ISLANDS_FROM = 11;
const FIELD_FROM = 12; // streets along the street field
const PLOTS_FROM = 13; // the blocks cut into plots, one house each
const ZONING_FROM = 14; // each block given a use
const LOTS_FROM = 15; // the plots joined into lots by use
const STREET_HALF = 0.55; // fine cells either side of a road's line, as the houses keep clear of them
const MAIN_HALF = 1.0;
const HIGHWAY_HALF = 1.6;
const ARTERIAL_HALF = 2.1; // fine cells either side of an arterial's line kept clear of houses
const MOTORWAY_HALF = 2.8;

export function makeBlocks(p_settings: Settings, p_size: number): Blocks {
    const random = makeRandom(p_settings.seed);
    const noise = makeNoise(random);
    const layout = islandLayout(p_size, p_settings, p_settings.step >= TWO_ISLANDS_FROM, makeRandom(p_settings.seed * 3301 + 9));
    const island = p_settings.step >= ISLAND_FROM ? makeIsland(p_size, layout, p_settings, noise) : null;
    const outside = island && p_settings.step >= CITY_ON_ISLAND ? cityOutside(p_settings.cells, p_size, island, p_settings.cityMargin) : null;
    const s = spread(p_settings, random, noise, outside);
    const borders = findBorders(s.n, outside ? withEdge(s.owner) : s.owner, p_settings.straighten, p_settings.smoothing); // on the island: with the ring road
    if (island && !outside) {
        return { settings: p_settings, spread: s, borders, districts: null, streets: null, houses: null, island, arterials: null, motorway: null, field: null, plots: null, zoning: null, lots: null };
    }
    const arterials = outside && p_settings.step >= ARTERIALS_FROM ? findArterials(borders, s, p_settings) : null;
    const motorway = island && outside && p_settings.step >= TWO_ISLANDS_FROM ? makeBridge(s, borders, island, p_size, p_settings) : null;
    const roads = borders.highways; // the walls between districts (the bridge stays outside the cities)
    const wide = [...(arterials?.lines ?? []).map((points) => ({ points, halfWidth: ARTERIAL_HALF })), ...(motorway ? [{ points: motorway.road.points, halfWidth: MOTORWAY_HALF }] : [])];
    // the districts only when shown (they take longer), from their own random numbers
    const districts = p_settings.step >= DISTRICTS_FROM ? makeDistricts(s, roads, p_settings, makeRandom(p_settings.seed * 7919 + 1), noise, outside !== null) : null;
    const field = districts && p_settings.step >= FIELD_FROM ? makeField(s.n, roads, districts.roads.highways, near(s), p_settings.fieldReach) : null;
    const streets = !districts || p_settings.step < STREETS_FROM ? null : field ? makeFieldStreets(districts, roads, field, p_settings) : makeStreets(districts, roads, p_settings);
    const plots = districts && streets && p_settings.step >= PLOTS_FROM ? makePlots(districts, plotRoads(districts, streets, roads, wide), p_settings.plotWidth, p_settings.plotDepth) : null;
    const zoning = plots && island && p_settings.step >= ZONING_FROM ? makeZoning(plots, island, p_size, bridgeEnds(motorway?.road.points ?? null), p_settings) : null;
    const lots = plots && zoning && p_settings.step >= LOTS_FROM ? makeLots(plots, zoning, p_settings.plotWidth * p_settings.plotDepth, makeRandom(p_settings.seed * 2741 + 5)) : null;
    const houseRandom = makeRandom(p_settings.seed * 104729 + 7);
    const houses = !districts || !streets || p_settings.step < HOUSES_FROM ? null : plots ? housesOnPlots(plots, zoning && houseZone(plots, zoning), p_settings, houseRandom) : placeHouses(districts, streets, roads, wide, field !== null, p_settings, houseRandom);
    return { settings: p_settings, spread: s, borders, districts, streets, houses, island, arterials, motorway, field, plots, zoning, lots };
}

// Every road the plots front onto, in fine cells, each with its half width: the streets (whole
// lines where there are), the main roads, the highways and the wide roads (arterials).
function plotRoads(p_d: Districts, p_s: Streets, p_highways: Highway[], p_wide: { points: Point[]; halfWidth: number }[]): Road[] {
    const fine = (pts: Point[]) => pts.map((p) => ({ x: p.x * DETAIL, y: p.y * DETAIL }));
    const streets = p_s.lines ?? p_s.streets.map((t) => [{ x: t.ax, y: t.ay }, { x: t.bx, y: t.by }]);
    return [
        ...streets.map((points) => ({ points, halfWidth: STREET_HALF, kind: 'street' as const })),
        ...p_d.roads.highways.map((h) => ({ points: h.points, halfWidth: MAIN_HALF, kind: 'main' as const })),
        ...p_highways.map((h) => ({ points: fine(h.points), halfWidth: HIGHWAY_HALF, kind: 'highway' as const })),
        ...p_wide.filter((w) => w.halfWidth <= 2.2).map((w) => ({ points: fine(w.points), halfWidth: w.halfWidth, kind: 'arterial' as const })),
    ];
}

// Per plot: 1 if its block is a house zone (it keeps its house), else 0. A plot's block: that of its
// first cell (a plot never crosses a road, so it is in one block).
function houseZone(p_p: Plots, p_z: Zoning): Uint8Array {
    const keep = new Uint8Array(p_p.plots.length);
    const seen = new Uint8Array(p_p.plots.length);
    for (let c = 0; c < p_p.id.length; c++) {
        const k = p_p.id[c];
        if (k < 0 || seen[k]) continue;
        seen[k] = 1;
        keep[k] = p_z.blocks[p_z.blockOf[c]].zone === 'houses' ? 1 : 0;
    }
    return keep;
}

// Per block cell: 1 in the city or next to it (where the street field is needed).
function near(p_s: Spread): Uint8Array {
    const n = p_s.n;
    const out = new Uint8Array(n * n);
    for (let j = 0; j < n; j++) {
        for (let i = 0; i < n; i++) {
            if (p_s.owner[j * n + i] < 0) continue;
            for (let dj = -1; dj <= 1; dj++) for (let di = -1; di <= 1; di++) if (i + di >= 0 && j + dj >= 0 && i + di < n && j + dj < n) out[(j + dj) * n + i + di] = 1;
        }
    }
    return out;
}

// Draws the step; on steps 1 - 2 returns the queue it showed (else null).
export function drawBlocks(p_ctx: CanvasRenderingContext2D, p_b: Blocks, p_progress: number, p_show: Show): QueueView | null {
    const size = p_ctx.canvas.width;
    const step = p_b.settings.step;
    if (p_b.island && (step < CITY_ON_ISLAND || p_show.island !== 'map')) { // the island alone, or one of its parts
        drawIsland(p_ctx, p_b.island, p_show.island, p_show.rings);
        return null;
    }
    if (p_b.island) { // the city on the island: the map under it, the rings over it
        drawIsland(p_ctx, p_b.island, 'map', false);
        drawCity(p_ctx, p_b, size);
        if (p_b.field && p_show.field) drawField(p_ctx, p_b.field, p_b.spread, size);
        if (p_show.rings) drawRings(p_ctx, p_b.island);
        return null;
    }
    const until = step === 1 ? 0 : step === 2 ? p_b.spread.lastTime * p_progress : Infinity;
    const early = step <= 2;
    drawCells(p_ctx, p_b.spread, size, until, early && p_show.ground);
    let queue: QueueView | null = null;
    if (early && p_show.queue && until < p_b.spread.lastTime) {
        queue = queueAt(p_b.spread, until);
        drawQueue(p_ctx, queue, p_b.spread.n, size);
    }
    if (early) drawGrid(p_ctx, p_b.spread.n, size);
    if (p_b.districts) {
        drawDistrictCells(p_ctx, p_b.districts, size);
        if (p_b.streets) drawStreets(p_ctx, p_b.streets, p_b.districts.n, size);
        drawMainRoads(p_ctx, p_b.districts.roads, p_b.districts.n, size);
        if (p_b.houses) drawHouses(p_ctx, p_b.houses, p_b.districts.n, size);
    }
    if (step >= 4) drawHighways(p_ctx, p_b.borders, p_b.spread.n, size);
    if (step <= 3) drawSeeds(p_ctx, p_b.spread, size, early);
    return queue;
}

// The city on the island (step 9 on): the districts, streets, roads, houses, highways, arterials
// and motorway, with no ground under them (outside the city the island shows).
function drawCity(p_ctx: CanvasRenderingContext2D, p_b: Blocks, p_size: number): void {
    if (!p_b.districts) return;
    drawDistrictCells(p_ctx, p_b.districts, p_size);
    if (p_b.plots) drawPlots(p_ctx, p_b.plots, p_size);
    if (p_b.zoning && p_b.plots) drawZones(p_ctx, p_b.zoning, p_b.plots.n, p_size);
    if (p_b.lots && p_b.plots) drawLots(p_ctx, p_b.lots, p_b.plots.n, p_size);
    if (p_b.streets) drawStreets(p_ctx, p_b.streets, p_b.districts.n, p_size);
    drawMainRoads(p_ctx, p_b.districts.roads, p_b.districts.n, p_size);
    if (p_b.houses) drawHouses(p_ctx, p_b.houses, p_b.districts.n, p_size);
    drawHighways(p_ctx, p_b.borders, p_b.spread.n, p_size, p_b.arterials?.pieces ?? null);
    if (p_b.arterials) drawArterials(p_ctx, p_b.arterials.lines, p_b.spread.n, p_size);
    if (p_b.motorway) drawMotorway(p_ctx, p_b.motorway.road, p_b.motorway.bridges, p_b.spread.n, p_size);
}
