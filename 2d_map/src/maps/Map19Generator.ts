// Puzzle Islands — seeded room puzzles (port of the game's SokobanGenerator).
//  1. Rooms: binary space partition into `rooms` rooms, one-cell doorways → a room tree.
//  2. Goals: start, a gold key in the farthest room, exit door on another
//     room's outer edge.
//  3. Locks: some route doorways become coloured gates; each key goes in a room reachable
//     before its gate (a side room if possible → explore, fetch, come back: Zelda / RE).
//     Blockers: other route doorways get a crate; extra crates.
//     Switch gates: a route doorway becomes a LASER gate (H) with an emitter + mis-turned
//     mirror + receiver in a room before it, or a PLATE gate (J) with a plate + crate.
//     Pulsing lasers sweep across rooms as timing hazards. Some room-wall sections are thin
//     wooden walls (lasers pass); wooden doors seal side rooms with a reward (enemies can't
//     get in — a bomb next to the door blasts it open).
//  4. Verify with the solver; otherwise retry (same seed → same level).
import { GATE_CHARS, KEY_CHARS, LASER_GATE_CHARS, parseLevel, RECEIVER_CHARS } from './Map19Rules';
import { EMITTER_CHARS } from './Map19Env';
import { solveKeyAndExit, type Push } from './Map19Solver';

export type GenParams = {
    seed: number;
    width: number;
    height: number;
    rooms: number;
    extraCrates: number;
    minPushes: number;
    locks: number; // coloured gates on the route (key placed somewhere reachable before it)
    // Enemies (not checked by the solver yet: they add timing / routing pressure)
    patrollers: number; // walk a line
    reds: number; // melee, stupid, short chase
    yellows: number; // long-range shooter, semi-intelligent, medium chase
    blues: number; // medium-range laser, intelligent (pathfinding), long chase
    // Environment mechanics
    laserGates: number; // route doors that open when a receiver is lit (emitter + mirror puzzle)
    mirrorChains: number; // laser gates whose beam needs TWO mirrors (one may sit off the line: push it in)
    woodLasers: number; // laser gates whose beam crosses a wooden wall into the next room
    goldKeys: number; // gold keys to collect (all of them) before the exit opens
    teleporters: number; // pad pairs; the first one leads into a sealed room holding a gold key
    plateGates: number; // route doors that open while a crate sits on a plate
    pulseLasers: number; // timed laser hazards across rooms (real time only)
    deadEmitterChance: number; // a laser gate's emitter starts dead; its battery lies in a room before the gate
    // Pickups (real-time play only)
    hearts: number;
    bombPickups: number; // random time / throw / remote ammo
    woodWalls: number; // room-wall sections made of planks (block walking, lasers pass, unbreakable)
    woodDoors: number; // doorways sealed by a wooden door (a bomb next to it blasts it open)
    maxAttempts: number;
};

export const DEFAULT_GEN: GenParams = {
    seed: 1, width: 23, height: 15, rooms: 5, extraCrates: 3, minPushes: 3,
    locks: 1, patrollers: 1, reds: 1, yellows: 1, blues: 1, laserGates: 0, mirrorChains: 1, woodLasers: 1, goldKeys: 2, teleporters: 1, plateGates: 1, pulseLasers: 1, deadEmitterChance: 0.5, hearts: 1, bombPickups: 3, woodWalls: 2, woodDoors: 1, maxAttempts: 200,
};

export type GenResult = {
    ok: boolean;
    rows: string[];
    pushes: number;
    attempts: number;
    states: number;
    legs: Push[][]; // the key → exit solution (for scoring / replay)
    rejected: { layout: number; unsolvable: number; easy: number };
};

const MIN_ROOM = 3;
const DX4 = [1, -1, 0, 0];
const DY4 = [0, 0, 1, -1];
const SAFE_SPAWN = 5; // enemies spawn at least this far (cells) from the player

/** Small deterministic PRNG (mulberry32). */
export class Rng {
    private s: number;
    constructor(seed: number) { this.s = seed >>> 0; }
    next(): number {
        this.s = (this.s + 0x6d2b79f5) >>> 0;
        let t = this.s;
        t = Math.imul(t ^ (t >>> 15), t | 1);
        t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    }
    int(lo: number, hi: number): number { return lo + Math.floor(this.next() * (hi - lo + 1)); }
    chance(p: number): boolean { return this.next() < p; }
    pick<T>(a: T[]): T { return a[this.int(0, a.length - 1)]; }
}

export type Rect = { x0: number; y0: number; x1: number; y1: number };
export type Door = { x: number; y: number; vertical: boolean };
export const rw = (r: Rect) => r.x1 - r.x0 + 1;
export const rh = (r: Rect) => r.y1 - r.y0 + 1;

/** Room layout + placement helpers (also used by the dungeon generator, Map19Dungeon). */
export class Builder {
    g: string[][] = [];
    rooms: Rect[] = [];
    doors: Door[] = [];
    adj: number[][] = [];
    reserved: Uint8Array;
    p: GenParams;
    rng: Rng;
    constructor(p: GenParams, rng: Rng) {
        this.p = p;
        this.rng = rng;
        this.reserved = new Uint8Array(p.width * p.height);
    }

    hasDoor(x: number, y: number) { return this.doors.some((d) => d.x === x && d.y === y); }
    roomOf(x: number, y: number) { return this.rooms.findIndex((r) => x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1); }
    reserve(x: number, y: number) {
        if (x >= 0 && y >= 0 && x < this.p.width && y < this.p.height) this.reserved[y * this.p.width + x] = 1;
    }
    isReserved(x: number, y: number) { return this.reserved[y * this.p.width + x] === 1; }

    // --- 1. Rooms (BSP); never split along a line that would wall off a border doorway.
    split(i: number): boolean {
        const r = this.rooms[i];
        const preferV = rw(r) >= rh(r);
        for (let attempt = 0; attempt < 2; attempt++) {
            const vertical = (attempt === 0) === preferV;
            const lo = (vertical ? r.x0 : r.y0) + MIN_ROOM;
            const hi = (vertical ? r.x1 : r.y1) - MIN_ROOM;
            const cands: number[] = [];
            for (let s = lo; s <= hi; s++) {
                const blocked = vertical ? this.hasDoor(s, r.y0 - 1) || this.hasDoor(s, r.y1 + 1) : this.hasDoor(r.x0 - 1, s) || this.hasDoor(r.x1 + 1, s);
                if (!blocked) cands.push(s);
            }
            if (!cands.length) continue;
            const s = this.rng.pick(cands);
            const a = { ...r };
            const b = { ...r };
            if (vertical) {
                a.x1 = s - 1; b.x0 = s + 1;
                this.doors.push({ x: s, y: this.rng.int(r.y0, r.y1), vertical: true });
            } else {
                a.y1 = s - 1; b.y0 = s + 1;
                this.doors.push({ x: this.rng.int(r.x0, r.x1), y: s, vertical: false });
            }
            this.rooms[i] = a;
            this.rooms.push(b);
            return true;
        }
        return false;
    }

    makeRooms(): boolean {
        const { width: W, height: H } = this.p;
        this.rooms = [{ x0: 1, y0: 1, x1: W - 2, y1: H - 2 }];
        while (this.rooms.length < this.p.rooms) {
            const order = this.rooms.map((_, i) => i).sort((a, b) => rw(this.rooms[b]) * rh(this.rooms[b]) - rw(this.rooms[a]) * rh(this.rooms[a]));
            if (!order.some((i) => this.split(i))) return false;
        }
        this.g = Array.from({ length: H }, () => new Array(W).fill('#'));
        for (const r of this.rooms) for (let y = r.y0; y <= r.y1; y++) for (let x = r.x0; x <= r.x1; x++) this.g[y][x] = '.';
        this.adj = this.rooms.map(() => []);
        for (let i = 0; i < this.doors.length; i++) {
            const d = this.doors[i];
            this.g[d.y][d.x] = '.';
            const [a, b] = this.doorRooms(d);
            if (a < 0 || b < 0) return false;
            this.adj[a].push(i);
            this.adj[b].push(i);
        }
        return true;
    }

    doorRooms(d: Door): [number, number] {
        return d.vertical ? [this.roomOf(d.x - 1, d.y), this.roomOf(d.x + 1, d.y)] : [this.roomOf(d.x, d.y - 1), this.roomOf(d.x, d.y + 1)];
    }
    otherRoom(di: number, r: number) {
        const [a, b] = this.doorRooms(this.doors[di]);
        return a === r ? b : a;
    }

    /** Doors crossed going p_from → p_to (rooms form a tree), with the room each is entered from. */
    doorPath(from: number, to: number): { door: number; from: number }[] {
        const via = this.rooms.map(() => -1);
        const prev = this.rooms.map(() => -1);
        const seen = this.rooms.map(() => false);
        const q = [from];
        seen[from] = true;
        for (let qi = 0; qi < q.length; qi++) {
            const r = q[qi];
            for (const di of this.adj[r]) {
                const n = this.otherRoom(di, r);
                if (!seen[n]) { seen[n] = true; prev[n] = r; via[n] = di; q.push(n); }
            }
        }
        const path: { door: number; from: number }[] = [];
        for (let r = to; r !== from && r >= 0; r = prev[r]) path.push({ door: via[r], from: prev[r] });
        return path.reverse();
    }

    /** Outer-wall cell next to a room: [wallX, wallY, insideX, insideY]. */
    outerCell(room: number): [number, number, number, number] | null {
        const { width: W, height: H } = this.p;
        const r = this.rooms[room];
        const c: [number, number, number, number][] = [];
        if (r.x0 === 1) for (let y = r.y0; y <= r.y1; y++) c.push([0, y, 1, y]);
        if (r.x1 === W - 2) for (let y = r.y0; y <= r.y1; y++) c.push([W - 1, y, W - 2, y]);
        if (r.y0 === 1) for (let x = r.x0; x <= r.x1; x++) c.push([x, 0, x, 1]);
        if (r.y1 === H - 2) for (let x = r.x0; x <= r.x1; x++) c.push([x, H - 1, x, H - 2]);
        const ok = c.filter((e) => this.g[e[1]][e[0]] === '#' && !this.isReserved(e[2], e[3]));
        return ok.length ? this.rng.pick(ok) : null;
    }

    freeCell(room: number): [number, number] | null {
        const r = this.rooms[room];
        for (let t = 0; t < 60; t++) {
            const x = this.rng.int(r.x0, r.x1);
            const y = this.rng.int(r.y0, r.y1);
            if (this.g[y][x] === '.' && !this.isReserved(x, y)) return [x, y];
        }
        return null;
    }

    /** Rooms reachable from `start` without crossing any door in `blocked`. */
    reachableRooms(start: number, blocked: Set<number>): number[] {
        const seen = new Set([start]);
        const q = [start];
        for (let qi = 0; qi < q.length; qi++) {
            for (const di of this.adj[q[qi]]) {
                if (blocked.has(di)) continue;
                const n = this.otherRoom(di, q[qi]);
                if (!seen.has(n)) { seen.add(n); q.push(n); }
            }
        }
        return q;
    }

    /**
     * Turn up to `locks` route doors (not the first) into coloured gates, in route order. Key j
     * goes in a room reachable with gate j and all later gates shut — earlier gates are fine,
     * their keys come first. Prefer side rooms off the main route (a detour to fetch it).
     */
    addLocks(start: number, routeDoors: number[]) {
        const unique = [...new Set(routeDoors)].slice(1);
        const chosen = [...unique].sort(() => this.rng.next() - 0.5).slice(0, Math.min(this.p.locks, KEY_CHARS.length));
        chosen.sort((a, b) => unique.indexOf(a) - unique.indexOf(b));
        const onRoute = new Set<number>([start]);
        for (const di of routeDoors) for (const r of this.doorRooms(this.doors[di])) onRoute.add(r);
        chosen.forEach((di, j) => {
            const candidates = this.reachableRooms(start, new Set(chosen.slice(j)));
            const side = candidates.filter((r) => !onRoute.has(r));
            const room = this.rng.pick(side.length ? side : candidates);
            const cell = this.freeCell(room);
            if (!cell) return;
            const d = this.doors[di];
            this.g[d.y][d.x] = GATE_CHARS[j];
            this.g[cell[1]][cell[0]] = KEY_CHARS[j];
            this.reserve(cell[0], cell[1]);
        });
    }

    /**
     * Switch gates on free route doorways. The puzzle sits in a room reachable with that door
     * (and every later route door) shut. Laser: emitter in a side wall on the mirror's row,
     * receiver in the top / bottom wall on its column, mirror turned the WRONG way (its beam
     * cuts the room instead) — bump it to light the receiver. Plate: a plate + a crate to push.
     */
    addSwitchGates(start: number, routeDoors: number[]) {
        const order = [...new Set(routeDoors)];
        const kinds = [
            ...Array(this.p.laserGates).fill('laser'), ...Array(this.p.mirrorChains).fill('chain'),
            ...Array(this.p.woodLasers).fill('wood'), ...Array(this.p.plateGates).fill('plate'),
        ].sort(() => this.rng.next() - 0.5);
        let ch = 0; // laser channel: each laser gate gets its own receivers
        for (const kind of kinds) {
            if (kind !== 'plate' && ch >= RECEIVER_CHARS.length) continue;
            const free = order.filter((di) => this.g[this.doors[di].y][this.doors[di].x] === '.');
            if (!free.length) return;
            const di = this.rng.pick(free);
            const rooms = this.reachableRooms(start, new Set(order.slice(order.indexOf(di)))).sort(() => this.rng.next() - 0.5);
            const build = (r: number) =>
                kind === 'laser' ? this.buildLaser(r, rooms, ch)
                : kind === 'chain' ? this.buildMirrorChain(r, rooms, ch)
                : kind === 'wood' ? this.buildWoodLaser(r, rooms, ch)
                : this.buildPlate(r);
            if (!rooms.some(build)) continue;
            const d = this.doors[di];
            this.g[d.y][d.x] = kind === 'plate' ? 'J' : LASER_GATE_CHARS[ch++];
        }
    }

    /** Free floor run (unreserved '.') from (x,y) stepping (dx,dy) for n cells. */
    clearRun(x: number, y: number, dx: number, dy: number, n: number): boolean {
        for (let k = 0; k < n; k++) if (this.g[y + dy * k][x + dx * k] !== '.' || this.isReserved(x + dx * k, y + dy * k)) return false;
        return true;
    }

    buildLaser(room: number, before: number[], ch: number): boolean {
        const r = this.rooms[room];
        if (rw(r) < 3 || rh(r) < 3) return false;
        for (let t = 0; t < 30; t++) {
            const mx = this.rng.int(r.x0 + 1, r.x1 - 1);
            const my = this.rng.int(r.y0 + 1, r.y1 - 1);
            const west = this.rng.chance(0.5);
            const north = this.rng.chance(0.5);
            // Emitter = a free-standing post on the room's edge column (not in the wall), so
            // you can walk around it and reach its battery slot from behind or the side.
            const ex = west ? r.x0 : r.x1;
            const ry = north ? r.y0 - 1 : r.y1 + 1;
            if (this.g[ry][mx] !== '#') continue;
            // Emitter post → mirror (row) and mirror → receiver (column) must be plain free floor.
            const rowOk = west ? this.clearRun(r.x0, my, 1, 0, mx - r.x0 + 1) : this.clearRun(mx, my, 1, 0, r.x1 - mx + 1);
            const sidesOk = this.g[my - 1][ex] === '.' && this.g[my + 1][ex] === '.';
            const colOk = north ? this.clearRun(mx, r.y0, 0, 1, my - r.y0) : this.clearRun(mx, my + 1, 0, 1, r.y1 - my);
            if (!rowOk || !colOk || !sidesOk) continue;
            const edir = west ? 0 : 1;
            const rdir = north ? 3 : 2;
            const right = (edir === 0) === (rdir === 3) ? '/' : '\\'; // turns the beam onto the receiver
            this.g[my][mx] = right === '/' ? '\\' : '/'; // start mis-turned
            this.g[ry][mx] = RECEIVER_CHARS[ch];
            for (let x = Math.min(ex, mx); x <= Math.max(ex, mx); x++) this.reserve(x, my);
            for (let y = Math.min(ry, my); y <= Math.max(ry, my); y++) this.reserve(mx, y);
            this.placeEmitter(ex, my, edir, before);
            return true;
        }
        return false;
    }

    /**
     * Emitter post at (x, y) firing `dir`; its sides stay free (E from there, out of the beam).
     * Sometimes dead: its battery lies in a room reachable before the gate.
     */
    placeEmitter(x: number, y: number, dir: number, before: number[]) {
        const [sx, sy] = dir < 2 ? [0, 1] : [1, 0];
        this.reserve(x + sx, y + sy);
        this.reserve(x - sx, y - sy);
        const battery = this.rng.chance(this.p.deadEmitterChance) ? this.freeCell(this.rng.pick(before)) : null;
        this.g[y][x] = EMITTER_CHARS[battery ? 8 + dir : dir];
        if (battery) { this.g[battery[1]][battery[0]] = 'Z'; this.reserve(battery[0], battery[1]); }
    }

    /** Mirror char turning a beam travelling `din` into `dout`. */
    static mirrorFor(din: number, dout: number): string {
        return [3, 2, 1, 0][din] === dout ? '/' : '\\';
    }

    /**
     * MIRROR CHAIN: emitter → mirror 1 (turns the beam up / down) → mirror 2 (turns it sideways)
     * → receiver in a side wall. Mirror 2 starts mis-turned; mirror 1 sometimes too; and
     * sometimes mirror 2 sits one cell past the bend, so you first push it back into line.
     */
    buildMirrorChain(room: number, before: number[], ch: number): boolean {
        const r = this.rooms[room];
        if (rw(r) < 4 || rh(r) < 4) return false;
        for (let t = 0; t < 40; t++) {
            const west = this.rng.chance(0.5);
            const ex = west ? r.x0 : r.x1;
            const edir = west ? 0 : 1;
            const y1 = this.rng.int(r.y0 + 1, r.y1 - 1);
            const m1x = west ? this.rng.int(r.x0 + 2, r.x1 - 1) : this.rng.int(r.x0 + 1, r.x1 - 2);
            const y2 = this.rng.int(r.y0, r.y1);
            if (Math.abs(y2 - y1) < 2) continue;
            const dv = y2 > y1 ? 2 : 3;
            const east = this.rng.chance(0.5); // receiver in the east or west wall
            const dh = east ? 0 : 1;
            const rx = east ? r.x1 + 1 : r.x0 - 1;
            if (this.g[y2][rx] !== '#') continue;
            const rowA = west ? this.clearRun(ex, y1, 1, 0, m1x - ex + 1) : this.clearRun(m1x, y1, 1, 0, ex - m1x + 1);
            const col = this.clearRun(m1x, Math.min(y1, y2) + (y2 > y1 ? 1 : 0), 0, 1, Math.abs(y2 - y1));
            const rowB = east ? this.clearRun(m1x, y2, 1, 0, r.x1 - m1x + 1) : this.clearRun(r.x0, y2, 1, 0, m1x - r.x0 + 1);
            const sides = this.g[y1 - 1][ex] === '.' && this.g[y1 + 1][ex] === '.';
            if (!rowA || !col || !rowB || !sides || (ex === m1x)) continue;
            // Optional twist: mirror 2 one cell past the bend (push it back toward mirror 1).
            const sv = dv === 2 ? 1 : -1;
            const off = this.rng.chance(0.5) && y2 + 2 * sv >= r.y0 && y2 + 2 * sv <= r.y1
                && this.clearRun(m1x, Math.min(y2 + sv, y2 + 2 * sv), 0, 1, 2) ? 1 : 0;
            const m1 = Builder.mirrorFor(edir, dv);
            const m2 = Builder.mirrorFor(dv, dh);
            const flip = (m: string) => (m === '/' ? '\\' : '/');
            this.g[y1][m1x] = this.rng.chance(0.5) ? flip(m1) : m1;
            this.g[y2 + off * sv][m1x] = flip(m2);
            this.g[y2][rx] = RECEIVER_CHARS[ch];
            for (let x = Math.min(ex, m1x); x <= Math.max(ex, m1x); x++) this.reserve(x, y1);
            for (let y = Math.min(y1, y2); y <= Math.max(y1, y2); y++) this.reserve(m1x, y);
            for (let x = Math.min(m1x, rx); x <= Math.max(m1x, rx); x++) this.reserve(x, y2);
            if (off) { this.reserve(m1x, y2 + sv); this.reserve(m1x, y2 + 2 * sv); }
            this.placeEmitter(ex, y1, edir, before);
            return true;
        }
        return false;
    }

    /**
     * LASER THROUGH A WOODEN WALL: an emitter post in this room, a few cells from the wall,
     * fires through a plank section of the room wall into a receiver post just inside the
     * next room. Something stops it first: a crate on the beam (push it aside) or a dead
     * emitter (battery). Short beams on both sides, so the lit beam doesn't cut rooms in two.
     */
    buildWoodLaser(room: number, before: number[], ch: number): boolean {
        const a = this.rooms[room];
        for (let t = 0; t < 40; t++) {
            const d = this.rng.int(0, 3); // beam direction
            const horizontal = d < 2;
            if (horizontal ? rh(a) < 3 : rw(a) < 3) continue;
            const along = horizontal ? this.rng.int(a.y0 + 1, a.y1 - 1) : this.rng.int(a.x0 + 1, a.x1 - 1);
            const at = (k: number): [number, number] => (horizontal ? [k, along] : [along, k]);
            const step = d === 0 || d === 2 ? 1 : -1;
            const edge = [a.x1, a.x0, a.y1, a.y0][d]; // last cell of this room before the wall
            const wall = edge + step;
            const [wx, wy] = at(wall);
            if (wx < 1 || wy < 1 || wx > this.p.width - 2 || wy > this.p.height - 2 || this.g[wy][wx] !== '#') continue;
            const bi = this.roomOf(...at(wall + step));
            if (bi < 0 || bi === room) continue;
            const crate = this.rng.chance(0.6);
            const dA = this.rng.int(crate ? 2 : 1, 3); // free cells between emitter and wall
            const dB = this.rng.int(1, 2); // receiver post this far past the wall
            const emitAt = edge - step * dA;
            const recvAt = wall + step * dB;
            const cells: number[] = []; // emitter .. receiver (excluding the wall)
            for (let k = emitAt; k !== recvAt + step; k += step) if (k !== wall) cells.push(k);
            const free = cells.every((k) => {
                const [x, y] = at(k);
                return x > 0 && y > 0 && x < this.p.width - 1 && y < this.p.height - 1 && this.g[y][x] === '.' && !this.isReserved(x, y);
            });
            if (!free || this.roomOf(...at(emitAt)) !== room || this.roomOf(...at(recvAt)) !== bi) continue;
            const [ex, ey] = at(emitAt);
            const [s1x, s1y] = horizontal ? [ex, ey - 1] : [ex - 1, ey];
            const [s2x, s2y] = horizontal ? [ex, ey + 1] : [ex + 1, ey];
            if (this.g[s1y][s1x] !== '.' || this.g[s2y][s2x] !== '.') continue;
            this.g[wy][wx] = 'F'; // the plank section the beam shines through (+ neighbours in the wall line)
            for (const o of [-1, 1]) {
                const [nx, ny] = horizontal ? [wx, wy + o] : [wx + o, wy];
                const [p, q] = horizontal ? [[nx - 1, ny], [nx + 1, ny]] : [[nx, ny - 1], [nx, ny + 1]];
                if (this.g[ny]?.[nx] === '#' && this.roomOf(p[0], p[1]) >= 0 && this.roomOf(q[0], q[1]) >= 0) this.g[ny][nx] = 'F';
            }
            const [rcx, rcy] = at(recvAt);
            this.g[rcy][rcx] = RECEIVER_CHARS[ch];
            for (const k of cells) { const [x, y] = at(k); this.reserve(x, y); }
            // Blocker: a crate on the beam between emitter and wall, pushable off the line.
            if (crate) {
                const [bx, by] = at(emitAt + step * this.rng.int(1, dA - 1 || 1));
                const sideFree = horizontal ? this.g[by - 1]?.[bx] === '.' && this.g[by + 1]?.[bx] === '.' : this.g[by]?.[bx - 1] === '.' && this.g[by]?.[bx + 1] === '.';
                if (sideFree) this.g[by][bx] = 'B';
            }
            this.placeEmitter(ex, ey, d, before);
            return true;
        }
        return false;
    }

    /**
     * TELEPORTERS: the first pair leads into a SEALED room (its doorways become hedge) that
     * holds a gold key — the only way in is the pad. Rooms that would cut the map in two are
     * never sealed. Otherwise (and for further pairs) a free doorway is replaced by a pad pair
     * (one each side); failing that, a shortcut between two rooms already connected without
     * crossing a gate.
     */
    addTeleporters(start: number, keyRoom: number, exitRoom: number, routeRooms: Set<number>): number {
        let goldPlaced = 0;
        const chars = '@&$';
        for (let i = 0; i < Math.min(this.p.teleporters, chars.length); i++) {
            let done = false;
            if (i === 0) {
                for (const ri of this.rooms.map((_, k) => k).sort(() => this.rng.next() - 0.5)) {
                    if (ri === start || ri === keyRoom || ri === exitRoom || routeRooms.has(ri)) continue;
                    const doors = this.adj[ri];
                    if (doors.some((di) => this.g[this.doors[di].y][this.doors[di].x] !== '.')) continue;
                    const others = this.reachableRooms(start, new Set(doors));
                    if (others.length !== this.rooms.length - 1) continue; // sealing it would cut other rooms off
                    const inside = this.freeCell(ri);
                    const key = this.freeCell(ri);
                    const outside = this.freeCell(this.rng.pick(others));
                    if (!inside || !key || !outside || (inside[0] === key[0] && inside[1] === key[1])) continue;
                    for (const di of doors) this.g[this.doors[di].y][this.doors[di].x] = '#';
                    this.g[inside[1]][inside[0]] = chars[i];
                    this.g[outside[1]][outside[0]] = chars[i];
                    this.g[key[1]][key[0]] = 'K';
                    for (const c of [inside, outside, key]) this.reserve(c[0], c[1]);
                    goldPlaced++;
                    done = true;
                    break;
                }
            }
            if (done) continue;
            // Teleporter doorway: a free doorway becomes hedge and a pad pair (one each side)
            // replaces it — the only way between those two rooms.
            for (const di of this.doors.map((_, k) => k).sort(() => this.rng.next() - 0.5)) {
                const d = this.doors[di];
                if (this.g[d.y][d.x] !== '.') continue;
                const [ra, rb] = this.doorRooms(d);
                const pa = this.freeCell(ra);
                const pb = this.freeCell(rb);
                if (!pa || !pb) continue;
                this.g[d.y][d.x] = '#';
                this.g[pa[1]][pa[0]] = chars[i];
                this.g[pb[1]][pb[0]] = chars[i];
                this.reserve(pa[0], pa[1]);
                this.reserve(pb[0], pb[1]);
                done = true;
                break;
            }
            if (done) continue;
            // Shortcut pair: only between rooms already connected WITHOUT passing a gate, so a
            // teleporter never lets you skip a lock / laser / plate puzzle.
            const ra = this.rng.int(0, this.rooms.length - 1);
            const gated = new Set(this.doors.map((_, k) => k).filter((k) => this.g[this.doors[k].y][this.doors[k].x] !== '.'));
            const region = this.reachableRooms(ra, gated).filter((r) => r !== ra);
            if (!region.length) continue;
            const rb = this.rng.pick(region);
            const pa = this.freeCell(ra);
            const pb = this.freeCell(rb);
            if (!pa || !pb) continue;
            this.g[pa[1]][pa[0]] = chars[i];
            this.g[pb[1]][pb[0]] = chars[i];
            this.reserve(pa[0], pa[1]);
            this.reserve(pb[0], pb[1]);
        }
        return goldPlaced;
    }

    buildPlate(room: number): boolean {
        const r = this.rooms[room];
        for (let t = 0; t < 30; t++) {
            const p = this.freeCell(room);
            const b = this.freeCell(room);
            if (!p || !b || Math.abs(p[0] - b[0]) + Math.abs(p[1] - b[1]) < 2) continue;
            const [bx, by] = b;
            if (bx <= r.x0 || bx >= r.x1 || by <= r.y0 || by >= r.y1) continue; // pushable from every side
            this.g[p[1]][p[0]] = '_';
            this.g[by][bx] = 'B';
            this.reserve(p[0], p[1]);
            this.reserve(bx, by);
            return true;
        }
        return false;
    }

    /** Pulsing emitters in side walls of non-start rooms, sweeping a full row or column. */
    addPulseLasers(start: number) {
        for (let i = 0; i < this.p.pulseLasers; i++) {
            for (let t = 0; t < 20; t++) {
                const room = this.rng.int(0, this.rooms.length - 1);
                if (room === start && this.rooms.length > 1) continue;
                const r = this.rooms[room];
                const horizontal = this.rng.chance(0.5);
                const flip = this.rng.chance(0.5);
                // A post on the room's edge (not in the wall), firing across the room.
                const [x, y, dir] = horizontal
                    ? [flip ? r.x1 : r.x0, this.rng.int(r.y0, r.y1), flip ? 1 : 0]
                    : [this.rng.int(r.x0, r.x1), flip ? r.y1 : r.y0, flip ? 3 : 2];
                if (this.g[y][x] !== '.' || this.isReserved(x, y) || this.g[y + DY4[dir]][x + DX4[dir]] !== '.') continue;
                this.g[y][x] = EMITTER_CHARS[4 + dir];
                this.reserve(x, y);
                const len = (horizontal ? rw(r) : rh(r)) - 1;
                for (let k = 1; k <= len; k++) this.reserve(x + DX4[dir] * k, y + DY4[dir] * k); // nothing spawns in the beam
                break;
            }
        }
    }

    // --- 2 + 3. Goals and blockers
    populate(): boolean {
        for (const d of this.doors) {
            this.reserve(d.x, d.y);
            this.reserve(d.x - 1, d.y); this.reserve(d.x + 1, d.y);
            this.reserve(d.x, d.y - 1); this.reserve(d.x, d.y + 1);
        }
        const n = this.rooms.length;
        const start = this.rng.int(0, n - 1);
        const ps = this.freeCell(start);
        if (!ps) return false;
        this.g[ps[1]][ps[0]] = 'P';
        this.reserve(ps[0], ps[1]);

        let keyRoom = start;
        let best = 0;
        for (let r = 0; r < n; r++) {
            const len = this.doorPath(start, r).length;
            if (len > best || (len === best && this.rng.chance(0.5))) { best = len; keyRoom = r; }
        }
        const ks = this.freeCell(keyRoom);
        if (!ks) return false;
        this.g[ks[1]][ks[0]] = 'K';
        this.reserve(ks[0], ks[1]);

        const exitRooms = this.rooms.map((_, i) => i).filter((r) => r !== start && (r !== keyRoom || n <= 2));
        let exitRoom = -1;
        for (const r of exitRooms.sort(() => this.rng.next() - 0.5)) {
            const c = this.outerCell(r);
            if (c) { this.g[c[1]][c[0]] = 'E'; this.reserve(c[2], c[3]); exitRoom = r; break; }
        }
        if (exitRoom < 0) return false;

        const route = [...this.doorPath(start, keyRoom), ...this.doorPath(keyRoom, exitRoom)];
        this.addLocks(start, route.map((r) => r.door));
        // Teleporters first (they may need a free doorway), then the gate puzzles on what's left.
        // Gold keys: the main one is in the farthest room; a teleporter may hide one in a sealed
        // room; the rest go to random rooms (not the start room).
        const routeRooms = new Set<number>([start]);
        for (const { door } of route) for (const r of this.doorRooms(this.doors[door])) routeRooms.add(r);
        let golds = 1 + this.addTeleporters(start, keyRoom, exitRoom, routeRooms);
        this.addSwitchGates(start, route.map((r) => r.door));
        for (let t = 0; t < 20 && golds < this.p.goldKeys; t++) {
            const r = this.rng.int(0, n - 1);
            const c = r === start && n > 1 ? null : this.freeCell(r);
            if (c) { this.g[c[1]][c[0]] = 'K'; this.reserve(c[0], c[1]); golds++; }
        }
        this.addPulseLasers(start);
        for (const { door } of route) {
            const d = this.doors[door];
            if (this.g[d.y][d.x] === '.') this.g[d.y][d.x] = 'B';
        }

        // Enemies away from the start room and never close enough to hit you on spawn (the
        // solver ignores them; the difficulty score weighs them by how close they sit to the route).
        const enemyRooms = this.rooms.map((_, i) => i).filter((r) => r !== start);
        const kinds = [[this.p.patrollers, 'p'], [this.p.reds, 'm'], [this.p.yellows, 's'], [this.p.blues, 'l']] as const;
        for (const [count, ch] of kinds) {
            for (let i = 0; i < count && enemyRooms.length; i++) {
                for (let t = 0; t < 8; t++) {
                    const e = this.freeCell(this.rng.pick(enemyRooms));
                    if (!e || Math.hypot(e[0] - ps[0], e[1] - ps[1]) < SAFE_SPAWN) continue;
                    this.g[e[1]][e[0]] = ch;
                    this.reserve(e[0], e[1]);
                    break;
                }
            }
        }
        // Pickups: hearts and bomb ammo, anywhere (the start room is fine).
        const pickups = [...Array(this.p.hearts).fill('h'), ...Array.from({ length: this.p.bombPickups }, () => this.rng.pick(['t', 'T', 'c']))];
        for (const ch of pickups) {
            const cell = this.freeCell(this.rng.int(0, n - 1));
            if (cell) { this.g[cell[1]][cell[0]] = ch; this.reserve(cell[0], cell[1]); }
        }
        for (let i = 0; i < this.p.extraCrates; i++) {
            const c = this.freeCell(this.rng.int(0, n - 1));
            if (c) { this.g[c[1]][c[0]] = 'B'; this.reserve(c[0], c[1]); }
        }
        this.addWoodDoors(start, keyRoom, exitRoom);
        this.addWoodWalls();
        return true;
    }

    /**
     * Wooden wall sections in ROOM WALLS: a 2–4 cell run of the hedge between two rooms
     * becomes planks. Still a wall (no walking, crates or bullets, blasts can't break it), but
     * lasers shine through — a beam can reach into the next room (receivers, hazards, enemies).
     */
    addWoodWalls() {
        const { width: W, height: H } = this.p;
        // 'v': rooms left / right (the wall runs vertically) · 'h': rooms above / below.
        const kind = (x: number, y: number): 'v' | 'h' | null => {
            if (x < 1 || y < 1 || x > W - 2 || y > H - 2 || this.g[y][x] !== '#') return null;
            const l = this.roomOf(x - 1, y), r = this.roomOf(x + 1, y);
            if (l >= 0 && r >= 0 && l !== r) return 'v';
            const u = this.roomOf(x, y - 1), d = this.roomOf(x, y + 1);
            if (u >= 0 && d >= 0 && u !== d) return 'h';
            return null;
        };
        for (let i = 0; i < this.p.woodWalls; i++) {
            for (let t = 0; t < 40; t++) {
                const x0 = this.rng.int(1, W - 2);
                const y0 = this.rng.int(1, H - 2);
                const k = kind(x0, y0);
                if (!k) continue;
                const [dx, dy] = k === 'v' ? [0, 1] : [1, 0];
                const len = this.rng.int(2, 4);
                let n = 0;
                while (n < len && kind(x0 + dx * n, y0 + dy * n) === k) n++;
                if (n < 2) continue;
                for (let j = 0; j < n; j++) this.g[y0 + dy * j][x0 + dx * j] = 'F';
                break;
            }
        }
    }

    /**
     * Wooden doors on free doorways OFF the route: the side rooms behind them hold nothing the
     * solution needs (no key, battery, plate, mirror, emitter, exit) — just a reward. Enemies
     * can't get in; you blast the door with a bomb.
     */
    addWoodDoors(start: number, keyRoom: number, exitRoom: number) {
        const needed = 'KrgyZ_/\\0123456789><vu)(wnOoP@&$';
        let placed = 0;
        for (const di of this.doors.map((_, i) => i).sort(() => this.rng.next() - 0.5)) {
            if (placed >= this.p.woodDoors) return;
            const d = this.doors[di];
            if (this.g[d.y][d.x] !== '.') continue; // route doors already hold crates / gates
            const near = new Set(this.reachableRooms(start, new Set([di])));
            const far = this.rooms.map((_, r) => r).filter((r) => !near.has(r));
            if (!far.length || far.includes(keyRoom) || far.includes(exitRoom)) continue;
            const holdsNeeded = far.some((ri) => {
                const r = this.rooms[ri];
                for (let y = r.y0; y <= r.y1; y++) for (let x = r.x0; x <= r.x1; x++) if (needed.includes(this.g[y][x])) return true;
                return false;
            });
            if (holdsNeeded) continue;
            this.g[d.y][d.x] = 'D';
            const prize = this.freeCell(this.rng.pick(far));
            if (prize) { this.g[prize[1]][prize[0]] = this.rng.pick(['h', 't', 'T', 'c']); this.reserve(prize[0], prize[1]); }
            placed++;
        }
        // Not enough side rooms: blastable SHORTCUTS through a hedge between two rooms (the
        // solver treats them as walls, so they never change solvability).
        for (let t = 0; t < 60 && placed < this.p.woodDoors; t++) {
            const r = this.rng.pick(this.rooms);
            const vertical = this.rng.chance(0.5); // door in a vertical hedge (rooms left / right)
            const [x, y] = vertical ? [this.rng.chance(0.5) ? r.x0 - 1 : r.x1 + 1, this.rng.int(r.y0, r.y1)] : [this.rng.int(r.x0, r.x1), this.rng.chance(0.5) ? r.y0 - 1 : r.y1 + 1];
            const [ax, ay, bx, by] = vertical ? [x - 1, y, x + 1, y] : [x, y - 1, x, y + 1];
            if (ax < 1 || ay < 1 || bx >= this.p.width - 1 || by >= this.p.height - 1 || this.g[y][x] !== '#') continue;
            const ra = this.roomOf(ax, ay);
            const rb = this.roomOf(bx, by);
            if (ra < 0 || rb < 0 || ra === rb || this.g[ay][ax] !== '.' || this.g[by][bx] !== '.' || this.isReserved(ax, ay) || this.isReserved(bx, by)) continue;
            this.g[y][x] = 'D';
            this.reserve(ax, ay); // keep both sides clear
            this.reserve(bx, by);
            placed++;
        }
    }
}

export function generateLevel(params: Partial<GenParams>): GenResult {
    const p: GenParams = { ...DEFAULT_GEN, ...params };
    p.width = Math.max(p.width, 2 * MIN_ROOM + 3);
    p.height = Math.max(p.height, MIN_ROOM + 2);
    const rng = new Rng(p.seed * 2654435761);
    const out: GenResult = { ok: false, rows: [], pushes: 0, attempts: 0, states: 0, legs: [], rejected: { layout: 0, unsolvable: 0, easy: 0 } };
    for (let attempt = 1; attempt <= p.maxAttempts; attempt++) {
        out.attempts = attempt;
        const b = new Builder(p, rng);
        if (!b.makeRooms() || !b.populate()) { out.rejected.layout++; continue; }
        const rows = b.g.map((r) => r.join(''));
        const L = parseLevel(rows);
        // A long search relaxes MIN PUSHES (counted in attempts, not time, so the same seed
        // always gives the same level).
        const minPushes = attempt <= 25 ? p.minPushes : Math.floor(p.minPushes / 2);
        const route = solveKeyAndExit(L, 8000);
        if (!route.solved) { out.rejected.unsolvable++; continue; }
        if (route.pushes < minPushes) { out.rejected.easy++; continue; }
        return { ...out, ok: true, rows, pushes: route.pushes, states: route.states, legs: route.legs ?? [] };
    }
    return out;
}
