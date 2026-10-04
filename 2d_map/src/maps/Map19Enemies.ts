// Puzzle Islands — real-time grid enemies. They run on their own clocks (seconds), not in
// lockstep with the player: patrol until they notice you, then chase / attack.
//
//   Patroller (p, orange)  walks a line, turns around when blocked. Touch = caught.
//   Red    (m)  melee · dumb · slow · short chase (2 cells): plods straight at you on the
//               main axis; if that cell is blocked it just stands there (easy to wall off),
//               and it forgets you the moment you leave its circle.
//   Yellow (s)  sentry shooter: patrols its line and only fires STRAIGHT AHEAD — when you
//               stand in front of it (the way it faces, clear line, ≤ 5) it stops and shoots.
//               It never turns to aim or chases: sneak up from behind or the side.
//   Blue   (l)  medium-range laser · intelligent · chase 6: hunts you with real
//               pathfinding; with a clear line of sight within 3 (ANY angle, 360°) it stops,
//               CHARGES (warning line), then ZAPS CONTINUOUSLY, the beam swinging after you
//               (turn-rate limited) for as long as you stay in range and in sight.
// Ranges are Euclidean circles (drawn around each enemy): outer = chase, dashed = attack.
// Damage (player has PLAYER_HEALTH): bullet 1 · melee bump 1 per attack (any enemy, with a
// cooldown) · laser LASER_DPS per second while you stand in the beam.
// Crates, walls, closed gates and the door block movement, bullets and beams. Everything is on
// grid cells; motion is smoothed only when drawn.
import { DX, DY, hasBox, tileAt, type Level, type State } from './Map19Rules';
import { envOf, laserGateOpen } from './Map19Env';

export type EnemyKind = 'patrol' | 'red' | 'yellow' | 'blue';
export type EnemyMode = 'patrol' | 'chase' | 'shoot' | 'charge' | 'beam';
export type Enemy = {
    kind: EnemyKind;
    cell: number;
    prev: number; // cell before the last step (for smooth drawing)
    dir: number;
    mode: EnemyMode;
    stepTimer: number; // seconds until it may step again
    stepPeriod: number; // seconds per step in the current mode
    actTimer: number; // charge / beam countdown; otherwise the bite-lunge flash
    cooldown: number; // until it may shoot / charge again
    beamAngle: number; // radians, locked when the charge starts
    beamLen: number; // beam length in cells (0 = no beam)
    alert: number; // seconds since it last noticed the player (keeps chasing a little while)
    hp: number; // melee hits left (bombs and lasers kill outright)
};
export type Bullet = { cell: number; prev: number; dir: number; timer: number };

type Spec = { chase: number; patrolStep: number; chaseStep: number; reach: number; hp: number; color: string };
export const ENEMY_SPEC: Record<EnemyKind, Spec> = {
    patrol: { chase: 0, patrolStep: 0.45, chaseStep: 0.45, reach: 0, hp: 1, color: '#ff9a3c' },
    red: { chase: 2, patrolStep: 0.9, chaseStep: 0.7, reach: 1, hp: 2, color: '#ff4d4d' },
    yellow: { chase: 0, patrolStep: 0.6, chaseStep: 0.6, reach: 5, hp: 1, color: '#ffd23f' }, // sentry: no chase
    blue: { chase: 6, patrolStep: 0.6, chaseStep: 0.42, reach: 3, hp: 2, color: '#4aa8ff' },
};
export const BULLET_STEP = 0.2; // seconds per cell (slow enough to sidestep)
const SHOT_COOLDOWN = 0.9;
export const PLAYER_HEALTH = 3;
const BULLET_DAMAGE = 1;
const MELEE_DAMAGE = 1;
const MELEE_COOLDOWN = 1.0; // seconds between bites
const LASER_DPS = 0.5; // damage per second inside the beam
const CHARGE_TIME = 0.7;
const BEAM_TURN = 3.0; // rad/s the beam can swing to follow you
const LASER_COOLDOWN = 0.6; // after losing you, before it can charge again
const FORGET_AFTER = 2.0; // keeps hunting this long after losing you (no flicker at the range edge)
const FORGET_MARGIN = 3; // ... while within chase range + this

const xOf = (L: Level, c: number) => c % L.w;
const yOf = (L: Level, c: number) => Math.floor(c / L.w);

function neighbour(L: Level, cell: number, dir: number): number {
    const x = xOf(L, cell) + DX[dir];
    const y = yOf(L, cell) + DY[dir];
    return x < 0 || y < 0 || x >= L.w || y >= L.h ? -1 : y * L.w + x;
}

/**
 * Enemy may stand here: ground (keys' cells, plates fine), no crate, no other enemy. They
 * keep out of always-on laser beams — except red, which is too dumb and walks right in.
 */
function standable(L: Level, s: State, enemies: Enemy[], cell: number, self: number): boolean {
    const c = tileAt(L, s, cell); // a blasted wooden door is floor
    const ground = c === '.' || c === 'K' || c === '_';
    if (!ground || hasBox(s, cell) || enemies.some((e, i) => i !== self && e.cell === cell)) return false;
    return enemies[self].kind === 'red' || !envOf(L, s).beam?.[cell];
}

/** Bullets / beams pass over ground and liquids, not walls, crates, closed gates or doors. */
export function transparent(L: Level, s: State, cell: number, laser = false): boolean {
    const c = tileAt(L, s, cell);
    if (c === 'F') return laser; // wooden wall: lasers pass, bullets don't
    if (c === '#' || c === 'D' || c === 'M' || c === 'O' || c === 'Q' || hasBox(s, cell)) return false;
    if (c === 'L') return (s.keys & (1 << L.gates.get(cell)!)) !== 0;
    if (c === 'H') return laserGateOpen(L, s, cell);
    if (c === 'J') return envOf(L, s).plateOpen;
    return true;
}

/** Direction to the player if they share a row / column within `reach` with a clear line. */
function sees(L: Level, s: State, from: number, reach: number): number {
    const dx = xOf(L, s.player) - xOf(L, from);
    const dy = yOf(L, s.player) - yOf(L, from);
    if ((dx !== 0 && dy !== 0) || (dx === 0 && dy === 0) || Math.abs(dx) + Math.abs(dy) > reach) return -1;
    const dir = dx > 0 ? 0 : dx < 0 ? 1 : dy > 0 ? 2 : 3;
    for (let c = neighbour(L, from, dir); c >= 0; c = neighbour(L, c, dir)) {
        if (c === s.player) return dir;
        if (!transparent(L, s, c)) return -1;
    }
    return -1;
}

/** Euclidean distance in cells between two cell centres. */
export const dist = (L: Level, a: number, b: number) => Math.hypot(xOf(L, a) - xOf(L, b), yOf(L, a) - yOf(L, b));

const cellAt = (L: Level, x: number, y: number) => {
    const ix = Math.floor(x), iy = Math.floor(y);
    return ix < 0 || iy < 0 || ix >= L.w || iy >= L.h ? -1 : iy * L.w + ix;
};

/** Clear straight line (any angle) between two cell centres: marches in small steps. */
function lineOfSight(L: Level, s: State, a: number, b: number): boolean {
    const x0 = xOf(L, a) + 0.5, y0 = yOf(L, a) + 0.5;
    const x1 = xOf(L, b) + 0.5, y1 = yOf(L, b) + 0.5;
    const n = Math.ceil(Math.hypot(x1 - x0, y1 - y0) / 0.1);
    for (let k = 1; k < n; k++) {
        const c = cellAt(L, x0 + ((x1 - x0) * k) / n, y0 + ((y1 - y0) * k) / n);
        if (c !== a && c !== b && (c < 0 || !transparent(L, s, c, true))) return false;
    }
    return true;
}

/** How far (cells) a beam at `angle` from the centre of `from` travels before something opaque. */
function beamLength(L: Level, s: State, from: number, angle: number, reach: number): number {
    const x0 = xOf(L, from) + 0.5, y0 = yOf(L, from) + 0.5;
    const ux = Math.cos(angle), uy = Math.sin(angle);
    for (let t = 0.1; t <= reach; t += 0.1) {
        const c = cellAt(L, x0 + ux * t, y0 + uy * t);
        if (c !== from && (c < 0 || !transparent(L, s, c, true))) return t;
    }
    return reach;
}

/** Player's cell centre within `half` cells of the beam segment? */
function beamHits(L: Level, s: State, e: Enemy, half = 0.45): boolean {
    const px = xOf(L, s.player) - xOf(L, e.cell), py = yOf(L, s.player) - yOf(L, e.cell);
    const along = px * Math.cos(e.beamAngle) + py * Math.sin(e.beamAngle);
    const side = -px * Math.sin(e.beamAngle) + py * Math.cos(e.beamAngle);
    return along > 0 && along <= e.beamLen + half && Math.abs(side) <= half;
}

const angleTo = (L: Level, a: number, b: number) => Math.atan2(yOf(L, b) - yOf(L, a), xOf(L, b) - xOf(L, a));

/** Rotate `from` toward `to` by at most `max` radians (shortest way round). */
function turnTowards(from: number, to: number, max: number): number {
    const d = Math.atan2(Math.sin(to - from), Math.cos(to - from));
    return Math.abs(d) <= max ? to : from + Math.sign(d) * max;
}

/** Directions toward `to`, main axis first. */
function towards(L: Level, from: number, to: number): number[] {
    const dx = xOf(L, to) - xOf(L, from);
    const dy = yOf(L, to) - yOf(L, from);
    const h = dx > 0 ? 0 : dx < 0 ? 1 : -1;
    const v = dy > 0 ? 2 : dy < 0 ? 3 : -1;
    return (Math.abs(dx) >= Math.abs(dy) ? [h, v] : [v, h]).filter((d) => d >= 0);
}

/** First step of a shortest walkable path to a cell where `goal` holds (BFS ≤ limit), or -1. */
function pathTo(L: Level, s: State, enemies: Enemy[], i: number, limit: number, goal: (cell: number) => boolean): number {
    const start = enemies[i].cell;
    const first = new Map<number, number>([[start, -1]]);
    let frontier = [start];
    for (let depth = 0; depth < limit && frontier.length; depth++) {
        const next: number[] = [];
        for (const c of frontier) {
            for (let d = 0; d < 4; d++) {
                const n = neighbour(L, c, d);
                if (n < 0 || first.has(n)) continue;
                const step = c === start ? d : first.get(c)!;
                if (n === s.player || (standable(L, s, enemies, n, i) && goal(n))) return step;
                if (!standable(L, s, enemies, n, i)) continue;
                first.set(n, step);
                next.push(n);
            }
        }
        frontier = next;
    }
    return -1;
}

/** First step of a shortest walkable path to the player, or -1. */
const pathStep = (L: Level, s: State, enemies: Enemy[], i: number, limit: number) => pathTo(L, s, enemies, i, limit, () => false);

/**
 * The direction this enemy wants to step now (or -1 to stay). Never steps straight back to
 * the cell it just left unless that's the only way — this stops left-right jitter when a
 * wall sits between it and the player.
 */
function chooseStep(L: Level, s: State, enemies: Enemy[], i: number): number {
    const e = enemies[i];
    const ok = (d: number) => {
        const n = neighbour(L, e.cell, d);
        return n >= 0 && (n === s.player || standable(L, s, enemies, n, i));
    };
    const notBack = (d: number) => neighbour(L, e.cell, d) !== e.prev;
    if (e.mode === 'patrol') {
        if (ok(e.dir)) return e.dir;
        e.dir ^= 1; // 0↔1, 2↔3: turn around (this step idle)
        return -1;
    }
    if (e.kind === 'red') {
        const d = towards(L, e.cell, s.player)[0]; // stupid: main axis only, no plan B
        return d !== undefined && ok(d) ? d : -1;
    }
    if (e.kind === 'yellow') {
        // A shooter hunts for a FIRING SPOT, not for you: walk (pathfinding) to the nearest
        // cell that has a clear row / column shot at you; fall back to closing in.
        const reach = ENEMY_SPEC.yellow.reach;
        const spot = pathTo(L, s, enemies, i, ENEMY_SPEC.yellow.chase * 3, (c) => sees(L, s, c, reach) >= 0);
        if (spot >= 0 && ok(spot)) return spot;
        const dirs = [...towards(L, e.cell, s.player), 0, 1, 2, 3];
        return dirs.find((d) => ok(d) && notBack(d)) ?? dirs.find(ok) ?? -1;
    }
    return pathStep(L, s, enemies, i, ENEMY_SPEC[e.kind].chase * 3); // blue: real pathfinding
}

export type DamageSource = EnemyKind | 'bullet';
export type TickResult = { enemies: Enemy[]; bullets: Bullet[]; damage: number; by?: DamageSource };

/** Advance enemies and bullets by `dt` seconds. */
export function advanceEnemies(L: Level, s: State, enemies0: Enemy[], bullets0: Bullet[], dt: number): TickResult {
    const enemies = enemies0.map((e) => ({ ...e }));
    const bullets: Bullet[] = [];
    let by: DamageSource | undefined;
    let damage = 0;

    // Bullets fly one cell per BULLET_STEP; stopped by anything opaque.
    for (const b0 of bullets0) {
        const b = { ...b0, timer: b0.timer - dt };
        let alive = true;
        while (alive && b.timer <= 0) {
            b.timer += BULLET_STEP;
            const n = neighbour(L, b.cell, b.dir);
            if (n < 0 || !transparent(L, s, n)) alive = false;
            else { b.prev = b.cell; b.cell = n; }
        }
        if (!alive) continue;
        if (b.cell === s.player) { damage += BULLET_DAMAGE; by = 'bullet'; } // bullet is spent
        else bullets.push(b);
    }

    for (let i = 0; i < enemies.length; i++) {
        const e = enemies[i];
        const spec = ENEMY_SPEC[e.kind];
        e.stepTimer -= dt;
        e.cooldown -= dt;
        if (e.mode !== 'charge' && e.mode !== 'beam') e.actTimer = Math.max(0, e.actTimer - dt); // bite flash

        // Blue laser: charge (warning) → continuous beam while you're in range and in sight.
        if (e.mode === 'charge' || e.mode === 'beam') {
            if (dist(L, e.cell, s.player) > spec.reach || !lineOfSight(L, s, e.cell, s.player)) {
                e.mode = 'chase'; // lost you: beam off
                e.cooldown = LASER_COOLDOWN;
                e.beamLen = 0;
                e.actTimer = 0;
            } else {
                e.beamAngle = turnTowards(e.beamAngle, angleTo(L, e.cell, s.player), BEAM_TURN * dt);
                e.beamLen = beamLength(L, s, e.cell, e.beamAngle, spec.reach); // a crate pushed in blocks it
                if (e.mode === 'charge' && (e.actTimer -= dt) <= 0) e.mode = 'beam';
                if (e.mode === 'beam' && beamHits(L, s, e)) { damage += LASER_DPS * dt; by = e.kind; }
                continue;
            }
        }

        // Perception: who sees / senses the player now.
        const d2p = dist(L, e.cell, s.player);
        // Yellow only shoots where it is already facing — it never turns to aim.
        const seen = e.kind === 'yellow' ? sees(L, s, e.cell, spec.reach) : -1;
        const lineDir = seen === e.dir ? seen : -1;
        const inSight = e.kind === 'blue' && d2p <= spec.reach && lineOfSight(L, s, e.cell, s.player); // laser: any angle
        if (e.kind === 'yellow' && lineDir >= 0) {
            e.mode = 'shoot';
            e.dir = lineDir;
            if (e.cooldown <= 0) {
                bullets.push({ cell: e.cell, prev: e.cell, dir: lineDir, timer: 0 });
                e.cooldown = SHOT_COOLDOWN;
            }
            continue; // stands still while shooting
        }
        if (inSight && e.cooldown <= 0) {
            e.mode = 'charge';
            e.prev = e.cell; // stop gliding
            e.beamAngle = angleTo(L, e.cell, s.player);
            e.actTimer = CHARGE_TIME;
            e.beamLen = beamLength(L, s, e.cell, e.beamAngle, spec.reach);
            continue;
        }
        // Notice the player within chase range; keep hunting a while after losing them.
        e.alert = spec.chase > 0 && (d2p <= spec.chase || lineDir >= 0 || inSight) ? 0 : e.alert + dt;
        const remembers = e.kind !== 'red' && e.mode !== 'patrol' && e.alert < FORGET_AFTER && d2p <= spec.chase + FORGET_MARGIN; // red: no memory
        const hunting = spec.chase > 0 && (e.alert === 0 || remembers);
        // Yellow / blue only hunt when there is a walking route to you (a wall between = keep
        // patrolling instead of pacing against it). Red is stupid: it tries anyway, then stands.
        const reachable = e.kind === 'red' || !hunting || (e.kind === 'yellow'
            ? pathTo(L, s, enemies, i, spec.chase * 3, (c) => sees(L, s, c, spec.reach) >= 0) >= 0
            : pathStep(L, s, enemies, i, spec.chase * 3) >= 0);
        e.mode = hunting && reachable ? 'chase' : 'patrol';
        e.stepPeriod = e.mode === 'chase' ? spec.chaseStep : spec.patrolStep;

        if (e.stepTimer > 0) continue;
        e.stepTimer += e.stepPeriod; // carry the remainder: steady walking speed
        if (e.stepTimer < 0) e.stepTimer = e.stepPeriod;
        const d = chooseStep(L, s, enemies, i);
        if (d < 0) {
            e.prev = e.cell; // standing still: nothing to animate (no jump-back jitter)
            continue;
        }
        const n = neighbour(L, e.cell, d);
        e.prev = e.cell;
        e.dir = d;
        if (n === s.player) {
            // Bump into the player = a melee attack (stays in its cell; one bite per cooldown).
            if (e.cooldown <= 0) { damage += MELEE_DAMAGE; by = e.kind; e.cooldown = MELEE_COOLDOWN; e.actTimer = 0.25; }
            continue;
        }
        e.cell = n;
    }
    return { enemies, bullets, damage, by };
}

const CHAR_KIND: Record<string, EnemyKind> = { p: 'patrol', m: 'red', s: 'yellow', l: 'blue' };

/** Initial enemies from the layout; each patrols along its longer free axis. */
export function parseEnemies(L: Level, rows: string[]): Enemy[] {
    const enemies: Enemy[] = [];
    for (let y = 0; y < rows.length; y++) {
        for (let x = 0; x < rows[y].length; x++) {
            const kind = CHAR_KIND[rows[y][x]];
            if (!kind) continue;
            const cell = y * L.w + x;
            const run = (d: number) => {
                let n = 0;
                for (let c = neighbour(L, cell, d); c >= 0 && L.base[c] === '.'; c = neighbour(L, c, d)) n++;
                return n;
            };
            const horizontal = run(0) + run(1) >= run(2) + run(3);
            enemies.push({
                kind, cell, prev: cell, dir: horizontal ? 0 : 2, mode: 'patrol',
                stepTimer: (cell % 7) * 0.06 /* stagger: not all in unison */, stepPeriod: ENEMY_SPEC[kind].patrolStep, actTimer: 0, cooldown: 0, beamAngle: 0, beamLen: 0, alert: 99, hp: ENEMY_SPEC[kind].hp,
            });
        }
    }
    return enemies;
}
