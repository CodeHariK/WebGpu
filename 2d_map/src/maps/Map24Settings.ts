// Street Map — every number worth tuning (the sliders in Map24.tsx) and the build steps.
// Lengths are canvas pixels.

export type Settings = {
    seed: number;
    step: number; // 1 .. STEPS.length: how much of the map is built
    boulevards: number; // long main roads across the map
    freeway: boolean;
    block: number; // a block's usual size (px)
    hills: number; // share of the map that is hills (0..1)
    lakes: number;
    lakeSize: number; // a lake's radius (px)
    gaps: number; // share of street pieces left out (bigger, irregular blocks)
};

export const DEFAULT_SETTINGS: Settings = {
    seed: 3,
    step: 6,
    boulevards: 7,
    freeway: true,
    block: 22,
    hills: 0.22,
    lakes: 2,
    lakeSize: 36,
    gaps: 0.14,
};

export const STEPS: { title: string; text: string }[] = [
    {
        title: 'Ground',
        text: 'A height map from noise: the highest part (the hills slider) is green hills. Lakes go in low, flat places, each a wobbly oval.',
    },
    {
        title: 'Boulevards',
        text: 'Each boulevard is a walker: it starts at the map\'s edge (or branches off another boulevard), heads for a point on the far side, wobbles gently, and steers round hills and lakes it sees ahead. Long, smooth, never zig-zag.',
    },
    {
        title: 'Districts',
        text: 'Every point belongs to its nearest boulevard, on one side or the other: each side of each boulevard is a district (tinted). Found for the whole map at once with a jump flood. A street runs where two districts meet.',
    },
    {
        title: 'Streets',
        text: 'Each district fills with streets lined up with its boulevard: some run alongside it (at fixed distances from it), some straight out from it (at fixed distances along it). Every district has its own block size and offset, so where two meet the streets don\'t line up: T-junctions. A few street pieces are left out: bigger, irregular blocks.',
    },
    {
        title: 'Hills and lakes',
        text: 'No grid in the hills: winding roads along the hills\' height lines instead, broken up so they don\'t all ring the hill. Round each lake, a park and a road.',
    },
    {
        title: 'The look',
        text: 'Boulevards as double lines, the freeway wider, trees on the hills and round the lakes, names along the boulevards and on the districts, a few pins.',
    },
];
