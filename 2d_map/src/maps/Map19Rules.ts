// Puzzle Islands — level model + push rules shared by the solver and the playable board.
// Mirrors the game's PuzzleGrid / SokobanSolver rules (minecraft/src/puzzle) so ideas
// proven here port 1:1.
//
// Layout characters:
//   #  wall (hedge)   E  exit door (solid)   A  alien (blocks)   K  gold key (walkable, boxes can't)
//   K  gold keys: there can be several — collect ALL, then escape through the exit door
//   @ & $     teleporter pad pairs: step on one, come out of its partner (unless a box is on it)
//   B  crate          X  box bomb            P  player start     .  floor
//   r g y     coloured keys (picked up by walking over them; never used up)
//   R G Y     locked gates: solid until you hold the matching key, then walkable (boxes never)
//   > < v u   laser emitters (always on) · ) ( w n pulsing ones — see Map19Env
//   / \       mirror boxes: pushable like crates; E flips them 90°   O  laser receiver (wall)
//   1 2 3 4   battery-slot emitters, empty · 5 6 7 8 with a battery in (E adds / takes it)
//   Z         battery (picked up by walking)
//   h t T c   heart · time-bomb / throw-bomb / remote-bomb pickups (real-time play only)
//   _         pressure plate (floor)    H  laser gate (open while every O is lit)
//   o U · 0 V  more laser channels: gate U opens when every o is lit, V when every 0 is lit
//   J         plate gate (open while every _ holds a crate)
//   D         wooden door: shut for everyone (enemies can't open it) until a bomb blast next to it breaks it
//   F         thin wooden wall (a room-wall section): blocks walking, crates, bullets and blasts;
//             lets LASERS through; can't be broken
//   (space)   outside

import { EMITTER_CHARS, envOf, MIRROR_CHARS, type Emitter, type Env } from './Map19Env';

export const DX = [1, -1, 0, 0];
export const KEY_CHARS = 'rgy';
export const GATE_CHARS = 'RGY';
export const LOCK_COLORS = ['#ff5a5a', '#4cd964', '#ffd23f'];
/** Laser channels: receiver char and laser-gate char per channel (O/H, o/U, 0/V). */
export const RECEIVER_CHARS = 'Oo0';
export const LASER_GATE_CHARS = 'HUV';
export const DY = [0, 0, 1, -1];

/** Static part of a level (never changes while playing). */
export type Level = {
    w: number;
    h: number;
    base: string[]; // per cell: '#', '.', 'K', 'A', ' ' and the mechanic tiles (see above)
    door: number[]; // door cells (solid)
    gates: Map<number, number>; // locked gate cell → colour index
    keys: Map<number, number>; // coloured key cell → colour index
    exits: number[]; // floor cells next to a door
    aliens: number[];
    golds: number[]; // gold keys: collect ALL of them, then the exit door opens
    pads: Map<number, number>; // teleporter pad → its partner pad
    padPairs: [number, number][];
    emitters: Emitter[];
    preloaded: number; // batteries already sitting in slot emitters at the start
    receivers: number[];
    channel: Map<number, number>; // receiver / laser-gate cell → channel (0..2): a gate opens when ITS receivers are lit
    plates: number[];
    batteries: number[]; // battery cells; bit i of State.batteries = picked up
    items: { cell: number; kind: ItemKind }[]; // hearts / bomb pickups (play only, solver ignores)
    start: State;
};

export type ItemKind = 'heart' | 'time' | 'throw' | 'remote';
const ITEM_CHARS: Record<string, ItemKind> = { h: 'heart', t: 'time', T: 'throw', c: 'remote' };

/** Box encoding in State.boxes: cell * 4 + kind (sorted). */
export const BOX = { crate: 0, bomb: 1, mirrorSlash: 2, mirrorBack: 3 } as const;
export const makeBox = (cell: number, kind: number) => cell * 4 + kind;
export const boxCell = (b: number) => b >> 2;
export const boxKind = (b: number) => b & 3;
export const isMirrorBox = (b: number) => (b & 3) >= 2;
/** Kind of the box on `cell`, or -1. */
export const boxAt = (s: State, cell: number) => {
    const b = s.boxes.find((v) => v >> 2 === cell);
    return b === undefined ? -1 : b & 3;
};

/** Dynamic part: boxes (see BOX), player cell, held keys, batteries, gold, powered emitters, broken doors. */
export type State = {
    boxes: number[];
    player: number;
    keys: number; // bitmask of colour indices
    gold: number; // bitmask over Level.golds: gold keys picked up
    batteries: number; // bitmask over Level.batteries: picked up
    powered: number; // bitmask over Level.emitters: dead emitter switched on
    broken: number[]; // wooden doors destroyed by blasts (sorted; the solver never changes it)
};

/** Base tile with blast damage applied: a broken wooden door is plain floor. */
export function tileAt(L: Level, s: State, cell: number): string {
    const c = L.base[cell];
    return c === 'D' && s.broken.includes(cell) ? '.' : c;
}

export function parseLevel(rows: string[]): Level {
    const h = rows.length;
    const w = rows.reduce((m, r) => Math.max(m, r.length), 0);
    const base: string[] = new Array(w * h).fill(' ');
    const door: number[] = [];
    const gates = new Map<number, number>();
    const keys = new Map<number, number>();
    const aliens: number[] = [];
    const boxes: number[] = [];
    const golds: number[] = [];
    const padCells: Record<string, number[]> = {};
    let player = -1;
    let powered = 0;
    const emitters: Emitter[] = [];
    const receivers: number[] = [];
    const channel = new Map<number, number>();
    const plates: number[] = [];
    const batteries: number[] = [];
    const items: Level['items'] = [];
    for (let y = 0; y < h; y++) {
        for (let x = 0; x < rows[y].length; x++) {
            const c = rows[y][x];
            const i = y * w + x;
            switch (c) {
                case '#': base[i] = '#'; break;
                case 'E': base[i] = '#'; door.push(i); break;
                case 'A': base[i] = 'A'; aliens.push(i); break;
                case 'K': base[i] = 'K'; golds.push(i); break;
                case '@': case '&': case '$': base[i] = '.'; (padCells[c] ??= []).push(i); break;
                case 'B': case 'X': base[i] = '.'; boxes.push(makeBox(i, c === 'X' ? BOX.bomb : BOX.crate)); break;
                case 'P': base[i] = '.'; player = i; break;
                case ' ': break;
                case 'O': case 'o': case '0': base[i] = 'O'; receivers.push(i); channel.set(i, RECEIVER_CHARS.indexOf(c)); break;
                case 'U': case 'V': base[i] = 'H'; channel.set(i, LASER_GATE_CHARS.indexOf(c)); break;
                case '_': base[i] = '_'; plates.push(i); break;
                case 'Z': base[i] = '.'; batteries.push(i); break;
                case 'H': base[i] = 'H'; channel.set(i, 0); break;
                case 'J': case 'D': case 'F': base[i] = c; break;
                default:
                    if (EMITTER_CHARS.includes(c)) {
                        const k = EMITTER_CHARS.indexOf(c);
                        base[i] = 'Q';
                        if (k >= 12) powered |= 1 << emitters.length; // slot emitter with a battery in
                        emitters.push({ cell: i, dir: k % 4, pulse: k >= 4 && k < 8, slot: k >= 8 });
                        break;
                    }
                    if (MIRROR_CHARS.includes(c)) { base[i] = '.'; boxes.push(makeBox(i, c === '/' ? BOX.mirrorSlash : BOX.mirrorBack)); break; }
                    if (ITEM_CHARS[c]) { base[i] = '.'; items.push({ cell: i, kind: ITEM_CHARS[c] }); break; }
                    if (KEY_CHARS.includes(c)) { base[i] = '.'; keys.set(i, KEY_CHARS.indexOf(c)); break; }
                    if (GATE_CHARS.includes(c)) { base[i] = 'L'; gates.set(i, GATE_CHARS.indexOf(c)); break; }
                    base[i] = '.'; // floor
            }
        }
    }
    boxes.sort((a, b) => a - b);
    const pads = new Map<number, number>();
    const padPairs: [number, number][] = [];
    for (const cells of Object.values(padCells)) {
        if (cells.length !== 2) continue; // a pad needs exactly one partner
        pads.set(cells[0], cells[1]);
        pads.set(cells[1], cells[0]);
        padPairs.push([cells[0], cells[1]]);
    }
    const exits: number[] = [];
    for (const d of door) {
        for (let k = 0; k < 4; k++) {
            const x = (d % w) + DX[k];
            const y = Math.floor(d / w) + DY[k];
            if (x >= 0 && y >= 0 && x < w && y < h && base[y * w + x] === '.') exits.push(y * w + x);
        }
    }
    return {
        w, h, base, door, gates, keys, exits, aliens, golds, pads, padPairs, emitters, receivers, channel, plates, batteries, items,
        preloaded: emitters.filter((_, i) => (powered >> i) & 1).length,
        start: { boxes, player, keys: 0, gold: 0, batteries: 0, powered, broken: [] },
    };
}

/** Every gold key collected → the exit opens. */
export const allGold = (L: Level, s: State) => s.gold === (1 << L.golds.length) - 1;

export const hasBox = (s: State, cell: number) => s.boxes.some((b) => b >> 2 === cell);
export const boxIndex = (s: State, cell: number) => s.boxes.findIndex((b) => b >> 2 === cell);

/**
 * Player may stand here (ignoring boxes). For the solver (`avoidBeams`) always-on laser beams
 * count as walls; in real-time play you may cross them and take damage.
 */
export function walkable(L: Level, s: State, cell: number, avoidBeams = true): boolean {
    return walkableIn(L, s, cell, envOf(L, s), avoidBeams);
}

/** walkable() with the state's Env already looked up (hot path: flood fills). */
function walkableIn(L: Level, s: State, cell: number, env: Env, avoidBeams = true): boolean {
    const c = s.broken.length ? tileAt(L, s, cell) : L.base[cell];
    let ok: boolean;
    if (c === 'L') ok = (s.keys & (1 << L.gates.get(cell)!)) !== 0;
    else if (c === 'H') ok = ((env.openChannels >> (L.channel.get(cell) ?? 0)) & 1) === 1;
    else if (c === 'J') ok = env.plateOpen;
    else ok = c === '.' || c === 'K' || c === '_';
    return ok && !(avoidBeams && env.beam?.[cell]);
}

/** Box occupancy grid for one state (flood fills test it per cell). */
function boxGrid(L: Level, s: State): Uint8Array {
    const g = new Uint8Array(L.w * L.h);
    for (const b of s.boxes) g[b >> 2] = 1;
    return g;
}

/** A box may come to rest here (ignoring other boxes). */
export function restable(L: Level, s: State, cell: number): boolean {
    const c = tileAt(L, s, cell);
    return c === '.' || c === '_';
}

/**
 * Push the box at `cell` one step in direction `dir`. Returns null if blocked.
 * The player ends on the box's old cell.
 */
export function pushBox(L: Level, s: State, cell: number, dir: number): { next: State; to: number } | null {
    const bi = boxIndex(s, cell);
    if (bi < 0) return null;
    const x = (cell % L.w) + DX[dir];
    const y = Math.floor(cell / L.w) + DY[dir];
    if (x < 0 || y < 0 || x >= L.w || y >= L.h) return null;
    const to = y * L.w + x;
    if (hasBox(s, to) || !restable(L, s, to)) return null;
    const boxes = s.boxes.map((b, i) => (i === bi ? makeBox(to, b & 3) : b)).sort((a, b) => a - b);
    return { next: { ...s, boxes, player: cell }, to };
}

/** Where you end up stepping onto `n`: a teleporter pad sends you to its partner (unless a box sits there). */
export function arrive(L: Level, s: State, n: number): number {
    const m = L.pads.get(n);
    return m !== undefined && !hasBox(s, m) ? m : n;
}

/** Flood fill of cells the player can walk to without pushing (teleporters included). */
export function reach(L: Level, s: State): Uint8Array {
    const seen = new Uint8Array(L.w * L.h);
    const env = envOf(L, s);
    const box = boxGrid(L, s);
    const stack = [s.player];
    seen[s.player] = 1;
    while (stack.length) {
        const c = stack.pop()!;
        for (let k = 0; k < 4; k++) {
            const x = (c % L.w) + DX[k];
            const y = Math.floor(c / L.w) + DY[k];
            if (x < 0 || y < 0 || x >= L.w || y >= L.h) continue;
            const n = y * L.w + x;
            if (box[n] || !walkableIn(L, s, n, env)) continue;
            const t = arrive(L, s, n);
            if (!seen[t]) {
                seen[t] = 1;
                stack.push(t);
            }
        }
    }
    return seen;
}

/** Shortest walk to `to` inside the free region (teleporters included): directions + cells you land on, or null. */
export function walkPath(L: Level, s: State, to: number): { dir: number; cell: number }[] | null {
    if (s.player === to) return [];
    const prev = new Int32Array(L.w * L.h).fill(-1);
    const how = new Int8Array(L.w * L.h);
    const env = envOf(L, s);
    const box = boxGrid(L, s);
    const q = [s.player];
    prev[s.player] = s.player;
    for (let qi = 0; qi < q.length; qi++) {
        const c = q[qi];
        for (let k = 0; k < 4; k++) {
            const x = (c % L.w) + DX[k];
            const y = Math.floor(c / L.w) + DY[k];
            if (x < 0 || y < 0 || x >= L.w || y >= L.h) continue;
            const n = y * L.w + x;
            if (box[n] || !walkableIn(L, s, n, env)) continue;
            const t = arrive(L, s, n);
            if (prev[t] >= 0) continue;
            prev[t] = c;
            how[t] = k;
            if (t === to) {
                const path: { dir: number; cell: number }[] = [];
                for (let p = t; p !== s.player; p = prev[p]) path.push({ dir: how[p], cell: p });
                return path.reverse();
            }
            q.push(t);
        }
    }
    return null;
}

export function bombNextToAlien(L: Level, s: State): boolean {
    return s.boxes.some((b) => {
        if ((b & 3) !== BOX.bomb) return false;
        const c = b >> 2;
        return L.aliens.some((a) => Math.abs((a % L.w) - (c % L.w)) + Math.abs(Math.floor(a / L.w) - Math.floor(c / L.w)) === 1);
    });
}

/**
 * Walk-reach with key pickup: keys cost no pushes, so any key in the free region is taken,
 * which may open gates and grow the region — repeat until stable.
 */
export function reachWithKeys(L: Level, s: State): { s: State; r: Uint8Array } {
    let cur = s;
    for (;;) {
        const r = reach(L, cur);
        let keys = cur.keys;
        for (const [cell, color] of L.keys) if (r[cell]) keys |= 1 << color;
        let batteries = cur.batteries;
        L.batteries.forEach((cell, i) => { if (r[cell]) batteries |= 1 << i; }); // batteries too (no gate effect)
        let gold = cur.gold;
        L.golds.forEach((cell, i) => { if (r[cell]) gold |= 1 << i; }); // gold keys too
        if (keys === cur.keys) return { s: batteries === cur.batteries && gold === cur.gold ? cur : { ...cur, batteries, gold }, r };
        cur = { ...cur, keys, batteries, gold };
    }
}
