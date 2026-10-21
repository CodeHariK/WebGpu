// Combat Sandbox — explosions (grenadier bombs, wind-up mice) and the grenadier's bombs in flight.
// A blast hurts everyone in range — the player and the toys alike — and breaks pipes it reaches,
// so a well-placed bomb (or a mouse you shot early) can do your work for you.
import { hitPlayer } from './Map21Enemies';
import { addShake, addStop, burst, ring } from './Map21Juice';
import { breakPipe, pipeDistance } from './Map21Hazards';
import { blastObjectives } from './Map21Objectives';
import type { Bomb, Sim, Vec } from './Map21State';

export const BOMB_RADIUS = 2.3;

export function explode(sim: Sim, at: Vec, radius: number, playerDamage: number, toyDamage: number): void {
    ring(sim, at, radius, 'rgba(251,146,60,0.9)', 0.3);
    burst(sim, at, 22, '#fb923c', 'spark', 10, NaN, 0.14);
    burst(sim, at, 10, 'rgba(120,113,108,0.8)', 'smoke', 2, NaN, 0.6);
    addShake(sim, 0.4);
    addStop(sim, 2);
    const p = sim.player;
    const dp = Math.hypot(p.pos.x - at.x, p.pos.y - at.y);
    if (dp < radius && playerDamage > 0) hitPlayer(sim, playerDamage, Math.atan2(p.pos.y - at.y, p.pos.x - at.x), 10);
    for (const e of sim.enemies) {
        if (e.spawnT > 0 || e.hp <= 0) continue;
        const d = Math.hypot(e.pos.x - at.x, e.pos.y - at.y);
        if (d > radius) continue;
        const a = Math.atan2(e.pos.y - at.y, e.pos.x - at.x);
        e.hp -= toyDamage;
        e.flash = 0.1;
        e.stagger = 0.5;
        e.vel.x += Math.cos(a) * 10;
        e.vel.y += Math.sin(a) * 10;
    }
    for (const pipe of sim.arena.pipes) if (!pipe.broken && pipeDistance(pipe, at) < radius) breakPipe(sim, pipe);
    blastObjectives(sim, at, radius, toyDamage);
}

// Where a bomb is now (on the ground plane) and how high above it (for drawing).
export function bombPos(b: Bomb): { pos: Vec; height: number } {
    const k = Math.min(1, b.t / b.flight);
    return {
        pos: { x: b.from.x + (b.to.x - b.from.x) * k, y: b.from.y + (b.to.y - b.from.y) * k },
        height: 4 * (b.batted ? 1.2 : 2.6) * k * (1 - k),
    };
}

export function stepBombs(sim: Sim, dt: number): void {
    for (const b of sim.bombs) {
        b.t += dt;
        if (b.t >= b.flight) explode(sim, b.to, BOMB_RADIUS, 2, b.batted ? 8 : 4);
    }
    sim.bombs = sim.bombs.filter((b) => b.t < b.flight);
}

// A bubble hit a bomb in the air: it flies back to whoever threw it (or straight on if they're gone).
export function batBomb(sim: Sim, b: Bomb): void {
    const here = bombPos(b).pos;
    const owner = sim.enemies.find((e) => e.id === b.owner);
    const back = owner ? { ...owner.pos } : { x: here.x + (here.x - b.from.x), y: here.y + (here.y - b.from.y) };
    b.from = here;
    b.to = back;
    b.t = 0;
    b.flight = 0.55;
    b.batted = true;
    ring(sim, here, 0.8, 'rgba(165,243,252,0.9)', 0.2);
}
