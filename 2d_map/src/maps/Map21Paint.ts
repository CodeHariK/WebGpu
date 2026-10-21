// Combat Sandbox — paint sprayers (Toy Assembly Plant, the PAINT station). A nozzle on a wall
// drips for a moment (the warning), then sprays a band of paint across the room. Caught in it,
// you're PAINTED: for a few seconds every toy knows where you are (riflemen aim as if a spotter had
// you lit). Dashing through a spray keeps you clean.
import { burst, floater } from './Map21Juice';
import type { Arena, Sim, Vec } from './Map21State';

export const SPRAY_WARN = 0.8;
export const SPRAY_TIME = 1.2;
export const SPRAY_HALF = 0.8; // half the band's width
const PAINTED_FOR = 4;
export const PAINT_COLORS = ['#f472b6', '#22d3ee', '#facc15', '#a78bfa'];

type Sprayer = Arena['sprayers'][number];

export function sprayState(s: Sprayer): 'idle' | 'warn' | 'spray' {
    const t = s.t % s.period;
    if (t < SPRAY_TIME) return 'spray';
    if (t > s.period - SPRAY_WARN) return 'warn';
    return 'idle';
}

/** Is p inside the sprayer's band? */
export function inSpray(s: Sprayer, p: Vec): boolean {
    const dx = p.x - s.pos.x;
    const dy = p.y - s.pos.y;
    const along = dx * s.dir.x + dy * s.dir.y;
    const across = Math.abs(dx * -s.dir.y + dy * s.dir.x);
    return along > 0 && along < s.len && across < SPRAY_HALF;
}

/** Each sprayer's colour (by its place in the list). */
export const sprayColor = (sim: Sim, s: Sprayer) => PAINT_COLORS[sim.arena.sprayers.indexOf(s) % PAINT_COLORS.length];

export function stepSprayers(sim: Sim, dt: number): void {
    const p = sim.player;
    for (const s of sim.arena.sprayers) {
        if (s.off) continue;
        const before = sprayState(s);
        s.t += dt;
        if (sprayState(s) !== 'spray') continue;
        if (before !== 'spray') burst(sim, s.pos, 6, sprayColor(sim, s), 'dot', 5, Math.atan2(s.dir.y, s.dir.x), 0.3);
        if (p.dashT <= 0 && inSpray(s, p.pos)) {
            if (p.paintT < PAINTED_FOR - 1) floater(sim, p.pos, 'PAINTED! they all see you', sprayColor(sim, s), 1.1);
            p.paintT = PAINTED_FOR;
        }
    }
}
