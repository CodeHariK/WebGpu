// Lockpick — every number worth tuning while you play (the sliders in Map23.tsx).
export type Settings = {
    walkSpeed: number; // m/s
    sneakSpeed: number;
    runSpeed: number;
    waterSlow: number; // fraction of your speed in water
    sightRange: number; // how far a guard sees you in full light (m)
    darkSight: number; // … and in the dark
    sightAngle: number; // half the cone (degrees)
    noticeRate: number; // how fast a guard's alert fills while it sees you
    patrolSpeed: number;
    chaseSpeed: number;
    houndSpeed: number;
    torchRadius: number;
    throwRange: number;
    pebbleNoise: number; // radius a landing pebble is heard (m)
    viewSpan: number;
    showCones: boolean;
};

export const DEFAULT_SETTINGS: Settings = {
    walkSpeed: 3.6,
    sneakSpeed: 1.8,
    runSpeed: 5.6,
    waterSlow: 0.6,
    sightRange: 8,
    darkSight: 2.4,
    sightAngle: 42,
    noticeRate: 1.3,
    patrolSpeed: 1.7,
    chaseSpeed: 4.4,
    houndSpeed: 3.9,
    torchRadius: 5.5,
    throwRange: 9,
    pebbleNoise: 7,
    viewSpan: 22,
    showCones: true,
};
