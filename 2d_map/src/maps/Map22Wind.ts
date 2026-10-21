// Wick — gusts of wind and wax vats (the Wax Works).
//
//   gust  over an open stretch: leaves start to rustle across it (the tell), then it BLOWS for a
//         couple of seconds. Inside it you're pushed along (dash through it, or brace against
//         it), your flames drift with it, embers burn out faster, and your own flame bends low:
//         your light shrinks — the dark comes in on the windy switchbacks.
//   vat   a pool of warm wax. It glows (a light the shades won't cross), and wading in it melts
//         wax back onto you — but it's thick, so you wade slowly.
import type { GustDef } from './Map22Level';
import type { Sim, Vec } from './Map22State';

export const GUST_WARN = 1.0;
export const GUST_BLOW = 2.2;
const PUSH = 3; // m/s you're pushed
const FLAME_DRIFT = 14; // m/s² on flames in flight
export const GUST_LIGHT = 0.6; // your light radius in a blowing gust (fraction)
const VAT_HEAL = 15; // wax per second while wading
export const VAT_SLOW = 0.5;

export function gustState(g: GustDef, time: number): 'calm' | 'warn' | 'blow' {
    const t = (time + g.phase) % g.period;
    if (t < GUST_BLOW) return 'blow';
    if (t > g.period - GUST_WARN) return 'warn';
    return 'calm';
}

export const inGust = (g: GustDef, p: Vec) => Math.hypot(p.x - g.pos.x, p.y - g.pos.y) < g.r;

/** The wind at p right now (zero where it's calm). */
export function windAt(sim: Sim, p: Vec): Vec {
    for (const g of sim.level.gusts) {
        if (gustState(g, sim.time) !== 'blow' || !inGust(g, p)) continue;
        const l = Math.hypot(g.dir.x, g.dir.y) || 1;
        return { x: g.dir.x / l, y: g.dir.y / l };
    }
    return { x: 0, y: 0 };
}

export const inVat = (sim: Sim, p: Vec) => sim.level.vats.some((v) => Math.hypot(p.x - v.x, p.y - v.y) < v.r);

export function stepWind(sim: Sim, dt: number): void {
    const p = sim.player;
    const w = windAt(sim, p.pos);
    if ((w.x || w.y) && p.dashT <= 0 && !p.held) { p.pos.x += w.x * PUSH * dt; p.pos.y += w.y * PUSH * dt; }
    for (const f of sim.flames) {
        const fw = windAt(sim, f.pos);
        f.vel.x += fw.x * FLAME_DRIFT * dt;
        f.vel.y += fw.y * FLAME_DRIFT * dt;
    }
    for (const e of sim.embers) { const ew = windAt(sim, e.pos); if (ew.x || ew.y) e.life -= dt; } // burns out twice as fast
    if (inVat(sim, p.pos)) p.wax = Math.min(sim.settings.maxWax, p.wax + VAT_HEAL * dt);
}

/** Your light shrinks while a gust blows on you. */
export const gustLight = (sim: Sim) => {
    const w = windAt(sim, sim.player.pos);
    return w.x || w.y ? GUST_LIGHT : 1;
};
