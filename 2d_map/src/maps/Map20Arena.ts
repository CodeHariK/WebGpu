// Enemy Arena — the test level: outer walls, a central block, pillars, crates and low walls, plus
// the hand-placed markers (cover spots, each enemy's home and patrol loop). Edit here to try a
// different layout.
import { SIZE, type Box, type World } from './Map20World';
import { v, type V } from './Map20Math';

export type Kind = 'melee' | 'ranged';
export type Camp = { home: V; patrol: V[]; kind: Kind };

const T = 0.5; // outer wall thickness

const WALLS: Box[] = [
    { x: 0, y: 0, w: SIZE, h: T }, { x: 0, y: SIZE - T, w: SIZE, h: T },
    { x: 0, y: 0, w: T, h: SIZE }, { x: SIZE - T, y: 0, w: T, h: SIZE },
    { x: 16, y: 17, w: 8, h: 6 }, // the central block
    { x: 7, y: 7, w: 1.2, h: 1.2 }, { x: 31.8, y: 7, w: 1.2, h: 1.2 }, // pillars
    { x: 7, y: 31.8, w: 1.2, h: 1.2 }, { x: 31.8, y: 31.8, w: 1.2, h: 1.2 },
    { x: 11, y: 12, w: 4, h: 0.6 }, { x: 25, y: 27.4, w: 4, h: 0.6 }, // low walls
    { x: 27, y: 11, w: 0.6, h: 4 }, { x: 12.4, y: 25, w: 0.6, h: 4 },
    { x: 20, y: 5, w: 1, h: 1 }, { x: 21.5, y: 5.6, w: 1, h: 1 }, // crates
    { x: 5, y: 20, w: 1, h: 1 }, { x: 34, y: 19.5, w: 1, h: 1 }, { x: 19.5, y: 34, w: 1, h: 1 },
];

// Behind each low wall / crate, on both sides, and at the corners of the central block.
const COVER: V[] = [
    v(13, 11.2), v(13, 13.4), v(27, 26.6), v(27, 28.8), v(26.2, 13), v(28.4, 13), v(11.6, 27), v(13.8, 27),
    v(21, 4.3), v(21, 7.3), v(4.3, 20.5), v(6.8, 20.5), v(35.7, 20), v(33.3, 20), v(20, 33.3), v(20, 35.7),
    v(15.3, 16.3), v(24.7, 16.3), v(15.3, 23.7), v(24.7, 23.7),
];

// One camp per enemy: where it lives and the loop it walks while nothing is going on.
const CAMPS: Camp[] = [
    { kind: 'melee', home: v(9, 10), patrol: [v(5, 5), v(14, 5), v(14, 10), v(5, 10)] },
    { kind: 'melee', home: v(31, 10), patrol: [v(29, 4), v(36, 4), v(36, 14), v(30, 14)] },
    { kind: 'melee', home: v(31, 30), patrol: [v(28, 30), v(36, 30), v(36, 36), v(28, 36)] },
    { kind: 'melee', home: v(9, 30), patrol: [v(4, 26), v(10, 26), v(10, 36), v(4, 36)] },
    { kind: 'melee', home: v(20, 13), patrol: [v(15, 14), v(25, 14)] },
    { kind: 'ranged', home: v(26.2, 13), patrol: [v(26.2, 13)] },
    { kind: 'ranged', home: v(13.8, 27), patrol: [v(13.8, 27)] },
];

export const PLAYER_START = v(20, 38);

export function makeArena(): { world: World; camps: Camp[] } {
    return { world: { walls: WALLS, markers: { cover: COVER, camps: CAMPS } }, camps: CAMPS };
}
