// Combat Sandbox — the Assembly Plant's mechs (Map21Enemies keeps the shared machinery).
//
//   welder    a heavy walker with a torch arm. It plods at you; close in, the torch IGNITES (a cone
//             outline), then BURNS for a second, sweeping the cone after you (one burn per sweep
//             if it catches you; dash through the flame and it doesn't). Then it COOLS, panels
//             open. Its back is armour-free: hits from behind always do double, and so does
//             anything while it cools. Its flame burns other toys too.
//   sniper    perches far away where it can see you. Its laser trails you (it lags behind if you
//             move); when the laser goes thick it fires one fast, heavy shot at the laser's point.
//             Then it moves to a new perch. Break its line of sight and it gives up and moves.
//   engineer  walks to a spot it can see you from and DEPLOYS a turret (1.4 s, knock it and it
//             fumbles). Two turrets each, then it keeps its distance and redeploys if one breaks.
//   turret    stationary. A laser beam sweeps round; when it crosses you it LOCKS (a beat), then
//             fires three bolts. Its beam stops at walls.
import { addShake, burst, floater, ring } from './Map21Juice';
import type { Enemy, Sim, Vec } from './Map21State';
import { distToPlayer, flies, hitPlayer, moveTo, RADIUS, sees, setState, spawnEnemy, toPlayer, turnTo } from './Map21Enemies';
import { clearLine, collide } from './Map21World';

const dist = (a: Vec, b: Vec) => Math.hypot(a.x - b.x, a.y - b.y);
const angleTo = (a: Vec, b: Vec) => Math.atan2(b.y - a.y, b.x - a.x);
const angleDiff = (a: number, b: number) => Math.abs(Math.atan2(Math.sin(a - b), Math.cos(a - b)));

// --- welder ------------------------------------------------------------------------------------

export const TORCH_RANGE = 3.4;
export const TORCH_HALF = 0.6; // radians: half the cone's width
export const IGNITE_TIME = 0.55;
const BURN_TIME = 1.1;
const COOL_TIME = 1.4;
const TORCH_TOY_DPS = 6;

/** Is p inside this welder's flame cone? */
export function inTorch(e: Enemy, p: Vec): boolean {
    const d = dist(e.pos, p);
    return d < TORCH_RANGE && d > 0.1 && angleDiff(angleTo(e.pos, p), e.facing) < TORCH_HALF;
}

export function welder(sim: Sim, e: Enemy, dt: number): void {
    const s = sim.settings;
    switch (e.state) {
        case 'walk':
            turnTo(e, toPlayer(sim, e), 2.4, dt);
            e.pos.x += Math.cos(e.facing) * s.enemySpeed * 0.6 * dt;
            e.pos.y += Math.sin(e.facing) * s.enemySpeed * 0.6 * dt;
            if (distToPlayer(sim, e) < TORCH_RANGE - 0.4 && sees(sim, e)) setState(e, 'ignite');
            break;
        case 'ignite':
            turnTo(e, toPlayer(sim, e), 2.4, dt);
            if (e.t > IGNITE_TIME) { e.shots = 1; setState(e, 'burn'); }
            break;
        case 'burn': {
            turnTo(e, toPlayer(sim, e), 1.3, dt); // the sweep: slower than you can circle it
            const p = sim.player;
            if (e.shots > 0 && inTorch(e, p.pos) && p.dashT <= 0) { hitPlayer(sim, 1, e.facing, 7); e.shots = 0; } // one burn per sweep
            for (const o of sim.enemies) {
                if (o === e || o.spawnT > 0 || flies(o) || !inTorch(e, o.pos)) continue;
                o.hp -= TORCH_TOY_DPS * dt;
                if (Math.random() < 0.2) burst(sim, o.pos, 1, '#fb923c', 'spark', 3, NaN, 0.1);
            }
            const tip = { x: e.pos.x + Math.cos(e.facing) * TORCH_RANGE * 0.6, y: e.pos.y + Math.sin(e.facing) * TORCH_RANGE * 0.6 };
            if (Math.random() < 0.8) burst(sim, tip, 1, Math.random() < 0.5 ? '#fdba74' : '#fde047', 'spark', 6, e.facing, 0.12);
            if (e.t > BURN_TIME) setState(e, 'cool');
            break;
        }
        case 'cool':
            if (Math.random() < 0.3) burst(sim, e.pos, 1, 'rgba(226,232,240,0.5)', 'smoke', 1.5, -Math.PI / 2, 0.5);
            if (e.t > COOL_TIME) setState(e, 'walk');
            break;
    }
}

/** Damage multiplier for a hit on a welder travelling at `angle` (radians). */
export function welderWeak(e: Enemy, angle: number): number {
    const fromBehind = angleDiff(angle + Math.PI, e.facing) > Math.PI * 0.6;
    return fromBehind || e.state === 'cool' ? 2 : 1;
}

// --- sniper ------------------------------------------------------------------------------------

export const TRACK_TIME = 1.7;
const TRACK_SPEED = 5.5; // how fast the laser point chases you (you run faster: keep moving)
const SNIPE_SPEED = 42;
const PERCH_MIN = 11;

export function sniper(sim: Sim, e: Enemy, dt: number): void {
    const p = sim.player.pos;
    switch (e.state) {
        case 'perch': { // walk to a far spot it can see you from
            if (!e.target || e.t > 5) e.target = pickPerch(sim, e);
            if (!e.target || moveTo(e, e.target, sim.settings.enemySpeed * 0.9, dt)) {
                if (sees(sim, e)) { e.aim = { ...p }; setState(e, 'track'); }
                else if (e.t > 2) e.target = null;
            }
            break;
        }
        case 'track': {
            if (!sees(sim, e)) { if (e.t > 0.4) { e.target = null; setState(e, 'perch'); } break; }
            if (e.stagger > 0.2) { e.t = Math.min(e.t, 0.3); } // knocked: the laser wobbles off
            const aim = e.aim!;
            const d = dist(aim, p);
            const step = Math.min(d, TRACK_SPEED * dt);
            if (d > 0.01) { aim.x += ((p.x - aim.x) / d) * step; aim.y += ((p.y - aim.y) / d) * step; }
            e.facing = angleTo(e.pos, aim);
            if (e.t > TRACK_TIME) {
                const a = e.facing;
                sim.shots.push({
                    pos: { x: e.pos.x + Math.cos(a) * 0.7, y: e.pos.y + Math.sin(a) * 0.7 }, vel: { x: Math.cos(a) * SNIPE_SPEED, y: Math.sin(a) * SNIPE_SPEED },
                    life: 1.2, damage: 2, friendly: false, radius: 0.22, origin: { ...e.pos },
                });
                burst(sim, e.pos, 6, '#f87171', 'spark', 9, a, 0.1);
                addShake(sim, 0.12);
                e.target = null;
                setState(e, 'perch');
            }
            break;
        }
    }
}

// A random far spot with a clear line to the player (tries a few).
function pickPerch(sim: Sim, e: Enemy): Vec | null {
    const p = sim.player.pos;
    let best: Vec | null = null;
    let bestD = Infinity;
    for (let i = 0; i < 14; i++) {
        const a = Math.random() * Math.PI * 2;
        const r = PERCH_MIN + Math.random() * 5;
        const spot = { x: p.x + Math.cos(a) * r, y: p.y + Math.sin(a) * r };
        collide(sim.arena, spot, RADIUS.sniper);
        if (dist(spot, p) < PERCH_MIN - 1 || !clearLine(sim.arena, spot, p) || !clearLine(sim.arena, e.pos, spot)) continue;
        const d = dist(spot, e.pos);
        if (d < bestD) { bestD = d; best = spot; }
    }
    return best;
}

// --- engineer and turrets ----------------------------------------------------------------------

export const DEPLOY_TIME = 1.4;
const MAX_TURRETS = 2;

export const turretsOf = (sim: Sim, e: Enemy) => sim.enemies.filter((t) => t.kind === 'turret' && t.owner === e.id && t.hp > 0);

export function engineer(sim: Sim, e: Enemy, dt: number): void {
    const p = sim.player.pos;
    const d = distToPlayer(sim, e);
    switch (e.state) {
        case 'walk': {
            const full = turretsOf(sim, e).length >= MAX_TURRETS;
            const a = toPlayer(sim, e);
            const want = d < 6 ? a + Math.PI : d > 9 ? a : a + Math.PI / 2; // 6 … 9 m, drifting sideways
            moveTo(e, { x: e.pos.x + Math.cos(want), y: e.pos.y + Math.sin(want) }, sim.settings.enemySpeed * 0.75, dt);
            if (!full && e.t > 1.5 && d < 11 && sees(sim, e)) setState(e, 'deploy');
            break;
        }
        case 'deploy':
            e.facing = toPlayer(sim, e);
            if (e.stagger > 0.2) { floater(sim, e.pos, 'fumbled', '#cbd5e1', 0.8); setState(e, 'walk'); break; }
            if (e.t > DEPLOY_TIME) {
                const at = { x: e.pos.x + Math.cos(e.facing) * 1.3, y: e.pos.y + Math.sin(e.facing) * 1.3 };
                collide(sim.arena, at, RADIUS.turret);
                const t = spawnEnemy(sim, 'turret', at, 1 + 0.12 * sim.level);
                t.spawnT = 0.3;
                t.owner = e.id;
                t.facing = angleTo(at, p) - 1;
                ring(sim, at, 1, 'rgba(250,204,21,0.8)', 0.2);
                setState(e, 'walk');
            }
            break;
    }
}

export const BEAM_RANGE = 10;
const SWEEP_RATE = 1.3; // radians per second
const LOCK_TIME = 0.4;
const VOLLEY = 3;

/** How far a turret's beam reaches along its facing before a wall (for drawing and spotting). */
export function beamLength(sim: Sim, e: Enemy): number {
    let len = 0;
    while (len < BEAM_RANGE) {
        const next = len + 0.25;
        const p = { x: e.pos.x + Math.cos(e.facing) * next, y: e.pos.y + Math.sin(e.facing) * next };
        if (!clearLine(sim.arena, e.pos, p)) break;
        len = next;
    }
    return len;
}

export function turret(sim: Sim, e: Enemy, dt: number): void {
    e.vel.x = 0; // bolted down
    e.vel.y = 0;
    switch (e.state) {
        case 'sweep': {
            e.facing += SWEEP_RATE * dt * (e.id % 2 ? 1 : -1);
            if (e.t < 0.8) break; // a breather after each volley
            const d = distToPlayer(sim, e);
            if (d < BEAM_RANGE && angleDiff(toPlayer(sim, e), e.facing) < 0.12 && sees(sim, e)) setState(e, 'lock');
            break;
        }
        case 'lock':
            e.facing = toPlayer(sim, e);
            if (e.stagger > 0.2) { setState(e, 'sweep'); break; }
            if (e.t > LOCK_TIME) { e.shots = VOLLEY; setState(e, 'fire'); }
            break;
        case 'fire':
            if (e.t < 0.11) break;
            e.t = 0;
            {
                const a = toPlayer(sim, e) + (Math.random() - 0.5) * 0.12;
                const speed = sim.settings.boltSpeed * 1.1;
                sim.shots.push({
                    pos: { x: e.pos.x + Math.cos(a) * 0.6, y: e.pos.y + Math.sin(a) * 0.6 }, vel: { x: Math.cos(a) * speed, y: Math.sin(a) * speed },
                    life: 2, damage: 1, friendly: false, radius: 0.16, origin: { ...e.pos },
                });
                burst(sim, e.pos, 2, '#fde047', 'spark', 5, a, 0.08);
            }
            if (--e.shots <= 0) setState(e, 'sweep');
            break;
    }
}
