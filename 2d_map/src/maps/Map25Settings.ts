// Islands — every number worth tuning (the sliders in Map25.tsx). Lengths are canvas pixels.

export type Settings = {
    seed: number;
    islandSize: number; // the biggest island's size (its falloff reaches about this far); the others are smaller
    channel: number; // narrowest water between two islands (where a bridge crosses)
    noiseScale: number; // size of the noise's biggest features (pixels)
    noiseStrength: number; // how much the noise wins over the falloff (0: plain circles)
    blend: number; // how smoothly a headland melts into its island (pixels)
    showParts: boolean; // draw the falloff circles, headlands and bridge ends
};

export const DEFAULT_SETTINGS: Settings = {
    seed: 4,
    islandSize: 118,
    channel: 36,
    noiseScale: 110,
    noiseStrength: 0.6,
    blend: 16,
    showParts: false,
};
