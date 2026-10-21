// Combat Sandbox — the creepy toys. Each is a small state machine; danger comes from the mix.
//
//   rifleman   hides behind cover it can shoot over (or walks in until it sees you), shows a red
//              sight line, fires a 3-bolt burst. Hitting it during the line cancels the burst.
//              After two bursts it winds its key: double damage. Too close → it runs.
//   spotter    hovers at a distance; its light on you makes riflemen aim faster and tighter.
//   riot       shield in front (bubbles bounce). Walks in, winds up (red lane), charges. Sidestep:
//              it slams into a block (stunned: no shield, double damage) or into its own side.
//   diver      a bolt drone: circles you, locks on (a line), dives straight through. Dodge it and
//              it ends up stuck in the floor for a moment: double damage.
//   grenadier  keeps its distance and lobs bombs at where you stand (a shadow grows there). A
//              bubble hitting a bomb in the air bats it back at the grenadier.
//   mouse      tiny wind-up toys that come in a pack and run straight at you; each explodes when it
//              touches you or breaks — shoot the leader and the pack can blow itself up.
//   rigger, rig bot, springer — the Assembly Plant's bots: see Map21Bots.
//   welder, sniper, engineer, turret — its mechs: see Map21Mechs.
//   magnet, projector — its support drones: see Map21Drones.
import { PLAYER_RADIUS, invincible } from './Map21Player';
import { addShake, addStop, burst, floater } from './Map21Juice';
import type { Enemy, EnemyKind, Sim, Vec } from './Map21State';
import { clearLine, collide, coverSpots } from './Map21World';
import { rigbot, rigger, springer } from './Map21Bots';
import { engineer, sniper, turret, welder } from './Map21Mechs';
import { magnet, projector } from './Map21Drones';
import { inGlue } from './Map21Hazards';

export const RADIUS: Record<EnemyKind, number> = {
    rifleman: 0.5, spotter: 0.45, riot: 0.75, diver: 0.45, grenadier: 0.55, mouse: 0.3, rigger: 0.55, rigbot: 0.5, springer: 0.45,
    welder: 0.65, sniper: 0.5, engineer: 0.5, turret: 0.55, magnet: 0.45, projector: 0.4,
};
const HP: Record<EnemyKind, number> = {
    rifleman: 6, spotter: 3, riot: 12, diver: 4, grenadier: 6, mouse: 1, rigger: 10, rigbot: 5, springer: 5,
    welder: 10, sniper: 5, engineer: 7, turret: 8, magnet: 5, projector: 4,
};
export const ARRIVE_TIME = 0.8;
const GLUE_SLOW = 0.3;

export function spawnEnemy(sim: Sim, kind: EnemyKind, at: Vec, hpScale = 1): Enemy {
    const start: Record<EnemyKind, string> = {
        rifleman: 'cover', spotter: 'hover', riot: 'walk', diver: 'circle', grenadier: 'move', mouse: 'run', rigger: 'hover', rigbot: 'walk', springer: 'rest',
        welder: 'walk', sniper: 'perch', engineer: 'walk', turret: 'sweep', magnet: 'hover', projector: 'follow',
    };
    const hp = Math.max(1, Math.round(HP[kind] * hpScale));
    const e: Enemy = {
        id: sim.nextId++, kind, pos: { ...at }, vel: { x: 0, y: 0 }, hp, maxHp: hp,
        facing: Math.PI / 2, state: start[kind], t: Math.random(), target: null, shots: 0, bursts: 0, flash: 0, stagger: 0,
        spawnT: ARRIVE_TIME,
    };
    sim.enemies.push(e);
    return e;
}

export const flies = (e: Enemy) => e.kind === 'spotter' || e.kind === 'rigger' || e.kind === 'magnet' || e.kind === 'projector' || (e.kind === 'diver' && e.state !== 'stuck') || (e.kind === 'springer' && e.state === 'hop');

export function stepEnemies(sim: Sim, dt: number): void {
    sim.marked = sim.player.paintT > 0; // painted: as good as lit by a spotter
    for (const e of sim.enemies) e.shielded = false; // projectors switch it back on below
    // Spotters first, so riflemen act on this frame's light.
    const order = [...sim.enemies.filter((e) => e.kind === 'spotter'), ...sim.enemies.filter((e) => e.kind !== 'spotter')];
    for (const e of order) {
        e.flash -= dt;
        e.stagger -= dt;
        e.t += dt;
        if (e.spawnT > 0) {
            e.spawnT -= dt;
            if (e.spawnT <= 0) burst(sim, e.pos, 8, '#64748b', 'square', 5, NaN, 0.15);
            continue;
        }
        if (e.held) continue; // a grabber arm has it
        const before = { ...e.pos };
        switch (e.kind) {
            case 'rifleman': rifleman(sim, e, dt); break;
            case 'spotter': spotter(sim, e, dt); break;
            case 'riot': riot(sim, e, dt); break;
            case 'diver': diver(sim, e, dt); break;
            case 'grenadier': grenadier(sim, e, dt); break;
            case 'mouse': mouse(sim, e, dt); break;
            case 'rigger': rigger(sim, e, dt); break;
            case 'rigbot': rigbot(sim, e, dt); break;
            case 'springer': springer(sim, e); break;
            case 'welder': welder(sim, e, dt); break;
            case 'sniper': sniper(sim, e, dt); break;
            case 'engineer': engineer(sim, e, dt); break;
            case 'turret': turret(sim, e, dt); break;
            case 'magnet': magnet(sim, e, dt); break;
            case 'projector': projector(sim, e, dt); break;
        }
        if (!flies(e) && inGlue(sim.arena, before)) { // stuck in glue: a fraction of the move
            e.pos.x = before.x + (e.pos.x - before.x) * GLUE_SLOW;
            e.pos.y = before.y + (e.pos.y - before.y) * GLUE_SLOW;
        }
        e.pos.x += e.vel.x * dt;
        e.pos.y += e.vel.y * dt;
        e.vel.x *= Math.exp(-dt * 7);
        e.vel.y *= Math.exp(-dt * 7);
        if (!flies(e)) collide(sim.arena, e.pos, RADIUS[e.kind]);
    }
}

export const toPlayer = (sim: Sim, e: Enemy) => Math.atan2(sim.player.pos.y - e.pos.y, sim.player.pos.x - e.pos.x);
export const distToPlayer = (sim: Sim, e: Enemy) => Math.hypot(sim.player.pos.x - e.pos.x, sim.player.pos.y - e.pos.y);
export const sees = (sim: Sim, e: Enemy) => clearLine(sim.arena, e.pos, sim.player.pos);

export function setState(e: Enemy, state: string): void {
    e.state = state;
    e.t = 0;
}

export function turnTo(e: Enemy, angle: number, rate: number, dt: number): void {
    turnToward(e, angle, rate, dt);
}

export function moveTo(e: Enemy, target: Vec, speed: number, dt: number): boolean {
    const dx = target.x - e.pos.x;
    const dy = target.y - e.pos.y;
    const d = Math.hypot(dx, dy);
    if (d < 0.2) return true;
    const step = Math.min(d, speed * dt);
    e.pos.x += (dx / d) * step;
    e.pos.y += (dy / d) * step;
    e.facing = Math.atan2(dy, dx);
    return false;
}

function turnToward(e: Enemy, angle: number, rate: number, dt: number): void {
    const diff = Math.atan2(Math.sin(angle - e.facing), Math.cos(angle - e.facing));
    e.facing += Math.max(-rate * dt, Math.min(rate * dt, diff));
}

export function fireBolt(sim: Sim, e: Enemy, spreadDeg: number): void {
    const spread = (spreadDeg * Math.PI) / 180;
    const a = toPlayer(sim, e) + (Math.random() - 0.5) * 2 * spread;
    const speed = sim.settings.boltSpeed;
    sim.shots.push({
        pos: { x: e.pos.x + Math.cos(a) * 0.6, y: e.pos.y + Math.sin(a) * 0.6 },
        vel: { x: Math.cos(a) * speed, y: Math.sin(a) * speed },
        life: 3, damage: 1, friendly: false, radius: 0.18, origin: { ...e.pos },
    });
    burst(sim, e.pos, 2, '#fb923c', 'spark', 5, a, 0.1);
}

// --- rifleman ----------------------------------------------------------------------------------

function rifleman(sim: Sim, e: Enemy, dt: number): void {
    const s = sim.settings;
    const close = distToPlayer(sim, e) < 3.5;
    if (e.state !== 'cover' && e.state !== 'advance') e.facing = toPlayer(sim, e);
    switch (e.state) {
        case 'cover': {
            if (!e.target || e.t > 4) e.target = pickCover(sim, e);
            if (!e.target) { setState(e, 'advance'); break; }
            if (moveTo(e, e.target, s.enemySpeed, dt)) setState(e, 'aim');
            break;
        }
        case 'advance': { // no cover it can shoot from: walk at the player until it can see them
            if (sees(sim, e)) { setState(e, 'aim'); break; }
            const a = toPlayer(sim, e);
            moveTo(e, { x: e.pos.x + Math.cos(a), y: e.pos.y + Math.sin(a) }, s.enemySpeed * 0.8, dt);
            if (e.t > 3) { e.target = null; setState(e, 'cover'); }
            break;
        }
        case 'aim': {
            if (close && e.t > 0.2) { e.target = null; setState(e, 'cover'); break; }
            if (e.stagger > 0) { e.t = 0; break; } // hit during the tell: start over
            if (!sees(sim, e)) {
                if (e.t > 0.8) { e.target = null; setState(e, 'cover'); }
                break;
            }
            if (e.t >= s.aimTell * (sim.marked ? 0.5 : 1)) { e.shots = 3; setState(e, 'burst'); }
            break;
        }
        case 'burst': {
            if (e.stagger > 0) break;
            if (e.t >= 0.12) {
                e.t = 0;
                fireBolt(sim, e, sim.marked ? 3 : 10);
                if (--e.shots <= 0) {
                    e.bursts++;
                    setState(e, e.bursts >= 2 ? 'wind' : 'rest');
                }
            }
            break;
        }
        case 'rest':
            if (e.t > 0.9) setState(e, close ? 'cover' : 'aim');
            break;
        case 'wind':
            if (e.t > s.windTime) { e.bursts = 0; setState(e, 'aim'); }
            break;
    }
}

// The nearest cover spot with its block between it and the player, that it can shoot from.
function pickCover(sim: Sim, e: Enemy): Vec | null {
    const p = sim.player.pos;
    let best: Vec | null = null;
    let bestScore = Infinity;
    for (const spot of coverSpots(sim.arena)) {
        const toP = { x: p.x - spot.pos.x, y: p.y - spot.pos.y };
        const dP = Math.hypot(toP.x, toP.y);
        if (dP < 6) continue;
        const bx = spot.block.x + spot.block.w / 2 - spot.pos.x;
        const by = spot.block.y + spot.block.h / 2 - spot.pos.y;
        const shield = (bx * toP.x + by * toP.y) / (Math.hypot(bx, by) * dP);
        if (shield < 0.4) continue;
        if (!clearLine(sim.arena, spot.pos, p)) continue; // must be able to shoot from there (over low cover)
        if (sim.enemies.some((o) => o !== e && o.target && Math.hypot(o.target.x - spot.pos.x, o.target.y - spot.pos.y) < 1)) continue;
        const score = Math.hypot(spot.pos.x - e.pos.x, spot.pos.y - e.pos.y) - shield * 3 + (spot.block.low ? 0 : 2);
        if (score < bestScore) { bestScore = score; best = spot.pos; }
    }
    return best;
}

// --- spotter -----------------------------------------------------------------------------------

function spotter(sim: Sim, e: Enemy, dt: number): void {
    const p = sim.player.pos;
    const orbit = sim.time * 0.4 + e.id;
    const want = {
        x: Math.max(2, Math.min(sim.arena.w - 2, p.x + Math.cos(orbit) * 8)),
        y: Math.max(2, Math.min(sim.arena.h - 2, p.y + Math.sin(orbit) * 8 - 2)),
    };
    moveTo(e, want, sim.settings.enemySpeed * 0.9, dt);
    e.facing = toPlayer(sim, e);
    const lit = e.stagger <= 0 && distToPlayer(sim, e) < 14 && sees(sim, e);
    e.state = lit ? 'spot' : 'hover';
    if (lit) sim.marked = true;
}

// --- riot toy ----------------------------------------------------------------------------------

function riot(sim: Sim, e: Enemy, dt: number): void {
    const s = sim.settings;
    switch (e.state) {
        case 'walk': {
            turnToward(e, toPlayer(sim, e), 2.2, dt);
            const speed = s.enemySpeed * 0.6;
            e.pos.x += Math.cos(e.facing) * speed * dt;
            e.pos.y += Math.sin(e.facing) * speed * dt;
            if (e.t > 1 && distToPlayer(sim, e) < 8 && sees(sim, e)) setState(e, 'windup');
            break;
        }
        case 'windup':
            turnToward(e, toPlayer(sim, e), 4, dt);
            if (e.t > 0.7) setState(e, 'charge');
            break;
        case 'charge': {
            const before = { ...e.pos };
            e.pos.x += Math.cos(e.facing) * s.chargeSpeed * dt;
            e.pos.y += Math.sin(e.facing) * s.chargeSpeed * dt;
            burst(sim, e.pos, 1, '#94a3b8', 'dot', 2, e.facing + Math.PI, 0.2);
            const pushed = collide(sim.arena, e.pos, RADIUS.riot);
            if (pushed && Math.hypot(e.pos.x - before.x, e.pos.y - before.y) < s.chargeSpeed * dt * 0.5) {
                slam(sim, e);
                break;
            }
            chargeHits(sim, e);
            if (e.t > 0.8) setState(e, 'rest');
            break;
        }
        case 'stunned':
            if (e.t > 1.8) setState(e, 'walk');
            break;
        case 'rest':
            if (e.t > 0.6) setState(e, 'walk');
            break;
    }
}

// Charged into a block or the wall: stunned.
function slam(sim: Sim, e: Enemy): void {
    setState(e, 'stunned');
    addShake(sim, 0.45);
    addStop(sim, 3);
    burst(sim, e.pos, 14, '#cbd5e1', 'square', 8, e.facing + Math.PI, 0.2);
    floater(sim, e.pos, 'STUNNED', '#fde047', 1.1);
}

// Whoever it runs into on the way: the player (2 damage, thrown) or its own side.
function chargeHits(sim: Sim, e: Enemy): void {
    const p = sim.player;
    if (Math.hypot(p.pos.x - e.pos.x, p.pos.y - e.pos.y) < RADIUS.riot + PLAYER_RADIUS && !invincible(p)) {
        hitPlayer(sim, 2, e.facing, 16);
        setState(e, 'rest');
        return;
    }
    for (const o of sim.enemies) {
        if (o === e || o.spawnT > 0 || flies(o)) continue;
        if (Math.hypot(o.pos.x - e.pos.x, o.pos.y - e.pos.y) < RADIUS.riot + RADIUS[o.kind]) {
            o.hp -= 3;
            o.flash = 0.1;
            o.stagger = 0.6;
            o.vel.x += Math.cos(e.facing) * 14;
            o.vel.y += Math.sin(e.facing) * 14;
            floater(sim, o.pos, 'friendly fire!', '#fde047', 0.9);
            addShake(sim, 0.3);
        }
    }
}

// --- diver (bolt drone) ------------------------------------------------------------------------

const DIVE_SPEED = 17;

function diver(sim: Sim, e: Enemy, dt: number): void {
    const p = sim.player.pos;
    switch (e.state) {
        case 'circle': {
            const orbit = sim.time * 0.9 + e.id * 2;
            moveTo(e, { x: p.x + Math.cos(orbit) * 7, y: p.y + Math.sin(orbit) * 7 }, sim.settings.enemySpeed * 1.4, dt);
            if (e.t > 2.5 + (e.id % 3) * 0.6 && distToPlayer(sim, e) < 10) {
                e.target = { ...p }; // locks onto where you are now
                setState(e, 'lock');
            }
            break;
        }
        case 'lock': // the tell: it stops and a line points at the spot it will dive through
            e.facing = Math.atan2(e.target!.y - e.pos.y, e.target!.x - e.pos.x);
            if (e.t > 0.6) setState(e, 'dive');
            break;
        case 'dive': {
            e.pos.x += Math.cos(e.facing) * DIVE_SPEED * dt;
            e.pos.y += Math.sin(e.facing) * DIVE_SPEED * dt;
            const pl = sim.player;
            if (Math.hypot(pl.pos.x - e.pos.x, pl.pos.y - e.pos.y) < RADIUS.diver + PLAYER_RADIUS && !invincible(pl)) {
                hitPlayer(sim, 1, e.facing, 10);
            }
            const hitWall = collide(sim.arena, { ...e.pos }, RADIUS.diver) !== null;
            if (hitWall || e.t > 1.0) { // it ploughs into the floor
                collide(sim.arena, e.pos, RADIUS.diver);
                setState(e, 'stuck');
                addShake(sim, 0.15);
                burst(sim, e.pos, 8, '#a8a29e', 'square', 4, NaN, 0.15);
            }
            break;
        }
        case 'stuck':
            if (e.t > 1.5) setState(e, 'circle');
            break;
    }
}

// --- grenadier ---------------------------------------------------------------------------------

function grenadier(sim: Sim, e: Enemy, dt: number): void {
    const d = distToPlayer(sim, e);
    e.facing = toPlayer(sim, e);
    if (e.state === 'move') {
        const a = toPlayer(sim, e);
        const want = d < 9 ? a + Math.PI : d > 13 ? a : a + Math.PI / 2; // keep 9 … 13 m, drift sideways
        moveTo(e, { x: e.pos.x + Math.cos(want), y: e.pos.y + Math.sin(want) }, sim.settings.enemySpeed * 0.6, dt);
        if (e.t > 2.2 && d < 16) setState(e, 'throw');
    } else if (e.state === 'throw') { // a short wind-up, then the bomb
        if (e.stagger > 0) { setState(e, 'move'); return; }
        if (e.t > 0.45) {
            const p = sim.player.pos;
            const lead = { x: p.x + sim.player.vel.x * 0.2, y: p.y + sim.player.vel.y * 0.2 };
            sim.bombs.push({ from: { ...e.pos }, to: lead, t: 0, flight: 1.15, owner: e.id, batted: false });
            setState(e, 'move');
        }
    }
}

// --- wind-up mouse -----------------------------------------------------------------------------

function mouse(sim: Sim, e: Enemy, dt: number): void {
    const a = toPlayer(sim, e) + Math.sin(sim.time * 9 + e.id) * 0.35; // wiggly run
    e.facing = a;
    const speed = sim.settings.enemySpeed * 2.1;
    e.pos.x += Math.cos(a) * speed * dt;
    e.pos.y += Math.sin(a) * speed * dt;
    if (distToPlayer(sim, e) < RADIUS.mouse + PLAYER_RADIUS + 0.1) e.hp = 0; // touched you: it goes off (Sim explodes it)
}

export const MOUSE_BLAST = 1.4;

// Damage the player (unless invincible) — shared by bolts, charges, blasts and hazards.
export function hitPlayer(sim: Sim, damage: number, angle: number, push: number): void {
    const p = sim.player;
    if (invincible(p) || sim.phase !== 'fight') return;
    p.hp -= damage;
    p.hurtT = 0.9;
    p.vel.x += Math.cos(angle) * push;
    p.vel.y += Math.sin(angle) * push;
    sim.stats.hitsTaken++;
    sim.stats.streak = 0;
    addShake(sim, 0.5);
    addStop(sim, 2);
    burst(sim, p.pos, 10, '#f472b6', 'dot', 6, angle, 0.2);
    floater(sim, p.pos, `-${damage}`, '#f87171', 1.2);
}
