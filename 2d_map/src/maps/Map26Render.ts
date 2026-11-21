// Colour Blocks — makes the spread, the borders and (from step 5) the districts and (step 6) the
// streets and (step 7) the houses from the settings,
// and draws a step (at a point in the spread's animation: p_progress 0..1).
import { findBorders, type Borders } from './Map26Borders';
import { makeDistricts, type Districts } from './Map26Districts';
import { drawCells, drawDistrictCells, drawGrid, drawHighways, drawHouses, drawMainRoads, drawQueue, drawSeeds, drawStreets } from './Map26Draw';
import { placeHouses, type Houses } from './Map26Houses';
import { makeStreets, type Streets } from './Map26Streets';
import { queueAt, type QueueView } from './Map26Queue';
import { makeNoise, makeRandom } from './Map24Random';
import type { Settings } from './Map26Settings';
import { spread, type Spread } from './Map26Spread';

export type Blocks = { settings: Settings; spread: Spread; borders: Borders; districts: Districts | null; streets: Streets | null; houses: Houses | null };
export type Show = { queue: boolean; ground: boolean }; // step 1 - 2 extras

const DISTRICTS_FROM = 5; // the step the districts appear
const STREETS_FROM = 6;
const HOUSES_FROM = 7;

export function makeBlocks(p_settings: Settings): Blocks {
    const random = makeRandom(p_settings.seed);
    const noise = makeNoise(random);
    const s = spread(p_settings, random, noise);
    const borders = findBorders(s.n, s.owner, p_settings.straighten, p_settings.smoothing);
    // the districts only when shown (they take longer), from their own random numbers
    const districts = p_settings.step >= DISTRICTS_FROM ? makeDistricts(s, borders.highways, p_settings, makeRandom(p_settings.seed * 7919 + 1), noise) : null;
    const streets = districts && p_settings.step >= STREETS_FROM ? makeStreets(districts, borders.highways, p_settings) : null;
    const houses = districts && streets && p_settings.step >= HOUSES_FROM ? placeHouses(districts, streets, borders.highways, p_settings, makeRandom(p_settings.seed * 104729 + 7)) : null;
    return { settings: p_settings, spread: s, borders, districts, streets, houses };
}

// Draws the step; on steps 1 - 2 returns the queue it showed (else null).
export function drawBlocks(p_ctx: CanvasRenderingContext2D, p_b: Blocks, p_progress: number, p_show: Show): QueueView | null {
    const size = p_ctx.canvas.width;
    const step = p_b.settings.step;
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
