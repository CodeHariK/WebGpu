// Wick — every number worth tuning while you play (the sliders in Map22.tsx).
export type Settings = {
    playerSpeed: number; // m/s
    dashDistance: number;
    dashTime: number;
    dashCooldown: number;
    maxWax: number;
    lightMin: number; // light radius at no wax (m)
    lightMax: number; // … and at full wax
    throwCost: number; // wax per flame thrown
    throwRate: number; // flames per second
    flameSpeed: number;
    emberTime: number; // seconds a flame burns where it lands
    flareCost: number;
    flareRadius: number;
    flareCooldown: number;
    lanternRadius: number;
    lanternHeal: number; // wax per second standing by a lit lantern
    shadeSpeed: number;
    shadeHit: number; // wax a shade's lunge takes
    burnRate: number; // damage per second a shade takes in full light
    mothDrain: number; // wax per second per moth on you
    weeperSpeed: number;
    weeperHit: number;
    ambientEvery: number; // seconds between shades rising from the dark (0 = never)
    viewSpan: number; // metres across the screen
    shake: boolean;
};

export const DEFAULT_SETTINGS: Settings = {
    playerSpeed: 6.5,
    dashDistance: 4,
    dashTime: 0.15,
    dashCooldown: 0.45,
    maxWax: 100,
    lightMin: 2.2,
    lightMax: 7,
    throwCost: 4,
    throwRate: 3.5,
    flameSpeed: 14,
    emberTime: 5,
    flareCost: 14,
    flareRadius: 5.5,
    flareCooldown: 2.5,
    lanternRadius: 7,
    lanternHeal: 12,
    shadeSpeed: 4.5,
    shadeHit: 10,
    burnRate: 6,
    mothDrain: 3,
    weeperSpeed: 5,
    weeperHit: 30,
    ambientEvery: 7,
    viewSpan: 24,
    shake: true,
};
