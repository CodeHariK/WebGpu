// Enemy Arena — the data: player, enemies, noises, slots, tokens. Behaviour lives in the other
// files (Map20Enemy, Map20Group, Map20Player); this is only what they read and write.
import type { V } from './Map20Math';
import type { Kind } from './Map20Arena';
import type { Settings } from './Map20Settings';
import type { World } from './Map20World';

/**
 * patrol      walking its loop (half speed), pausing at each point
 * suspicious  the meter is filling or it heard something: turns to look, walks over slowly
 * fight       knows where you are: melee goes to its slot on the ring, ranged to cover / range
 * windup      melee with a token: crouches (telegraph), then lunges
 * lunge       a fast dash at where you were; hits if it reaches you
 * recover     after a lunge: a short pause, the token goes back
 * aim         ranged: a laser line for windupTime, then the shot
 * stagger     you hit it: knocked back, can't act
 * retreat     hurt: backs away from you for a while
 * search      lost you: goes to the last seen spot and looks around
 * return      walks home, then patrols again
 */
export type EnemyState =
    | 'patrol' | 'suspicious' | 'fight' | 'windup' | 'lunge' | 'recover'
    | 'aim' | 'stagger' | 'retreat' | 'search' | 'return' | 'dead';

export type Enemy = {
    id: number;
    kind: Kind;
    pos: V;
    vel: V;
    facing: number; // radians
    home: V;
    patrol: V[];
    patrolIndex: number;
    state: EnemyState;
    timer: number; // seconds in the current state
    thinkLeft: number; // seconds to the next decision
    meter: number; // 0..1 detection
    lastSeen: V | null; // where it last saw (or was told) you were
    unseen: number; // seconds since it (or a squadmate) last saw you
    goal: V | null; // where it's walking to
    slot: number; // index into the slot ring, −1 = none
    hasToken: boolean;
    lungeDir: V;
    aim: V; // ranged: where the laser points
    hitDone: boolean;
    hp: number;
    bark: string;
    barkLeft: number;
};

export type Player = { pos: V; facing: number; hp: number; attackLeft: number; swingLeft: number; hurtLeft: number; sneaking: boolean };

export type Noise = { pos: V; radius: number; age: number };
export type Shot = { from: V; to: V; age: number; hit: boolean };
export type Slot = { pos: V; valid: boolean; taken: number }; // taken = enemy id or −1

export type Tokens = { holders: number[]; resting: number[] };

export type Input = { up: boolean; down: boolean; left: boolean; right: boolean; sneak: boolean; mouse: V; attack: boolean; shout: boolean };

export type Sim = {
    world: World;
    settings: Settings;
    player: Player;
    enemies: Enemy[];
    noises: Noise[];
    shots: Shot[];
    slots: Slot[];
    tokens: Tokens;
    time: number;
    rand: () => number;
    stats: { hitsTaken: number; hitsGiven: number; kills: number };
};

export const ENEMY_RADIUS = 0.45;
export const PLAYER_RADIUS = 0.4;
