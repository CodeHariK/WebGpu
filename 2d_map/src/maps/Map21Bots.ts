// Combat Sandbox — the Assembly Plant's bots (Map21Enemies keeps the shared machinery).
//
//   rigger    a cable drone hanging back at ~10 m. It works three RIG BOTS on cables: they walk
//             where it puts them (a fan in front of you) and can't shoot on their own. Every few
//             seconds it PULLS (the cables glow), and every rig bot that can see you fires at once.
//             Shoot a cable and that bot drops; break the rigger and they all drop.
//   rig bot   a boxy walker on a cable. Dumb alone, dangerous in a volley.
//   springer  hides in a supply crate (Map21Objectives pops it when you come close or shoot the
//             crate). It hops at you on a spring: a red ring shows where it lands, and the
//             landing knocks you down. Between hops it sits and recoils: that's your moment.
import { addShake, burst, floater, ring } from './Map21Juice';
import type { Enemy, Shot, Sim, Vec } from './Map21State';
import { fireBolt, hitPlayer, moveTo, setState, spawnEnemy } from './Map21Enemies';
import { clearLine } from './Map21World';

const RIG_RANGE = 10; // the rigger keeps this far back
const PULL_EVERY = 3.4;
export const PULL_TELL = 0.75;
const FAN = 0.65; // radians between rig bots in the fan
const BOT_RANGE = 5.5; // how far in front of you the fan stands
const CABLE_HIT = 0.3;

const HOP_TIME = 0.6;
const HOP_REST = 0.7;
export const LAND_R = 1.4;
const HOP_MAX = 6;

const dist = (a: Vec, b: Vec) => Math.hypot(a.x - b.x, a.y - b.y);
const angleTo = (a: Vec, b: Vec) => Math.atan2(b.y - a.y, b.x - a.x);

/** A rigger arrives with its three rig bots. */
export function spawnRig(sim: Sim, at: Vec, hpScale: number): void {
    const rigger = spawnEnemy(sim, 'rigger', at, hpScale);
    for (let i = 0; i < 3; i++) {
        const bot = spawnEnemy(sim, 'rigbot', { x: at.x + (i - 1) * 1.2, y: at.y + 1.2 }, hpScale);
        bot.owner = rigger.id;
    }
}

/** The rig bots on this rigger's cables, in a stable order. */
export const rigBots = (sim: Sim, rigger: Enemy) => sim.enemies.filter((e) => e.owner === rigger.id && e.hp > 0);
export const riggerOf = (sim: Sim, bot: Enemy) => sim.enemies.find((e) => e.id === bot.owner && e.hp > 0) ?? null;

export function rigger(sim: Sim, e: Enemy, dt: number): void {
    const p = sim.player.pos;
    const bots = rigBots(sim, e);
    // Hang back behind its bots: on the far side of them from you.
    const cx = bots.length ? bots.reduce((s, b) => s + b.pos.x, 0) / bots.length : e.pos.x;
    const cy = bots.length ? bots.reduce((s, b) => s + b.pos.y, 0) / bots.length : e.pos.y;
    const away = angleTo(p, { x: cx, y: cy }) + Math.sin(sim.time * 0.5 + e.id) * 0.5;
    const want = { x: p.x + Math.cos(away) * RIG_RANGE, y: p.y + Math.sin(away) * RIG_RANGE };
    want.x = Math.max(2, Math.min(sim.arena.w - 2, want.x));
    want.y = Math.max(2, Math.min(sim.arena.h - 2, want.y));
    e.facing = angleTo(e.pos, p);
    if (e.state === 'hover') {
        moveTo(e, want, sim.settings.enemySpeed * 0.8, dt);
        if (e.t > PULL_EVERY && bots.length > 0) setState(e, 'pull');
    } else if (e.state === 'pull') { // the tell: cables glow; then every bot that sees you fires
        if (e.stagger > 0.3) { setState(e, 'hover'); return; } // knocked: the pull fails
        if (e.t < PULL_TELL) return;
        for (const b of bots) if (clearLine(sim.arena, b.pos, p)) { b.facing = angleTo(b.pos, p); fireBolt(sim, b, 4); }
        setState(e, 'hover');
    }
}

export function rigbot(sim: Sim, e: Enemy, dt: number): void {
    const owner = riggerOf(sim, e);
    if (!owner) { collapse(sim, e, 'DROPPED'); return; }
    const p = sim.player.pos;
    const bots = rigBots(sim, owner);
    const i = bots.indexOf(e);
    const base = angleTo(p, owner.pos);
    const a = base + (i - (bots.length - 1) / 2) * FAN;
    const want = { x: p.x + Math.cos(a) * BOT_RANGE, y: p.y + Math.sin(a) * BOT_RANGE };
    if (owner.state !== 'pull') moveTo(e, want, sim.settings.enemySpeed * 0.85, dt);
    e.facing = angleTo(e.pos, p);
    e.state = owner.state === 'pull' ? 'aim' : 'walk';
}

function collapse(sim: Sim, e: Enemy, text: string): void {
    e.hp = 0;
    floater(sim, e.pos, text, '#fde047', 1);
    burst(sim, e.pos, 6, '#94a3b8', 'square', 4, NaN, 0.15);
}

/** A friendly shot crossing a rig cable cuts it: that bot drops. True if it stopped. */
export function shotCutsCable(sim: Sim, shot: Shot): boolean {
    for (const bot of sim.enemies) {
        if (bot.kind !== 'rigbot' || bot.hp <= 0 || bot.spawnT > 0) continue;
        const owner = riggerOf(sim, bot);
        if (!owner || owner.spawnT > 0) continue;
        const from = owner.pos;
        const to = bot.pos;
        const len = dist(from, to);
        if (len < 1.2) continue;
        const k = ((shot.pos.x - from.x) * (to.x - from.x) + (shot.pos.y - from.y) * (to.y - from.y)) / (len * len);
        if (k < 0.12 || k > 0.88) continue; // not right at either end (that's a body hit)
        const near = { x: from.x + (to.x - from.x) * k, y: from.y + (to.y - from.y) * k };
        if (dist(near, shot.pos) > CABLE_HIT) continue;
        shot.life = 0;
        ring(sim, near, 0.6, 'rgba(253,224,71,0.9)', 0.2);
        burst(sim, near, 8, '#fde047', 'spark', 6, NaN, 0.12);
        addShake(sim, 0.15);
        collapse(sim, bot, 'CUT!');
        return true;
    }
    return false;
}

// --- springer --------------------------------------------------------------------------------

export function springer(sim: Sim, e: Enemy): void {
    const p = sim.player;
    if (e.state === 'rest') {
        e.facing = angleTo(e.pos, p.pos);
        if (e.t < HOP_REST) return;
        const d = Math.min(HOP_MAX, dist(e.pos, p.pos));
        const a = angleTo(e.pos, p.pos);
        const to = { x: e.pos.x + Math.cos(a) * d + p.vel.x * 0.15, y: e.pos.y + Math.sin(a) * d + p.vel.y * 0.15 };
        e.hop = { from: { ...e.pos }, to };
        setState(e, 'hop');
        return;
    }
    if (e.state === 'hop' && e.hop) {
        const k = Math.min(1, e.t / HOP_TIME);
        e.pos.x = e.hop.from.x + (e.hop.to.x - e.hop.from.x) * k;
        e.pos.y = e.hop.from.y + (e.hop.to.y - e.hop.from.y) * k;
        if (k < 1) return;
        // Landing: a shockwave.
        ring(sim, e.pos, LAND_R, 'rgba(248,113,113,0.9)', 0.25);
        burst(sim, e.pos, 10, '#a8a29e', 'square', 5, NaN, 0.15);
        addShake(sim, 0.2);
        if (dist(e.pos, p.pos) < LAND_R + 0.3) hitPlayer(sim, 1, angleTo(e.pos, p.pos), 12);
        e.hop = undefined;
        setState(e, 'rest');
    }
}

/** How high a springer is (for drawing): 0 on the floor, up to 1 mid-hop. */
export function hopHeight(e: Enemy): number {
    if (e.state !== 'hop') return 0;
    const k = Math.min(1, e.t / HOP_TIME);
    return 4 * k * (1 - k);
}

/** A trap crate opens: a springer jumps out (straight into a hop at you). */
export function springOut(sim: Sim, at: Vec): void {
    const e = spawnEnemy(sim, 'springer', at, 1 + 0.12 * sim.level);
    e.spawnT = 0;
    e.state = 'rest';
    e.t = HOP_REST - 0.25;
    floater(sim, at, 'AMBUSH!', '#f87171', 1.3);
    burst(sim, at, 16, '#b08850', 'square', 8, NaN, 0.22);
    addShake(sim, 0.35);
}
