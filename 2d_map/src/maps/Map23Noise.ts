// Lockpick — sound. Everything you do makes some: footsteps (how loud depends on how you move and
// what you walk on), doors, dousing a torch, splashing, stepping on an alarm plate, a pebble landing.
// A guard who hears a noise goes to look (walls muffle it: through rock it carries half as far).
// Sleepers wake. That's also how you DISTRACT them: throw a pebble where you want them to go.
import type { Noise, Sim, Vec } from './Map23State';
import { lineOfSight } from './Map23Grid';

const LIFE = 0.8; // seconds a noise ring is shown

/** Make a noise at pos heard within r metres (loud: wakes sleepers from further, alarms). */
export function makeNoise(sim: Sim, pos: Vec, r: number, loud = false): void {
    sim.noises.push({ pos: { ...pos }, r, t: 0, loud });
    sim.stats.noises++;
}

/** Can a guard at `ear` hear this noise? */
export function hears(sim: Sim, ear: Vec, n: Noise, sleepy = false): boolean {
    const d = Math.hypot(ear.x - n.pos.x, ear.y - n.pos.y);
    const reach = n.r * (lineOfSight(sim, ear, n.pos) ? 1 : 0.5) * (sleepy && !n.loud ? 0.6 : 1);
    return d < reach;
}

export function ageNoises(sim: Sim, dt: number): void {
    for (const n of sim.noises) n.t += dt;
    sim.noises = sim.noises.filter((n) => n.t < LIFE);
}

/** New noises this frame (the guards react only once, to fresh ones). */
export const freshNoises = (sim: Sim) => sim.noises.filter((n) => n.t === 0);

export const NOISE_LIFE = LIFE;
