// Puzzle Islands — BFS over pushes (same algorithm as the game's SokobanSolver).
// The player's free walking is folded into a flood-filled region, so each BFS edge is one
// push and the first goal hit is the minimum number of pushes. Keeps parent links so the
// solution can be replayed.
import { allGold, BOX, boxCell, boxKind, DX, DY, bombNextToAlien, isMirrorBox, pushBox, reachWithKeys, walkable, type Level, type State } from './Map19Rules';
import { envOf, freeBatteries, isPowered, powerEmitter, rotateMirror, unpowerEmitter } from './Map19Env';

/**
 * One solver move: push the box at `cell` in `dir`; or (act) stand behind `cell` facing `dir`
 * and press E — flip the mirror box there, or put a battery into / take it out of its slot.
 */
export type Act = 'rotate' | 'power' | 'unpower';
export type Push = { cell: number; dir: number; act?: Act };
export type SolveResult = { solved: boolean; pushes: number; states: number; moves: Push[]; end?: State; legs?: Push[][] };

type Node = { s: State; parent: number; move: Push | null; pushes: number };

/** 52-bit numeric state hash (two 32-bit FNV-1a lanes): far cheaper Set keys than strings. */
function stateHash(boxes: number[], player: number, s: State): number {
    let a = 0x811c9dc5;
    let b = 0x9e3779b9;
    const mix = (v: number) => {
        a = Math.imul(a ^ v, 0x01000193);
        b = Math.imul(b ^ (v + 0x5bd1e995), 0x5bd1e995);
    };
    for (const v of boxes) mix(v);
    mix(-1);
    mix(player);
    mix(s.keys);
    mix(s.gold);
    mix(s.batteries);
    mix(s.powered);
    for (const v of s.broken) mix(v);
    return (a >>> 0) * 1048576 + ((b >>> 0) & 0xfffff);
}

function search(L: Level, start: State, goal: (s: State, r: Uint8Array) => number, maxStates: number): SolveResult {
    const nodes: Node[] = [{ s: start, parent: -1, move: null, pushes: 0 }];
    const seen = new Set<number>();
    const queued = new Set<number>();
    let states = 0;
    for (let qi = 0; qi < nodes.length && states < maxStates; qi++) {
        const n = nodes[qi];
        const picked = reachWithKeys(L, n.s); // free walking also collects reachable keys
        n.s = picked.s;
        const r = picked.r;
        const norm = r.indexOf(1); // canonical player = smallest reachable cell
        const key = stateHash(n.s.boxes, norm, n.s);
        if (seen.has(key)) continue;
        seen.add(key);
        states++;
        const endCell = goal(n.s, r);
        if (endCell >= 0) {
            const moves: Push[] = [];
            for (let i = qi; nodes[i].move; i = nodes[i].parent) moves.push(nodes[i].move!);
            moves.reverse();
            return { solved: true, pushes: n.pushes, states, moves, end: { ...n.s, player: endCell } };
        }
        const inBeam = (st: State) => !!envOf(L, st).beam?.[st.player];
        for (const b of n.s.boxes) {
            const c = boxCell(b);
            for (let k = 0; k < 4; k++) {
                const px = (c % L.w) - DX[k];
                const py = Math.floor(c / L.w) - DY[k];
                if (px < 0 || py < 0 || px >= L.w || py >= L.h || !r[py * L.w + px]) continue;
                const res = pushBox(L, n.s, c, k);
                if (!res || inBeam(res.next)) continue; // never end a move standing in a live beam
                const qk = stateHash(res.next.boxes, res.next.player, res.next);
                if (queued.has(qk)) continue;
                queued.add(qk);
                nodes.push({ s: res.next, parent: qi, move: { cell: c, dir: k }, pushes: n.pushes + 1 });
            }
        }
        // Interactions (E): flip a mirror box, put a battery into / take it out of a slot emitter.
        // `last`: a side to use only if no other is reachable (an emitter's own front = its beam).
        const interact = (cell: number, apply: State, act: Act, last = -1) => {
            for (const k of [0, 1, 2, 3].filter((d) => d !== last).concat(last >= 0 ? [last] : [])) {
                const px = (cell % L.w) - DX[k];
                const py = Math.floor(cell / L.w) - DY[k];
                if (px < 0 || py < 0 || px >= L.w || py >= L.h || !r[py * L.w + px]) continue;
                const next = { ...apply, player: py * L.w + px };
                if (inBeam(next)) continue; // e.g. flipping a mirror / powering an emitter onto yourself
                const qk = stateHash(next.boxes, next.player, next);
                if (!queued.has(qk)) {
                    queued.add(qk);
                    nodes.push({ s: next, parent: qi, move: { cell, dir: k, act }, pushes: n.pushes + 1 });
                }
                return;
            }
        };
        for (const b of n.s.boxes) if (isMirrorBox(b)) interact(boxCell(b), rotateMirror(n.s, boxCell(b)), 'rotate');
        const spare = freeBatteries(L, n.s);
        L.emitters.forEach((e, i) => {
            if (!e.slot) return;
            const front = e.dir ^ 1; // standing in front = standing in the beam once it's on
            if (isPowered(n.s, i)) interact(e.cell, unpowerEmitter(n.s, i), 'unpower', front);
            else if (spare > 0) interact(e.cell, powerEmitter(n.s, i), 'power', front);
        });
    }
    return { solved: false, pushes: -1, states, moves: [] };
}

/**
 * Fewest moves to collect EVERY gold key (keys are picked up for free by walking, so the
 * search state carries the collected set) and then stand beside the exit door.
 */
export function solveKeyAndExit(L: Level, maxStates = 15000): SolveResult {
    if (L.start.player < 0 || !L.golds.length || !L.exits.length) return { solved: false, pushes: -1, states: 0, moves: [] };
    const r = search(L, L.start, (s, reachable) => (allGold(L, s) ? (L.exits.find((e) => reachable[e]) ?? -1) : -1), maxStates);
    return { ...r, legs: r.solved ? [r.moves] : undefined };
}

/** Fewest moves from state `from` until the player can stand on `target` (used for single-room puzzles). */
export function solveReach(L: Level, from: State, target: number, maxStates = 3000): SolveResult {
    return search(L, from, (_s, r) => (r[target] ? target : -1), maxStates);
}

/** Fewest pushes to get a box bomb next to an alien. */
export function solveBombToAlien(L: Level, maxStates = 4000): SolveResult {
    if (L.start.player < 0 || !L.aliens.length) return { solved: false, pushes: -1, states: 0, moves: [] };
    return search(L, L.start, (s) => (bombNextToAlien(L, s) ? s.player : -1), maxStates);
}

/**
 * Local (fast) bomb check used by the generator: can some box bomb be pushed next to an
 * alien from where it sits? Plain crates are frozen into walls, all gates open, lasers off, the
 * player starts beside the bomb — so the BFS only moves bombs (a few hundred states at
 * most, instead of thousands for the whole map on big levels).
 */
export function solveBombToAlienLocal(L: Level, maxStates = 400): SolveResult {
    if (!L.aliens.length) return { solved: false, pushes: -1, states: 0, moves: [] };
    const base = [...L.base];
    for (const b of L.start.boxes) if (boxKind(b) !== BOX.bomb) base[boxCell(b)] = '#'; // crates + mirrors frozen
    for (let i = 0; i < base.length; i++) if (base[i] === 'H' || base[i] === 'J') base[i] = '.'; // switch gates open
    const local: Level = { ...L, base, emitters: [], plates: [], receivers: [], preloaded: 0 }; // no lasers
    const bombs = L.start.boxes.filter((b) => boxKind(b) === BOX.bomb);
    let states = 0;
    for (const b of bombs) {
        for (let d = 0; d < 4; d++) {
            const x = (boxCell(b) % L.w) + DX[d];
            const y = Math.floor(boxCell(b) / L.w) + DY[d];
            if (x < 0 || y < 0 || x >= L.w || y >= L.h) continue;
            const at = y * L.w + x;
            const start: State = { ...L.start, boxes: bombs, player: at, keys: 0xff };
            if (!walkable(local, start, at)) continue;
            const r = search(local, start, (s) => (bombNextToAlien(local, s) ? s.player : -1), maxStates);
            states += r.states;
            if (r.solved) return { ...r, states };
        }
    }
    return { solved: false, pushes: -1, states, moves: [] };
}
