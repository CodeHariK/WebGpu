// Wick — level 2, the Wax Works: the old candle-makers' village up a wooded hill. Off the paths is
// the black wood (roots grab from its edge). The path climbs in switchbacks and forks:
//
//   start ─ lane ─ ① Market Square ─┬─ west lane ──────────┬─ ② the Chandlery ─ fog ─ switchbacks
//                                    └─ east ridge (windy) ─┘     (wax vats)              (windy)
//                                                                     ─ ③ the Bell Tower (end)
//                                         the Old Kiln (side nook: a lantern, candles) ─┘
//
// New here: GUSTS of wind over the exposed stretches (leaves rustle, then it blows: it pushes you
// and your flames and bends your flame down — less light), and WAX VATS — pools of warm wax that
// glow and refill you while you wade, slowly.
import type { LevelDef } from './Map22Level';

const p = (x: number, y: number, w: number) => ({ x, y, w });
const S = 'shade' as const, M = 'moth' as const, W = 'weeper' as const;
const SN = 'snuffer' as const, G = 'gloom' as const, L = 'leech' as const;

export const WAX_WORKS: LevelDef = {
    name: 'The Wax Works',
    w: 70, h: 94,
    start: { x: 10, y: 87.5 },
    paths: [
        { // the main way: lane → Market → west lane → Chandlery → switchbacks → Bell Tower
            pts: [
                p(6, 89, 1.8), p(10, 87.5, 1.8), p(16, 86, 1.8), p(23, 84.5, 1.9), p(31, 82, 1.9), p(37, 77, 2), p(42, 70, 2.5),
                p(36, 64, 1.7), p(30, 59, 1.6), p(27, 53, 1.6), p(30, 47, 1.8), p(40, 42, 2.5),
                p(39, 35, 1.8), p(34, 30, 1.7), p(25, 28, 1.6), p(16, 25, 1.6), p(14, 20, 1.6), p(20, 16.5, 1.6), p(30, 15.5, 1.6),
                p(42, 14.5, 1.7), p(50, 12, 2), p(56, 9, 2.5), p(58, 6, 2),
            ],
        },
        { // the east ridge: narrow and exposed
            pts: [p(42, 70, 2), p(49, 64, 1.4), p(55, 58, 1.25), p(56, 51, 1.3), p(51, 45, 1.6), p(40, 42, 2)],
        },
        { // up to the Old Kiln
            pts: [p(14, 20, 1.4), p(9, 16, 1.3), p(6, 11, 1.8)],
        },
    ],
    clearings: [
        {
            name: 'MARKET SQUARE', sub: 'two ways on: the lane, or the windy ridge', pos: { x: 42, y: 70 }, r: 8,
            lanterns: [{ x: 36.5, y: 72.5 }, { x: 48, y: 73 }, { x: 44, y: 64.5 }],
            waves: [[S, S, L, L], [SN, S, S], [M, M, M, M, S, S, S]],
            gates: [{ x: 36.3, y: 64.2, r: 2.4 }, { x: 48.6, y: 64.4, r: 2.2 }],
        },
        {
            name: 'THE CHANDLERY', sub: 'wade in the warm wax to fill up', pos: { x: 40, y: 42 }, r: 8,
            lanterns: [{ x: 34, y: 39.5 }, { x: 46, y: 45.5 }],
            waves: [[W, S, S], [G, S, S, L, L], [SN, SN, M, M, M, S]],
            gates: [{ x: 39.2, y: 34.6, r: 2.4 }],
        },
        {
            name: 'THE BELL TOWER', sub: 'light the tower and ring the bell', pos: { x: 56, y: 9 }, r: 8,
            lanterns: [{ x: 50.5, y: 11 }, { x: 60.5, y: 14 }, { x: 62, y: 6 }],
            waves: [[S, S, S, G], [W, W, S, S], [SN, G, M, M, M, M, S, S, S, L]],
            gates: [], end: true,
        },
        {
            name: 'THE OLD KILN', sub: 'still warm', pos: { x: 6, y: 9 }, r: 3.8,
            lanterns: [{ x: 8.5, y: 7 }],
            waves: [],
            gates: [],
        },
    ],
    lanterns: [{ x: 21, y: 82.6 }, { x: 28.6, y: 56 }, { x: 16, y: 22.6 }],
    candles: [{ x: 55.3, y: 52.5 }, { x: 4.4, y: 10 }, { x: 7.4, y: 11 }],
    hands: [{ x: 27.25, y: 86.25 }, { x: 34.25, y: 59.5 }, { x: 17.75, y: 28.5 }, { x: 25, y: 18.25 }, { x: 45, y: 16.5 }, { x: 25, y: 50 }],
    chapel: { x: 58.5, y: 2.5 },
    ground: 'woods', landmark: 'belltower', winText: 'THE BELL RINGS',
    gusts: [
        { pos: { x: 21, y: 23 }, r: 8, dir: { x: 1, y: 0.15 }, period: 6, phase: 0 },
        { pos: { x: 36, y: 15 }, r: 7, dir: { x: -1, y: 0 }, period: 5, phase: 2.5 },
        { pos: { x: 54, y: 56 }, r: 6, dir: { x: 1, y: 0 }, period: 4.5, phase: 1 },
    ],
    vats: [{ x: 37, y: 45.5, r: 1.6 }, { x: 44.5, y: 38.5, r: 1.3 }, { x: 4, y: 7, r: 1 }],
};
