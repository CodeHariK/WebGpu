// Combat Sandbox — the TOY ASSEMBLY PLANT theme: where captured clowns are sealed into toys. Not an
// open arena but a production route that snakes through the building, station by station, the
// way a clown goes in — and you follow it to the end to shut it down:
//
//   INTAKE    cages of captured clowns; where you come in              (bottom-left)
//   STUFFING  a belt of stuffed bodies runs the length of the hall     (bottom, heading right)
//   PAINT     sprayers on the walls paint anything crossing them       (middle, heading left)
//   SEALING   the line runs into the press; grabber arms feed it       (middle-left)
//   PACKING   the last line, the last arms, then the loading dock exit  (top, heading right)
//
//   Line Two adds new bots and machines along a new route:
//   RECEIVING  crates come in — some of them aren't crates (springers)    (bottom-left)
//   STAMPING   a corridor of stamp heads slamming in a travelling wave    (bottom, heading right)
//   GLUE       glue pipes and puddles: stuck in glue you crawl, no dash   (middle, heading left)
//   TESTING    the rigger drones put their rig bots through their paces  (middle-left)
//   SHIPPING   the last line and arms, then out                          (top, heading right)
//
// Each level has its own bots (the factory's toys don't come here):
//   Line One   welder, sniper, magnet drone, projector drone          (Map21Mechs, Map21Drones)
//   Line Two   rigger + rig bots, engineer + turrets, springer        (Map21Bots, Map21Mechs)
//
// Each station's waves start when you walk in (zones). Inner walls make the route; gaps are the
// doorways between stations.
//
// Plain data: positions in metres, the arena's top-left is (0, 0).
import { box, EN, MG, PJ, RG, rider, SN, SP, W, type LevelDef } from './Map21Levels';

export const PLANT_LEVELS: LevelDef[] = [
    {
        name: 'Line One', tagline: 'Welders torch you up close (hit their backs), snipers lase you from afar, magnets drag you into the line, projectors shield.',
        w: 50, h: 40, floor: '#36313f',
        walls: [
            { x: 16, y: 28, w: 1, h: 4 }, { x: 16, y: 36, w: 1, h: 3 }, // Intake | Stuffing (doorway y 32 … 36)
            { x: 1, y: 27, w: 40, h: 1 }, // bottom band | middle band (doorway x 41 … 49)
            { x: 25, y: 15, w: 1, h: 4 }, { x: 25, y: 23, w: 1, h: 4 }, // Sealing | Paint (doorway y 19 … 23, the belt runs through it)
            { x: 9, y: 14, w: 40, h: 1 }, // middle band | top band (doorway x 1 … 9)
        ],
        zones: [
            { name: 'INTAKE', sub: 'where clowns come in', rect: { x: 1, y: 28, w: 15, h: 11 }, spawns: [{ x: 12, y: 30 }, { x: 13, y: 37 }, { x: 9, y: 30 }] },
            { name: 'STUFFING', sub: 'stuffed bodies ride the belt', rect: { x: 17, y: 28, w: 32, h: 11 }, spawns: [{ x: 30, y: 29.5 }, { x: 40, y: 37 }, { x: 46, y: 30 }, { x: 26, y: 37 }] },
            { name: 'PAINT', sub: 'don’t get painted: every toy sees you', rect: { x: 26, y: 15, w: 23, h: 12 }, spawns: [{ x: 28, y: 17 }, { x: 28, y: 25 }, { x: 35, y: 17 }] },
            { name: 'SEALING', sub: 'the arms feed the press', rect: { x: 1, y: 15, w: 24, h: 12 }, spawns: [{ x: 4, y: 16.5 }, { x: 10, y: 25 }, { x: 19, y: 16.5 }] },
            { name: 'PACKING', sub: 'the loading dock is through here', rect: { x: 1, y: 1, w: 48, h: 13 }, spawns: [{ x: 20, y: 3 }, { x: 30, y: 11 }, { x: 40, y: 3 }, { x: 46, y: 11 }] },
        ],
        cover: [
            // Intake
            box(8, 31, 3, 1), box(11, 35, 1.2, 2.5, false),
            // Stuffing (the belt runs y 31 … 34)
            box(22, 36, 3, 1), box(32, 36, 3, 1), box(42, 35, 1.2, 2.5, false), box(36, 28.6, 1.2, 1.4, false),
            rider(20, 32), rider(29, 32), rider(37, 32),
            // Paint
            box(31, 20, 3, 1), box(39, 22.5, 3, 1), box(44, 19.5, 1.2, 2.5, false), box(35, 16.5, 1.2, 1.5, false),
            // Sealing (the belt runs y 19 … 22)
            box(8, 24, 3, 1), box(16, 24, 3, 1), box(6, 16, 3, 1), box(18, 16.3, 1.2, 1.5, false),
            rider(10, 20), rider(19, 20),
            // Packing (the belt runs y 6 … 9)
            box(14, 2, 1.2, 2.5, false), box(28, 2, 1.2, 2.5, false), box(18, 11, 3, 1), box(32, 11, 3, 1), box(4, 8, 3, 1),
            rider(12, 7), rider(22, 7), rider(32, 7),
        ],
        pools: [],
        conveyors: [
            { x: 17, y: 31, w: 24, h: 3, dir: { x: 1, y: 0 }, speed: 2.2 }, // stuffing
            { x: 4, y: 19, w: 22, h: 3, dir: { x: -1, y: 0 }, speed: 2.4 }, // sealing, into its press
            { x: 9, y: 6, w: 33, h: 3, dir: { x: 1, y: 0 }, speed: 2.6 }, // packing, into its press
        ],
        crushers: [{ x: 1, y: 19, w: 3, h: 3, period: 5, phase: 0 }, { x: 42, y: 6, w: 3, h: 3, period: 5, phase: 2.5 }],
        vents: [], pipes: [], lifts: [],
        arms: [
            { y: 20.5, x0: 6, x1: 14, press: 0, speed: 3 }, { y: 20.5, x0: 15, x1: 23, press: 0, speed: 3.4 },
            { y: 7.5, x0: 12, x1: 24, press: 1, speed: 3.2 }, { y: 7.5, x0: 25, x1: 38, press: 1, speed: 3.6 },
        ],
        sprayers: [
            { pos: { x: 29, y: 15 }, dir: { x: 0, y: 1 }, len: 12, period: 4, phase: 0 },
            { pos: { x: 33, y: 27 }, dir: { x: 0, y: -1 }, len: 12, period: 4, phase: 1.3 },
            { pos: { x: 37, y: 15 }, dir: { x: 0, y: 1 }, len: 12, period: 4, phase: 2.6 },
            { pos: { x: 40.4, y: 27 }, dir: { x: 0, y: -1 }, len: 12, period: 4.5, phase: 0.6 },
            { pos: { x: 46, y: 15 }, dir: { x: 0, y: 1 }, len: 12, period: 4.5, phase: 2.2 },
        ],
        props: [
            { kind: 'cage', x: 1.3, y: 29, w: 2.6, h: 2 }, { kind: 'cage', x: 1.3, y: 32, w: 2.6, h: 2 }, { kind: 'cage', x: 1.3, y: 35, w: 2.6, h: 2 },
            { kind: 'cage', x: 5, y: 37, w: 2.6, h: 1.8 },
            { kind: 'fluff', x: 23, y: 28.4, w: 3, h: 1.6 }, { kind: 'fluff', x: 30, y: 37.2, w: 2.5, h: 1.6 }, { kind: 'fluff', x: 46, y: 34, w: 2.6, h: 2 },
            { kind: 'boxes', x: 2, y: 1.2, w: 9, h: 1.4 }, { kind: 'boxes', x: 36, y: 12.4, w: 8, h: 1.4 }, { kind: 'boxes', x: 20, y: 1.2, w: 6, h: 1.4 },
        ],
        core: null, tremor: 0, gas: false,
        start: { x: 6, y: 35 },
        objectives: [
            { kind: 'generator', pos: { x: 13, y: 29.5 }, hp: 30 }, // intake: the cage power
            { kind: 'pump', pos: { x: 46, y: 37 }, hp: 40 }, // stuffing: the blower
            { kind: 'valve', pos: { x: 47.3, y: 17 }, hp: 30, stops: 'paint' }, // paint: the paint feed
            { kind: 'pump', pos: { x: 21, y: 25.3 }, hp: 40 }, // sealing: the glue pump
            { kind: 'terminal', pos: { x: 24, y: 11.6 }, hp: 45, stops: 'arms' }, // packing: the line control
        ],
        doors: [], supplies: [{ x: 6, y: 30 }, { x: 28, y: 37.5 }, { x: 47, y: 25 }, { x: 13, y: 25.5 }, { x: 4, y: 3 }, { x: 40, y: 12 }],
        exit: { x: 45, y: 0, w: 4, h: 1 },
        waves: [
            { toys: [W], zone: 0 },
            { toys: [W, W], zone: 1 }, { toys: [SN, W], zone: 1, delay: 10 },
            { toys: [SN, MG, W], zone: 2 }, { toys: [W, W, PJ], zone: 2 },
            { toys: [W, MG, PJ, SN], zone: 3 }, { toys: [W, W, MG], zone: 3 },
            { toys: [W, W, PJ, SN], zone: 4 }, { toys: [MG, MG, W, W, PJ, SN], zone: 4 },
        ],
    },
    {
        name: 'Line Two', tagline: 'Riggers work rig bots on cables (shoot a cable!), engineers deploy turrets, springers hide in crates. Stamps and glue.',
        w: 44, h: 44, floor: '#33303c',
        walls: [
            { x: 14, y: 31, w: 1, h: 5 }, { x: 14, y: 40, w: 1, h: 3 }, // Receiving | Stamping (doorway y 36 … 40)
            { x: 19, y: 31, w: 13, h: 2.5 }, { x: 19, y: 40.5, w: 13, h: 2.5 }, // the stamp corridor's sides
            { x: 1, y: 30, w: 35, h: 1 }, // bottom band | middle band (doorway x 36 … 43)
            { x: 21, y: 16, w: 1, h: 5 }, { x: 21, y: 25, w: 1, h: 5 }, // Testing | Glue (doorway y 21 … 25)
            { x: 7, y: 15, w: 36, h: 1 }, // middle band | top band (doorway x 1 … 7)
        ],
        zones: [
            { name: 'RECEIVING', sub: 'not every crate is a crate', rect: { x: 1, y: 31, w: 13, h: 12 }, spawns: [{ x: 10, y: 33 }, { x: 11, y: 41 }] },
            { name: 'STAMPING', sub: 'run in behind the wave', rect: { x: 15, y: 31, w: 28, h: 12 }, spawns: [{ x: 36, y: 33 }, { x: 41, y: 41 }, { x: 37, y: 37 }] },
            { name: 'GLUE', sub: 'stuck in glue you can\u2019t dash — nor can they', rect: { x: 22, y: 16, w: 21, h: 14 }, spawns: [{ x: 25, y: 18 }, { x: 25, y: 28 }, { x: 32, y: 18 }] },
            { name: 'TESTING', sub: 'cut the cables, or drop the drone', rect: { x: 1, y: 16, w: 20, h: 14 }, spawns: [{ x: 3, y: 18 }, { x: 10, y: 28 }, { x: 17, y: 18 }] },
            { name: 'SHIPPING', sub: 'the dock is at the far end', rect: { x: 1, y: 1, w: 42, h: 14 }, spawns: [{ x: 15, y: 3 }, { x: 25, y: 12 }, { x: 35, y: 3 }, { x: 40, y: 12 }] },
        ],
        cover: [
            box(7, 33, 3, 1), box(9, 38, 1.2, 2.5, false), // receiving
            box(35, 35, 3, 1), box(39, 39.5, 1.2, 2, false), // stamping, past the corridor
            box(27, 20, 3, 1), box(34, 24.5, 3, 1), box(30, 17, 1.2, 1.5, false), box(39, 20.5, 1.2, 2.5, false), // glue
            box(4, 20, 3, 1), box(14, 20, 3, 1), box(9, 24.5, 3, 1), box(17, 26, 1.2, 2, false), box(3, 26, 1.2, 2, false), // testing
            box(10, 2.5, 1.2, 2.5, false), box(23, 2.5, 1.2, 2.5, false), box(16, 11, 3, 1), box(28, 11, 3, 1), box(4, 10, 3, 1), // shipping
            rider(11, 7), rider(21, 7), rider(29, 7),
        ],
        pools: [{ pos: { x: 30, y: 26.5 }, r: 1.4, glue: true }, { pos: { x: 36, y: 19 }, r: 1.2, glue: true }],
        conveyors: [{ x: 8, y: 6, w: 26, h: 3, dir: { x: 1, y: 0 }, speed: 2.6 }],
        crushers: [{ x: 34, y: 6, w: 3, h: 3, period: 5, phase: 0 }],
        vents: [],
        pipes: [
            { a: { x: 23, y: 16 }, b: { x: 33, y: 16 }, spill: { x: 28, y: 18.5 }, glue: true },
            { a: { x: 43, y: 18 }, b: { x: 43, y: 28 }, spill: { x: 40.5, y: 23 }, glue: true },
            { a: { x: 24, y: 30 }, b: { x: 34, y: 30 }, spill: { x: 29, y: 27.5 }, glue: true },
        ],
        lifts: [],
        arms: [{ y: 7.5, x0: 10, x1: 20, press: 0, speed: 3.2 }, { y: 7.5, x0: 21, x1: 31, press: 0, speed: 3.6 }],
        stamps: [{ x: 19.5, y: 33.5, w: 12, h: 7, n: 6, period: 3.2, step: 0.3 }],
        props: [
            { kind: 'boxes', x: 2, y: 31.4, w: 6, h: 1.2 }, { kind: 'boxes', x: 2, y: 41.4, w: 5, h: 1.2 },
            { kind: 'boxes', x: 2, y: 1.2, w: 6, h: 1.2 }, { kind: 'boxes', x: 26, y: 1.2, w: 9, h: 1.2 },
        ],
        traps: [{ x: 4, y: 35.5 }, { x: 37, y: 42 }, { x: 24, y: 29 }, { x: 6, y: 28.5 }, { x: 31, y: 3 }],
        core: null, tremor: 0, gas: false,
        start: { x: 5, y: 39 },
        objectives: [
            { kind: 'generator', pos: { x: 12, y: 33.5 }, hp: 30 }, // receiving: the dock power
            { kind: 'terminal', pos: { x: 41, y: 34 }, hp: 40, stops: 'stamps' }, // stamping: the stamp control
            { kind: 'pump', pos: { x: 24.5, y: 26.5 }, hp: 40 }, // glue: the glue pump
            { kind: 'generator', pos: { x: 3, y: 17.5 }, hp: 40 }, // testing: the test-rig power
            { kind: 'terminal', pos: { x: 33, y: 12.5 }, hp: 45, stops: 'arms' }, // shipping: the line control
        ],
        doors: [], supplies: [{ x: 3, y: 33 }, { x: 33, y: 42 }, { x: 42, y: 28.5 }, { x: 19.5, y: 28.5 }, { x: 3, y: 3 }, { x: 41, y: 3 }],
        exit: { x: 38, y: 0, w: 4, h: 1 },
        waves: [
            { toys: [EN], zone: 0 },
            { toys: [RG], zone: 1 }, { toys: [EN, SP], zone: 1 },
            { toys: [RG, SP], zone: 2 }, { toys: [EN, EN], zone: 2 },
            { toys: [RG, EN], zone: 3 }, { toys: [RG, RG], zone: 3 },
            { toys: [RG, EN, SP], zone: 4 }, { toys: [RG, RG, EN, SP, SP], zone: 4 },
        ],
    },
];
