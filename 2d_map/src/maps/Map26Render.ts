// Colour Blocks — makes the spread and the borders from the settings, and draws a step (at a point
// in the spread's animation: p_progress 0..1).
import { findBorders, type Borders } from './Map26Borders';
import { drawCells, drawGrid, drawHighways, drawQueue, drawSeeds } from './Map26Draw';
import { queueAt, type QueueView } from './Map26Queue';
import { makeNoise, makeRandom } from './Map24Random';
import type { Settings } from './Map26Settings';
import { spread, type Spread } from './Map26Spread';

export type Blocks = { settings: Settings; spread: Spread; borders: Borders };
export type Show = { queue: boolean; ground: boolean }; // step 1 - 2 extras

export function makeBlocks(p_settings: Settings): Blocks {
    const random = makeRandom(p_settings.seed);
    const s = spread(p_settings, random, makeNoise(random));
    return { settings: p_settings, spread: s, borders: findBorders(s.n, s.owner, p_settings.straighten, p_settings.smoothing) };
}

// Draws the step; on step 2 returns the queue it showed (else null).
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
    if (step >= 4) drawHighways(p_ctx, p_b.borders, p_b.spread.n, size);
    if (step <= 3) drawSeeds(p_ctx, p_b.spread, size, early);
    return queue;
}
