// Combat Sandbox — stamp rows (Toy Assembly Plant). A corridor of stamp heads side by side; they
// slam one after another, so a wave of stamping runs down the row and comes round again. Each head
// glows red for a moment before it drops. Caught under one: 2 damage and thrown; a toy under one is
// stamped flat. Run in right behind the wave, or dash through a head as it lands. Breaking the
// row's terminal stops them all.
import { flies, hitPlayer } from './Map21Enemies';
import { addShake, burst, floater } from './Map21Juice';
import type { Arena, Sim } from './Map21State';
import { inRect } from './Map21World';

export const STAMP_WARN = 0.5;
export const STAMP_SLAM = 0.25;
const PLAYER_DAMAGE = 2;
const TOY_DAMAGE = 10;

type Stamp = Arena['stamps'][number];
export type Head = { x: number; y: number; w: number; h: number };

/** The rectangle under head i. */
export function stampHead(s: Stamp, i: number): Head {
    const w = s.w / s.n;
    return { x: s.x + i * w, y: s.y, w, h: s.h };
}

/** Where head i is in its cycle. */
export function headState(s: Stamp, i: number): 'up' | 'warn' | 'slam' {
    if (s.off) return 'up';
    const t = (((s.t - i * s.step) % s.period) + s.period) % s.period;
    if (t > s.period - STAMP_SLAM) return 'slam';
    if (t > s.period - STAMP_SLAM - STAMP_WARN) return 'warn';
    return 'up';
}

export function stepStamps(sim: Sim, dt: number): void {
    for (const s of sim.arena.stamps) {
        if (s.off) continue;
        const before = Array.from({ length: s.n }, (_, i) => headState(s, i));
        s.t += dt;
        for (let i = 0; i < s.n; i++) if (before[i] !== 'slam' && headState(s, i) === 'slam') slam(sim, stampHead(s, i));
    }
}

function slam(sim: Sim, h: Head): void {
    const c = { x: h.x + h.w / 2, y: h.y + h.h / 2 };
    burst(sim, c, 6, '#94a3b8', 'square', 5, NaN, 0.15);
    addShake(sim, 0.08);
    const p = sim.player;
    if (inRect(p.pos, h)) hitPlayer(sim, PLAYER_DAMAGE, p.pos.x < c.x ? Math.PI : 0, 14);
    for (const e of sim.enemies) {
        if (e.spawnT > 0 || flies(e) || !inRect(e.pos, h)) continue;
        e.hp -= TOY_DAMAGE;
        e.flash = 0.1;
        floater(sim, e.pos, 'STAMPED', '#fde047', 1.1);
    }
}
