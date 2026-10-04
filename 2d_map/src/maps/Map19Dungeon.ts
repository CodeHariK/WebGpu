// Puzzle Islands — DUNGEON generator (kid-friendly redesign: World 1 crates + gold keys + exit,
// World 2 enemies + your bombs, World 3 lasers + mirror crates, World 4 teleporter pads, World 5
// coloured keys + gates and pressure plates, World 6 batteries, wooden walls / doors, pulsing lasers). Link's Awakening structure: the level is a small tree of rooms; every room has a
// ROLE and holds at most one small, self-contained puzzle.
//
//   start   the player's room — nothing to solve
//   puzzle  crates block the way to the next doorway on the route (push them aside)
//   rest    nothing to solve, a heart — breathing space after a puzzle
//   bonus   a side room: a gold key behind a crate puzzle (get it and get back out)
//   final   the exit room: the last gold key behind the level's hardest crate puzzle
//   combat  (enemies on) no puzzle — patrollers / reds up to the room's threat budget
//   mixed   (enemies on) a smaller crate puzzle plus a patroller or two
//   laser   (lasers on) the way on is a LASER GATE: turn / push the mirror crate(s) so the
//           emitter's beam lights the receiver in the wall, and the gate opens
//   teleport (teleports on) the doorway on is hedge; a teleporter pad behind a crate puzzle
//           takes you into the next room (and back — pads work both ways)
//   lock    (locks on) a crate puzzle whose doorway on is a COLOURED GATE; its key lies in a
//           side room you can reach first (the "key" room — go fetch it, Zelda style)
//   key     a side room holding a coloured key for a lock further on
//   plate   (plates on) the doorway on is a PLATE GATE: push a crate onto the plate to open it
//   treasure (wood on) a side room sealed by a WOODEN DOOR: bomb it open for hearts + bombs
//   World 6 also twists laser rooms (dead emitter + battery; beam through a wooden wall) and
//   puts a PULSING laser across one rest / combat room (a timing hazard)
//
// Pacing: along the route start → … → final, puzzle rooms with a rest room every few, puzzle
// targets (pushes) ramping up to the final room's peak; DIFFICULTY (1..10) sets the peak and
// how often you get a rest room.
// Each room puzzle is built and solved ALONE (a tiny Sokoban, fast), then one whole-level
// solve confirms the rooms fit together. Same seed → same level.
import { PICKUP_AMMO } from './Map19Bombs';
import { Builder, DEFAULT_GEN, Rng, rh, rw, type GenResult, type Rect } from './Map19Generator';
import { GATE_CHARS, KEY_CHARS, LASER_GATE_CHARS, RECEIVER_CHARS } from './Map19Rules';
import { EMITTER_CHARS } from './Map19Env';
import { parseLevel } from './Map19Rules';
import { solveKeyAndExit, solveLegs, solveReach } from './Map19Solver';

export type RoomRole = 'start' | 'puzzle' | 'rest' | 'bonus' | 'final' | 'combat' | 'mixed' | 'laser' | 'teleport' | 'lock' | 'key' | 'plate' | 'treasure';
export type RoomInfo = { rect: Rect; role: RoomRole; target: number; pushes: number; onRoute: boolean; budget: number; threat: number; enemies: number; ammo: number };
export type DungeonParams = {
    seed: number; width: number; height: number; rooms: number; difficulty: number;
    enemies: boolean; // World 2+: combat / mixed rooms with patrollers and reds
    lasers: boolean; // World 3+: laser rooms (mirror crates → receiver opens the way on)
    teleports: boolean; // World 4+: teleport rooms (the way on is a pad behind a crate puzzle)
    locks: boolean; // World 5+: lock rooms (coloured gate; key in a side room) + a plate room
    wood: boolean; // World 6+: batteries, wooden walls / doors (treasure room), a pulsing laser
    threatScale: number; // × every room's threat budget (0.5–2; still capped by room size)
    ammoPerEnemy: number; // bombs handed out per enemy (generosity, 1–2)
    maxAttempts: number;
};
export const DEFAULT_DUNGEON: DungeonParams = { seed: 1, width: 25, height: 17, rooms: 7, difficulty: 3, enemies: true, lasers: false, teleports: false, locks: false, wood: false, threatScale: 1, ammoPerEnemy: 1.5, maxAttempts: 60 };

/**
 * THREAT BUDGETS (step 2). Threat = Σ enemy weights in a room (patroller 1, red 2). Each role
 * gets a budget from DIFFICULTY; enemies are drawn to fill it exactly, never above:
 *   start / rest / bonus / puzzle  0 — think and breathe without pressure
 *   mixed   1–2 — a small puzzle while dodging a patroller
 *   combat  2–5 — no puzzle: get through (or clear) a room with enemies in it
 *   final   0 until DIFFICULTY 5, then 1–2 — the peak
 */
export const ENEMY_WEIGHT = { p: 1, m: 2 } as const; // layout char → weight (patroller, red)
export function threatBudget(role: RoomRole, difficulty: number): number {
    const d = Math.max(1, Math.min(10, difficulty));
    if (role === 'combat') return 2 + Math.round(d * 0.3);
    if (role === 'mixed') return d >= 6 ? 2 : 1;
    if (role === 'final') return d >= 5 ? 1 + Math.floor((d - 5) / 3) : 0;
    return 0;
}
const SAFE_DIST = 3; // enemies spawn at least this far (cells) from every doorway and the player
export type DungeonResult = GenResult & { roomInfo: RoomInfo[]; sequence: string };

const PAD_CHARS = '@&$'; // teleporter pad pairs (one pair per teleport room)
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
        ok: false, rows: [], pushes: 0, attempts: 0, states: 0, legs: [],
        rejected: { layout: 0, unsolvable: 0, easy: 0 }, roomInfo: [], sequence: '',
    };
    for (let attempt = 1; attempt <= p.maxAttempts; attempt++) {
        out.attempts = attempt;
        const built = buildOnce(p, rng);
        if (!built) { out.rejected.layout++; continue; }
        const L = parseLevel(built.rows);
        let sol = solveLegs(L, built.goals);
        if (!sol.solved) sol = solveKeyAndExit(L, 20000); // the greedy legs can miss one
        if (!sol.solved) { out.rejected.unsolvable++; continue; }
        return { ...out, ok: true, rows: built.rows, pushes: sol.pushes, states: sol.states, legs: sol.legs ?? [], roomInfo: built.info, sequence: built.sequence };
    }
    return out;
}

/** One layout attempt: rooms, roles, a puzzle per room. Null if something didn't fit. */
function buildOnce(p: DungeonParams, rng: Rng): { rows: string[]; info: RoomInfo[]; sequence: string; goals: number[] } | null {
    const b = new Builder({ ...DEFAULT_GEN, seed: p.seed, width: p.width, height: p.height, rooms: p.rooms, deadEmitterChance: 0 }, rng); // batteries: a later world
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

    // Roles + targets between start and final: a rest room every few rooms; the others cycle
    // puzzle → combat → mixed when enemies are on (puzzle only in World 1); puzzles ramp to the peak.
    const peak = peakPushes(p.difficulty);
    const roles = new Map<number, RoomRole>();
    const target = new Map<number, number>();
    roles.set(start, 'start');
    roles.set(final, 'final');
    target.set(final, peak);
    const middle = route.slice(1, -1);
    const every = restEvery(p.difficulty);
    const cycle: RoomRole[] = ['puzzle', ...(p.locks ? (['lock', 'plate'] as const) : []), ...(p.teleports ? (['teleport'] as const) : []), ...(p.lasers ? (['laser'] as const) : []), ...(p.enemies ? (['combat', 'mixed'] as const) : [])];
    let c = 0;
    const midRoles = middle.map((_, j): RoomRole => (j % every === every - 1 ? 'rest' : cycle[c++ % cycle.length]));
    // Fairness: combat goes to the BIGGEST of those rooms (enemies need space) — never the room
    // right after the start (warm up first); a room too small for even a 2-threat budget becomes
    // a puzzle instead.
    const fight = midRoles.map((role, j) => (role === 'combat' ? j : -1)).filter((j) => j >= 0);
    const bySize = middle.map((r, j) => [j, rw(b.rooms[r]) * rh(b.rooms[r])]).filter(([j]) => midRoles[j] !== 'rest' && (j > 0 || middle.length < 2)).sort((a, z) => z[1] - a[1]).map(([j]) => j);
    fight.forEach((j, i) => { const big = bySize[i]; [midRoles[j], midRoles[big]] = [midRoles[big], midRoles[j]]; });
    midRoles.forEach((role, j) => { if (role === 'combat' && threatCap(b.rooms[middle[j]]) < 2) midRoles[j] = 'puzzle'; });
    // Every idea the world has must SHOW UP in every level, even short ones. Priority:
    //  1. the NEWEST world's ideas (taught early: first puzzle slots) — no slot → another layout;
    //  2. a fight (World 2+): the biggest remaining puzzle / rest slot (not the first room if
    //     there's a choice); with no slot left, the final room gets the enemies;
    //  3. older ideas, if a slot is left.
    const slots = (roles: RoomRole[]) => midRoles.map((role, j) => (roles.includes(role) ? j : -1)).filter((j) => j >= 0);
    const ideas = [[p.locks, 'plate'], [p.locks, 'lock'], [p.teleports, 'teleport'], [p.lasers, 'laser']] as const;
    const newest = p.wood ? ['laser'] : p.locks ? ['plate', 'lock'] : p.teleports ? ['teleport'] : p.lasers ? ['laser'] : [];
    const ensure = (role: RoomRole, required: boolean): boolean => {
        if (midRoles.includes(role)) return true;
        const j = [...slots(['puzzle']), ...slots(['rest'])][0];
        if (j === undefined) return !required;
        midRoles[j] = role;
        return true;
    };
    for (const [on, role] of ideas) if (on && newest.includes(role) && !ensure(role, true)) return null;
    let finalFight = false;
    if (p.enemies && !midRoles.some((role) => role === 'combat' || role === 'mixed')) {
        const free = slots(['puzzle', 'rest']).filter((j) => j > 0 || middle.length < 2 || slots(['puzzle', 'rest']).length < 2);
        const area = (j: number) => threatCap(b.rooms[middle[j]]);
        const j = free.sort((a, z) => area(z) - area(a))[0];
        if (j !== undefined && area(j) >= 1) midRoles[j] = area(j) >= 2 ? 'combat' : 'mixed';
        else finalFight = true;
    }
    for (const [on, role] of ideas) if (on && !newest.includes(role)) ensure(role, false);
    let lasersLeft = RECEIVER_CHARS.length;
    let padsLeft = PAD_CHARS.length;
    let locksLeft = KEY_CHARS.length;
    let platesLeft = 1; // plate gates open when EVERY plate holds a crate: one plate room per level
    midRoles.forEach((role, j) => {
        if (role === 'lock' && locksLeft-- <= 0) midRoles[j] = 'puzzle';
        if (role === 'plate' && platesLeft-- <= 0) midRoles[j] = 'puzzle';
        if (role === 'laser' && lasersLeft-- <= 0) midRoles[j] = 'puzzle';
        if (role === 'teleport' && padsLeft-- <= 0) midRoles[j] = 'puzzle';
    });
    const PUZZLE_ROLES: RoomRole[] = ['puzzle', 'laser', 'teleport', 'lock', 'plate'];
    const puzzles = midRoles.filter((r) => r === 'mixed' || PUZZLE_ROLES.includes(r)).length;
    let k = 0;
    middle.forEach((r, j) => {
        const role = midRoles[j];
        roles.set(r, role);
        const t = Math.max(1, Math.round(peak * (0.35 + (0.45 * k) / Math.max(1, puzzles))));
        if (PUZZLE_ROLES.includes(role)) { target.set(r, t); k++; }
        if (role === 'mixed') { target.set(r, Math.max(1, t - 1)); k++; } // smaller: you're also dodging
    });
    let bonusMade = false;
    for (let r = 0; r < n; r++) {
        if (roles.has(r)) continue;
        roles.set(r, bonusMade ? 'rest' : 'bonus'); // first side room hides a gold key
        if (!bonusMade) target.set(r, Math.max(1, Math.round(peak * 0.6)));
        bonusMade = true;
    }
    // Wooden door (World 6): a dead-end side rest room becomes a TREASURE room behind a wooden door.
    let treasureDoor = -1;
    if (p.wood) {
        for (const [r, role] of roles) {
            if (role !== 'rest' || route.includes(r) || b.adj[r].length !== 1) continue;
            roles.set(r, 'treasure');
            treasureDoor = b.adj[r][0];
            break;
        }
    }
    // Pulsing laser (World 6): across the first rest / combat room on the route where one fits.
    let pulseLeft = p.wood ? 1 : 0;
    // Locks: each lock's key goes in a side room reachable with that lock (and later ones) shut —
    // a side rest room becomes the "key" room; else it shares the bonus side room; else it lies
    // in the start room.
    const keyOf = new Map<number, number>(); // key room → colour
    const lockColour = new Map<number, number>(); // lock room → colour
    let colour = 0;
    route.forEach((r, ri) => {
        if (roles.get(r) !== 'lock') return;
        const shut = new Set(route.slice(ri).filter((x) => roles.get(x) === 'lock').map((x) => path[route.indexOf(x)].door));
        const open = b.reachableRooms(start, shut);
        const side = open.find((x) => roles.get(x) === 'rest' && !route.includes(x) && !keyOf.has(x));
        const bonus = open.find((x) => roles.get(x) === 'bonus' && !keyOf.has(x)); // shares the room with its gold key
        lockColour.set(r, colour);
        if (side !== undefined) { roles.set(side, 'key'); keyOf.set(side, colour); } else keyOf.set(bonus ?? start, colour);
        colour++;
    });

    // Keep every doorway's inside cell free (no crates / props on them).
    const doorInside = new Set<number>();
    b.doors.forEach((_, di) => { for (const r of b.doorRooms(b.doors[di])) doorInside.add(inside(di, r)); });
    doorInside.add(exitInside);

    for (const c of doorInside) b.reserve(c % W, Math.floor(c / W)); // laser builders keep off doorways too

    // Player.
    const sr = b.rooms[start];
    const startCells = cellsOf(sr, W).filter((c) => !doorInside.has(c));
    if (!startCells.length) return null;
    const player = startCells[rng.int(0, startCells.length - 1)];
    b.g[Math.floor(player / W)][player % W] = 'P';
    if (keyOf.has(start)) { // fallback: the key waits in the start room
        const free = startCells.filter((c) => c !== player && b.g[Math.floor(c / W)][c % W] === '.');
        if (!free.length) return null;
        const c = farCell(free, player, W, rng);
        b.g[Math.floor(c / W)][c % W] = KEY_CHARS[keyOf.get(start)!];
    }

    // Rooms, in route order then side rooms.
    const info: RoomInfo[] = [];
    const order = [...route, ...[...roles.keys()].filter((r) => !route.includes(r))];
    let channel = 0;
    let pads = 0;
    for (const r of order) {
        const role = roles.get(r)!;
        const rect = b.rooms[r];
        let pushes = 0;
        if (role === 'laser') {
            const ri = route.indexOf(r);
            const firstLaser = channel === 0;
            const res = laserRoom(b, r, inside(path[ri - 1].door, r), path[ri].door, channel++, target.get(r)!, rng,
                { battery: p.wood && (firstLaser || rng.chance(0.5)), wood: p.wood });
            if (res < 0) return null;
            pushes = res;
        } else if (role === 'teleport') {
            // Crate puzzle in front of a pad; the doorway on becomes hedge and the partner pad
            // sits just inside the next room (where you'd have walked in).
            const ri = route.indexOf(r);
            const entry = inside(path[ri - 1].door, r);
            const free = cellsOf(rect, W).filter((c) => !doorInside.has(c) && b.g[Math.floor(c / W)][c % W] === '.');
            if (free.length < 7) return null;
            const pad = farCell(free, entry, W, rng);
            const res = roomPuzzle(b, r, entry, [pad], target.get(r)!, new Set([...doorInside, pad]), rng);
            if (!res) return null;
            pushes = res;
            const ch = PAD_CHARS[pads++];
            const d = b.doors[path[ri].door];
            const there = inside(path[ri].door, route[ri + 1]);
            b.g[d.y][d.x] = '#';
            b.g[Math.floor(pad / W)][pad % W] = ch;
            b.g[Math.floor(there / W)][there % W] = ch;
        } else if (role === 'plate') {
            const ri = route.indexOf(r);
            const res = plateRoom(b, r, inside(path[ri - 1].door, r), path[ri].door, target.get(r)!, rng);
            if (res < 0) return null;
            pushes = res;
        } else if (role === 'treasure') {
            // Hearts + bombs inside; the door is wood (blast it); a bomb pickup waits outside.
            const free = cellsOf(rect, W).filter((c) => !doorInside.has(c) && b.g[Math.floor(c / W)][c % W] === '.');
            for (const ch of ['h', 't', 'h']) { if (!free.length) break; const c = free.splice(rng.int(0, free.length - 1), 1)[0]; b.g[Math.floor(c / W)][c % W] = ch; }
            const d = b.doors[treasureDoor];
            b.g[d.y][d.x] = 'D';
            const outside = b.otherRoom(treasureDoor, r);
            const near = cellsOf(b.rooms[outside], W).filter((c) => !doorInside.has(c) && b.g[Math.floor(c / W)][c % W] === '.' && !b.isReserved(c % W, Math.floor(c / W)))
                .sort((a, z) => dist(a, d.y * W + d.x, W) - dist(z, d.y * W + d.x, W));
            if (near.length) b.g[Math.floor(near[0] / W)][near[0] % W] = 't';
        } else if (role === 'key') {
            const entryDoor = b.adj[r].find((di) => roles.get(b.otherRoom(di, r)) !== undefined)!;
            const free = cellsOf(rect, W).filter((c) => !doorInside.has(c) && b.g[Math.floor(c / W)][c % W] === '.');
            if (!free.length) return null;
            const c = farCell(free, inside(entryDoor, r), W, rng);
            b.g[Math.floor(c / W)][c % W] = KEY_CHARS[keyOf.get(r)!];
        } else if (role === 'combat') {
            // no puzzle: the enemies are the challenge
        } else if (role === 'rest') {
            const free = cellsOf(rect, W).filter((c) => !doorInside.has(c) && b.g[Math.floor(c / W)][c % W] === '.');
            if (free.length) { const c = free[rng.int(0, free.length - 1)]; b.g[Math.floor(c / W)][c % W] = 'h'; }
        } else if (role !== 'start') {
            const ri = route.indexOf(r);
            const entryDoor = ri > 0 ? path[ri - 1].door : b.adj[r].find((di) => roles.get(b.otherRoom(di, r)) !== undefined)!;
            const entry = inside(entryDoor, r);
            const goals: (number | 'KEY')[] = role === 'puzzle' || role === 'mixed' || role === 'lock' ? [inside(path[ri].door, r)] : role === 'final' ? ['KEY', exitInside] : ['KEY', entry];
            const res = roomPuzzle(b, r, entry, goals, target.get(r)!, doorInside, rng);
            if (!res) return null;
            pushes = res;
            if (role === 'lock') { const d = b.doors[path[ri].door]; b.g[d.y][d.x] = GATE_CHARS[lockColour.get(r)!]; }
            if (role === 'bonus' && keyOf.has(r)) { // a lock's key shares this side room (out of the crates' way)
                const at = (c: number) => b.g[Math.floor(c / W)][c % W];
                const cells = cellsOf(rect, W).filter((c) => !doorInside.has(c) && at(c) === '.');
                const clear = cells.filter((c) => cellsOf({ x0: c % W - 1, x1: c % W + 1, y0: Math.floor(c / W) - 1, y1: Math.floor(c / W) + 1 }, W).every((n) => at(n) !== 'N'));
                const pool = clear.length ? clear : cells;
                if (!pool.length) return null;
                const c = pool[rng.int(0, pool.length - 1)];
                b.g[Math.floor(c / W)][c % W] = KEY_CHARS[keyOf.get(r)!];
            }
        }
        if (pulseLeft && route.includes(r) && (role === 'rest' || role === 'combat') && placePulse(b, r, doorInside, rng)) pulseLeft--;
        const base = Math.round(threatBudget(role, p.difficulty) * (p.threatScale ?? 1));
        const budget = p.enemies ? Math.min(role === 'final' && finalFight ? Math.max(1, base) : base, threatCap(rect)) : 0;
        const { threat, count } = placeEnemies(b, r, budget, [...doorInside, player], rng);
        // Ammo by threat: ~1.5 bombs per enemy as pickups right inside the entrance (the safe
        // side: enemies spawn SAFE_DIST away), plus a bomb SUPPLY crate in combat rooms.
        let ammo = 0;
        if (count > 0) {
            const ri = route.indexOf(r);
            const entryDoor = ri > 0 ? path[ri - 1].door : b.adj[r][0];
            ammo = placeAmmo(b, r, inside(entryDoor, r), Math.ceil(count * (p.ammoPerEnemy ?? 1.5)), role === 'combat', doorInside);
        }
        info.push({ rect, role, target: target.get(r) ?? 0, pushes, onRoute: route.includes(r), budget, threat, enemies: count, ammo });
    }
    if (p.wood && treasureDoor < 0) woodShortcut(b, doorInside, rng);
    for (const r of route.slice(1)) if (pulseLeft && placePulse(b, r, doorInside, rng)) pulseLeft--; // no rest / combat room fitted one
    const label = (i: RoomInfo) => {
        if (i.role === 'laser' || i.role === 'teleport' || i.role === 'lock' || i.role === 'plate') return `${i.role}(${i.pushes})`;
        if (i.role === 'treasure') return 'treasure';
        const parts = [i.pushes ? `${i.pushes}` : '', i.threat ? `⚔${i.threat}` : '', i.ammo ? `💣${i.ammo}` : ''].filter(Boolean).join(' ');
        return parts ? `${i.role}(${parts})` : i.role;
    };
    const seq = info.filter((i) => i.onRoute).map(label).join(' → ');
    const side = info.filter((i) => !i.onRoute).map(label);
    // Leg goals for the whole-level check: room by room along the route (a lock's key first),
    // then the bonus room's gold key; the solver finishes with the last gold keys + exit.
    const find = (chars: string, rect?: Rect) => {
        for (let c = 0; c < b.g.length * W; c++) {
            const x = c % W, y = Math.floor(c / W);
            if (chars.includes(b.g[y][x]) && (!rect || (x >= rect.x0 && x <= rect.x1 && y >= rect.y0 && y <= rect.y1))) return c;
        }
        return -1;
    };
    const goals: number[] = [];
    route.forEach((r, ri) => {
        if (ri === 0 || ri === route.length - 1) return;
        const role = roles.get(r)!;
        if (role === 'lock') goals.push(find(KEY_CHARS[lockColour.get(r)!]));
        const d = b.doors[path[ri].door];
        goals.push(role === 'teleport' ? inside(path[ri].door, route[ri + 1]) : role === 'laser' || role === 'plate' ? d.y * W + d.x : inside(path[ri].door, r));
    });
    for (const [r, role] of roles) if (role === 'bonus') goals.push(find('K', b.rooms[r]));
    return { rows: b.g.map((row) => row.join('')), info, sequence: side.length ? `${seq} · side: ${side.join(', ')}` : seq, goals: goals.filter((g) => g >= 0) };
}

/**
 * Fill room `r` with enemies whose weights add up to exactly `budget` (reds when there's room
 * for one, patrollers otherwise), on free floor at least SAFE_DIST from every doorway and the
 * player. Returns the threat actually placed (less only if the room had no safe cells).
 */
function placeEnemies(b: Builder, r: number, budget: number, avoid: number[], rng: Rng): { threat: number; count: number } {
    const W = b.p.width;
    const spots = cellsOf(b.rooms[r], W).filter((c) => b.g[Math.floor(c / W)][c % W] === '.' && !b.isReserved(c % W, Math.floor(c / W)) && avoid.every((a) => dist(c, a, W) >= SAFE_DIST)); // reserved: beams
    const placed: number[] = [];
    let threat = 0;
    while (threat < budget && spots.length) {
        // Spread evenly: farthest-point sampling (pick among the 3 spots farthest from the
        // enemies already placed; the first one anywhere).
        const away = (c: number) => (placed.length ? Math.min(...placed.map((e) => dist(c, e, W))) : rng.next());
        spots.sort((a, c) => away(c) - away(a));
        const c = spots.splice(rng.int(0, Math.min(2, spots.length - 1)), 1)[0];
        const ch = budget - threat >= 2 && rng.chance(0.5) ? 'm' : 'p';
        b.g[Math.floor(c / W)][c % W] = ch;
        placed.push(c);
        threat += ENEMY_WEIGHT[ch];
    }
    return { threat, count: placed.length };
}

/**
 * Put `bombs` worth of time-bomb pickups ('t', PICKUP_AMMO each) and optionally a bomb SUPPLY
 * crate ('S') on free floor nearest the room's entrance, away from crates (a crate pushed over
 * a pickup would hide it) and from enemies. Returns the bombs actually placed (supply counts
 * as SUPPLY_BOMBS since it refills forever).
 */
const SUPPLY_BOMBS = 3;
function placeAmmo(b: Builder, r: number, entry: number, bombs: number, supply: boolean, keep: Set<number>): number {
    const W = b.p.width;
    const at = (c: number) => b.g[Math.floor(c / W)][c % W];
    const cells = cellsOf(b.rooms[r], W);
    const crates = cells.filter((c) => at(c) === 'B' || at(c) === 'N');
    const foes = cells.filter((c) => at(c) === 'p' || at(c) === 'm');
    const cheb = (a: number, c: number) => Math.max(Math.abs((a % W) - (c % W)), Math.abs(Math.floor(a / W) - Math.floor(c / W)));
    const want = (supply ? 1 : 0) + Math.ceil(bombs / PICKUP_AMMO);
    const free = (gap: number) => cells
        .filter((c) => at(c) === '.' && !keep.has(c) && crates.every((k) => cheb(c, k) > gap) && foes.every((e) => dist(c, e, W) >= 2))
        .sort((a, c) => dist(a, entry, W) - dist(c, entry, W));
    let spots = free(1);
    if (spots.length < want) spots = free(0); // cramped room: allow cells next to crates
    if (spots.length < want) spots = cells.filter((c) => at(c) === '.' && !keep.has(c)).sort((a, c) => dist(a, entry, W) - dist(c, entry, W));
    let placed = 0;
    const put = (ch: string) => { const c = spots.shift(); if (c === undefined) return false; b.g[Math.floor(c / W)][c % W] = ch; return true; };
    if (supply && put('S')) placed += SUPPLY_BOMBS;
    for (let k = Math.ceil(bombs / PICKUP_AMMO); k > 0 && put('t'); k--) placed += PICKUP_AMMO;
    return placed;
}

/** Room-size cap on threat: about one patroller per CELLS_PER_THREAT floor cells (no crowding small rooms). */
const CELLS_PER_THREAT = 10;
export const threatCap = (r: Rect) => Math.floor((rw(r) * rh(r)) / CELLS_PER_THREAT);

const cellsOf = (r: Rect, W: number) => {
    const out: number[] = [];
    for (let y = r.y0; y <= r.y1; y++) for (let x = r.x0; x <= r.x1; x++) out.push(y * W + x);
    return out;
};

/**
 * LASER room (World 3): the doorway on is a laser gate; inside, an emitter post, a receiver in
 * the wall and one mirror crate (or a two-mirror chain for bigger targets), built by the classic
 * generator's laser builders (mirrors start mis-turned). For more pushes a mirror is nudged off
 * the beam line. Each candidate is solved in a sealed copy of the room (entry → through the
 * gate); keeps the one closest to `target` pushes. Returns its pushes (0 = just flip), or -1.
 */
function laserRoom(b: Builder, room: number, entry: number, gateDoor: number, ch: number, target: number, rng: Rng, opts: { battery: boolean; wood: boolean } = { battery: false, wood: false }): number {
    const W = b.p.width;
    const d = b.doors[gateDoor];
    const gate = d.y * W + d.x;
    const g0 = b.g.map((row) => [...row]);
    const res0 = b.reserved.slice();
    let best: { g: string[][]; reserved: Uint8Array; pushes: number } | null = null;
    for (let t = 0; t < ROOM_TRIES && !(best && best.pushes === target); t++) {
        b.g = g0.map((row) => [...row]);
        b.reserved = res0.slice();
        // World 6 twists: a DEAD emitter (its battery lies in this room — pick it up, E to put it
        // in) and / or a beam through a WOODEN WALL into a receiver in the neighbouring room.
        b.p.deadEmitterChance = opts.battery ? 1 : 0;
        const chain = target >= 3 && rng.chance(0.6);
        const built = opts.wood && rng.chance(0.7) ? b.buildWoodLaser(room, [room], ch) : chain ? b.buildMirrorChain(room, [room], ch) : b.buildLaser(room, [room], ch);
        b.p.deadEmitterChance = 0;
        if (!built) continue;
        b.g[d.y][d.x] = LASER_GATE_CHARS[ch];
        // Nudge mirrors off the beam (pushes) — more for bigger targets.
        const mirrors = cellsOf(b.rooms[room], W).filter((c) => '/\\'.includes(b.g[Math.floor(c / W)][c % W]));
        for (let k = rng.int(0, Math.min(2, target - 1)); k > 0 && mirrors.length; k--) {
            const m = mirrors[rng.int(0, mirrors.length - 1)];
            const step = [1, -1, W, -W][rng.int(0, 3)];
            const to = m + step;
            const inRoom = b.roomOf(to % W, Math.floor(to / W)) === room;
            if (!inRoom || b.g[Math.floor(to / W)][to % W] !== '.' || b.isReserved(to % W, Math.floor(to / W)) && !mirrors.includes(to)) continue;
            b.g[Math.floor(to / W)][to % W] = b.g[Math.floor(m / W)][m % W];
            b.g[Math.floor(m / W)][m % W] = '.';
            mirrors[mirrors.indexOf(m)] = to;
        }
        const pushes = solveThroughGate(b, room, entry, gate);
        if (pushes < 0) continue;
        if (!best || Math.abs(pushes - target) < Math.abs(best.pushes - target)) best = { g: b.g.map((row) => [...row]), reserved: b.reserved.slice(), pushes };
    }
    b.g = best ? best.g : g0;
    b.reserved = best ? best.reserved : res0;
    return best ? best.pushes : -1;
}

/** A pulsing emitter post on the room's edge firing across it (beam cells reserved: nothing spawns there). */
function placePulse(b: Builder, room: number, keep: Set<number>, rng: Rng): boolean {
    const W = b.p.width;
    const r = b.rooms[room];
    for (let t = 0; t < 30; t++) {
        const horizontal = rng.chance(0.5);
        const flip = rng.chance(0.5);
        const [x, y, dir] = horizontal
            ? [flip ? r.x1 : r.x0, rng.int(r.y0, r.y1), flip ? 1 : 0]
            : [rng.int(r.x0, r.x1), flip ? r.y1 : r.y0, flip ? 3 : 2];
        const dx = [1, -1, 0, 0][dir], dy = [0, 0, 1, -1][dir];
        const len = (horizontal ? rw(r) : rh(r)) - 1;
        let clear = true;
        for (let k = 0; k <= len && clear; k++) { const cx = x + dx * k, cy = y + dy * k; clear = b.g[cy][cx] === '.' && !keep.has(cy * W + cx); }
        if (!clear) continue;
        b.g[y][x] = EMITTER_CHARS[4 + dir];
        for (let k = 0; k <= len; k++) b.reserve(x + dx * k, y + dy * k);
        return true;
    }
    return false;
}

/**
 * No dead-end side room for a treasure room: a blastable wooden SHORTCUT through the hedge
 * between two rooms instead (the solver treats it as a wall, so it never changes solvability).
 */
function woodShortcut(b: Builder, keep: Set<number>, rng: Rng): boolean {
    const W = b.p.width;
    for (let t = 0; t < 80; t++) {
        const r = b.rooms[rng.int(0, b.rooms.length - 1)];
        const vertical = rng.chance(0.5); // door in a vertical hedge (rooms left / right)
        const [x, y] = vertical ? [rng.chance(0.5) ? r.x0 - 1 : r.x1 + 1, rng.int(r.y0, r.y1)] : [rng.int(r.x0, r.x1), rng.chance(0.5) ? r.y0 - 1 : r.y1 + 1];
        const [ax, ay, bx, by] = vertical ? [x - 1, y, x + 1, y] : [x, y - 1, x, y + 1];
        if (ax < 1 || ay < 1 || bx >= W - 1 || by >= b.p.height - 1 || b.g[y][x] !== '#') continue;
        const ra = b.roomOf(ax, ay);
        const rb = b.roomOf(bx, by);
        const free = (cx: number, cy: number) => b.g[cy][cx] === '.' && !b.isReserved(cx, cy) && !keep.has(cy * W + cx);
        if (ra < 0 || rb < 0 || ra === rb || !free(ax, ay) || !free(bx, by)) continue;
        b.g[y][x] = 'D';
        b.reserve(ax, ay);
        b.reserve(bx, by);
        return true;
    }
    return false;
}

/**
 * PLATE room (World 5): the doorway on is a plate gate (J); a pressure plate and a crate (+ a few
 * metal crates in the way for bigger targets). Candidates are solved in a sealed copy (entry →
 * through the gate); keeps the one closest to `target` pushes (at least 1). Returns pushes or -1.
 */
function plateRoom(b: Builder, room: number, entry: number, gateDoor: number, target: number, rng: Rng): number {
    const W = b.p.width;
    const d = b.doors[gateDoor];
    const gate = d.y * W + d.x;
    const cells = cellsOf(b.rooms[room], W).filter((c) => b.g[Math.floor(c / W)][c % W] === '.' && !b.isReserved(c % W, Math.floor(c / W)) && c !== entry);
    if (cells.length < 6) return -1;
    const at = (c: number) => [c % W, Math.floor(c / W)] as const;
    let best: { plate: number; crates: number[]; pushes: number } | null = null;
    for (let t = 0; t < ROOM_TRIES && !(best && best.pushes === target); t++) {
        const plate = cells[rng.int(0, cells.length - 1)];
        const near = cells.filter((c) => c !== plate && dist(c, plate, W) <= 1 + target);
        if (!near.length) continue;
        const crates = [near.splice(rng.int(0, near.length - 1), 1)[0]];
        for (let k = rng.int(0, Math.floor(target / 2)); k > 0 && near.length; k--) crates.push(near.splice(rng.int(0, near.length - 1), 1)[0]);
        const g0 = b.g.map((row) => [...row]);
        b.g[at(plate)[1]][at(plate)[0]] = '_';
        for (const c of crates) b.g[at(c)[1]][at(c)[0]] = 'N';
        b.g[d.y][d.x] = 'J';
        const pushes = solveThroughGate(b, room, entry, gate);
        b.g = g0;
        if (pushes < 1) continue;
        if (!best || Math.abs(pushes - target) < Math.abs(best.pushes - target)) best = { plate, crates, pushes };
    }
    if (!best) return -1;
    b.g[at(best.plate)[1]][at(best.plate)[0]] = '_';
    for (const c of best.crates) b.g[at(c)[1]][at(c)[0]] = 'N';
    b.g[d.y][d.x] = 'J';
    return best.pushes;
}

/** Sealed copy of a room + its wall ring (other doorways walled up): pushes from `entry` through `gate`, or -1. */
function solveThroughGate(b: Builder, room: number, entry: number, gate: number): number {
    const W = b.p.width;
    const r = b.rooms[room];
    // A beam through a wooden wall ends at a receiver in a neighbouring room: include that room.
    const extra = new Set<number>();
    for (let c = 0; c < W * b.p.height; c++) {
        const x = c % W, y = Math.floor(c / W);
        if (!RECEIVER_CHARS.includes(b.g[y][x])) continue;
        const k = b.roomOf(x, y);
        if (k >= 0 && k !== room) extra.add(k);
    }
    const inExtra = (x: number, y: number) => [...extra].some((k) => { const q = b.rooms[k]; return x >= q.x0 && x <= q.x1 && y >= q.y0 && y <= q.y1; });
    const rows: string[] = [];
    for (let y = 0; y < b.p.height; y++) {
        let row = '';
        for (let x = 0; x < W; x++) {
            const c = y * W + x;
            const inRoom = x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1;
            const ring = !inRoom && x >= r.x0 - 1 && x <= r.x1 + 1 && y >= r.y0 - 1 && y <= r.y1 + 1;
            const ch = b.g[y][x];
            row += c === entry ? 'P' : inRoom || inExtra(x, y) ? (ch === 'P' ? '.' : ch) : ring && (c === gate || ch !== '.') ? ch : '#';
        }
        rows.push(row);
    }
    const L = parseLevel(rows);
    const sol = solveReach(L, L.start, gate, ROOM_STATES * 2);
    return sol.solved ? sol.pushes : -1;
}

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
    const cells = cellsOf(rect, W).filter((c) => !keep.has(c) && b.g[Math.floor(c / W)][c % W] === '.' && !b.isReserved(c % W, Math.floor(c / W))); // reserved: beams / posts
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
    for (const c of best.crates) b.g[Math.floor(c / W)][c % W] = 'N'; // metal: bombs can't wreck the puzzle
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
