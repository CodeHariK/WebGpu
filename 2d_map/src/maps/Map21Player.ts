// Combat Sandbox — the player: move (WASD), aim (mouse), hold to blow bubbles (each costs joy),
// dash (Space / Shift: invincible at the start — dodge a bolt that way and it's PERFECT: slow-mo
// and a joy bonus), and pop (right click / E): a close-range burst that shoves and hurts everything
// round you. Close-range kills free more joy spirits, so pushing in keeps you fed. Fizz cans (from
// supply crates) give big bubbles (Q): slow, and they burst in a blast that breaks machines fast.
import { addShake, addStop, burst, floater, ring } from './Map21Juice';
import type { Enemy, Input, Player, Sim, Vec } from './Map21State';
import { collide } from './Map21World';
import { welderWeak } from './Map21Mechs';
import { inGlue } from './Map21Hazards';

export const PLAYER_RADIUS = 0.5;
const FIZZ_SPEED = 12;
const RADIUS_SHIELD = 1.1;
const GLUE_SPEED = 0.35; // fraction of your speed in glue
export const FIZZ_BLAST = 2.4; // radius
export const FIZZ_DAMAGE = 6; // to toys and machines (never to you)

export function createPlayer(start: Vec, hp: number, joy: number): Player {
    return {
        pos: { ...start }, vel: { x: 0, y: 0 }, aim: -Math.PI / 2, hp, joy,
        fireCd: 0, dashT: 0, dashDir: { x: 0, y: -1 }, dashCd: 0, iframes: 0, hurtT: 0, popCd: 0,
        perfectArmed: false, recoil: 0, keys: [], held: false, struggle: 0, paintT: 0, fizz: 1,
    };
}

export function invincible(p: Player): boolean {
    return p.iframes > 0 || p.hurtT > 0;
}

export function stepPlayer(sim: Sim, input: Input, dt: number): void {
    const p = sim.player;
    const s = sim.settings;
    p.fireCd -= dt;
    p.dashCd -= dt;
    p.iframes -= dt;
    p.hurtT -= dt;
    p.popCd -= dt;
    p.paintT = Math.max(0, p.paintT - dt);
    p.recoil *= Math.exp(-dt * 20);
    p.aim = Math.atan2(input.mouse.y - p.pos.y, input.mouse.x - p.pos.x);
    let mx = (input.right ? 1 : 0) - (input.left ? 1 : 0);
    let my = (input.down ? 1 : 0) - (input.up ? 1 : 0);
    const len = Math.hypot(mx, my);
    if (len > 0) { mx /= len; my /= len; }

    if (p.held) { // in a claw: dashing is struggling; you can still shoot (the claw, too)
        if (input.dash) { p.struggle++; p.recoil = 1; addShake(sim, 0.12); }
        input.dash = false;
        input.pop = false;
        input.fizz = false;
        if (input.fire && p.fireCd <= 0) fire(sim);
        return;
    }
    const glued = inGlue(sim.arena, p.pos);
    if (input.dash && glued) floater(sim, p.pos, 'stuck in glue!', '#fef3c7', 0.8);
    else if (input.dash && p.dashCd <= 0 && p.dashT <= 0) dash(sim, len > 0 ? { x: mx, y: my } : { x: Math.cos(p.aim), y: Math.sin(p.aim) });
    input.dash = false;
    if (input.pop && p.popCd <= 0) pop(sim);
    input.pop = false;
    if (input.fizz && p.dashT <= 0) fireFizz(sim);
    input.fizz = false;

    if (p.dashT > 0) {
        p.dashT -= dt;
        const speed = s.dashDistance / s.dashTime;
        p.pos.x += p.dashDir.x * speed * dt;
        p.pos.y += p.dashDir.y * speed * dt;
        if (p.dashT <= 0) p.dashCd = s.dashCooldown;
        if (Math.random() < 0.6) burst(sim, p.pos, 1, 'rgba(255,170,220,0.8)', 'dot', 1, NaN, 0.25);
    } else {
        const speed = s.playerSpeed * (glued ? GLUE_SPEED : 1);
        p.pos.x += (mx * speed + p.vel.x) * dt;
        p.pos.y += (my * speed + p.vel.y) * dt;
    }
    p.vel.x *= Math.exp(-dt * 10);
    p.vel.y *= Math.exp(-dt * 10);
    collide(sim.arena, p.pos, PLAYER_RADIUS);

    if (input.fire && p.fireCd <= 0 && p.dashT <= 0) fire(sim);
}

function dash(sim: Sim, dir: { x: number; y: number }): void {
    const p = sim.player;
    p.dashT = sim.settings.dashTime;
    p.dashDir = dir;
    p.iframes = sim.settings.iframes;
    p.perfectArmed = true;
}

function fire(sim: Sim): void {
    const p = sim.player;
    const s = sim.settings;
    p.fireCd = 1 / s.fireRate;
    if (p.joy <= 0) {
        floater(sim, p.pos, 'no joy!', '#94a3b8', 0.8);
        p.fireCd = 0.4;
        return;
    }
    p.joy -= 1;
    const a = p.aim + ((Math.random() - 0.5) * s.spread * Math.PI) / 180;
    const muzzle = { x: p.pos.x + Math.cos(a) * 0.7, y: p.pos.y + Math.sin(a) * 0.7 };
    sim.shots.push({
        pos: muzzle, vel: { x: Math.cos(a) * s.bubbleSpeed, y: Math.sin(a) * s.bubbleSpeed },
        life: 1.2, damage: s.bubbleDamage, friendly: true, radius: 0.22, origin: { ...p.pos },
    });
    if (s.recoil) {
        p.recoil = 1;
        p.vel.x -= Math.cos(a) * 1.2;
        p.vel.y -= Math.sin(a) * 1.2;
        sim.juice.kick.x -= Math.cos(a) * 0.06;
        sim.juice.kick.y -= Math.sin(a) * 0.06;
    }
    burst(sim, muzzle, 2, 'rgba(160,240,255,0.9)', 'dot', 4, a, 0.12);
}

// A big fizz bubble: slow and fat; it bursts (FIZZ_BLAST) when it hits anything or runs out.
function fireFizz(sim: Sim): void {
    const p = sim.player;
    if (p.fizz <= 0) {
        floater(sim, p.pos, 'no fizz — break crates', '#94a3b8', 0.9);
        return;
    }
    p.fizz--;
    const a = p.aim;
    sim.shots.push({
        pos: { x: p.pos.x + Math.cos(a) * 0.9, y: p.pos.y + Math.sin(a) * 0.9 }, vel: { x: Math.cos(a) * FIZZ_SPEED, y: Math.sin(a) * FIZZ_SPEED },
        life: 1.4, damage: 0, friendly: true, radius: 0.45, origin: { ...p.pos }, big: true,
    });
    p.vel.x -= Math.cos(a) * 4;
    p.vel.y -= Math.sin(a) * 4;
    addShake(sim, 0.15);
}

// The pop: shove and hurt everything within popRadius.
function pop(sim: Sim): void {
    const p = sim.player;
    const s = sim.settings;
    p.popCd = s.popCooldown;
    ring(sim, p.pos, s.popRadius, 'rgba(160,240,255,0.9)', 0.25);
    burst(sim, p.pos, 18, 'rgba(200,250,255,0.9)', 'dot', 9, NaN, 0.2);
    addShake(sim, 0.35);
    let hit = 0;
    for (const e of sim.enemies) {
        const d = Math.hypot(e.pos.x - p.pos.x, e.pos.y - p.pos.y);
        if (d > s.popRadius + 0.5 || e.spawnT > 0) continue;
        if (e.kind === 'diver' && e.state === 'circle') e.state = 'stuck'; // knocked out of the air
        hit++;
        hurtEnemy(sim, e, s.popDamage, Math.atan2(e.pos.y - p.pos.y, e.pos.x - p.pos.x), 14, true);
    }
    if (hit > 0) addStop(sim, 2);
}

// Damage an enemy from direction `angle` (radians the hit travels), with `push` m/s of knockback.
// `pop` = the close-range attack (ignores shields, stuns riot toys).
export function hurtEnemy(sim: Sim, e: Enemy, damage: number, angle: number, push: number, popHit = false): void {
    const s = sim.settings;
    if (e.kind === 'riot' && !popHit && e.state !== 'stunned') {
        const toShooter = angle + Math.PI;
        const facingDiff = Math.abs(Math.atan2(Math.sin(toShooter - e.facing), Math.cos(toShooter - e.facing)));
        if (facingDiff < Math.PI * 0.45) { // hit the shield
            burst(sim, e.pos, 3, '#e2e8f0', 'spark', 6, toShooter, 0.1);
            floater(sim, e.pos, 'tink', '#cbd5e1', 0.7);
            return;
        }
    }
    if (e.shielded && !popHit) { // a projector's shield: bubbles bounce (a pop gets through)
        ring(sim, e.pos, RADIUS_SHIELD, 'rgba(125,211,252,0.9)', 0.15);
        if (Math.random() < 0.3) floater(sim, e.pos, 'shielded', '#7dd3fc', 0.7);
        return;
    }
    let dmg = damage;
    if (e.kind === 'welder') dmg *= welderWeak(e, angle); // from behind, or while it cools
    if (e.kind === 'rifleman' && e.state === 'wind') dmg *= 2; // its spring is exposed
    if (e.kind === 'riot' && e.state === 'stunned') dmg *= 2;
    if (e.kind === 'diver' && e.state === 'stuck') dmg *= 2; // ploughed into the floor
    if (popHit && e.kind === 'riot' && e.state !== 'stunned') { e.state = 'stunned'; e.t = 0; }
    e.hp -= dmg;
    e.flash = s.flash ? 0.08 : 0;
    e.stagger = Math.max(e.stagger, popHit ? 0.5 : 0.12);
    if (s.knockback) {
        const weight = e.kind === 'riot' ? 0.4 : 1;
        e.vel.x += Math.cos(angle) * push * weight;
        e.vel.y += Math.sin(angle) * push * weight;
    }
    floater(sim, e.pos, String(dmg), dmg > damage ? '#fde047' : '#ffffff', dmg > damage ? 1.3 : 1);
    burst(sim, e.pos, 3, '#cbd5e1', 'square', 5, angle, 0.12);
    addShake(sim, 0.08);
    addStop(sim, 1);
}
