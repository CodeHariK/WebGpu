// Combat Sandbox — the shapes of everything in the simulation (plain data, no behaviour).
import type { ArmDef, CrusherDef, DoorDef, SprayerDef, StampDef, KeyColor, LevelDef, ObjectiveDef, PipeDef, VentDef } from './Map21Levels';
import type { Settings } from './Map21Settings';

export type Vec = { x: number; y: number };

export type Input = {
    up: boolean;
    down: boolean;
    left: boolean;
    right: boolean;
    fire: boolean; // left mouse held
    dash: boolean; // Space / Shift pressed (consumed)
    pop: boolean; // right mouse / E pressed (consumed)
    fizz: boolean; // Q pressed (consumed): the big bubble
    mouse: Vec; // metres
};

// A grabber arm at runtime. The claw is at (x, def.y); `who` is what it holds (or is reaching for).
export type ArmState = 'patrol' | 'lower' | 'carry' | 'recover' | 'jam' | 'off';
export type Arm = { def: ArmDef; x: number; dir: number; state: ArmState; t: number; who: 'player' | number | null; hp: number; flash: number };

export type Rect = { x: number; y: number; w: number; h: number; low: boolean; slides?: boolean; rides?: boolean }; // low cover: shoot over it; rides: a crate on a belt

// The level as it is now (the LevelDef plus everything that changes while you play).
export type Arena = {
    def: LevelDef;
    w: number;
    h: number;
    cover: Rect[]; // tremors slide some of it
    pools: { pos: Vec; r: number; grow: number; glue?: boolean }[]; // grow: 0 … 1 (spilled pools spread)
    crushers: (CrusherDef & { t: number })[]; // t: time in its cycle
    vents: (VentDef & { t: number; off: boolean })[]; // off: its valve is broken
    pipes: (PipeDef & { hp: number; broken: boolean })[];
    lifts: { pos: Vec; hp: number; broken: boolean; busy: number }[]; // busy: seconds of the arrival animation
    core: { pos: Vec; r: number; hp: number; maxHp: number; flash: number } | null;
    objectives: (ObjectiveDef & { hp: number; maxHp: number; broken: boolean; flash: number })[];
    doors: (DoorDef & { open: number })[]; // open: 0 shut … 1 slid away (solid while 0)
    supplies: { pos: Vec; hp: number; broken: boolean; flash: number; trap: boolean }[]; // trap: a springer inside
    exitOpen: boolean;
    arms: Arm[];
    sprayers: (SprayerDef & { t: number; off: boolean })[];
    stamps: (StampDef & { t: number; off: boolean })[]; // off: the paint feed is broken
    armsOn: boolean; // until their terminal breaks
    beltsOn: boolean; // the belts (and crushers) run until their terminal breaks
    tremorT: number; // seconds until the next tremor (warning shows in the last second)
    decals: { pos: Vec; r: number; kind: number; rot: number }[]; // stains, cracks, bolts (drawn only)
};

export type Player = {
    pos: Vec;
    vel: Vec; // knockback / dash carry / belts
    aim: number; // radians
    hp: number;
    joy: number;
    fireCd: number;
    dashT: number; // > 0 while dashing
    dashDir: Vec;
    dashCd: number;
    iframes: number;
    hurtT: number; // invincible after a hit
    popCd: number;
    perfectArmed: boolean; // this dash hasn't earned a perfect yet
    recoil: number; // visual kick
    keys: KeyColor[];
    held: boolean; // in a grabber arm's claw
    paintT: number; // seconds left painted (every toy sees you)
    struggle: number; // dashes while held (enough and you wriggle free)
    fizz: number; // big bubbles left (Q)
};

export type EnemyKind = 'rifleman' | 'spotter' | 'riot' | 'diver' | 'grenadier' | 'mouse' | 'rigger' | 'rigbot' | 'springer'
    | 'welder' | 'sniper' | 'engineer' | 'turret' | 'magnet' | 'projector';

export type Enemy = {
    id: number;
    kind: EnemyKind;
    pos: Vec;
    vel: Vec; // knockback
    hp: number;
    maxHp: number;
    facing: number;
    state: string; // per kind (see Map21Enemies)
    t: number; // time in the current state
    target: Vec | null; // where it's going
    shots: number; // shots left in a burst
    bursts: number; // bursts since the last wind-up
    flash: number; // white flash time
    stagger: number; // interrupted (shots / tell) time
    spawnT: number; // arriving: invulnerable, can't act
    held?: boolean; // in a grabber arm's claw: can't act
    owner?: number; // a rig bot: its rigger · a turret: its engineer · a projector: the toy it guards
    hop?: { from: Vec; to: Vec }; // a springer mid-hop
    aim?: Vec; // a sniper's laser point (it trails you)
    shielded?: boolean; // a projector drone's shield is on it (set every step)
};

export type Shot = {
    pos: Vec;
    vel: Vec;
    life: number; // seconds left
    damage: number;
    friendly: boolean; // the player's bubble
    radius: number;
    origin: Vec; // where it was fired: shots from right behind low cover fly over it
    big?: boolean; // a fizz bubble: bursts in a blast when it stops
};

// A grenadier's bomb: flies in an arc from `from` to `to` over `flight` seconds, then bursts.
export type Bomb = { from: Vec; to: Vec; t: number; flight: number; owner: number; batted: boolean };

// Things lying on the floor: a joy jar, a heart, a fizz can (big-bubble ammo) or a key.
export type PickupKind = 'joy' | 'heart' | 'fizz' | 'key';
export type Pickup = { pos: Vec; vel: Vec; kind: PickupKind; color?: KeyColor; t: number };

export type Spirit = { pos: Vec; vel: Vec; wait: number }; // joy from a freed clown, flies to the player

export type Particle = {
    pos: Vec;
    vel: Vec;
    life: number;
    maxLife: number;
    size: number;
    color: string;
    kind: 'square' | 'ring' | 'spark' | 'dot' | 'smoke';
    spin: number;
};

export type Floater = { pos: Vec; text: string; color: string; life: number; size: number };

export type Juice = {
    shake: number; // trauma 0 … 1 (shake = trauma²)
    stop: number; // seconds of hit-stop left
    slow: number; // seconds of slow-motion left (real time)
    kick: Vec; // view kick from recoil
};

// The camera: centred on (x, y), showing `span` metres across (square view).
export type View = { x: number; y: number; span: number };

export type Phase = 'intro' | 'fight' | 'clear' | 'over';

export type Sim = {
    settings: Settings;
    view: View;
    arena: Arena;
    theme: number; // index into THEMES
    level: number; // index into the theme's levels
    phase: Phase;
    phaseT: number; // seconds in this phase
    time: number;
    player: Player;
    enemies: Enemy[];
    shots: Shot[];
    bombs: Bomb[];
    spirits: Spirit[];
    pickups: Pickup[];
    particles: Particle[];
    floaters: Floater[];
    juice: Juice;
    marked: boolean; // a spotter's light is on the player
    gasY: number; // the gas line (metres from the top); everything below it is gas
    wave: number; // waves started in this level
    zonesEntered: boolean[]; // stations you've walked into (zoned levels)
    waveT: number; // seconds since the last wave started
    nextId: number;
    stats: { kills: number; perfects: number; hitsTaken: number; deaths: number; bestStreak: number; streak: number; collected: number };
    banner: { text: string; sub: string; life: number } | null;
};
