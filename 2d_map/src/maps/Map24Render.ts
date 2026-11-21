// Street Map — makes the world from the settings (ground, roads, districts) and draws it.
import { drawDistrictNames, drawPins, drawRoadNames, drawRoads, drawTrees } from './Map24Draw';
import { makeField } from './Map24Field';
import { paintMap } from './Map24Paint';
import { makeRandom } from './Map24Random';
import { makeRoads, type Road } from './Map24Roads';
import { sampleRoads } from './Map24Samples';
import type { Settings } from './Map24Settings';
import type { World } from './Map24Streets';
import { makeTerrain } from './Map24Terrain';

export type StreetMap = World & { roads: Road[] };

export function makeMap(p_settings: Settings, p_width: number, p_height: number): StreetMap {
    const random = makeRandom(p_settings.seed);
    const terrain = makeTerrain(p_settings, p_width, p_height, random);
    const roads = makeRoads(p_settings, terrain, random);
    const samples = sampleRoads(roads);
    const field = makeField(samples, terrain);
    return { settings: p_settings, terrain, roads, samples, field };
}

export function drawMap(p_ctx: CanvasRenderingContext2D, p_map: StreetMap): void {
    const step = p_map.settings.step;
    const image = p_ctx.createImageData(p_map.terrain.width, p_map.terrain.height);
    paintMap(image, p_map);
    p_ctx.putImageData(image, 0, 0);
    if (step >= 6) drawTrees(p_ctx, p_map);
    if (step >= 2) drawRoads(p_ctx, p_map.roads, step >= 6);
    if (step >= 6) {
        drawRoadNames(p_ctx, p_map.roads, p_map);
        drawDistrictNames(p_ctx, p_map);
        drawPins(p_ctx, p_map.roads, p_map);
    }
}
