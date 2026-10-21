// Combat Sandbox — the TOXIC FACTORY theme (the Drill Works) as six levels, each adding one place mechanic and one toy, the
// numbers climbing as you go. In every level you break its OBJECTIVES (machines), which opens the
// EXIT; some objectives drop a KEY for a locked DOOR, or switch a hazard off when they break.
// SUPPLY boxes break open into joy jars, hearts or fizz (big bubble) ammo.
//
//   1 Loading Bay     crates and pillars                        riflemen
//   2 Conveyor Hall   belts carry everything; crushers at the ends   + bolt drones
//   3 Vent Gallery    vents burst toxic gas on a rhythm               + grenadiers, spotters
//   4 Pipe Works      shoot pipes to spill sludge pools               + wind-up mice, riot toys
//   5 Lift Shaft      toys arrive by lift — break the lifts           everything, gas creeps
//   6 Drill Core      tremors slide the crates; break the drill       everything
//
// Plain data: positions in metres, the arena's top-left is (0, 0).
import type { EnemyKind, Vec } from './Map21State';

export type RectDef = { x: number; y: number; w: number; h: number };
export type CoverDef = RectDef & { low: boolean; slides?: boolean; rides?: boolean }; // slides: tremors move it; rides: a parts crate the belt carries to the press (it comes back at the start)
export type ConveyorDef = RectDef & { dir: Vec; speed: number };
export type CrusherDef = RectDef & { period: number; phase: number };
export type VentDef = { pos: Vec; r: number; period: number; phase: number };
export type PipeDef = { a: Vec; b: Vec; spill: Vec; glue?: boolean }; // along a wall; breaking it spills sludge (or glue) at `spill`
// delay: seconds after the previous wave started (default: when it's cleared). zone: the wave waits
// until you walk into that zone, and the toys arrive at its spawn points.
export type WaveDef = { toys: EnemyKind[]; delay?: number; zone?: number };
// A station along a production route: entering it shows its name and starts its waves.
export type ZoneDef = { name: string; sub: string; rect: RectDef; spawns: Vec[] };
// A paint sprayer on a wall: drips (warning), then sprays a band `len` long along `dir`. Get caught
// and you're painted — every toy knows where you are for a while.
export type SprayerDef = { pos: Vec; dir: Vec; len: number; period: number; phase: number };
// Set dressing that only gets drawn (cages of captured clowns, stuffing piles, box stacks).
// A row of stamp heads across a corridor: n heads side by side along x, each the full height. They
// slam one after another (step seconds apart) every period: a wave that travels along the row.
export type StampDef = RectDef & { n: number; period: number; step: number };
export type PropDef = RectDef & { kind: 'cage' | 'fluff' | 'boxes' };
// A grabber arm: its claw rides a gantry rail along y, patrolling x0 … x1. It snatches anyone
// (you or a toy) standing under it and carries them to its press — crushers[press].
export type ArmDef = { y: number; x0: number; x1: number; press: number; speed: number };
export type KeyColor = 'red' | 'blue';
export type ObjectiveKind = 'generator' | 'terminal' | 'valve' | 'locker' | 'pump';
// stops: what breaking it switches off — the belts (and crushers), the vents near it, the nearest
// lift, or the drill core's shield. drops: a key it leaves behind.
export type ObjectiveDef = { kind: ObjectiveKind; pos: Vec; hp: number; stops?: 'belts' | 'vents' | 'lift' | 'shield' | 'arms' | 'paint' | 'stamps'; drops?: KeyColor };
export type DoorDef = RectDef & { key: KeyColor };

export type LevelDef = {
    name: string;
    tagline: string; // shown when the level starts: what's new
    w: number;
    h: number;
    floor: string; // floor colour (each level a little different)
    cover: CoverDef[];
    pools: { pos: Vec; r: number; glue?: boolean }[]; // glue: slows you right down (no dash) instead of hurting
    conveyors: ConveyorDef[];
    crushers: CrusherDef[];
    vents: VentDef[];
    pipes: PipeDef[];
    lifts: Vec[]; // toys arrive here; empty = they drop in along the top
    arms: ArmDef[];
    core: { pos: Vec; r: number; hp: number } | null; // the drill: break it too (shielded while any 'shield' objective stands)
    objectives: ObjectiveDef[]; // break them all to open the exit
    doors: DoorDef[];
    supplies: Vec[]; // breakable boxes with a pickup inside
    exit: RectDef; // in the outer wall; opens when the objectives are done
    tremor: number; // seconds between tremors (0 = none)
    gas: boolean; // the gas creeps up from the bottom
    start: Vec;
    waves: WaveDef[];
    walls?: RectDef[]; // inner walls (partitions between rooms): solid, block everything
    zones?: ZoneDef[]; // stations along the route (waves can wait for them)
    sprayers?: SprayerDef[];
    props?: PropDef[];
    stamps?: StampDef[];
    traps?: Vec[]; // supply crates with a springer bot hiding inside (they look like the others… almost)
};

// Short names for writing levels (shared by every theme's level file).
export const R = 'rifleman' as const, S = 'spotter' as const, T = 'riot' as const, D = 'diver' as const, G = 'grenadier' as const, M = 'mouse' as const;
export const RG = 'rigger' as const; // comes with its three rig bots
export const W = 'welder' as const, SN = 'sniper' as const, EN = 'engineer' as const, MG = 'magnet' as const, PJ = 'projector' as const, SP = 'springer' as const;
export const mice = (n: number): EnemyKind[] => Array.from({ length: n }, () => M);
export const box = (x: number, y: number, w: number, h: number, low = true, slides = false): CoverDef => ({ x, y, w, h, low, slides });
/** A parts crate riding a belt (Assembly Plant). */
export const rider = (x: number, y: number): CoverDef => ({ x, y, w: 2, h: 1, low: true, rides: true });

export const FACTORY_LEVELS: LevelDef[] = [
    {
        name: 'Loading Bay', tagline: 'Riflemen duck behind crates — shoot them while they aim.',
        w: 28, h: 30, floor: '#1f2a28',
        cover: [box(5, 8, 4, 1), box(19, 8, 4, 1), box(12, 6, 1.2, 3, false), box(4, 15, 1.2, 4, false), box(11, 14, 3, 1),
            box(22, 15, 1.2, 4, false), box(7, 21, 4, 1), box(17, 21, 4, 1), box(13, 24, 2, 2, false)],
        pools: [{ pos: { x: 9, y: 12 }, r: 1.3 }, { pos: { x: 20, y: 12 }, r: 1.1 }],
        conveyors: [], crushers: [], vents: [], pipes: [], lifts: [], arms: [], core: null, tremor: 0, gas: false,
        start: { x: 14, y: 27 },
        objectives: [{ kind: 'generator', pos: { x: 4, y: 4 }, hp: 40 }, { kind: 'generator', pos: { x: 24, y: 4 }, hp: 40 }],
        doors: [], supplies: [{ x: 3, y: 26 }, { x: 25, y: 26 }, { x: 14, y: 11 }],
        exit: { x: 12, y: 0, w: 4, h: 1 },
        waves: [{ toys: [R, R] }, { toys: [R, R, R] }, { toys: [R, R, R, R] }],
    },
    {
        name: 'Conveyor Hall', tagline: 'Belts carry everyone. Crushers wait at the ends. Bolt drones dive.',
        w: 32, h: 32, floor: '#22262b',
        cover: [box(6, 6, 1.2, 3, false), box(25, 6, 1.2, 3, false), box(14, 7, 4, 1), box(6, 25, 4, 1), box(22, 25, 4, 1),
            box(15, 17, 2, 1.2, false), box(5, 1, 1.2, 5.6, false)],
        pools: [],
        conveyors: [{ x: 1, y: 11, w: 27, h: 3, dir: { x: 1, y: 0 }, speed: 3.2 }, { x: 4, y: 20, w: 27, h: 3, dir: { x: -1, y: 0 }, speed: 3.2 }],
        crushers: [{ x: 28, y: 11, w: 3, h: 3, period: 3.2, phase: 0 }, { x: 1, y: 20, w: 3, h: 3, period: 3.2, phase: 1.6 }],
        vents: [], pipes: [], lifts: [], arms: [], core: null, tremor: 0, gas: false,
        start: { x: 16, y: 29 },
        objectives: [
            { kind: 'terminal', pos: { x: 16, y: 3.5 }, hp: 45, stops: 'belts' },
            { kind: 'locker', pos: { x: 29, y: 28 }, hp: 20, drops: 'red' },
        ],
        doors: [{ x: 1, y: 6, w: 4, h: 0.6, key: 'red' }], supplies: [{ x: 9, y: 17 }, { x: 24, y: 17 }, { x: 3, y: 28 }],
        exit: { x: 1, y: 0, w: 4, h: 1 },
        waves: [{ toys: [R, R, D] }, { toys: [R, D, D, R] }, { toys: [R, R, R, D, D] }],
    },
    {
        name: 'Vent Gallery', tagline: 'Vents hiss, then burst. Grenadiers lob bombs — bubble one to bat it back.',
        w: 30, h: 34, floor: '#202a22',
        cover: [box(4, 9, 4, 1), box(22, 9, 4, 1), box(13, 12, 4, 1), box(7, 18, 1.2, 4, false), box(22, 18, 1.2, 4, false),
            box(13, 23, 4, 1), box(5, 27, 4, 1), box(21, 27, 4, 1)],
        pools: [{ pos: { x: 15, y: 18 }, r: 1.2 }],
        conveyors: [], crushers: [],
        vents: [
            { pos: { x: 9, y: 13 }, r: 1.8, period: 4, phase: 0 }, { pos: { x: 21, y: 13 }, r: 1.8, period: 4, phase: 2 },
            { pos: { x: 15, y: 8 }, r: 1.6, period: 5, phase: 1 }, { pos: { x: 11, y: 24 }, r: 1.8, period: 4.5, phase: 3 },
            { pos: { x: 19, y: 24 }, r: 1.8, period: 4.5, phase: 0.8 }, { pos: { x: 15, y: 29 }, r: 1.6, period: 5, phase: 2.5 },
        ],
        pipes: [], lifts: [], arms: [], core: null, tremor: 0, gas: false,
        start: { x: 15, y: 31 },
        objectives: [
            { kind: 'valve', pos: { x: 4, y: 13 }, hp: 30, stops: 'vents' },
            { kind: 'valve', pos: { x: 26, y: 13 }, hp: 30, stops: 'vents' },
            { kind: 'valve', pos: { x: 15, y: 4 }, hp: 30, stops: 'vents' },
        ],
        doors: [], supplies: [{ x: 3, y: 30 }, { x: 27, y: 30 }, { x: 15, y: 16 }],
        exit: { x: 13, y: 0, w: 4, h: 1 },
        waves: [{ toys: [R, R, G] }, { toys: [R, S, G, R] }, { toys: [R, R, G, G, S] }],
    },
    {
        name: 'Pipe Works', tagline: 'Shoot a pipe to spill sludge where you want it. Mice swarm; riot toys charge.',
        w: 32, h: 32, floor: '#262420',
        cover: [box(6, 9, 1.2, 3, false), box(25, 9, 1.2, 3, false), box(12, 13, 8, 1), box(5, 19, 4, 1), box(23, 19, 4, 1),
            box(14, 23, 4, 1), box(15, 7, 2, 1.2, false), box(26, 1, 1.2, 5, false)],
        pools: [],
        conveyors: [], crushers: [], vents: [],
        pipes: [
            { a: { x: 1, y: 6 }, b: { x: 1, y: 12 }, spill: { x: 3.5, y: 9 } }, { a: { x: 31, y: 6 }, b: { x: 31, y: 12 }, spill: { x: 28.5, y: 9 } },
            { a: { x: 1, y: 17 }, b: { x: 1, y: 24 }, spill: { x: 3.5, y: 20.5 } }, { a: { x: 31, y: 17 }, b: { x: 31, y: 24 }, spill: { x: 28.5, y: 20.5 } },
            { a: { x: 10, y: 1 }, b: { x: 22, y: 1 }, spill: { x: 16, y: 4 } },
        ],
        lifts: [], arms: [], core: null, tremor: 0, gas: false,
        start: { x: 16, y: 29 },
        objectives: [
            { kind: 'pump', pos: { x: 5, y: 27 }, hp: 45 }, { kind: 'pump', pos: { x: 27, y: 27 }, hp: 45 },
            { kind: 'locker', pos: { x: 16, y: 10.5 }, hp: 20, drops: 'blue' },
        ],
        doors: [{ x: 27.2, y: 5.4, w: 3.8, h: 0.6, key: 'blue' }], supplies: [{ x: 9, y: 4 }, { x: 22, y: 27 }, { x: 10, y: 27 }],
        exit: { x: 27.2, y: 0, w: 3.8, h: 1 },
        waves: [{ toys: [...mice(5), R, R] }, { toys: [T, ...mice(5), R] }, { toys: [T, ...mice(6), R, S, G] }],
    },
    {
        name: 'Lift Shaft', tagline: 'Toys ride up the lifts. Break a lift and nothing more comes from it. The gas is rising.',
        w: 30, h: 34, floor: '#21232c',
        cover: [box(4, 10, 4, 1), box(22, 10, 4, 1), box(13, 9, 1.2, 3, false), box(8, 16, 1.2, 4, false), box(21, 16, 1.2, 4, false),
            box(12, 19, 6, 1), box(5, 25, 4, 1), box(21, 25, 4, 1)],
        pools: [{ pos: { x: 9, y: 22 }, r: 1.2 }],
        conveyors: [], crushers: [], vents: [{ pos: { x: 15, y: 14 }, r: 1.8, period: 5, phase: 0 }],
        pipes: [{ a: { x: 1, y: 14 }, b: { x: 1, y: 22 }, spill: { x: 3.5, y: 18 } }, { a: { x: 29, y: 14 }, b: { x: 29, y: 22 }, spill: { x: 26.5, y: 18 } }],
        lifts: [{ x: 6, y: 3.5 }, { x: 15, y: 3.5 }, { x: 24, y: 3.5 }], arms: [], core: null, tremor: 0, gas: true,
        start: { x: 15, y: 31 },
        objectives: [
            { kind: 'generator', pos: { x: 3, y: 7 }, hp: 50, stops: 'lift' }, { kind: 'generator', pos: { x: 18.5, y: 7 }, hp: 50, stops: 'lift' },
            { kind: 'generator', pos: { x: 27, y: 7 }, hp: 50, stops: 'lift' },
        ],
        doors: [], supplies: [{ x: 3, y: 30 }, { x: 27, y: 30 }, { x: 15, y: 23 }],
        exit: { x: 0, y: 9, w: 1, h: 3 },
        waves: [
            { toys: [R, R, D] }, { toys: [G, R, ...mice(4)], delay: 14 }, { toys: [T, R, S], delay: 14 },
            { toys: [R, R, D, D, G], delay: 16 }, { toys: [T, T, ...mice(6), S], delay: 16 },
        ],
    },
    {
        name: 'Drill Core', tagline: 'The drill itself. Tremors slide the crates. Break the core.',
        w: 34, h: 34, floor: '#2a2220',
        cover: [box(6, 7, 4, 1, true, true), box(24, 7, 4, 1, true, true), box(5, 15, 1.2, 4, false), box(28, 15, 1.2, 4, false),
            box(8, 25, 4, 1, true, true), box(22, 25, 4, 1, true, true), box(15, 28, 4, 1, true, true)],
        pools: [{ pos: { x: 8, y: 20 }, r: 1.2 }, { pos: { x: 26, y: 20 }, r: 1.2 }],
        conveyors: [], crushers: [],
        vents: [{ pos: { x: 11, y: 11 }, r: 1.6, period: 5, phase: 0 }, { pos: { x: 23, y: 11 }, r: 1.6, period: 5, phase: 2.5 }],
        pipes: [{ a: { x: 1, y: 22 }, b: { x: 1, y: 30 }, spill: { x: 3.5, y: 26 } }, { a: { x: 33, y: 22 }, b: { x: 33, y: 30 }, spill: { x: 30.5, y: 26 } }],
        lifts: [{ x: 5, y: 3.5 }, { x: 29, y: 3.5 }], arms: [], core: { pos: { x: 17, y: 15 }, r: 3, hp: 90 }, tremor: 9, gas: true,
        start: { x: 17, y: 31 },
        objectives: [
            { kind: 'pump', pos: { x: 6, y: 11 }, hp: 60, stops: 'shield' }, { kind: 'pump', pos: { x: 28, y: 11 }, hp: 60, stops: 'shield' },
        ],
        doors: [], supplies: [{ x: 3, y: 30 }, { x: 31, y: 30 }, { x: 17, y: 22 }, { x: 3, y: 8 }],
        exit: { x: 15, y: 0, w: 4, h: 1 },
        waves: [
            { toys: [R, R, S] }, { toys: [T, D, R, ...mice(4)], delay: 15 }, { toys: [G, G, R, D], delay: 15 },
            { toys: [T, R, R, S, ...mice(5)], delay: 16 }, { toys: [T, T, G, D, D, R], delay: 18 },
        ],
    },
];
