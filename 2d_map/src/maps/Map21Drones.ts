// Combat Sandbox — the Assembly Plant's support drones (Map21Enemies keeps the shared machinery).
//
//   magnet     hovers at a distance. Every few seconds it CHARGES (blue rings close in on it), then
//              PULLS for a while: you're dragged toward it — onto belts, under arms, into glue or
//              stamps. Dashing breaks the pull for that moment; a hit while it charges cancels it.
//   projector  a little drone that tucks in behind whichever toy is closest to you and throws a
//              SHIELD over it: bubbles bounce off. Pops, fizz blasts and the place itself still get
//              through — or break the projector first.
import { burst, floater } from './Map21Juice';
import type { Enemy, Sim, Vec } from './Map21State';
import { moveTo, setState } from './Map21Enemies';

const dist = (a: Vec, b: Vec) => Math.hypot(a.x - b.x, a.y - b.y);
const angleTo = (a: Vec, b: Vec) => Math.atan2(b.y - a.y, b.x - a.x);

// --- magnet ------------------------------------------------------------------------------------

export const CHARGE_TIME = 0.8;
const PULL_TIME = 1.8;
const PULL_SPEED = 4.2; // m/s you're dragged (you walk 7.5: you can fight it, but not stand still)
export const PULL_RANGE = 11;
const MAGNET_REST = 3.5;

export function magnet(sim: Sim, e: Enemy, dt: number): void {
    const p = sim.player;
    e.facing = angleTo(e.pos, p.pos);
    switch (e.state) {
        case 'hover': {
            const orbit = sim.time * 0.35 + e.id * 1.7;
            const want = { x: p.pos.x + Math.cos(orbit) * 8, y: p.pos.y + Math.sin(orbit) * 8 };
            want.x = Math.max(2, Math.min(sim.arena.w - 2, want.x));
            want.y = Math.max(2, Math.min(sim.arena.h - 2, want.y));
            moveTo(e, want, sim.settings.enemySpeed * 0.8, dt);
            if (e.t > MAGNET_REST && dist(e.pos, p.pos) < PULL_RANGE) setState(e, 'charge');
            break;
        }
        case 'charge':
            if (e.stagger > 0.2) { floater(sim, e.pos, 'shorted', '#93c5fd', 0.8); setState(e, 'hover'); break; }
            if (e.t > CHARGE_TIME) setState(e, 'pull');
            break;
        case 'pull': {
            const d = dist(e.pos, p.pos);
            if (d > 1.4 && d < PULL_RANGE && p.dashT <= 0 && !p.held) {
                p.pos.x += ((e.pos.x - p.pos.x) / d) * PULL_SPEED * dt;
                p.pos.y += ((e.pos.y - p.pos.y) / d) * PULL_SPEED * dt;
                if (Math.random() < 0.3) burst(sim, p.pos, 1, '#93c5fd', 'dot', 2, angleTo(p.pos, e.pos), 0.2);
            }
            for (const k of sim.pickups) { // it drags the loot too
                const dk = dist(e.pos, k.pos);
                if (dk > 0.5 && dk < PULL_RANGE) { k.pos.x += ((e.pos.x - k.pos.x) / dk) * PULL_SPEED * dt; k.pos.y += ((e.pos.y - k.pos.y) / dk) * PULL_SPEED * dt; }
            }
            if (e.t > PULL_TIME) setState(e, 'hover');
            break;
        }
    }
}

// --- projector ---------------------------------------------------------------------------------

const SHIELD_REACH = 3.5; // it has to be this close to its ward to keep the shield up

/** The toy a projector is guarding: the one nearest the player (not another projector or magnet). */
export function wardOf(sim: Sim, e: Enemy): Enemy | null {
    let best: Enemy | null = null;
    let bestD = Infinity;
    for (const o of sim.enemies) {
        if (o === e || o.hp <= 0 || o.spawnT > 0 || o.kind === 'projector' || o.kind === 'magnet' || o.held) continue;
        if (sim.enemies.some((q) => q !== e && q.kind === 'projector' && q.owner === o.id)) continue; // already guarded
        const d = dist(o.pos, sim.player.pos);
        if (d < bestD) { bestD = d; best = o; }
    }
    return best;
}

export function projector(sim: Sim, e: Enemy, dt: number): void {
    const ward = wardOf(sim, e);
    e.owner = ward?.id;
    if (!ward) { // nobody to guard: hang about at a distance
        const p = sim.player.pos;
        const a = angleTo(p, e.pos);
        moveTo(e, { x: p.x + Math.cos(a) * 7, y: p.y + Math.sin(a) * 7 }, sim.settings.enemySpeed, dt);
        e.state = 'idle';
        return;
    }
    // Tuck in behind the ward, on the side away from you.
    const away = angleTo(sim.player.pos, ward.pos);
    const spot = { x: ward.pos.x + Math.cos(away) * 1.6, y: ward.pos.y + Math.sin(away) * 1.6 };
    moveTo(e, spot, sim.settings.enemySpeed * 1.3, dt);
    e.facing = angleTo(e.pos, ward.pos);
    const on = e.stagger <= 0 && dist(e.pos, ward.pos) < SHIELD_REACH;
    e.state = on ? 'shield' : 'follow';
    if (on) ward.shielded = true;
}
