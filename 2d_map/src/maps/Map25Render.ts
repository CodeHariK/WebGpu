// Islands — makes the islands from the settings (layout, parts, shape) and draws them.
import { drawBridges, drawParts } from './Map25Draw';
import { makeLayout, type Layout } from './Map25Layout';
import { paintIslands } from './Map25Paint';
import { makeParts, type Parts } from './Map25Parts';
import { makeNoise, makeRandom } from './Map24Random';
import type { Settings } from './Map25Settings';
import { makeShape, type Shape } from './Map25Shape';

export type Islands = { settings: Settings; layout: Layout; parts: Parts[]; shape: Shape };

export function makeIslands(p_settings: Settings, p_width: number, p_height: number): Islands {
    const random = makeRandom(p_settings.seed);
    const layout = makeLayout(p_settings.islandSize, p_settings.channel, p_width, p_height, random);
    const parts = layout.islands.map((i) => makeParts(i, layout, p_settings, random));
    const shape = makeShape(p_width, p_height, layout, parts, p_settings, makeNoise(random));
    return { settings: p_settings, layout, parts, shape };
}

export function drawIslands(p_ctx: CanvasRenderingContext2D, p_islands: Islands): void {
    const image = p_ctx.createImageData(p_ctx.canvas.width, p_ctx.canvas.height);
    paintIslands(image, p_islands.shape);
    p_ctx.putImageData(image, 0, 0);
    drawBridges(p_ctx, p_islands.layout, p_islands.parts);
    if (p_islands.settings.showParts) drawParts(p_ctx, p_islands.parts);
}
