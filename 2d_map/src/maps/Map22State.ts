// Wick — the shapes of everything in the simulation (plain data, no behaviour).
import type { ClearingDef, LevelDef } from './Map22Level';
import type { Settings } from './Map22Settings';

export type Vec = { x: number; y: number };

export type Input = {
    up: boolean;
    down: boolean;
    left: boolean;
    right: boolean;
    throw: boolean; // left mouse held: throw flames
    dash: boolean; // Space / Shift pressed (consumed)
    flare: boolean; // right mouse / E pressed (consumed)
    mouse: Vec; // metres
};

// The path, sampled from the level's splines: short straight pieces with a half-width at each end.
export type Seg = { a: Vec; b: Vec; wa: number; wb: number };

export type Player = {
    pos: Vec;
    vel: Vec;
    aim: number;
    wax: number; // health, ammo and light all at once
    throwCd: number;
    dashT: number;
    dashDir: Vec;
    dashCd: number;
    iframes: number;
    hurtT: number;
    flareCd: number;
    flareT: number; // > 0 while the flare burst shines
    kindle: number; // seconds spent touching an unlit lantern
    held: number; // id of whatever has hold of you (0: nothing) — a drowned hand or a snuffer
    struggle: number; // dashes while held
};

export type EnemyKind = 'shade' | 'moth' | 'weeper' | 'snuffer' | 'hand' | 'gloom' | 'leech';

export type Enemy = {
    id: number;
    kind: EnemyKind;
    pos: Vec;
    vel: Vec;
    hp: number;
    maxHp: number;
    state: string;
    t: number;
    facing: number;
    flash: number;
    spawnT: number; // rising out of the fog
    lit: number; // how much light it stands in (0 … 1), for drawing
    target: Vec | null;
    clearing: number; // which clearing sent it (-1: the dark itself)
    eaten: number; // a leech: wax blobs it has swallowed (it grows)
    home: Vec | null; // a drowned hand: where it waits under the water
};

// Wax knocked off you: blobs on the ground. Walk over them to get it back — if a leech doesn't first.
export type Blob = { pos: Vec; vel: Vec; wax: number; t: number };
export type Flame = { pos: Vec; vel: Vec; life: number };
export type Ember = { pos: Vec; life: number; maxLife: number };
export type Lantern = { pos: Vec; lit: boolean; litT: number; clearing: number };
export type Clearing = { def: ClearingDef; entered: boolean; done: boolean; wave: number; waveT: number };

export type Particle = { pos: Vec; vel: Vec; life: number; maxLife: number; size: number; color: string; kind: 'spark' | 'smoke' | 'dot' | 'ring' | 'drip' };
export type Floater = { pos: Vec; text: string; color: string; life: number; size: number };

export type View = { x: number; y: number; span: number };
export type Phase = 'play' | 'snuffed' | 'won';

export type Sim = {
    settings: Settings;
    level: LevelDef;
    segs: Seg[];
    view: View;
    time: number;
    phase: Phase;
    phaseT: number;
    player: Player;
    checkpoint: Vec; // the last lantern you lit: where you come back if your flame goes out
    enemies: Enemy[];
    flames: Flame[];
    embers: Ember[];
    blobs: Blob[];
    lanterns: Lantern[];
    candles: { pos: Vec; taken: boolean }[];
    clearings: Clearing[];
    particles: Particle[];
    floaters: Floater[];
    shake: number;
    stop: number;
    windLight: number; // your light's size in the wind (1: still air; less while a gust blows on you)
    ambientT: number; // seconds until the dark sends another shade
    nextId: number;
    banner: { text: string; sub: string; life: number } | null;
    stats: { shades: number; moths: number; weepers: number; snuffed: number; others: number };
};
