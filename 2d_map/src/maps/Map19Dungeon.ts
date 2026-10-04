// Puzzle Islands — DUNGEON generator (kid-friendly redesign, step 1 = World 1: crates, gold
// keys, exit). Link's Awakening structure: the level is a small tree of rooms; every room has a
// ROLE and holds at most one small, self-contained puzzle.
//
//   start   the player's room — nothing to solve
//   puzzle  crates block the way to the next doorway on the route (push them aside)
//   rest    nothing to solve, a heart — breathing space after a puzzle
//   bonus   a side room: a gold key behind a crate puzzle (get it and get back out)
//   final   the exit room: the last gold key behind the level's hardest crate puzzle
//
// Pacing: along the route start → … → final, puzzle rooms with a rest room every few, puzzle
// targets (pushes) ramping up to the final room's peak; DIFFICULTY (1..10) sets the peak and
// how often you get a rest room.
// Each room puzzle is built and solved ALONE (a tiny Sokoban, fast), then one whole-level
// solve confirms the rooms fit together. Same seed → same level.
import { Builder, DEFAULT_GEN, Rng, rh, rw, type GenResult, type Rect } from './Map19Generator';
import { parseLevel } from './Map19Rules';
import { solveKeyAndExit, solveReach } from './Map19Solver';

export type RoomRole = 'start' | 'puzzle' | 'rest' | 'bonus' | 'final';
export type RoomInfo = { rect: Rect; role: RoomRole; target: number; pushes: number; onRoute: boolean };
export type DungeonParams = { seed: number; width: number; height: number; rooms: number; difficulty: number; maxAttempts: number };
export const DEFAULT_DUNGEON: DungeonParams = { seed: 1, width: 23, height: 15, rooms: 5, difficulty: 3, maxAttempts: 60 };
export type DungeonResult = GenResult & { roomInfo: RoomInfo[]; sequence: string };

const ROOM_TRIES = 50; // random crate layouts tried per room puzzle
const ROOM_STATES = 1500; // solver budget for one room

/** Pushes the level's hardest room (final) aims for. */
// Kid-sized: "clear the way" room puzzles naturally need 1–5 pushes, so DIFFICULTY raises the
// peak a little and mostly packs puzzles closer together (fewer rest rooms).
export const peakPushes = (difficulty: number) => 1 + Math.round(Math.max(1, Math.min(10, difficulty)) * 0.4);
/** A rest room after every N−1 puzzle rooms along the route. */
const restEvery = (difficulty: number) => (difficulty <= 3 ? 2 : difficulty <= 7 ? 3 : 4);

export function generateDungeon(params: Partial<DungeonParams>): DungeonResult {
    const p: DungeonParams = { ...DEFAULT_DUNGEON, ...params };
    const rng = new Rng(p.seed * 2654435761 + 17);
    const out: DungeonResult = {
        ok: false, rows: [], pushes: 0, bombPushes: 0, attempts: 0, states: 0, legs: [],
        rejected: { layout: 0, unsolvable: 0, easy: 0, bomb: 0 }, roomInfo: [], sequence: '',
    };
    for (let attempt = 1; attempt <= p.maxAttempts; attempt++) {
        out.attempts = attempt;
        const built = buildOnce(p, rng);
        if (!built) { out.rejected.layout++; continue; }
        const L = parseLevel(built.rows);
        const sol = solveKeyAndExit(L, 20000);
        if (!sol.solved) { out.rejected.unsolvable++; continue; }
        return { ...out, ok: true, rows: built.rows, pushes: sol.pushes, states: sol.states, legs: sol.legs ?? [], roomInfo: built.info, sequence: built.sequence };
    }
    return out;
}

/** One layout attempt: rooms, roles, a puzzle per room. Null if something didn't fit. */
function buildOnce(p: DungeonParams, rng: Rng): { rows: string[]; info: RoomInfo[]; sequence: string } | null {
    const b = new Builder({ ...DEFAULT_GEN, seed: p.seed, width: p.width, height: p.height, rooms: p.rooms }, rng);
    if (!b.makeRooms()) return null;
    const W = p.width;
    const n = b.rooms.length;
    const start = rng.int(0, n - 1);
    // Final room: on the outer wall (for the exit door), as far from the start as possible.
    let final = -1;
    let far = -1;
    let exit: [number, number, number, number] | null = null;
    for (let r = 0; r < n; r++) {
        if (r === start && n > 1) continue;
        const len = b.doorPath(start, r).length;
        const e = len >= far ? b.outerCell(r) : null;
        if (e && (len > far || rng.chance(0.5))) { far = len; final = r; exit = e; }
    }
    if (final < 0 || !exit) return null;
    b.g[exit[1]][exit[0]] = 'E';
    const exitInside = exit[3] * W + exit[2];

    // Route rooms in order + the doorway cells inside each room.
    const path = b.doorPath(start, final);
    const route = [start, ...path.map((s) => b.otherRoom(s.door, s.from))];
    const inside = (di: number, room: number) => { // the room cell just inside doorway di
        const d = b.doors[di];
        const r = b.rooms[room];
        for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
            const x = d.x + dx, y = d.y + dy;
            if (x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1) return y * W + x;
        }
        return -1;
    };

    // Roles + targets: alternate puzzle / rest between start and final, ramping to the peak.
    const peak = peakPushes(p.difficulty);
    const roles = new Map<number, RoomRole>();
    const target = new Map<number, number>();
    roles.set(start, 'start');
    roles.set(final, 'final');
    target.set(final, peak);
    const middle = route.slice(1, -1);
    const every = restEvery(p.difficulty);
    const isPuzzleAt = (j: number) => j % every !== every - 1;
    const puzzles = middle.filter((_, j) => isPuzzleAt(j)).length;
    let k = 0;
    middle.forEach((r, j) => {
        const isPuzzle = isPuzzleAt(j);
        roles.set(r, isPuzzle ? 'puzzle' : 'rest');
        if (isPuzzle) target.set(r, Math.max(1, Math.round(peak * (0.35 + (0.45 * k++) / Math.max(1, puzzles)))));
    });
    let bonusMade = false;
    for (let r = 0; r < n; r++) {
        if (roles.has(r)) continue;
        roles.set(r, bonusMade ? 'rest' : 'bonus'); // first side room hides a gold key
        if (!bonusMade) target.set(r, Math.max(1, Math.round(peak * 0.6)));
        bonusMade = true;
    }

    // Keep every doorway's inside cell free (no crates / props on them).
    const doorInside = new Set<number>();
    b.doors.forEach((_, di) => { for (const r of b.doorRooms(b.doors[di])) doorInside.add(inside(di, r)); });
    doorInside.add(exitInside);

    // Player.
    const sr = b.rooms[start];
    const startCells = cellsOf(sr, W).filter((c) => !doorInside.has(c));
    if (!startCells.length) return null;
    const player = startCells[rng.int(0, startCells.length - 1)];
    b.g[Math.floor(player / W)][player % W] = 'P';

    // Rooms, in route order then side rooms.
    const info: RoomInfo[] = [];
    const order = [...route, ...[...roles.keys()].filter((r) => !route.includes(r))];
    for (const r of order) {
        const role = roles.get(r)!;
        const rect = b.rooms[r];
        let pushes = 0;
        if (role === 'rest') {
            const free = cellsOf(rect, W).filter((c) => !doorInside.has(c) && b.g[Math.floor(c / W)][c % W] === '.');
            if (free.length) { const c = free[rng.int(0, free.length - 1)]; b.g[Math.floor(c / W)][c % W] = 'h'; }
        } else if (role !== 'start') {
            const ri = route.indexOf(r);
            const entryDoor = ri > 0 ? path[ri - 1].door : b.adj[r].find((di) => roles.get(b.otherRoom(di, r)) !== undefined)!;
            const entry = inside(entryDoor, r);
            const goals: (number | 'KEY')[] = role === 'puzzle' ? [inside(path[ri].door, r)] : role === 'final' ? ['KEY', exitInside] : ['KEY', entry];
            const res = roomPuzzle(b, r, entry, goals, target.get(r)!, doorInside, rng);
            if (!res) return null;
            pushes = res;
        }
        info.push({ rect, role, target: target.get(r) ?? 0, pushes, onRoute: route.includes(r) });
    }
    const label = (i: RoomInfo) => (i.role === 'start' || i.role === 'rest' ? i.role : `${i.role}(${i.pushes})`);
    const seq = info.filter((i) => i.onRoute).map(label).join(' → ');
    const side = info.filter((i) => !i.onRoute).map(label);
    return { rows: b.g.map((row) => row.join('')), info, sequence: side.length ? `${seq} · side: ${side.join(', ')}` : seq };
}

const cellsOf = (r: Rect, W: number) => {
    const out: number[] = [];
    for (let y = r.y0; y <= r.y1; y++) for (let x = r.x0; x <= r.x1; x++) out.push(y * W + x);
    return out;
};

/**
 * Build one room's crate puzzle, kid-sized: crates and a few rocks only in a small ZONE around
 * the first goal (the next doorway, or a gold key placed far from the entry); the rest of the
 * room stays open. Each candidate is solved in a sealed copy of the room (start at `entry`,
 * reach each goal in turn). Keeps the candidate whose push count is closest to `target`
 * (at least 1), writes it into the builder grid and returns its pushes, or null.
 */
function roomPuzzle(b: Builder, room: number, entry: number, goals: (number | 'KEY')[], target: number, keep: Set<number>, rng: Rng): number | null {
    const W = b.p.width;
    const rect = b.rooms[room];
    const cells = cellsOf(rect, W).filter((c) => !keep.has(c) && b.g[Math.floor(c / W)][c % W] === '.');
    const wantKey = goals.includes('KEY');
    if (cells.length < 6) return null;
    let best: { crates: number[]; rocks: number[]; key: number; pushes: number } | null = null;
    for (let t = 0; t < ROOM_TRIES * Math.max(1, target - 1); t++) {
        const key = wantKey ? farCell(cells, entry, W, rng) : -1;
        const goal = goals[0] === 'KEY' ? key : (goals[0] as number);
        // VAULT: a square around the goal, fenced by rocks except an opening or two; crates
        // inside, scrambled by PULLS from the solved state. You can only get in by pushing.
        const R = target <= 2 ? 1 : target <= 4 ? 2 : 3;
        const gx = goal % W, gy = Math.floor(goal / W);
        const cheb = (c: number) => Math.max(Math.abs((c % W) - gx), Math.abs(Math.floor(c / W) - gy));
        const inner = cells.filter((c) => c !== key && cheb(c) <= R && c !== entry);
        const ring = cells.filter((c) => c !== key && cheb(c) === R + 1 && c !== entry);
        if (inner.length < 3 || ring.length < 2) continue;
        ring.sort((a, c) => dist(a, entry, W) - dist(c, entry, W));
        const gaps = new Set([ring[rng.int(0, Math.min(2, ring.length - 1))]]);
        if (rng.chance(0.35)) gaps.add(ring[rng.int(0, ring.length - 1)]);
        const rocks = ring.filter((c) => !gaps.has(c));
        const take = () => inner.splice(rng.int(0, inner.length - 1), 1)[0];
        for (let k = rng.int(0, Math.floor(target / 4)); k > 0 && inner.length > 4; k--) rocks.push(take());
        let crates = Array.from({ length: Math.min(inner.length - 1, Math.min(4, 1 + Math.ceil(target / 2) + rng.int(0, 1))) }, take);
        crates = pullScramble(cells, rocks, crates, goal, key, W, 8 + target * 5, rng);
        const pushes = solveRoom(b, rect, entry, goals.map((g) => (g === 'KEY' ? key : g)), crates, rocks, key);
        if (pushes < 1) continue;
        if (!best || Math.abs(pushes - target) < Math.abs(best.pushes - target)) best = { crates, rocks, key, pushes };
        if (Math.abs(best.pushes - target) <= 0) break;
    }
    if (!best) return null;
    for (const c of best.crates) b.g[Math.floor(c / W)][c % W] = 'B';
    for (const c of best.rocks) b.g[Math.floor(c / W)][c % W] = '#';
    if (best.key >= 0) b.g[Math.floor(best.key / W)][best.key % W] = 'K';
    return best.pushes;
}

const dist = (a: number, b: number, W: number) => Math.abs((a % W) - (b % W)) + Math.abs(Math.floor(a / W) - Math.floor(b / W));

/**
 * Reverse Sokoban: the player starts on `goal` (solved) and walks randomly; walking away from
 * a crate behind them sometimes PULLS it along. Undoing those pulls is a solution, so the
 * result is usually solvable and needs several pushes.
 */
function pullScramble(cells: number[], rocks: number[], crates0: number[], goal: number, key: number, W: number, steps: number, rng: Rng): number[] {
    const floor = new Set(cells.filter((c) => !rocks.includes(c)));
    floor.add(goal);
    const crates = new Set(crates0);
    const step = [1, -1, W, -W];
    let p = goal;
    for (let i = 0; i < steps; i++) {
        const d = step[rng.int(0, 3)];
        const np = p + d;
        if (!floor.has(np) || crates.has(np) || np === key) continue;
        const behind = p - d;
        if (crates.has(behind) && p !== goal && p !== key && rng.chance(0.75)) { crates.delete(behind); crates.add(p); }
        p = np;
    }
    return [...crates];
}

/** A key cell far from the entry (random among the farthest third). */
function farCell(cells: number[], entry: number, W: number, rng: Rng): number {
    const sorted = [...cells].sort((a, b) => dist(b, entry, W) - dist(a, entry, W));
    return sorted[rng.int(0, Math.max(0, Math.floor(sorted.length / 3) - 1))];
}

/** Solve a sealed copy of one room (doorways walled off): pushes to reach every goal in order, or -1. */
function solveRoom(b: Builder, rect: Rect, entry: number, goals: number[], crates: number[], rocks: number[], key: number): number {
    const W = b.p.width;
    const H = b.p.height;
    const rows: string[] = [];
    for (let y = 0; y < H; y++) {
        let row = '';
        for (let x = 0; x < W; x++) {
            const c = y * W + x;
            const inRoom = x >= rect.x0 && x <= rect.x1 && y >= rect.y0 && y <= rect.y1;
            row += !inRoom || rocks.includes(c) ? '#' : c === entry ? 'P' : crates.includes(c) ? 'B' : c === key ? 'K' : b.g[y][x] === '.' ? '.' : '#';
        }
        rows.push(row);
    }
    const L = parseLevel(rows);
    let s = L.start;
    let pushes = 0;
    for (const g of goals) {
        const r = solveReach(L, s, g, ROOM_STATES);
        if (!r.solved || !r.end) return -1;
        pushes += r.pushes;
        s = r.end;
    }
    return pushes;
}

/** Room size helper for callers (labels). */
export const roomArea = (r: Rect) => rw(r) * rh(r);
