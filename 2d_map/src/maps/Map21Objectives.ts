// Combat Sandbox — what you're here to do, and what you pick up on the way.
//
//   objectives  machines to break (generator, terminal, valve, locker, pump). Some switch a hazard
//               off when they break (the belts, nearby vents, a lift, the drill core's shield);
//               a locker drops a KEY. When every objective (and the core) is broken the EXIT opens.
//   doors       shut and solid until you walk up to one carrying its key.
//   supplies    crates: shoot one open for a joy jar, a heart or a fizz can (2 big bubbles, Q).
//   pickups     lie on the floor and drift to you when you're close; toys sometimes drop joy jars.
import { springOut } from './Map21Bots';
import { wreckLift } from './Map21Hazards';
import { addShake, addStop, banner, burst, floater, ring } from './Map21Juice';
import type { KeyColor } from './Map21Levels';
import type { Arena, PickupKind, Shot, Sim, Vec } from './Map21State';
import { OBJECTIVE_R, SUPPLY_HALF } from './Map21World';

const VALVE_REACH = 9; // a broken valve shuts the vents this close to it
const MAGNET = 2.6; // pickups this close drift to you
const COLLECT = 0.7;
const JAR_JOY = 15;
const FIZZ_PER_CAN = 2;
export const FIZZ_MAX = 6;
const TOY_JAR_CHANCE = 0.12;
const DOOR_REACH = 1.6;
const EXIT_REACH = 1.0;
const TRAP_REACH = 2.8; // a springer jumps out of its crate when you're this close

type Objective = Arena['objectives'][number];
type Supply = Arena['supplies'][number];

export const KEY_COLOR: Record<KeyColor, string> = { red: '#f87171', blue: '#60a5fa' };

export const objectivesLeft = (a: Arena) => a.objectives.filter((o) => !o.broken).length;
export const coreShielded = (a: Arena) => a.objectives.some((o) => o.stops === 'shield' && !o.broken);

// --- hits ------------------------------------------------------------------------------------

/** A friendly shot meeting an objective or a supply crate. True if it stopped. */
export function shotHitsObjective(sim: Sim, shot: Shot): boolean {
    const a = sim.arena;
    for (const o of a.objectives) {
        if (o.broken || Math.hypot(shot.pos.x - o.pos.x, shot.pos.y - o.pos.y) > OBJECTIVE_R + shot.radius) continue;
        shot.life = 0;
        if (!shot.big) damageObjective(sim, o, shot.damage, shot.pos);
        return true;
    }
    for (const s of a.supplies) {
        if (s.broken || Math.abs(shot.pos.x - s.pos.x) > SUPPLY_HALF + shot.radius || Math.abs(shot.pos.y - s.pos.y) > SUPPLY_HALF + shot.radius) continue;
        shot.life = 0;
        if (!shot.big) damageSupply(sim, s, shot.damage);
        return true;
    }
    return false;
}

/** A blast reaching objectives and crates (bombs, mice, fizz bubbles). */
export function blastObjectives(sim: Sim, at: Vec, radius: number, damage: number): void {
    if (damage <= 0) return;
    for (const o of sim.arena.objectives) {
        if (!o.broken && Math.hypot(o.pos.x - at.x, o.pos.y - at.y) < radius + OBJECTIVE_R) damageObjective(sim, o, damage, o.pos);
    }
    for (const s of sim.arena.supplies) {
        if (!s.broken && Math.hypot(s.pos.x - at.x, s.pos.y - at.y) < radius + SUPPLY_HALF) damageSupply(sim, s, damage);
    }
}

function damageObjective(sim: Sim, o: Objective, damage: number, at: Vec): void {
    o.hp -= damage;
    o.flash = 0.08;
    burst(sim, at, 2, '#fbbf24', 'spark', 5, NaN, 0.1);
    if (sim.settings.numbers) floater(sim, at, String(damage), '#fde68a', 0.8);
    if (o.hp <= 0) breakObjective(sim, o);
}

function damageSupply(sim: Sim, s: Supply, damage: number): void {
    s.hp -= damage;
    s.flash = 0.08;
    burst(sim, s.pos, 2, '#d6b98c', 'square', 4, NaN, 0.1);
    if (s.hp > 0) return;
    s.broken = true;
    if (s.trap) { springOut(sim, s.pos); return; }
    burst(sim, s.pos, 14, '#b08850', 'square', 7, NaN, 0.22);
    addShake(sim, 0.15);
    const roll = Math.random();
    dropPickup(sim, s.pos, roll < 0.45 ? 'joy' : roll < 0.8 ? 'fizz' : 'heart');
}

function breakObjective(sim: Sim, o: Objective): void {
    const a = sim.arena;
    o.broken = true;
    o.hp = 0;
    burst(sim, o.pos, 30, '#94a3b8', 'square', 10, NaN, 0.28);
    burst(sim, o.pos, 16, '#fbbf24', 'spark', 11, NaN, 0.14);
    burst(sim, o.pos, 8, 'rgba(120,113,108,0.8)', 'smoke', 2, NaN, 0.7);
    ring(sim, o.pos, 2.6, 'rgba(251,191,36,0.9)', 0.35);
    addShake(sim, 0.6);
    addStop(sim, 4);
    const left = objectivesLeft(a);
    let text = left > 0 ? `${left} TO GO` : '';
    if (o.stops === 'belts') {
        a.beltsOn = false;
        for (const c of a.crushers) c.t = 0; // left hanging open
        text = 'BELTS STOPPED';
    } else if (o.stops === 'vents') {
        for (const v of a.vents) if (Math.hypot(v.pos.x - o.pos.x, v.pos.y - o.pos.y) < VALVE_REACH) v.off = true;
        if (!a.objectives.some((x) => x.stops === 'vents' && !x.broken)) for (const v of a.vents) v.off = true;
        text = 'VENTS SHUT';
    } else if (o.stops === 'lift') {
        const lift = a.lifts.filter((l) => !l.broken).sort((p, q) => dist(p.pos, o.pos) - dist(q.pos, o.pos))[0];
        if (lift) wreckLift(sim, lift);
        text = 'LIFT POWER CUT';
    } else if (o.stops === 'paint') {
        for (const s of a.sprayers) s.off = true;
        text = 'PAINT OFF';
    } else if (o.stops === 'stamps') {
        for (const s of a.stamps) s.off = true;
        text = 'STAMPS STOPPED';
    } else if (o.stops === 'arms') {
        a.armsOn = false;
        text = 'ARMS OFFLINE';
    } else if (o.stops === 'shield' && !coreShielded(a)) {
        banner(sim, 'SHIELD DOWN', 'the drill core can be hurt — break it');
    }
    if (o.drops) {
        dropPickup(sim, o.pos, 'key', o.drops);
        text = `${o.drops.toUpperCase()} KEY DROPPED`;
    }
    if (text) floater(sim, o.pos, text, '#fde047', 1.4);
}

// --- pickups -----------------------------------------------------------------------------------

export function dropPickup(sim: Sim, at: Vec, kind: PickupKind, color?: KeyColor): void {
    const a = Math.random() * Math.PI * 2;
    sim.pickups.push({ pos: { ...at }, vel: { x: Math.cos(a) * 3, y: Math.sin(a) * 3 }, kind, color, t: 0 });
}

/** A broken toy sometimes leaves a joy jar behind. */
export function toyDrop(sim: Sim, at: Vec): void {
    if (Math.random() < TOY_JAR_CHANCE) dropPickup(sim, at, 'joy');
}

function collect(sim: Sim, kind: PickupKind, color: KeyColor | undefined): void {
    const p = sim.player;
    const s = sim.settings;
    if (kind === 'joy') {
        p.joy = Math.min(s.maxJoy, p.joy + JAR_JOY);
        floater(sim, p.pos, `+${JAR_JOY} joy`, '#a5f3fc', 1);
    } else if (kind === 'heart') {
        if (p.hp < s.playerHp) { p.hp++; floater(sim, p.pos, '+♥', '#f472b6', 1.2); }
        else { p.joy = Math.min(s.maxJoy, p.joy + 8); floater(sim, p.pos, '+joy', '#a5f3fc', 1); }
    } else if (kind === 'fizz') {
        p.fizz = Math.min(FIZZ_MAX, p.fizz + FIZZ_PER_CAN);
        floater(sim, p.pos, `+${FIZZ_PER_CAN} FIZZ (Q)`, '#c4b5fd', 1.2);
    } else if (color) {
        p.keys.push(color);
        banner(sim, `${color.toUpperCase()} KEY`, `opens the ${color} door`);
    }
    ring(sim, p.pos, 1.2, 'rgba(255,255,255,0.7)', 0.2);
}

// --- every step ----------------------------------------------------------------------------------

export function stepObjectives(sim: Sim, dt: number): void {
    const a = sim.arena;
    const p = sim.player;
    for (const o of a.objectives) o.flash = Math.max(0, o.flash - dt);
    for (const s of a.supplies) s.flash = Math.max(0, s.flash - dt);
    // A trap crate bursts open when you come close.
    for (const s of a.supplies) {
        if (s.trap && !s.broken && sim.phase === 'fight' && Math.hypot(p.pos.x - s.pos.x, p.pos.y - s.pos.y) < TRAP_REACH) {
            s.broken = true;
            springOut(sim, s.pos);
        }
    }

    for (const k of sim.pickups) {
        k.t += dt;
        const dx = p.pos.x - k.pos.x;
        const dy = p.pos.y - k.pos.y;
        const d = Math.hypot(dx, dy);
        if (d < COLLECT && k.t > 0.3) {
            k.t = -1;
            collect(sim, k.kind, k.color);
            continue;
        }
        if (d < MAGNET && k.t > 0.3) {
            k.vel.x += (dx / d) * 40 * dt;
            k.vel.y += (dy / d) * 40 * dt;
        }
        k.vel.x *= Math.exp(-dt * 5);
        k.vel.y *= Math.exp(-dt * 5);
        k.pos.x += k.vel.x * dt;
        k.pos.y += k.vel.y * dt;
    }
    sim.pickups = sim.pickups.filter((k) => k.t >= 0);

    // Doors: walk up with the key and it slides away (the key is used up).
    for (const door of a.doors) {
        if (door.open > 0) { door.open = Math.min(1, door.open + dt * 2); continue; }
        if (rectDistance(p.pos, door) > DOOR_REACH) continue;
        const i = p.keys.indexOf(door.key);
        if (i < 0) {
            if (Math.random() < dt * 1.5) floater(sim, { x: door.x + door.w / 2, y: door.y }, `needs the ${door.key} key`, KEY_COLOR[door.key], 0.9);
            continue;
        }
        p.keys.splice(i, 1);
        door.open = 0.01;
        burst(sim, { x: door.x + door.w / 2, y: door.y + door.h / 2 }, 14, KEY_COLOR[door.key], 'spark', 7, NaN, 0.15);
        addShake(sim, 0.2);
        floater(sim, p.pos, 'UNLOCKED', KEY_COLOR[door.key], 1.3);
    }

    // The exit opens once everything is broken.
    const coreDone = !a.core || a.core.hp <= 0;
    if (!a.exitOpen && coreDone && objectivesLeft(a) === 0) {
        a.exitOpen = true;
        banner(sim, 'EXIT OPEN', 'get to the green gate');
        const e = a.def.exit;
        burst(sim, { x: e.x + e.w / 2, y: e.y + e.h / 2 }, 20, '#4ade80', 'spark', 8, NaN, 0.2);
    }
}

/** The player is standing in the open exit. */
export function atExit(sim: Sim): boolean {
    return sim.arena.exitOpen && rectDistance(sim.player.pos, sim.arena.def.exit) < EXIT_REACH;
}

/** Where to go next (for the off-screen arrows and the bot): the objectives, then the exit. */
export function nextTargets(sim: Sim): Vec[] {
    const a = sim.arena;
    if (a.exitOpen) return [{ x: a.def.exit.x + a.def.exit.w / 2, y: a.def.exit.y + a.def.exit.h / 2 }];
    const left = a.objectives.filter((o) => !o.broken);
    // On a route (zoned level) point at the next machine along it, not the nearest through a wall.
    const out = (a.def.zones && left.length > 0 ? left.slice(0, 1) : left).map((o) => o.pos);
    if (out.length === 0 && a.core && a.core.hp > 0) out.push(a.core.pos);
    return out;
}

function rectDistance(p: Vec, r: { x: number; y: number; w: number; h: number }): number {
    const nx = Math.max(r.x, Math.min(p.x, r.x + r.w));
    const ny = Math.max(r.y, Math.min(p.y, r.y + r.h));
    return Math.hypot(p.x - nx, p.y - ny);
}

const dist = (p: Vec, q: Vec) => Math.hypot(p.x - q.x, p.y - q.y);
