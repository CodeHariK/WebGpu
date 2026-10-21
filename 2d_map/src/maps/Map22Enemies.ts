// Wick — the things in the dark. Each is a small state machine, and every one of them is about light.
//
//   shade   rises out of the water and LURKS just outside your circle of light, circling with the
//           others. It can't stand light: it burns in it and flees. To get you it has to come in —
//           its eyes flare (the tell), then it LUNGES straight through and melts wax off you, then
//           pulls back to the dark. The smaller your light, the closer it lurks.
//   moth    flies to the brightest thing near it. That's usually you — it clings and drinks your
//           wax (and dims you) until you dash it off or flare. But a lit lantern or a fresh ember
//           nearby is brighter: it'll circle that instead, harmless — and singe itself to death.
//   weeper  tall and pale, it only moves while it's unseen in the dark. Lit by a lantern or an ember
//           it freezes, and so it does in your own light — but only while you face it (your
//           light behind you doesn't count). Only frozen can it be hurt. Turn away to fight
//           something else, with nothing lit behind you, and it's on you.
import { burst, floater, ring, shake } from './Map22Fx';
import { awayFromLight, EMBER_R, type Light, lightAt, playerRadius, seenLightAt } from './Map22Light';
import { hurtPlayer, PLAYER_R } from './Map22Player';
import type { Enemy, EnemyKind, Sim, Vec } from './Map22State';
import { keepOnGround } from './Map22World';
import { burstLeech, gloom, hand, handUp, knockSnuffer, leech, release, snuffer } from './Map22Creatures';

export const RADIUS: Record<EnemyKind, number> = { shade: 0.5, moth: 0.22, weeper: 0.55, snuffer: 0.5, hand: 0.45, gloom: 0.45, leech: 0.3 };
const HP: Record<EnemyKind, number> = { shade: 5, moth: 1, weeper: 14, snuffer: 7, hand: 6, gloom: 4, leech: 2 };
const WAX_BACK: Record<EnemyKind, number> = { shade: 5, moth: 1, weeper: 25, snuffer: 8, hand: 6, gloom: 6, leech: 2 };
const START: Record<EnemyKind, string> = { shade: 'lurk', moth: 'fly', weeper: 'stalk', snuffer: 'stalk', hand: 'under', gloom: 'drift', leech: 'crawl' };
export const RISE_TIME = 0.9;
export const TELL_TIME = 0.5;
const LUNGE_SPEED = 13;
const LUNGE_TIME = 0.45;

const dist = (a: Vec, b: Vec) => Math.hypot(a.x - b.x, a.y - b.y);
const angleTo = (a: Vec, b: Vec) => Math.atan2(b.y - a.y, b.x - a.x);

export function spawnEnemy(sim: Sim, kind: EnemyKind, at: Vec, clearing: number): Enemy {
    const hp = HP[kind];
    const e: Enemy = {
        id: sim.nextId++, kind, pos: { ...at }, vel: { x: 0, y: 0 }, hp, maxHp: hp, state: START[kind],
        t: Math.random(), facing: 0, flash: 0, spawnT: RISE_TIME, lit: 0, target: null, clearing, eaten: 0, home: null,
    };
    sim.enemies.push(e);
    burst(sim, at, 10, 'rgba(30,27,46,0.9)', 'smoke', 2, NaN, 0.35);
    return e;
}

export function stepEnemies(sim: Sim, all: Light[], dt: number): void {
    for (const e of sim.enemies) {
        e.t += dt;
        e.flash -= dt;
        e.lit = e.kind === 'weeper' ? seenLightAt(all, e.pos, sim.player.aim) : lightAt(all, e.pos);
        if (e.spawnT > 0) { e.spawnT -= dt; continue; }
        if (e.kind === 'shade') shade(sim, e, all, dt);
        else if (e.kind === 'moth') moth(sim, e, dt);
        else if (e.kind === 'weeper') weeper(sim, e, dt);
        else if (e.kind === 'snuffer') snuffer(sim, e, dt);
        else if (e.kind === 'hand') hand(sim, e, dt);
        else if (e.kind === 'gloom') gloom(sim, e, dt);
        else leech(sim, e, dt);
        e.pos.x += e.vel.x * dt;
        e.pos.y += e.vel.y * dt;
        e.vel.x *= Math.exp(-dt * 6);
        e.vel.y *= Math.exp(-dt * 6);
    }
    const dead = sim.enemies.filter((e) => e.hp <= 0);
    if (dead.length) {
        sim.enemies = sim.enemies.filter((e) => e.hp > 0);
        for (const e of dead) die(sim, e);
    }
}

function setState(e: Enemy, state: string): void {
    e.state = state;
    e.t = 0;
}

function moveToward(e: Enemy, to: Vec, speed: number, dt: number): void {
    const d = dist(e.pos, to);
    if (d < 0.05) return;
    const step = Math.min(d, speed * dt);
    e.pos.x += ((to.x - e.pos.x) / d) * step;
    e.pos.y += ((to.y - e.pos.y) / d) * step;
    e.facing = angleTo(e.pos, to);
}

// --- shade -------------------------------------------------------------------------------------

function shade(sim: Sim, e: Enemy, all: Light[], dt: number): void {
    const s = sim.settings;
    const p = sim.player.pos;
    // Light burns it (only the bright part of a light; the soft edge it can stand).
    const burn = Math.max(0, (e.lit - 0.25) / 0.75);
    if (burn > 0) {
        e.hp -= s.burnRate * burn * dt;
        if (Math.random() < burn * 0.6) burst(sim, e.pos, 1, Math.random() < 0.5 ? '#f97316' : 'rgba(30,27,46,0.9)', 'smoke', 1.5, -Math.PI / 2, 0.2);
    }
    switch (e.state) {
        case 'lurk': {
            if (burn > 0.15) { // too bright: get out
                const a = awayFromLight(all, e.pos);
                moveToward(e, { x: e.pos.x + Math.cos(a), y: e.pos.y + Math.sin(a) }, s.shadeSpeed * 1.4, dt);
                break;
            }
            const ring = playerRadius(sim) + 1.3;
            const around = angleTo(p, e.pos) + dt * 0.5 * (e.id % 2 ? 1 : -1); // circle slowly
            moveToward(e, { x: p.x + Math.cos(around) * ring, y: p.y + Math.sin(around) * ring }, s.shadeSpeed, dt);
            e.facing = angleTo(e.pos, p);
            const others = sim.enemies.filter((o) => o.kind === 'shade' && (o.state === 'tell' || o.state === 'lunge')).length;
            if (e.t > 1.6 + (e.id % 4) * 0.5 && others < 2 && dist(e.pos, p) < ring + 2.5) setState(e, 'tell');
            break;
        }
        case 'tell':
            e.facing = angleTo(e.pos, p);
            if (e.t > TELL_TIME) { e.target = { x: Math.cos(e.facing), y: Math.sin(e.facing) }; setState(e, 'lunge'); }
            break;
        case 'lunge': {
            const d = e.target!;
            e.pos.x += d.x * LUNGE_SPEED * dt;
            e.pos.y += d.y * LUNGE_SPEED * dt;
            if (Math.random() < 0.6) burst(sim, e.pos, 1, 'rgba(30,27,46,0.8)', 'smoke', 1, NaN, 0.25);
            if (dist(e.pos, p) < RADIUS.shade + PLAYER_R) { hurtPlayer(sim, s.shadeHit, Math.atan2(d.y, d.x), 8); setState(e, 'retreat'); break; }
            if (e.t > LUNGE_TIME) setState(e, 'retreat');
            break;
        }
        case 'retreat': {
            const a = awayFromLight(all, e.pos);
            moveToward(e, { x: e.pos.x + Math.cos(a), y: e.pos.y + Math.sin(a) }, s.shadeSpeed * 1.2, dt);
            if (e.t > 1.1) setState(e, 'lurk');
            break;
        }
    }
}

// --- moth --------------------------------------------------------------------------------------

/** What a moth is drawn to: a lit lantern or ember near it beats you. */
function mothTarget(sim: Sim, e: Enemy): { pos: Vec; you: boolean } {
    let best: Vec | null = null;
    let bestD = Infinity;
    for (const l of sim.lanterns) {
        if (!l.lit) continue;
        const d = dist(l.pos, e.pos);
        if (d < 8 && d < bestD) { bestD = d; best = l.pos; }
    }
    for (const em of sim.embers) {
        const d = dist(em.pos, e.pos);
        if (d < EMBER_R + 2.5 && d < bestD) { bestD = d; best = em.pos; }
    }
    return best ? { pos: best, you: false } : { pos: sim.player.pos, you: true };
}

function moth(sim: Sim, e: Enemy, dt: number): void {
    const p = sim.player;
    if (p.flareT > 0.6 && dist(e.pos, p.pos) < sim.settings.flareRadius) { e.hp = 0; return; }
    switch (e.state) {
        case 'fly': {
            const tgt = mothTarget(sim, e);
            const wob = Math.sin(sim.time * 9 + e.id) * 1.2;
            const a = angleTo(e.pos, tgt.pos) + wob * 0.5;
            const d = dist(e.pos, tgt.pos);
            const orbit = !tgt.you && d < 1.6; // circling a lantern or ember
            const dir = orbit ? angleTo(tgt.pos, e.pos) + Math.PI / 2 : a;
            if (orbit) e.hp -= dt * 0.3; // too close to the flame: it singes and drops after a few turns
            e.pos.x += Math.cos(dir) * 5.5 * dt;
            e.pos.y += Math.sin(dir) * 5.5 * dt;
            e.facing = dir;
            if (tgt.you && d < PLAYER_R + 0.25 && p.dashT <= 0) { e.target = { x: e.pos.x - p.pos.x, y: e.pos.y - p.pos.y }; setState(e, 'cling'); }
            break;
        }
        case 'cling':
            e.pos.x = p.pos.x + e.target!.x;
            e.pos.y = p.pos.y + e.target!.y;
            p.wax -= sim.settings.mothDrain * dt;
            if (Math.random() < dt * 4) burst(sim, p.pos, 1, '#fde68a', 'drip', 1, Math.PI / 2, 0.06);
            break;
        case 'dazed':
            if (e.t > 1.2) setState(e, 'fly');
            break;
    }
}

// --- weeper ------------------------------------------------------------------------------------

export const FREEZE_LIGHT = 0.2;

export function weeperFrozen(e: Enemy): boolean {
    return e.kind === 'weeper' && e.lit > FREEZE_LIGHT;
}

function weeper(sim: Sim, e: Enemy, dt: number): void {
    const p = sim.player;
    if (weeperFrozen(e)) {
        if (e.state !== 'frozen') { setState(e, 'frozen'); burst(sim, e.pos, 4, '#cbd5e1', 'dot', 2, NaN, 0.08); }
        e.hp -= 2 * Math.max(0, e.lit - FREEZE_LIGHT) * dt; // light wears at it, slowly
        return;
    }
    if (e.state === 'frozen') setState(e, 'stalk');
    if (e.state === 'recoil') {
        moveToward(e, { x: e.pos.x - Math.cos(e.facing), y: e.pos.y - Math.sin(e.facing) }, sim.settings.weeperSpeed * 0.6, dt);
        keepOnGround(sim.segs, sim.clearings, e.pos, RADIUS.weeper);
        if (e.t > 1.4) setState(e, 'stalk');
        return;
    }
    moveToward(e, p.pos, sim.settings.weeperSpeed, dt);
    keepOnGround(sim.segs, sim.clearings, e.pos, RADIUS.weeper);
    if (dist(e.pos, p.pos) < RADIUS.weeper + PLAYER_R + 0.1) {
        hurtPlayer(sim, sim.settings.weeperHit, e.facing, 12);
        shake(sim, 0.6);
        setState(e, 'recoil');
    }
}

// --- damage and death ----------------------------------------------------------------------------

/** A flame (or the flare) hits a dark thing. */
export function hurtEnemy(sim: Sim, e: Enemy, damage: number, angle: number): void {
    if (e.spawnT > 0) return;
    if (e.kind === 'weeper' && !weeperFrozen(e)) {
        if (Math.random() < 0.5) floater(sim, e.pos, 'light it first', '#cbd5e1', 0.8);
        return;
    }
    if (e.kind === 'hand' && !handUp(e)) return; // under the water
    if (e.kind === 'snuffer') knockSnuffer(sim, e);
    e.hp -= damage;
    e.flash = 0.08;
    if (e.kind !== 'weeper' && e.kind !== 'hand') { e.vel.x += Math.cos(angle) * 6; e.vel.y += Math.sin(angle) * 6; }
    if (e.kind === 'hand' && e.hp <= 0) { release(sim, e); e.hp = e.maxHp; e.state = 'under'; e.t = -2; floater(sim, e.pos, 'it sinks', '#93c5fd', 0.9); sim.stats.others++; return; } // hands sink, they don't die
    if (e.kind === 'shade' && e.state === 'tell') { e.state = 'retreat'; e.t = 0; } // knocked out of its lunge
    if (e.kind === 'moth' && e.state === 'cling') e.hp = 0;
    burst(sim, e.pos, 4, '#fb923c', 'spark', 5, angle, 0.1);
}

function die(sim: Sim, e: Enemy): void {
    const p = sim.player;
    release(sim, e);
    if (e.kind === 'leech') burstLeech(sim, e);
    p.wax = Math.min(sim.settings.maxWax, p.wax + WAX_BACK[e.kind]);
    if (e.kind === 'moth') {
        sim.stats.moths++;
        burst(sim, e.pos, 5, '#d6d3d1', 'dot', 3, NaN, 0.08);
        return;
    }
    if (e.kind === 'shade') sim.stats.shades++;
    else if (e.kind === 'weeper') sim.stats.weepers++;
    else sim.stats.others++;
    burst(sim, e.pos, e.kind === 'weeper' ? 30 : 16, 'rgba(30,27,46,0.95)', 'smoke', 4, NaN, 0.4);
    burst(sim, e.pos, 10, '#fde68a', 'spark', 6, NaN, 0.1);
    ring(sim, e.pos, e.kind === 'weeper' ? 3 : 1.6, 'rgba(253,230,138,0.7)', 0.35);
    floater(sim, e.pos, `+${WAX_BACK[e.kind]} wax`, '#fde68a', e.kind === 'weeper' ? 1.3 : 0.9);
    shake(sim, e.kind === 'weeper' ? 0.7 : 0.2);
}
