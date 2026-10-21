// Lockpick — the Old Keep's dungeon, one level built from rooms (rectangles carved out of solid
// rock), doors between them, and what's in each. 1 tile = 1 m; (0, 0) is the top-left.
//
//      ┌ Barracks ┐┌──── Watch Room ────┐┌ Treasury ┐
//      │ sleepers ├┤ watcher, pillars,  ├┤ GATE KEY │   (treasury door: IRON key)
//      │ IRON KEY ││ alarm plates       ││ creaky   │
//      └────┬─────┘└──────┬─────────────┘└────┬─────┘
//      ┌────┴── Guard Hall ───┐┌─ Kennel ─┐┌─ Exit Hall ─┐
//      │ torches, barrels,    ├┤ hound,   ├┤ two guards, ├→ EXIT (GATE key)
//      │ creaky boards        ││ water    ││ puddles     │
//      └────┬─────────────────┘└──────────┘└─────────────┘
//      ┌ Cell ┐ (you start here)
//
// Two ways round: the Barracks → Watch Room → Treasury, or the Kennel → Exit Hall → back up to the
// Treasury. Either way you need the iron key from the barracks first.
import type { GuardKind, KeyKind, Vec } from './Map23State';

export type Rect = { x: number; y: number; w: number; h: number };
export type GuardDef = { kind: GuardKind; at: Vec; route?: Vec[]; sweep?: [number, number] };

export type LevelDef = {
    name: string;
    w: number;
    h: number;
    rooms: (Rect & { name: string })[]; // carved floors (named ones show their name when you walk in)
    halls: Rect[]; // short corridors (carved floor, no name)
    doors: (Vec & { lock?: KeyKind; exit?: boolean })[];
    pillars: Vec[]; // solid tiles inside rooms
    creak: Rect[]; // old boards: loud unless you sneak
    glass: Rect[]; // broken glass: loud even sneaking
    water: Rect[]; // shallow water: slow, splashy — and the hound loses your scent in it
    plates: Vec[]; // alarm plates: step on one and a bell rings
    barrels: Vec[];
    torches: (Vec & { dir: Vec })[]; // a torch on the wall at (x, y), shining toward dir
    keys: (Vec & { key: KeyKind })[];
    pebbles: Vec[]; // a handful of pebbles to pick up
    start: Vec;
    outside: Vec; // the tile past the exit door
    guards: GuardDef[];
};

const r = (x: number, y: number, w: number, h: number) => ({ x, y, w, h });
const v = (x: number, y: number) => ({ x, y });

export const KEEP: LevelDef = {
    name: 'The Old Keep',
    w: 48, h: 34,
    rooms: [
        { name: 'YOUR CELL', ...r(2, 26, 6, 6) },
        { name: 'THE GUARD HALL', ...r(2, 17, 20, 8) },
        { name: 'THE KENNEL', ...r(23, 17, 10, 8) },
        { name: 'THE BARRACKS', ...r(2, 5, 12, 11) },
        { name: 'THE WATCH ROOM', ...r(15, 4, 16, 10) },
        { name: 'THE TREASURY', ...r(32, 4, 10, 8) },
        { name: 'THE EXIT HALL', ...r(34, 14, 12, 10) },
    ],
    halls: [r(25, 14, 2, 2), r(37, 13, 2, 1)],
    doors: [
        v(4, 25), // cell → guard hall
        v(22, 20), // guard hall → kennel
        v(6, 16), // guard hall → barracks
        v(14, 9), // barracks → watch room
        v(25, 16), // kennel → watch room
        { x: 31, y: 9, lock: 'iron' }, // watch room → treasury
        v(37, 12), // treasury → exit hall
        v(33, 20), // kennel → exit hall
        { x: 46, y: 19, lock: 'gate', exit: true }, // the way out
    ],
    pillars: [v(19, 7), v(26, 7), v(19, 10), v(26, 10)],
    creak: [r(9, 19, 4, 3), r(32, 4, 10, 8), r(16, 20, 3, 2)],
    glass: [r(5, 10, 5, 2)],
    water: [r(27, 17, 2, 8), r(38, 18, 3, 3)],
    plates: [v(17, 12), v(22, 12), v(23, 12), v(28, 5), v(28, 6), v(20, 5)],
    barrels: [v(6, 19), v(14, 22), v(19, 18), v(23, 24), v(13, 14), v(33, 11), v(41, 11), v(35, 16), v(45, 23)],
    torches: [
        { x: 11, y: 16, dir: v(0, 1) }, { x: 18, y: 16, dir: v(0, 1) }, { x: 22, y: 3, dir: v(0, 1) },
        { x: 36, y: 3, dir: v(0, 1) }, { x: 40, y: 13, dir: v(0, 1) }, { x: 46, y: 16, dir: v(-1, 0) }, { x: 1, y: 21, dir: v(1, 0) },
    ],
    keys: [{ x: 12, y: 6, key: 'iron' }, { x: 40, y: 5, key: 'gate' }],
    pebbles: [v(3, 14), v(31, 18)],
    start: v(4, 29),
    outside: v(47, 19),
    guards: [
        { kind: 'guard', at: v(3, 20), route: [v(3, 20), v(20, 20), v(20, 23), v(3, 23)] },
        { kind: 'hound', at: v(30, 21), route: [v(24, 18), v(31, 18), v(31, 23), v(24, 23)] },
        { kind: 'sleeper', at: v(4, 7) }, { kind: 'sleeper', at: v(9, 7) }, { kind: 'sleeper', at: v(11, 12) },
        { kind: 'watcher', at: v(22, 8), sweep: [0.2, 2.9] },
        { kind: 'guard', at: v(33, 5), route: [v(33, 5), v(40, 5), v(40, 10), v(33, 10)] },
        { kind: 'guard', at: v(35, 15), route: [v(35, 15), v(44, 15), v(44, 22), v(36, 22)] },
        { kind: 'guard', at: v(39, 22), route: [v(39, 22), v(39, 15), v(42, 17), v(42, 21)] },
    ],
};
