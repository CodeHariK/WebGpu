// Combat Sandbox — game feel: screen shake (trauma squared, so small hits barely move it and big
// ones really do), hit-stop (freeze a few frames when a hit lands), slow motion, particles and
// floating numbers. Every effect checks its switch in Settings, so you can feel what each one adds.
import type { Particle, Sim, Vec } from './Map21State';

export function addShake(sim: Sim, trauma: number): void {
    if (!sim.settings.shake) return;
    sim.juice.shake = Math.min(1, sim.juice.shake + trauma * sim.settings.shakeAmount);
}

export function addStop(sim: Sim, times: number): void {
    if (!sim.settings.hitStop) return;
    sim.juice.stop = Math.max(sim.juice.stop, (sim.settings.hitStopMs / 1000) * times);
}

export function slowMotion(sim: Sim, seconds: number): void {
    if (!sim.settings.slowmo) return;
    sim.juice.slow = Math.max(sim.juice.slow, seconds);
}

export function floater(sim: Sim, pos: Vec, text: string, color: string, size = 1): void {
    if (!sim.settings.numbers && /^\d/.test(text)) return;
    sim.floaters.push({ pos: { x: pos.x + (Math.random() - 0.5) * 0.4, y: pos.y - 0.4 }, text, color, life: 0.8, size });
}

export const BANNER_LIFE = 2.0;

export function banner(sim: Sim, text: string, sub = ''): void {
    sim.banner = { text, sub, life: BANNER_LIFE };
}

// A spray of particles from `pos`, mostly along `dir` (radians; NaN = all round).
export function burst(sim: Sim, pos: Vec, count: number, color: string, kind: Particle['kind'], speed: number, dir = NaN, size = 0.18): void {
    if (!sim.settings.particles) return;
    for (let i = 0; i < count; i++) {
        const a = Number.isNaN(dir) ? Math.random() * Math.PI * 2 : dir + (Math.random() - 0.5) * 1.4;
        const v = speed * (0.4 + Math.random() * 0.8);
        const life = 0.25 + Math.random() * 0.45;
        sim.particles.push({
            pos: { x: pos.x, y: pos.y },
            vel: { x: Math.cos(a) * v, y: Math.sin(a) * v },
            life, maxLife: life, size: size * (0.6 + Math.random() * 0.8), color, kind,
            spin: (Math.random() - 0.5) * 20,
        });
    }
}

// One expanding ring (bubble pops, the pop attack, explosions).
export function ring(sim: Sim, pos: Vec, size: number, color: string, life = 0.3): void {
    if (!sim.settings.particles) return;
    sim.particles.push({ pos: { ...pos }, vel: { x: 0, y: 0 }, life, maxLife: life, size, color, kind: 'ring', spin: 0 });
}

// Advance shake decay, view kick, particles and floaters (real-time effects run even in hit-stop).
export function stepJuice(sim: Sim, dt: number): void {
    sim.juice.shake = Math.max(0, sim.juice.shake - dt * 1.8);
    sim.juice.kick.x *= Math.exp(-dt * 18);
    sim.juice.kick.y *= Math.exp(-dt * 18);
    for (const p of sim.particles) {
        if (p.kind === 'smoke') p.vel.y -= dt * 1.5; // smoke drifts "up" the screen
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.vel.x *= Math.exp(-dt * 4);
        p.vel.y *= Math.exp(-dt * 4);
        p.life -= dt;
    }
    sim.particles = sim.particles.filter((p) => p.life > 0);
    for (const f of sim.floaters) {
        f.pos.y -= dt * 1.4;
        f.life -= dt;
    }
    sim.floaters = sim.floaters.filter((f) => f.life > 0);
    if (sim.banner) {
        sim.banner.life -= dt;
        if (sim.banner.life <= 0) sim.banner = null;
    }
}
