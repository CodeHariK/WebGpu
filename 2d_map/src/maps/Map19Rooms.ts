// Puzzle Islands — room reset (Link's Awakening style): walk out of an unsolved room and back
// in, and its crates jump back to where they started. A kid can never wreck a puzzle for good.
//
// A room counts as SOLVED (never resets again) once you hold every gold key inside it, or
// (rooms without keys) once a doorway you couldn't reach when you walked in (blocked by crates,
// or a closed laser gate) opens up, or once you leave it by a teleporter pad. Re-entering by pushing a crate in from the
// doorway doesn't reset (that push is part of the puzzle).

import { boxCell, walkable, type Level, type State } from './Map19Rules';
import type { Rect } from './Map19Generator';

/** Static room data for one level. */
export type RoomMap = {
    of: Int16Array; // cell → room index, -1 outside every room (doorways, walls)
    doorInside: number[][]; // per room: floor cells just inside each doorway
    golds: number[][]; // per room: indexes into Level.golds lying inside it
    startBoxes: number[][]; // per room: the boxes it starts with (encoded, see BOX)
    zone: Set<number>[]; // per room: its cells + its doorway cells (boxes here belong to it)
};

/** Live room tracking (part of the play state). */
export type RoomTrack = { room: number; open: number[]; solved: boolean[] }; // open: doorway cells reachable on entry

const DOORWAY = '.HJLD_'; // tiles a doorway gap can hold (floor, laser / plate / colour gates, wooden door, plate)

export function buildRooms(L: Level, rects: Rect[]): RoomMap {
    const of = new Int16Array(L.w * L.h).fill(-1);
    const zone: Set<number>[] = rects.map(() => new Set<number>());
    const doorInside: number[][] = rects.map(() => []);
    rects.forEach((r, k) => {
        for (let y = r.y0; y <= r.y1; y++) for (let x = r.x0; x <= r.x1; x++) { of[y * L.w + x] = k; zone[k].add(y * L.w + x); }
    });
    rects.forEach((_, k) => {
        for (const c of zone[k]) {
            for (const n of neighbours(L, c)) {
                if (of[n] !== -1 || !DOORWAY.includes(L.base[n])) continue; // receivers / emitters in the wall aren't doorways
                zone[k].add(n); // doorway cell (walkable gap in the room wall)
                if (!doorInside[k].includes(c)) doorInside[k].push(c);
            }
        }
    });
    const golds = rects.map((_, k) => L.golds.map((g, i) => (of[g] === k ? i : -1)).filter((i) => i >= 0));
    const startBoxes = rects.map((_, k) => L.start.boxes.filter((b) => of[boxCell(b)] === k));
    return { of, doorInside, golds, startBoxes, zone };
}

export const startTrack = (L: Level, rooms: RoomMap, s: State): RoomTrack => {
    const room = rooms.of[s.player];
    return { room, open: room >= 0 ? openDoors(L, rooms, room, s) : [], solved: rooms.doorInside.map(() => false) };
};

/** Doorway cells of room `k` you can walk to right now without pushing anything (closed gates count as blocked). */
function openDoors(L: Level, rooms: RoomMap, k: number, s: State): number[] {
    const zone = rooms.zone[k];
    const boxes = new Set(s.boxes.map(boxCell));
    const seen = new Set([s.player]);
    const stack = [s.player];
    while (stack.length) {
        for (const n of neighbours(L, stack.pop()!)) {
            if (seen.has(n) || !zone.has(n) || boxes.has(n) || !walkable(L, s, n, false)) continue;
            seen.add(n);
            stack.push(n);
        }
    }
    return [...zone].filter((c) => rooms.of[c] === -1 && seen.has(c));
}

/**
 * Update room tracking after the player moved from state `prev` to `s` (`pushed`: the move
 * pushed a crate). Returns the (possibly reset) state, the new track and whether a reset happened.
 */
export function trackRooms(L: Level, rooms: RoomMap, t: RoomTrack, s: State, pushed: boolean, blocked: (cell: number) => boolean, lit: number[] = [], from = -1): { s: State; t: RoomTrack; reset: boolean } {
    const here = rooms.of[s.player];
    let solved = t.solved;
    const markSolved = (k: number) => { if (k >= 0 && !solved[k]) solved = solved.map((v, i) => v || i === k); };
    // Solved checks: gold keys anywhere, and "reached a doorway that was blocked when you came
    // in" for the room you're in.
    rooms.golds.forEach((g, k) => { if (g.length && g.every((i) => (s.gold >> i) & 1)) markSolved(k); });
    if (here < 0 && t.room >= 0 && !rooms.golds[t.room].length && rooms.zone[t.room].has(s.player) && !t.open.includes(s.player)) markSolved(t.room);
    // …or the moment a doorway that was blocked on entry opens up (crates moved, laser gate lit):
    // the puzzle is done even if you leave by the way you came.
    const jumped = from >= 0 && Math.abs((from % L.w) - (s.player % L.w)) + Math.abs(Math.floor(from / L.w) - Math.floor(s.player / L.w)) > 1;
    if (jumped && t.room >= 0 && rooms.of[from] === t.room && !rooms.golds[t.room].length) markSolved(t.room); // left by teleporter pad
    const cur = here >= 0 ? here : t.room; // stepping out into a doorway still counts
    if (cur >= 0 && cur === t.room && !solved[cur] && !rooms.golds[cur].length && rooms.zone[cur].has(s.player) && openDoors(L, rooms, cur, s).some((c) => !t.open.includes(c))) markSolved(cur);
    if (here < 0) return { s, t: { ...t, room: -1, solved }, reset: false }; // in a doorway: you've left
    if (here === t.room) return { s, t: solved === t.solved ? t : { ...t, solved }, reset: false };
    // Entered room `here`.
    const t2: RoomTrack = { room: here, open: [], solved };
    if (pushed || solved[here]) return { s, t: { ...t2, open: openDoors(L, rooms, here, s) }, reset: false };
    const zone = rooms.zone[here];
    const mine = s.boxes.filter((b) => zone.has(boxCell(b)));
    const start = rooms.startBoxes[here];
    const same = mine.length === start.length && mine.every((b, i) => b === start[i]);
    const stuck = start.some((b) => boxCell(b) === s.player || blocked(boxCell(b))) || lit.some((c) => zone.has(c)); // something in the way, or a bomb still ticking: skip
    if (same || stuck) return { s, t: { ...t2, open: openDoors(L, rooms, here, s) }, reset: false };
    const s2 = { ...s, boxes: [...s.boxes.filter((b) => !zone.has(boxCell(b))), ...start].sort((a, b) => a - b) };
    return { s: s2, t: { ...t2, open: openDoors(L, rooms, here, s2) }, reset: true };
}

function neighbours(L: Level, c: number): number[] {
    const x = c % L.w;
    const out: number[] = [];
    if (x > 0) out.push(c - 1);
    if (x < L.w - 1) out.push(c + 1);
    if (c >= L.w) out.push(c - L.w);
    if (c < L.w * (L.h - 1)) out.push(c + L.w);
    return out;
}
