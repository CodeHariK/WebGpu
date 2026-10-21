// Lockpick — the shapes of everything in the simulation (plain data, no behaviour).
import type { LevelDef } from './Map23Level';
import type { Settings } from './Map23Settings';

export type Vec = { x: number; y: number };

export type Input = {
    up: boolean;
    down: boolean;
    left: boolean;
    right: boolean;
    sneak: boolean; // Shift held: slow and quiet
    run: boolean; // Space held: fast and loud
    use: boolean; // E pressed (consumed): doors, torches, barrels, keys
    throw: boolean; // click (consumed): throw a pebble at the mouse
    mouse: Vec;
};

// Floor kinds: what a tile is (walls are solid and block sight; the rest are floors that sound
// different when you cross them).
export const WALL = 0, FLOOR = 1, CREAK = 2, GLASS = 3, WATER = 4, PLATE = 5;
export type Tile = typeof WALL | typeof FLOOR | typeof CREAK | typeof GLASS | typeof WATER | typeof PLATE;

export type KeyKind = 'iron' | 'gate';
export type Door = { x: number; y: number; open: boolean; lock: KeyKind | null; shutT: number };
export type Torch = { x: number; y: number; light: Vec; lit: boolean }; // x, y: its wall tile; light: where it shines from
export type Item = { pos: Vec; kind: 'key' | 'pebbles'; key?: KeyKind; taken: boolean };

export type Player = {
    pos: Vec;
    facing: number;
    hidden: number; // index of the barrel you're in (-1: not hiding)
    pebbles: number;
    keys: KeyKind[];
    stepT: number; // time to the next footstep sound
    scentT: number;
    moving: boolean;
    mode: 'walk' | 'sneak' | 'run';
};

export type GuardKind = 'guard' | 'sleeper' | 'watcher' | 'hound';

export type Guard = {
    id: number;
    kind: GuardKind;
    pos: Vec;
    facing: number;
    state: string; // per kind (see Map23Guards, Map23Hound)
    t: number;
    route: Vec[]; // patrol waypoints (tile centres)
    routeI: number;
    path: Vec[]; // the tiles it's walking now
    goal: Vec | null; // where it's going (investigating, chasing, relighting…)
    alert: number; // 0 … 1: how sure it is it saw someone (≥ 1: it knows)
    lastSeen: Vec | null;
    home: Vec; // sleepers' bunk, watchers' post
    sweep: [number, number]; // watchers: the arc the lantern sweeps (radians)
    repath: number;
};

export type Noise = { pos: Vec; r: number; t: number; loud: boolean }; // t: age (for the ring)
export type Pebble = { from: Vec; to: Vec; t: number; flight: number };
export type Scent = { pos: Vec; t: number }; // the hound follows these (time laid)

export type Floater = { pos: Vec; text: string; color: string; life: number };

export type Sim = {
    settings: Settings;
    level: LevelDef;
    w: number;
    h: number;
    tiles: Tile[];
    doors: Door[];
    barrels: Vec[]; // tile coords (solid; you can hide in one)
    torches: Torch[];
    light: number[]; // per tile 0 … 1 (recomputed when a torch or door changes)
    lightDirty: boolean;
    items: Item[];
    time: number;
    phase: 'play' | 'caught' | 'escaped';
    phaseT: number;
    player: Player;
    checkpoint: Vec;
    guards: Guard[];
    noises: Noise[];
    pebbles: Pebble[];
    scent: Scent[];
    floaters: Floater[];
    view: { x: number; y: number; span: number };
    banner: { text: string; sub: string; life: number } | null;
    stats: { caught: number; spotted: number; noises: number; time: number };
    nextId: number;
    exposed: number; // the barrel the hound barked at (-1: none)
    roomName: string; // the room you're in (its name shows when you walk in)
};
