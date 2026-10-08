// Enemy Arena — what an enemy notices.
//   sight    inside the view cone and range, and one ray to the player is clear. Very close
//            (FEEL) it notices you even from behind.
//   meter    fills while it sees you — faster when you're close, slower when you sneak — and
//            drains when it doesn't. Full = spotted. So you get a moment to duck back out.
//   hearing  a noise (footsteps, a shout) reaches it if it's within the noise's radius.
import { angleDiff, angleOf, dist, sub } from './Map20Math';
import { clear } from './Map20World';
import type { Enemy, Noise, Sim } from './Map20State';

const FEEL = 1.3; // metres: this close it notices you whatever way it faces
const DRAIN = 0.25; // meter lost per second unseen

export function canSee(sim: Sim, e: Enemy): boolean {
    const p = sim.player.pos;
    const d = dist(e.pos, p);
    if (d > sim.settings.viewRange) return false;
    const inCone = Math.abs(angleDiff(e.facing, angleOf(sub(p, e.pos)))) <= (sim.settings.viewAngle * Math.PI) / 360;
    if (!inCone && d > FEEL) return false;
    return clear(sim.world, e.pos, p);
}

/** Fill or drain the detection meter. Returns true the moment it fills. */
export function updateMeter(sim: Sim, e: Enemy, sees: boolean, dt: number): boolean {
    if (!sees) {
        e.meter = Math.max(0, e.meter - DRAIN * dt);
        return false;
    }
    const d = dist(e.pos, sim.player.pos);
    const near = 1 + 2 * Math.max(0, 1 - d / sim.settings.viewRange); // ×3 at point blank, ×1 at the edge
    const sneak = sim.player.sneaking ? 0.5 : 1;
    const before = e.meter;
    e.meter = Math.min(1, e.meter + (dt / sim.settings.detectTime) * near * sneak * 0.5);
    return before < 1 && e.meter >= 1;
}

/** The newest noise this enemy can hear this frame, or null. */
export function heard(sim: Sim, e: Enemy): Noise | null {
    if (!sim.settings.hearing) return null;
    for (const n of sim.noises) {
        if (n.age === 0 && dist(n.pos, e.pos) <= n.radius) return n;
    }
    return null;
}
