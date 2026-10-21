// Wick — the Drowned Valley: one level made of winding paths, not a box. The paths are splines
// (control points with a half-width each), sampled into short pieces; the clearings are circles on
// them. Everything off the paths is black water and fog: the shadow things live out there.
//
//   start ─ path ─ ① Old Crossing ─ fog ─ bridge ─ ② Reed Hollow ─ fog ─ ③ the Chapel (end)
//                         └─ side path ─ the Hermit's Lamp (a lantern, spare candles)
//
// Each clearing has unlit lanterns. Walking in wakes it: shapes rise out of the fog in waves. Light
// every lantern there (throw a flame at one, or stand at it) and beat its waves, and the FOG WALL
// across the path ahead parts. Lit lanterns are safe light, refill your wax, and are where you come
// back if your flame goes out. Light the chapel's lanterns to finish.
//
// Plain data: positions in metres; the world's top-left is (0, 0).
import type { EnemyKind, Vec } from './Map22State';

export type PathDef = { pts: (Vec & { w: number })[] }; // w: half-width at that point
export type ClearingDef = {
    name: string;
    sub: string;
    pos: Vec;
    r: number;
    lanterns: Vec[];
    waves: EnemyKind[][];
    gates: (Vec & { r: number })[]; // fog walls across the paths out of it
    end?: boolean; // finishing this clearing finishes the level
};
export type LevelDef = {
    name: string;
    w: number;
    h: number;
    start: Vec;
    paths: PathDef[];
    clearings: ClearingDef[];
    lanterns: Vec[]; // lanterns along the paths (rest stops)
    candles: Vec[]; // spare candles: wax pickups
    hands: Vec[]; // grabbers waiting just off the path: drowned hands in water, roots in the woods
    chapel: Vec; // where the landmark at the end stands (drawn)
    ground: 'water' | 'woods'; // what's off the paths
    landmark: 'chapel' | 'belltower';
    winText: string;
    gusts: GustDef[];
    vats: (Vec & { r: number })[]; // pools of warm wax: they glow, and refill you while you wade
};
// A gust of wind across an open stretch: rustling leaves (the tell), then it blows for a while —
// pushing you and your flames along `dir` and bending your flame down (less light).
export type GustDef = { pos: Vec; r: number; dir: Vec; period: number; phase: number };

const p = (x: number, y: number, w: number) => ({ x, y, w });
const S = 'shade' as const, M = 'moth' as const, W = 'weeper' as const;
const SN = 'snuffer' as const, G = 'gloom' as const, L = 'leech' as const;

export const VALLEY: LevelDef = {
    name: 'The Drowned Valley',
    w: 90, h: 80,
    start: { x: 7, y: 71 },
    paths: [
        { // the main way: start → Old Crossing → bridge → Reed Hollow → the Chapel
            pts: [
                p(4, 74, 1.8), p(7, 71, 1.8), p(12, 68, 1.8), p(18, 64, 1.9), p(23, 58, 2), p(28, 50, 2.5),
                p(34, 44, 1.8), p(40, 41, 1.7), p(46, 40, 1.6), p(50, 41, 0.95), p(55, 42, 0.9), p(60, 41, 0.9),
                p(64, 39.5, 1.6), p(73, 33, 2.5), p(79, 27, 1.7), p(81, 21, 1.8), p(79, 15, 1.8), p(74, 11, 1.9),
                p(67, 9.5, 2), p(56, 9, 2.5), p(52, 7, 2),
            ],
        },
        { // the side path to the Hermit's Lamp
            pts: [p(40, 41, 1.5), p(41, 46, 1.4), p(44, 52, 1.3), p(49, 56, 1.3), p(54, 58, 1.5), p(58, 60, 2)],
        },
    ],
    clearings: [
        {
            name: 'THE OLD CROSSING', sub: 'light both lanterns', pos: { x: 28, y: 50 }, r: 7,
            lanterns: [{ x: 23.5, y: 47 }, { x: 32.5, y: 54 }],
            waves: [[S, S], [S, S, M, M, M], [L, L, S]],
            gates: [{ x: 34.5, y: 43.6, r: 2.6 }],
        },
        {
            name: 'REED HOLLOW', sub: 'it only moves in the dark', pos: { x: 73, y: 33 }, r: 8,
            lanterns: [{ x: 67.5, y: 31 }, { x: 77, y: 38.5 }, { x: 76.5, y: 28 }],
            waves: [[S, S, M, M, M], [SN, S, S], [W, G], [S, S, L, L, M, M]],
            gates: [{ x: 79.3, y: 26.6, r: 2.5 }],
        },
        {
            name: 'THE CHAPEL', sub: 'light the chapel and the valley is yours', pos: { x: 56, y: 9 }, r: 9,
            lanterns: [{ x: 49, y: 11 }, { x: 58, y: 16 }, { x: 63, y: 5.5 }],
            waves: [[S, S, S, M, M, M, M, L, L], [W, SN, S, S], [W, G, M, M, M, M, S, S, SN]],
            gates: [], end: true,
        },
        {
            name: 'THE HERMIT’S LAMP', sub: 'a quiet place, spare candles', pos: { x: 58, y: 60 }, r: 4.5,
            lanterns: [{ x: 60.5, y: 62.5 }],
            waves: [],
            gates: [],
        },
    ],
    lanterns: [{ x: 14.5, y: 68 }, { x: 45, y: 37.8 }, { x: 82.5, y: 19 }],
    candles: [{ x: 56, y: 61.5 }, { x: 59.5, y: 57.8 }, { x: 18, y: 61.5 }],
    hands: [{ x: 22, y: 64 }, { x: 52.5, y: 43.45 }, { x: 57.75, y: 39.8 }, { x: 83.55, y: 23 }, { x: 75, y: 14.7 }, { x: 44.1, y: 47.75 }],
    chapel: { x: 56, y: 1.5 },
    ground: 'water', landmark: 'chapel', winText: 'THE VALLEY IS LIT', gusts: [], vats: [],
};
