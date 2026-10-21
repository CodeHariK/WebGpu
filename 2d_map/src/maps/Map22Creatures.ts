// Wick — the second set of things in the dark, each a different way of attacking your light.
//
//   snuffer  a hunched thing with a long snuffer-cone for a head. It goes for the LIGHTS, not you:
//            it floats to the lit lantern or ember nearest you and lowers its cone over it (a
//            ring fills — hit it to stop it). A snuffed lantern has to be lit again. With nothing
//            lit near, it comes for your flame: it holds you and drinks wax until you struggle free.
//   hand     a drowned hand under the water by the path. Walk the edge near it and the water
//            ripples (the tell), then it grabs you and drags you toward the water, drinking wax.
//            Dash twice to wrench free, or burn it while it's up. It sinks and waits again.
//   gloom    a floating ball of un-light. Everything near it goes dim: your light shrinks round
//            it, shades can come in, a weeper in it wakes. It drifts in behind you. Light can't
//            hurt it — only flames.
//   leech    a wax leech. Wax knocked off you lands as blobs you can pick back up — leeches race
//            you for them, and every blob they swallow makes them bigger and tougher. Burst one
//            and the wax it ate spills back out. Close to a strong flame it slowly melts.
import { burst, floater, ring, shake } from './Map22Fx';
import { hurtPlayer } from './Map22Player';
import type { Enemy, Sim, Vec } from './Map22State';

const dist = (a: Vec, b: Vec) => Math.hypot(a.x - b.x, a.y - b.y);
const angleTo = (a: Vec, b: Vec) => Math.atan2(b.y - a.y, b.x - a.x);

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

/** Let go of you (whatever had you). */
export function release(sim: Sim, e: Enemy): void {
    if (sim.player.held === e.id) { sim.player.held = 0; sim.player.struggle = 0; }
}

function hold(sim: Sim, e: Enemy): void {
    sim.player.held = e.id;
    sim.player.struggle = 0;
    shake(sim, 0.35);
}

// --- snuffer -----------------------------------------------------------------------------------

export const SNUFF_TIME = 1.6;
const SNUFFER_SPEED = 3.2;
const SNUFF_REACH = 14; // it goes for lights this close to you
const SNUFFER_HOLD = 2.2;
const SNUFFER_DRAIN = 9;

// The lit thing nearest you that it can reach: a lantern or an ember (or null: go for you).
function snuffTarget(sim: Sim): Vec | null {
    const p = sim.player.pos;
    let best: Vec | null = null;
    let bestD = SNUFF_REACH;
    for (const l of sim.lanterns) if (l.lit && dist(l.pos, p) < bestD) { bestD = dist(l.pos, p); best = l.pos; }
    for (const em of sim.embers) if (dist(em.pos, p) < bestD) { bestD = dist(em.pos, p); best = em.pos; }
    return best;
}

export function snuffer(sim: Sim, e: Enemy, dt: number): void {
    const p = sim.player;
    switch (e.state) {
        case 'stalk': {
            const tgt = snuffTarget(sim);
            if (tgt) { e.target = tgt; setState(e, 'seek'); break; }
            moveToward(e, p.pos, SNUFFER_SPEED, dt);
            if (dist(e.pos, p.pos) < 0.9 && !p.held && p.dashT <= 0) { hold(sim, e); floater(sim, p.pos, 'SNUFFED! dash ×2', '#a8a29e', 1.1); setState(e, 'hold'); }
            break;
        }
        case 'seek': {
            const tgt = e.target!;
            const stillLit = sim.lanterns.some((l) => l.lit && l.pos === tgt) || sim.embers.some((em) => em.pos === tgt);
            if (!stillLit) { e.target = null; setState(e, 'stalk'); break; }
            moveToward(e, { x: tgt.x, y: tgt.y - 0.3 }, SNUFFER_SPEED, dt);
            if (dist(e.pos, tgt) < 0.6) setState(e, 'snuff');
            break;
        }
        case 'snuff': {
            const tgt = e.target!;
            if (e.t < SNUFF_TIME) break;
            const lantern = sim.lanterns.find((l) => l.pos === tgt);
            if (lantern && lantern.lit) {
                lantern.lit = false;
                lantern.litT = 0;
                floater(sim, tgt, 'SNUFFED OUT', '#a8a29e', 1.2);
                burst(sim, tgt, 14, 'rgba(120,113,108,0.8)', 'smoke', 2, -Math.PI / 2, 0.3);
            }
            sim.embers = sim.embers.filter((em) => em.pos !== tgt);
            e.target = null;
            setState(e, 'stalk');
            break;
        }
        case 'hold':
            e.pos.x = p.pos.x;
            e.pos.y = p.pos.y - 0.4;
            p.wax -= SNUFFER_DRAIN * dt;
            if (p.struggle >= 2 || e.t > SNUFFER_HOLD) {
                release(sim, e);
                e.vel = { x: Math.cos(p.aim + Math.PI) * 8, y: Math.sin(p.aim + Math.PI) * 8 };
                setState(e, 'recoil');
            }
            break;
        case 'recoil':
            if (e.t > 1.5) setState(e, 'stalk');
            break;
    }
}

/** A hit on a snuffer stops whatever it was doing. */
export function knockSnuffer(sim: Sim, e: Enemy): void {
    if (e.state === 'snuff' || e.state === 'hold') {
        release(sim, e);
        floater(sim, e.pos, 'stopped', '#fde68a', 0.8);
        e.target = null;
        setState(e, 'recoil');
    }
}

// --- drowned hand ------------------------------------------------------------------------------

export const RISE_TELL = 0.6;
const HAND_REACH = 2.3; // it reaches for you when you're this close to where it waits
const HAND_GRAB = 2.7;
const HAND_DRAIN = 6;
const HAND_PULL = 1.4;
const HAND_REST = 3.5;

export const handUp = (e: Enemy) => e.kind === 'hand' && (e.state === 'rise' || e.state === 'hold' || e.state === 'grope');

export function hand(sim: Sim, e: Enemy, dt: number): void {
    const p = sim.player;
    const home = e.home!;
    switch (e.state) {
        case 'under':
            e.pos = { ...home };
            if (e.t > HAND_REST && !p.held && dist(p.pos, home) < HAND_REACH) setState(e, 'rise');
            break;
        case 'rise':
            if (Math.random() < 0.5) burst(sim, { x: home.x + (Math.random() - 0.5), y: home.y + (Math.random() - 0.5) }, 1, 'rgba(148,163,184,0.6)', 'dot', 1, NaN, 0.08);
            if (e.t < RISE_TELL) break;
            if (dist(p.pos, home) < HAND_GRAB && !p.held && p.dashT <= 0) {
                hold(sim, e);
                floater(sim, p.pos, 'GRABBED! dash ×2', '#93c5fd', 1.1);
                setState(e, 'hold');
            } else setState(e, 'grope');
            break;
        case 'hold': {
            const a = angleTo(p.pos, home);
            p.pos.x += Math.cos(a) * HAND_PULL * dt; // dragged toward the water (the bank holds you)
            p.pos.y += Math.sin(a) * HAND_PULL * dt;
            e.pos = { x: p.pos.x + Math.cos(a) * 0.5, y: p.pos.y + Math.sin(a) * 0.5 };
            p.wax -= HAND_DRAIN * dt;
            if (p.struggle >= 2) { release(sim, e); floater(sim, p.pos, 'FREE!', '#fde68a', 1); setState(e, 'under'); }
            break;
        }
        case 'grope':
            moveToward(e, { x: home.x + (p.pos.x - home.x) * 0.3, y: home.y + (p.pos.y - home.y) * 0.3 }, 2, dt);
            if (e.t > 1.3) setState(e, 'under');
            break;
    }
}

// --- gloom -------------------------------------------------------------------------------------

export function gloom(sim: Sim, e: Enemy, dt: number): void {
    const p = sim.player;
    const behind = p.aim + Math.PI + Math.sin(sim.time * 0.7 + e.id) * 0.8;
    const want = { x: p.pos.x + Math.cos(behind) * 2.6, y: p.pos.y + Math.sin(behind) * 2.6 };
    moveToward(e, want, 2.8, dt);
}

// --- leech -------------------------------------------------------------------------------------

const BITE = 3;
const MAX_EATEN = 6; // full: it stops chasing blobs
export const leechRadius = (e: Enemy) => 0.28 + 0.07 * Math.min(6, e.eaten);

export function leech(sim: Sim, e: Enemy, dt: number): void {
    const p = sim.player;
    if (e.lit > 0.6) { e.hp -= 2.5 * dt; if (Math.random() < 0.1) burst(sim, e.pos, 1, '#fde68a', 'drip', 1, Math.PI / 2, 0.06); } // too near a flame: it melts
    let blob = null;
    let bd = 12;
    if (e.eaten < MAX_EATEN) for (const b of sim.blobs) { const d = dist(b.pos, e.pos); if (d < bd) { bd = d; blob = b; } }
    if (blob) {
        moveToward(e, blob.pos, 5.5, dt);
        if (bd < leechRadius(e) + 0.2) {
            blob.wax = 0;
            sim.blobs = sim.blobs.filter((b) => b.wax > 0);
            e.eaten++;
            e.hp += 2;
            e.maxHp += 2;
            floater(sim, e.pos, 'gulp', '#fde68a', 0.8);
        }
        return;
    }
    moveToward(e, p.pos, 3.4 - Math.min(1.5, e.eaten * 0.2), dt);
    if (e.t > 1.2 && dist(e.pos, p.pos) < leechRadius(e) + 0.5) { hurtPlayer(sim, BITE, e.facing, 2, false); e.t = 0; } // it drinks: nothing spills
}

/** A leech bursts: the wax it swallowed spills back out as blobs. */
export function burstLeech(sim: Sim, e: Enemy): void {
    for (let i = 0; i < e.eaten * 2; i++) {
        const a = Math.random() * Math.PI * 2;
        sim.blobs.push({ pos: { ...e.pos }, vel: { x: Math.cos(a) * 3, y: Math.sin(a) * 3 }, wax: 2, t: 0 });
    }
    if (e.eaten) { ring(sim, e.pos, 1.4, 'rgba(253,230,138,0.8)', 0.3); floater(sim, e.pos, 'your wax!', '#fde68a', 1); }
}
