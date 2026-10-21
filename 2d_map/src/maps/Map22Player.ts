// Wick — you, the candle. WASD to walk, the mouse to aim. Your WAX is everything: your health (a
// hit melts some off), your ammo (each flame you throw costs some) and your light (the less wax,
// the smaller your circle of light, and the bolder the dark gets).
//
//   hold click   throw flames: they burn what they hit and leave an EMBER where they land — a
//                small light that keeps burning for a few seconds (shades won't cross it)
//   right / E    FLARE: a burst of bright light all round you — burns shades, kills moths, hurts a
//                frozen weeper. Costs a lot of wax.
//   Space        dash (shakes off moths)
//   stand by     an unlit lantern for a moment to kindle it (costs a little wax); lit lanterns
//                melt wax back onto you
import { burst, floater, ring, shake, stop } from './Map22Fx';
import type { Input, Player, Sim, Vec } from './Map22State';
import { keepOnGround } from './Map22World';
import { inVat, VAT_SLOW } from './Map22Wind';

export const PLAYER_R = 0.45;
const KINDLE_TIME = 0.7;
const KINDLE_COST = 6;
const KINDLE_REACH = 1.4;
const CANDLE_WAX = 30;
const SPILLED = 0.6; // of the wax a hit knocks off, this much lands as blobs you can pick back up
const BLOB_REACH = 1.7; // blobs this close slide to you

export function createPlayer(at: Vec, wax: number): Player {
    return {
        pos: { ...at }, vel: { x: 0, y: 0 }, aim: -Math.PI / 2, wax, throwCd: 0, dashT: 0, dashDir: { x: 0, y: -1 }, dashCd: 0,
        iframes: 0, hurtT: 0, flareCd: 0, flareT: 0, kindle: 0, held: 0, struggle: 0,
    };
}

export function stepPlayer(sim: Sim, input: Input, dt: number): void {
    const p = sim.player;
    const s = sim.settings;
    p.throwCd -= dt;
    p.dashCd -= dt;
    p.iframes -= dt;
    p.hurtT -= dt;
    p.flareCd -= dt;
    p.flareT = Math.max(0, p.flareT - dt * 2.5);
    p.aim = Math.atan2(input.mouse.y - p.pos.y, input.mouse.x - p.pos.x);
    let mx = (input.right ? 1 : 0) - (input.left ? 1 : 0);
    let my = (input.down ? 1 : 0) - (input.up ? 1 : 0);
    const len = Math.hypot(mx, my);
    if (len > 0) { mx /= len; my /= len; }

    if (p.held) { // something has you: dashing is struggling; you can still throw
        if (input.dash) { p.struggle++; burst(sim, p.pos, 4, '#fde68a', 'spark', 3, NaN, 0.08); shake(sim, 0.12); }
        input.dash = false;
        input.flare = false;
        if (input.throw && p.throwCd <= 0) throwFlame(sim);
        return;
    }
    if (input.dash && p.dashCd <= 0 && p.dashT <= 0) dash(sim, len > 0 ? { x: mx, y: my } : { x: Math.cos(p.aim), y: Math.sin(p.aim) });
    input.dash = false;
    if (input.flare && p.flareCd <= 0) flare(sim);
    input.flare = false;

    if (p.dashT > 0) {
        p.dashT -= dt;
        const v = s.dashDistance / s.dashTime;
        p.pos.x += p.dashDir.x * v * dt;
        p.pos.y += p.dashDir.y * v * dt;
        if (p.dashT <= 0) p.dashCd = s.dashCooldown;
        if (Math.random() < 0.7) burst(sim, p.pos, 1, 'rgba(253,186,116,0.8)', 'spark', 1, NaN, 0.1);
    } else {
        const speed = s.playerSpeed * (inVat(sim, p.pos) ? VAT_SLOW : 1);
        p.pos.x += (mx * speed + p.vel.x) * dt;
        p.pos.y += (my * speed + p.vel.y) * dt;
    }
    p.vel.x *= Math.exp(-dt * 9);
    p.vel.y *= Math.exp(-dt * 9);
    keepOnGround(sim.segs, sim.clearings, p.pos, PLAYER_R);

    if (input.throw && p.throwCd <= 0 && p.dashT <= 0) throwFlame(sim);
    tendLanterns(sim, dt);
    pickCandles(sim);
    pickBlobs(sim, dt);
    if (len > 0 && Math.random() < dt * 3) burst(sim, { x: p.pos.x, y: p.pos.y + 0.2 }, 1, '#fde68a', 'drip', 0.5, Math.PI / 2, 0.07); // a drip of wax as you go
}

function dash(sim: Sim, dir: Vec): void {
    const p = sim.player;
    p.dashT = sim.settings.dashTime;
    p.dashDir = dir;
    p.iframes = sim.settings.dashTime + 0.08;
    // Moths can't hold on.
    for (const e of sim.enemies) {
        if (e.kind === 'moth' && e.state === 'cling') { e.state = 'dazed'; e.t = 0; e.vel = { x: (Math.random() - 0.5) * 8, y: (Math.random() - 0.5) * 8 }; }
    }
}

function throwFlame(sim: Sim): void {
    const p = sim.player;
    const s = sim.settings;
    p.throwCd = 1 / s.throwRate;
    if (p.wax <= s.throwCost + 2) { floater(sim, p.pos, 'too little wax', '#a8a29e', 0.8); p.throwCd = 0.4; return; }
    p.wax -= s.throwCost;
    const a = p.aim + (Math.random() - 0.5) * 0.06;
    sim.flames.push({ pos: { x: p.pos.x + Math.cos(a) * 0.6, y: p.pos.y + Math.sin(a) * 0.6 }, vel: { x: Math.cos(a) * s.flameSpeed, y: Math.sin(a) * s.flameSpeed }, life: 0.75 });
    burst(sim, p.pos, 3, '#fdba74', 'spark', 3, a, 0.1);
}

function flare(sim: Sim): void {
    const p = sim.player;
    const s = sim.settings;
    if (p.wax <= s.flareCost + 2) { floater(sim, p.pos, 'too little wax to flare', '#a8a29e', 0.8); p.flareCd = 0.5; return; }
    p.wax -= s.flareCost;
    p.flareCd = s.flareCooldown;
    p.flareT = 1;
    ring(sim, p.pos, s.flareRadius, 'rgba(254,240,138,0.95)', 0.4);
    burst(sim, p.pos, 26, '#fde68a', 'spark', 10, NaN, 0.14);
    shake(sim, 0.3);
    stop(sim, 0.04);
}

/** Your flame takes a hit: wax melts off. */
export function hurtPlayer(sim: Sim, wax: number, angle: number, push: number, spills = true): void {
    const p = sim.player;
    if (p.iframes > 0 || p.hurtT > 0 || sim.phase !== 'play') return;
    p.wax -= wax;
    p.hurtT = 0.7;
    p.vel.x += Math.cos(angle) * push;
    p.vel.y += Math.sin(angle) * push;
    floater(sim, p.pos, `-${Math.round(wax)} wax`, '#fca5a5', 1);
    burst(sim, p.pos, 8, '#fde68a', 'drip', 4, angle, 0.12);
    if (spills) spill(sim, wax * SPILLED, angle);
    shake(sim, 0.4);
    stop(sim, 0.05);
}

// Lanterns: stand at an unlit one to kindle it; by a lit one, wax melts back onto you.
function tendLanterns(sim: Sim, dt: number): void {
    const p = sim.player;
    const s = sim.settings;
    let touching = false;
    for (const l of sim.lanterns) {
        const d = Math.hypot(p.pos.x - l.pos.x, p.pos.y - l.pos.y);
        if (l.lit) {
            if (d < 3 && p.wax < s.maxWax) p.wax = Math.min(s.maxWax, p.wax + s.lanternHeal * dt);
            continue;
        }
        if (d > KINDLE_REACH) continue;
        touching = true;
        p.kindle += dt;
        if (p.kindle >= KINDLE_TIME && p.wax > KINDLE_COST + 2) {
            p.wax -= KINDLE_COST;
            lightLantern(sim, l);
            p.kindle = 0;
        }
    }
    if (!touching) p.kindle = 0;
}

/** A lantern catches: a ring of light, and it becomes where you come back to. */
export function lightLantern(sim: Sim, l: Sim['lanterns'][number]): void {
    if (l.lit) return;
    l.lit = true;
    l.litT = 0;
    sim.checkpoint = { x: l.pos.x, y: l.pos.y + 1.2 };
    ring(sim, l.pos, sim.settings.lanternRadius, 'rgba(253,224,71,0.8)', 0.6);
    burst(sim, l.pos, 18, '#fde68a', 'spark', 6, NaN, 0.12);
    floater(sim, l.pos, 'LIT', '#fde047', 1.3);
    shake(sim, 0.2);
}

// Knocked-off wax lands round you as blobs.
function spill(sim: Sim, wax: number, angle: number): void {
    const n = Math.max(1, Math.round(wax / 3));
    for (let i = 0; i < n; i++) {
        const a = angle + (Math.random() - 0.5) * 2.2;
        const v = 2 + Math.random() * 3;
        sim.blobs.push({ pos: { ...sim.player.pos }, vel: { x: Math.cos(a) * v, y: Math.sin(a) * v }, wax: wax / n, t: 0 });
    }
}

// Blobs settle, slide to you when you're near, and melt back on.
function pickBlobs(sim: Sim, dt: number): void {
    const p = sim.player;
    for (const b of sim.blobs) {
        b.t += dt;
        b.pos.x += b.vel.x * dt;
        b.pos.y += b.vel.y * dt;
        b.vel.x *= Math.exp(-dt * 5);
        b.vel.y *= Math.exp(-dt * 5);
        const d = Math.hypot(p.pos.x - b.pos.x, p.pos.y - b.pos.y);
        if (b.t < 0.5) continue;
        if (d < 0.6) { p.wax = Math.min(sim.settings.maxWax, p.wax + b.wax); b.wax = 0; continue; }
        if (d < BLOB_REACH) { b.pos.x += ((p.pos.x - b.pos.x) / d) * 6 * dt; b.pos.y += ((p.pos.y - b.pos.y) / d) * 6 * dt; }
    }
    sim.blobs = sim.blobs.filter((b) => b.wax > 0);
}

function pickCandles(sim: Sim): void {
    const p = sim.player;
    for (const c of sim.candles) {
        if (c.taken || Math.hypot(p.pos.x - c.pos.x, p.pos.y - c.pos.y) > 0.9) continue;
        c.taken = true;
        p.wax = Math.min(sim.settings.maxWax, p.wax + CANDLE_WAX);
        floater(sim, p.pos, `+${CANDLE_WAX} wax`, '#fde68a', 1.2);
        ring(sim, p.pos, 1.5, 'rgba(253,230,138,0.8)', 0.3);
    }
}
