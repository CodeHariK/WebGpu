// Puzzle Islands — the playable board: one grid step per key press, same push rules.
import { allGold, arrive, boxAt, BOX, DX, DY, hasBox, makeBox, pushBox, reach, walkable, walkPath, type ItemKind, type Level, type State } from './Map19Rules';
import type { Push } from './Map19Solver';
import { advanceEnemies, PLAYER_HEALTH, type Bullet, type DamageSource, type Enemy } from './Map19Enemies';
import { freeBatteries, isPowered, liveBeams, powerEmitter, rotateMirror, slotAt, unpowerEmitter } from './Map19Env';
import { buildRooms, startTrack, trackRooms, type RoomMap, type RoomTrack } from './Map19Rooms';
import type { Rect } from './Map19Generator';
import { BLAST_SHOW, detonate, FLIGHT, FUSE, PICKUP_AMMO, throwTarget, type Ammo, type Blast, type Bomb, type BombKind } from './Map19Bombs';

const MELEE_COOLDOWN = 0.35;
const START_AMMO: Ammo = { time: 1, throw: 0, remote: 0 };

const ENV_LASER_DPS = 2; // standing in an emitter's beam (always-on swept onto you, or a pulse)
const ENV_LASER_ENTER = 1; // a live beam newly on you (you step in, or it switches on / swings onto you)

export type Play = {
    L: Level;
    s: State;
    hasKey: boolean;
    won: boolean;
    pushes: number;
    steps: number;
    message: string;
    enemies: Enemy[];
    bullets: Bullet[];
    caught: boolean; // health ran out
    health: number; // 0..PLAYER_HEALTH (laser damage is fractional)
    hurt: number; // seconds left of the red hurt flash
    time: number; // beat clock (seconds since start): drives pulsing lasers
    zapped: number; // enemies destroyed by lasers
    facing: number; // last direction the player tried to move (melee / throw aim)
    ammo: Ammo;
    bombs: Bomb[];
    blasts: Blast[]; // flames on screen
    items: { cell: number; kind: ItemKind }[]; // hearts / bomb pickups still lying around
    kills: number; // enemies destroyed by the player (melee, bombs)
    meleeCd: number;
    swing: number; // seconds left of the melee swing (drawing)
    inBeam: boolean; // standing in a live emitter beam last tick
    rooms?: RoomMap; // dungeon levels: rooms reset when you walk out of an unsolved one and back in
    track?: RoomTrack;
};

const COLOR_NAMES = ['red', 'green', 'yellow'];

export function startPlay(L: Level, enemies: Enemy[] = [], rects?: Rect[]): Play {
    const rooms = rects?.length ? buildRooms(L, rects) : undefined;
    return {
        rooms, track: rooms ? startTrack(L, rooms, L.start) : undefined,
        L, s: L.start, hasKey: false, won: false, pushes: 0, steps: 0, 
        message: `Find all ${L.golds.length} gold keys 🔑, then the exit door`, enemies, bullets: [], caught: false,
        health: PLAYER_HEALTH, hurt: 0, time: 0, zapped: 0,
        facing: 0, ammo: { ...START_AMMO }, bombs: [], blasts: [], items: L.items, kills: 0, meleeCd: 0, swing: 0, inBeam: false,
    };
}

/**
 * The player steps in `dir` (0 +x, 1 -x, 2 +y, 3 -y). Enemies don't wait for this: they run
 * in real time through tick(). Returns the same object when the step is blocked.
 */
export function step(p: Play, dir: number): Play {
    if (p.won || p.caught) return p;
    if (p.facing !== dir) p = { ...p, facing: dir };
    const target = p.s.player + DX[dir] + DY[dir] * p.L.w;
    if (p.enemies.some((e) => e.cell === target)) return p; // enemies are solid; they bite, you don't
    if (p.bombs.some((b) => b.cell === target && b.flight <= 0)) return { ...p, message: 'A bomb is in the way' };
    let moved = movePlayer(p, dir);
    if (moved === p || moved.s.player === p.s.player) return moved;
    moved = roomReset(moved, moved.pushes > p.pushes, p.s.player);
    const hit = moved.bullets.filter((b) => b.cell === moved.s.player);
    if (!hit.length) return moved;
    return hurt({ ...moved, bullets: moved.bullets.filter((b) => b.cell !== moved.s.player) }, hit.length, 'bullet');
}

/** Dungeon rooms: walking back into an unsolved room puts its crates back (see Map19Rooms). */
function roomReset(p: Play, pushed: boolean, from: number): Play {
    if (!p.rooms || !p.track) return p;
    const blocked = (c: number) => p.enemies.some((e) => e.cell === c) || p.bombs.some((b) => b.cell === c);
    const r = trackRooms(p.L, p.rooms, p.track, p.s, pushed, blocked, p.bombs.map((b) => b.cell), from);
    if (r.t === p.track && !r.reset) return p;
    return { ...p, s: r.s, track: r.t, message: r.reset ? 'Room reset ↺ — crates are back where they started' : p.message };
}

const HOW: Record<DamageSource | 'laser' | 'bomb', string> = { laser: 'Burned by a laser', bomb: 'Caught in your own blast', bullet: 'Shot by a yellow shooter', blue: 'Zapped by a blue laser', red: 'Bitten by a red', yellow: 'Bumped by a yellow', patrol: 'Bumped by a patroller' };

/** Apply damage; out of health = caught. */
function hurt(p: Play, damage: number, by: DamageSource | 'laser' | 'bomb'): Play {
    const health = Math.max(0, p.health - damage);
    if (health <= 0) return { ...p, health, hurt: 0.35, caught: true, message: `${HOW[by]}! Out of hearts — Z to undo or R to restart` };
    return { ...p, health, hurt: damage >= 0.5 ? 0.35 : Math.max(p.hurt, 0.08), message: damage >= 0.5 ? `${HOW[by]}! −${damage} ❤` : p.message };
}

/**
 * Real-time update: the beat clock ticks, enemies patrol / chase / shoot, bullets fly, and
 * emitter beams (always-on + pulsing) burn the player and destroy enemies standing in them.
 */
export function tick(p: Play, dt: number): Play {
    if (p.won || p.caught) return p;
    let next: Play = {
        ...p, time: p.time + dt, hurt: Math.max(0, p.hurt - dt), meleeCd: Math.max(0, p.meleeCd - dt), swing: Math.max(0, p.swing - dt),
        blasts: p.blasts.map((b) => ({ ...b, t: b.t - dt })).filter((b) => b.t > 0),
    };
    if (p.bombs.length) {
        const bombs = p.bombs.map((b) => ({ ...b, flight: Math.max(0, b.flight - dt), fuse: b.kind === 'time' ? b.fuse - dt : b.fuse }));
        const due = bombs.filter((b) => (b.kind === 'time' && b.fuse <= 0) || (b.kind === 'throw' && b.flight <= 0)).map((b) => b.cell);
        next = { ...next, bombs };
        if (due.length) next = explode(next, due);
    }
    if (next.caught) return next;
    if (next.enemies.length) {
        // Bombs on the ground block enemies like crates do.
        const blockers = next.bombs.filter((b) => b.flight <= 0).map((b) => makeBox(b.cell, BOX.crate));
        const sEnemy = blockers.length ? { ...p.s, boxes: [...p.s.boxes, ...blockers].sort((a, b) => a - b) } : p.s;
        const r = advanceEnemies(p.L, sEnemy, next.enemies, next.bullets, dt);
        next = { ...next, enemies: r.enemies, bullets: r.bullets };
        if (r.damage > 0 && r.by) next = hurt(next, r.damage, r.by);
    }
    if (!p.L.emitters.length) return next;
    const hot = new Set<number>();
    const beams = liveBeams(p.L, next.s, next.time);
    for (const b of beams) for (const c of b.path) hot.add(c);
    // Lasers set bombs off: your bombs lying in a beam.
    const lasered = next.bombs.filter((b) => b.flight <= 0 && hot.has(b.cell)).map((b) => b.cell);
    if (lasered.length) next = explode(next, lasered);
    if (next.caught) return next;
    const survivors = next.enemies.filter((e) => !hot.has(e.cell));
    if (survivors.length < next.enemies.length) {
        next = { ...next, enemies: survivors, zapped: next.zapped + next.enemies.length - survivors.length, message: 'Zap! A laser got an enemy' };
    }
    // Lasers are crossable but hurt: 1 heart the moment a beam is on you, then a burn while you stay.
    const inBeam = hot.has(next.s.player);
    next = { ...next, inBeam };
    if (!inBeam || next.caught) return next;
    return hurt(next, p.inBeam ? ENV_LASER_DPS * dt : ENV_LASER_ENTER, 'laser');
}

/** Set off bombs at `centers` (and their chain). */
function explode(p: Play, centers: number[]): Play {
    const d = detonate(p.L, p.s, p.bombs, p.enemies, centers);
    const fallen = p.enemies.filter((e) => !d.enemies.includes(e)).map((e) => e.cell);
    let next: Play = {
        ...p, s: d.s, bombs: d.bombs, enemies: d.enemies, kills: p.kills + d.kills, items: helpDrops(p, fallen),
        blasts: [...p.blasts, { cells: d.cells, t: BLAST_SHOW }],
        message: d.kills ? `Boom! ${d.kills} down` : d.doors ? 'Boom! Blasted a way through' : d.crates ? `Boom! ${d.crates} crate${d.crates > 1 ? 's' : ''} gone` : 'Boom!',
    };
    if (d.playerHit) next = hurt(next, 1, 'bomb');
    return next;
}

/** Low on hearts? Defeated enemies drop a heart where they fell (kids won't notice, it just feels fair). */
function helpDrops(p: Play, cells: number[]): Play['items'] {
    if (p.health > LOW_HEALTH || !cells.length) return p.items;
    return [...p.items, ...cells.filter((c) => !p.items.some((it) => it.cell === c)).map((cell) => ({ cell, kind: 'heart' as const }))];
}

/** Drop a time / remote bomb on your cell. */
export function dropBomb(p: Play, kind: 'time' | 'remote'): Play {
    if (p.won || p.caught) return p;
    if (p.ammo[kind] <= 0) return { ...p, message: `No ${kind} bombs — find a pickup` };
    if (p.bombs.some((b) => b.cell === p.s.player)) return p;
    const bomb: Bomb = { kind, cell: p.s.player, from: p.s.player, fuse: kind === 'time' ? FUSE : Infinity, flight: 0 };
    return { ...p, bombs: [...p.bombs, bomb], ammo: { ...p.ammo, [kind]: p.ammo[kind] - 1 }, message: kind === 'time' ? 'Tick, tick… run!' : 'Remote bomb placed — X to detonate' };
}

/** Throw a bomb ahead (facing); it explodes where it lands. */
export function throwBomb(p: Play): Play {
    if (p.won || p.caught) return p;
    if (p.ammo.throw <= 0) return { ...p, message: 'No throw bombs — find a pickup' };
    const cell = throwTarget(p.L, p.s, p.enemies, p.bombs, p.s.player, p.facing);
    const bomb: Bomb = { kind: 'throw', cell, from: p.s.player, fuse: 0, flight: cell === p.s.player ? 0.01 : FLIGHT };
    return { ...p, bombs: [...p.bombs, bomb], ammo: { ...p.ammo, throw: p.ammo.throw - 1 }, message: 'Catch!' };
}

/** Detonate every remote bomb on the ground. */
export function detonateRemotes(p: Play): Play {
    if (p.won || p.caught) return p;
    const cells = p.bombs.filter((b) => b.kind === 'remote').map((b) => b.cell);
    return cells.length ? explode(p, cells) : { ...p, message: 'No remote bombs out' };
}

/** Melee swipe at the facing cell: 1 damage, knocks the enemy back a cell and stuns it. */
export function melee(p: Play): Play {
    if (p.won || p.caught || p.meleeCd > 0) return p;
    const { L } = p;
    const target = p.s.player + DX[p.facing] + DY[p.facing] * L.w;
    const swung = { ...p, meleeCd: MELEE_COOLDOWN, swing: 0.18 };
    const i = p.enemies.findIndex((e) => e.cell === target);
    if (i < 0) return swung;
    const e = p.enemies[i];
    if (e.hp <= 1) return { ...swung, enemies: p.enemies.filter((_, k) => k !== i), kills: p.kills + 1, items: helpDrops(p, [e.cell]), message: 'Bonk! Got one' };
    const x = (target % L.w) + DX[p.facing];
    const y = Math.floor(target / L.w) + DY[p.facing];
    const back = y * L.w + x;
    const free = x >= 0 && y >= 0 && x < L.w && y < L.h && walkable(L, p.s, back) && !hasBox(p.s, back)
        && !p.enemies.some((o) => o.cell === back) && !p.bombs.some((b) => b.cell === back);
    const hit = { ...e, hp: e.hp - 1, cell: free ? back : e.cell, prev: free ? back : e.cell, stepTimer: e.stepTimer + 0.7, mode: 'chase' as const, beamLen: 0 };
    return { ...swung, enemies: p.enemies.map((o, k) => (k === i ? hit : o)), message: 'Bonk! (one more hit)' };
}

/** The player's half of a turn (push rules, keys, door). Enemies block pushed crates. */
function movePlayer(p: Play, dir: number): Play {
    const { L, s } = p;
    const x = (s.player % L.w) + DX[dir];
    const y = Math.floor(s.player / L.w) + DY[dir];
    if (x < 0 || y < 0 || x >= L.w || y >= L.h) return p;
    const n = y * L.w + x;
    if (L.door.includes(n)) {
        if (!p.hasKey) return { ...p, message: `Locked — find every gold key first (${goldCount(p)}/${L.golds.length})` };
        return { ...p, won: true, message: `Escaped! ${p.steps} steps, ${p.pushes} pushes` };
    }
    if (hasBox(s, n)) {
        // Enemies stop a pushed crate like another crate would.
        const blockers = [...p.enemies, ...p.bombs.filter((b) => b.flight <= 0)].map((o) => makeBox(o.cell, BOX.crate));
        const withEnemies = { ...s, boxes: [...s.boxes, ...blockers].sort((a, b) => a - b) };
        const pushed = pushBox(L, withEnemies, n, dir);
        if (!pushed) return p;
        const r = { ...pushed, next: { ...pushed.next, boxes: pushed.next.boxes.filter((b) => !blockers.includes(b)) } };
        return collect({ ...p, s: r.next, pushes: p.pushes + 1, steps: p.steps + 1 }, n); // a crate may have been sitting on a key
    }
    const slot = slotAt(L, n);
    if (slot >= 0) return { ...p, message: isPowered(s, slot) ? 'Emitter — press E to take its battery out' : 'Empty battery slot — press E to put a battery in 🔋' };
    if (!walkable(L, s, n, false)) {
        if (L.gates.has(n)) return { ...p, message: 'Locked gate — find the matching coloured key' };
        if (L.base[n] === 'H') return { ...p, message: 'Laser gate — light every receiver ◎ with a beam' };
        if (L.base[n] === 'D') return { ...p, message: 'Wooden door — stuck shut (enemies can\'t open it either). Plant a bomb next to it 💣' };
        if (L.base[n] === 'F') return { ...p, message: 'Wooden wall — lasers shine through, but it won\'t break' };
        if (L.base[n] === 'J') return { ...p, message: 'Plate gate — put a crate on every plate' };
        return p;
    }
    const to = arrive(L, s, n); // a teleporter pad sends you out of its partner
    const moved = collect({ ...p, s: { ...s, player: to }, steps: p.steps + 1 }, to);
    return to !== n ? { ...moved, message: moved.message === p.message ? 'Whoosh! Teleported' : moved.message } : moved;
}

const goldCount = (p: Play) => p.L.golds.filter((_, i) => (p.s.gold >> i) & 1).length;

const SUPPLY_AMMO = 3; // a supply crate tops time bombs up to this
const LOW_HEALTH = 1.5; // at or below this, defeated enemies drop a heart
const ITEM_NAME: Record<ItemKind, string> = { supply: `Bomb supply — time bombs topped up to ${SUPPLY_AMMO}`, heart: '❤ +1', time: `+${PICKUP_AMMO} time bombs (B)`, throw: `+${PICKUP_AMMO} throw bombs (T)`, remote: `+${PICKUP_AMMO} remote bombs (C, X to detonate)` };

/** Pick up whatever lies on `n` (the player's new cell): keys, batteries, hearts, bomb ammo. */
function collect(p: Play, n: number): Play {
    const item = p.items.find((it) => it.cell === n);
    if (item?.kind === 'supply') {
        if (p.ammo.time < SUPPLY_AMMO) p = { ...p, ammo: { ...p.ammo, time: SUPPLY_AMMO }, message: ITEM_NAME.supply };
    } else if (item) {
        const rest = p.items.filter((it) => it !== item);
        p = item.kind === 'heart'
            ? { ...p, items: rest, health: Math.min(PLAYER_HEALTH, p.health + 1), message: ITEM_NAME.heart }
            : { ...p, items: rest, ammo: { ...p.ammo, [item.kind]: p.ammo[item.kind as BombKind] + PICKUP_AMMO }, message: ITEM_NAME[item.kind] };
    }
    const bi = p.L.batteries.indexOf(n);
    if (bi >= 0 && !((p.s.batteries >> bi) & 1)) p = { ...p, s: { ...p.s, batteries: p.s.batteries | (1 << bi) }, message: 'Got a battery 🔋 — face an empty emitter slot and press E' };
    const gi = p.L.golds.indexOf(n);
    if (gi >= 0 && !((p.s.gold >> gi) & 1)) {
        const s = { ...p.s, gold: p.s.gold | (1 << gi) };
        const all = allGold(p.L, s);
        const got = p.L.golds.filter((_, i) => (s.gold >> i) & 1).length;
        p = { ...p, s, hasKey: all, message: all ? 'Got every gold key! Now reach the exit door' : `Gold key ${got}/${p.L.golds.length} 🔑` };
    }
    const color = p.L.keys.get(n);
    if (color === undefined || p.s.keys & (1 << color)) return p;
    return { ...p, s: { ...p.s, keys: p.s.keys | (1 << color) }, message: `Got a ${COLOR_NAMES[color]} key — its gates open now` };
}

/** Replay tokens: 0..3 step, FACE + d turn in place, INTERACT press E. */
export const FACE = 4;
export const INTERACT = 8;

/** Apply one replay / input token. */
export function applyToken(p: Play, tok: number): Play {
    return tok === INTERACT ? interact(p) : tok >= FACE ? face(p, tok - FACE) : step(p, tok);
}

/** Turn in place (Shift + arrow): aim melee / throws / E without moving or pushing. */
export function face(p: Play, dir: number): Play {
    return p.won || p.caught || p.facing === dir ? p : { ...p, facing: dir };
}

/**
 * E: interact with the faced cell (or, if nothing there, any neighbour): flip a mirror box,
 * or put a battery into / take it out of a slot emitter.
 */
export function interact(p: Play): Play {
    if (p.won || p.caught) return p;
    const { L, s } = p;
    const order = [p.facing, 0, 1, 2, 3];
    for (const d of order) {
        const x = (s.player % L.w) + DX[d];
        const y = Math.floor(s.player / L.w) + DY[d];
        if (x < 0 || y < 0 || x >= L.w || y >= L.h) continue;
        const n = y * L.w + x;
        const kind = boxAt(s, n);
        if (kind === BOX.mirrorSlash || kind === BOX.mirrorBack) {
            return { ...p, facing: d, s: rotateMirror(s, n), steps: p.steps + 1, message: 'Mirror flipped' };
        }
        const slot = slotAt(L, n);
        if (slot < 0) continue;
        if (isPowered(s, slot)) return { ...p, facing: d, s: unpowerEmitter(s, slot), steps: p.steps + 1, message: 'Took the battery out 🔋 — the emitter goes dark' };
        if (freeBatteries(L, s) > 0) return { ...p, facing: d, s: powerEmitter(s, slot), steps: p.steps + 1, message: 'Battery in — the emitter powers up!' };
        return { ...p, facing: d, message: 'Empty battery slot — bring a battery 🔋' };
    }
    return { ...p, message: 'Nothing to use here (E works on mirrors and emitter slots)' };
}

/**
 * Replay tokens for a solution: its moves (walking to each, fetching keys / batteries / gold
 * the moment they are reachable), then every gold key, then beside the door and bump it.
 * Interactions turn to their target and press E.
 */
export function solutionSteps(L: Level, legs: Push[][]): number[] {
    const dirs: number[] = [];
    let s = L.start;
    const pick = (c: number) => {
        const color = L.keys.get(c);
        if (color !== undefined) s = { ...s, keys: s.keys | (1 << color) };
        const bi = L.batteries.indexOf(c);
        if (bi >= 0) s = { ...s, batteries: s.batteries | (1 << bi) };
        const gi = L.golds.indexOf(c);
        if (gi >= 0) s = { ...s, gold: s.gold | (1 << gi) };
    };
    const walkRaw = (to: number): boolean => {
        const path = walkPath(L, s, to);
        if (!path) return false;
        for (const st of path) {
            dirs.push(st.dir);
            pick(st.cell); // picked up on the way (teleporters: the cell you come out on)
        }
        s = { ...s, player: to };
        pick(to); // also when already standing on it (a pushed crate can uncover a key under you)
        return true;
    };
    // The solver picks up every key the moment it is reachable (free walking), and a later
    // push may cut the way back — so fetch reachable keys eagerly before each walk.
    const walkTo = (to: number): boolean => {
        for (;;) {
            const r = reach(L, s);
            const key = [...L.keys.entries()].find(([cell, color]) => r[cell] && !(s.keys & (1 << color)));
            const battery = L.batteries.find((cell, i) => r[cell] && !((s.batteries >> i) & 1));
            const gold = L.golds.find((cell, i) => r[cell] && !((s.gold >> i) & 1));
            const fetch = key ? key[0] : battery ?? gold;
            if (fetch === undefined) break;
            if (!walkRaw(fetch)) return false;
        }
        const ok = walkRaw(to);
        return ok;
    };
    const pushAll = (moves: Push[]): boolean => {
        for (const m of moves) {
            if (!walkTo(m.cell - DX[m.dir] - DY[m.dir] * L.w)) return false;
            if (m.act) {
                dirs.push(FACE + m.dir, INTERACT); // turn to it, press E
                const i = slotAt(L, m.cell);
                s = m.act === 'rotate' ? rotateMirror(s, m.cell) : m.act === 'power' ? powerEmitter(s, i) : unpowerEmitter(s, i);
                continue;
            }
            const r = pushBox(L, s, m.cell, m.dir);
            if (!r) return false;
            dirs.push(m.dir);
            s = r.next;
            pick(s.player); // stepped into the crate's old cell
        }
        return true;
    };
    for (const leg of legs) if (!pushAll(leg)) return dirs;
    const exit = L.exits.find((e) => walkTo(e)); // walkTo fetches keys if a gate is in the way
    if (exit === undefined) return dirs;
    const door = L.door.find((d) => Math.abs((d % L.w) - (exit % L.w)) + Math.abs(Math.floor(d / L.w) - Math.floor(exit / L.w)) === 1);
    if (door !== undefined) dirs.push(dirBetween(L, exit, door));
    return dirs;
}

export function dirBetween(L: Level, a: number, b: number): number {
    const dx = (b % L.w) - (a % L.w);
    const dy = Math.floor(b / L.w) - Math.floor(a / L.w);
    return dx === 1 ? 0 : dx === -1 ? 1 : dy === 1 ? 2 : 3;
}
