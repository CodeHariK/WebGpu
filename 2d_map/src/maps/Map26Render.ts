// Colour Blocks — makes the spread, the borders and (from step 5) the districts and (step 6) the
// streets and (step 7) the houses from the settings, or (step 8) the island on its own, or (step 9)
// the city on the island, kept cityMargin off the coast, a ring road round it; and draws a step (at a point in the spread's animation: p_progress 0..1).
import { findBorders, type Borders } from './Map26Borders';
import { makeDistricts, type Districts } from './Map26Districts';
import { drawCells, drawDistrictCells, drawGrid, drawHighways, drawHouses, drawMainRoads, drawQueue, drawSeeds, drawStreets } from './Map26Draw';
import { placeHouses, type Houses } from './Map26Houses';
import { cityOutside, withEdge } from './Map26City';
import { drawIsland, drawRings, makeIsland, type Island, type IslandView } from './Map26Island';
import { makeStreets, type Streets } from './Map26Streets';
import { queueAt, type QueueView } from './Map26Queue';
import { makeNoise, makeRandom } from './Map24Random';
import type { Settings } from './Map26Settings';
import { spread, type Spread } from './Map26Spread';

export type Blocks = { settings: Settings; spread: Spread; borders: Borders; districts: Districts | null; streets: Streets | null; houses: Houses | null; island: Island | null };
export type Show = { queue: boolean; ground: boolean; island: IslandView; rings: boolean }; // steps 1 - 2 and 8 extras

const DISTRICTS_FROM = 5; // the step the districts appear
const STREETS_FROM = 6;
const HOUSES_FROM = 7;
const ISLAND_FROM = 8; // the island, on its own
const CITY_ON_ISLAND = 9; // the city again, on the island, kept off the coast, a ring road round it

export function makeBlocks(p_settings: Settings, p_size: number): Blocks {
    const random = makeRandom(p_settings.seed);
    const noise = makeNoise(random);
    const island = p_settings.step >= ISLAND_FROM ? makeIsland(p_size, p_settings, noise) : null;
    const outside = island && p_settings.step >= CITY_ON_ISLAND ? cityOutside(p_settings.cells, p_size, island, p_settings.cityMargin) : null;
    const s = spread(p_settings, random, noise, outside);
    const borders = findBorders(s.n, outside ? withEdge(s.owner) : s.owner, p_settings.straighten, p_settings.smoothing); // on the island: with the ring road
    if (island && !outside) {
        return { settings: p_settings, spread: s, borders, districts: null, streets: null, houses: null, island };
    }
    // the districts only when shown (they take longer), from their own random numbers
    const districts = p_settings.step >= DISTRICTS_FROM ? makeDistricts(s, borders.highways, p_settings, makeRandom(p_settings.seed * 7919 + 1), noise, outside !== null) : null;
    const streets = districts && p_settings.step >= STREETS_FROM ? makeStreets(districts, borders.highways, p_settings) : null;
    const houses = districts && streets && p_settings.step >= HOUSES_FROM ? placeHouses(districts, streets, borders.highways, p_settings, makeRandom(p_settings.seed * 104729 + 7)) : null;
    return { settings: p_settings, spread: s, borders, districts, streets, houses, island };
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

// The city on the island (step 9): the districts, streets, roads, houses and highways, with no
// ground under them (outside the city the island shows).
function drawCity(p_ctx: CanvasRenderingContext2D, p_b: Blocks, p_size: number): void {
    if (!p_b.districts) return;
    drawDistrictCells(p_ctx, p_b.districts, p_size);
    if (p_b.streets) drawStreets(p_ctx, p_b.streets, p_b.districts.n, p_size);
    drawMainRoads(p_ctx, p_b.districts.roads, p_b.districts.n, p_size);
    if (p_b.houses) drawHouses(p_ctx, p_b.houses, p_b.districts.n, p_size);
    drawHighways(p_ctx, p_b.borders, p_b.spread.n, p_size);
}
