// Wick — effects: screen shake, a short hit-stop, particles (sparks, smoke, wax drips, rings),
// floating text and the big banner. Stepped in real time, even during hit-stop.
import type { Particle, Sim, Vec } from './Map22State';

export const BANNER_LIFE = 2.4;

export function shake(sim: Sim, amount: number): void {
    if (sim.settings.shake) sim.shake = Math.min(1, sim.shake + amount);
}

export function stop(sim: Sim, seconds: number): void {
    sim.stop = Math.max(sim.stop, seconds);
}

export function floater(sim: Sim, pos: Vec, text: string, color: string, size = 1): void {
    sim.floaters.push({ pos: { x: pos.x, y: pos.y - 0.5 }, text, color, life: 1, size });
}

export function banner(sim: Sim, text: string, sub = ''): void {
    sim.banner = { text, sub, life: BANNER_LIFE };
}

/** `count` particles from `pos`, spread round `dir` (radians; NaN: all round). */
export function burst(sim: Sim, pos: Vec, count: number, color: string, kind: Particle['kind'], speed: number, dir = NaN, size = 0.15): void {
    for (let i = 0; i < count; i++) {
        const a = Number.isNaN(dir) ? Math.random() * Math.PI * 2 : dir + (Math.random() - 0.5) * 1.2;
        const v = speed * (0.3 + Math.random() * 0.9);
        const life = 0.3 + Math.random() * 0.5;
        sim.particles.push({ pos: { ...pos }, vel: { x: Math.cos(a) * v, y: Math.sin(a) * v }, life, maxLife: life, size: size * (0.6 + Math.random() * 0.8), color, kind });
    }
}

export function ring(sim: Sim, pos: Vec, size: number, color: string, life = 0.35): void {
    sim.particles.push({ pos: { ...pos }, vel: { x: 0, y: 0 }, life, maxLife: life, size, color, kind: 'ring' });
}

export function stepFx(sim: Sim, dt: number): void {
    sim.shake = Math.max(0, sim.shake - dt * 2);
    for (const p of sim.particles) {
        if (p.kind === 'smoke' || p.kind === 'spark') p.vel.y -= dt * 2.5; // heat rises (up the screen)
        if (p.kind === 'drip') p.vel.y += dt * 6;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.vel.x *= Math.exp(-dt * 3);
        p.vel.y *= Math.exp(-dt * 3);
        p.life -= dt;
    }
    sim.particles = sim.particles.filter((p) => p.life > 0);
    for (const f of sim.floaters) { f.pos.y -= dt * 1.2; f.life -= dt; }
    sim.floaters = sim.floaters.filter((f) => f.life > 0);
    if (sim.banner) {
        sim.banner.life -= dt;
        if (sim.banner.life <= 0) sim.banner = null;
    }
}
