// Wick — light. Everything here runs on how lit a place is: shades burn in it, the weeper freezes in
// it, moths fly to the brightest thing near them. The sources are your own flame (its radius shrinks
// with your wax), lit lanterns, embers where your flames landed, flames in flight, and the flare.
// A gloom is the opposite: a light with negative strength that dims everything round it.
import type { Sim, Vec } from './Map22State';

export type Light = { pos: Vec; r: number; k: number; you?: boolean }; // k: strength 0 … 1 (below 0: a gloom); you: your own flame

export const GLOOM_R = 4.5;

export const EMBER_R = 2.4;
export const FLAME_R = 1.8;

/** Your flame's radius: small at no wax, big at full. */
export function playerRadius(sim: Sim): number {
    const s = sim.settings;
    return (s.lightMin + (s.lightMax - s.lightMin) * Math.max(0, sim.player.wax) / s.maxWax) * sim.windLight;
}

/** Every light in the world this frame. */
export function lights(sim: Sim): Light[] {
    const out: Light[] = [];
    const p = sim.player;
    if (sim.phase !== 'snuffed') out.push({ pos: p.pos, r: playerRadius(sim), k: 1, you: true });
    if (p.flareT > 0) out.push({ pos: p.pos, r: sim.settings.flareRadius * (0.6 + p.flareT), k: 1 });
    for (const l of sim.lanterns) if (l.lit) out.push({ pos: l.pos, r: sim.settings.lanternRadius * Math.min(1, 0.4 + l.litT), k: 1 });
    for (const e of sim.embers) out.push({ pos: e.pos, r: EMBER_R * Math.min(1, e.life / 1.5 + 0.2), k: 0.9 });
    for (const f of sim.flames) out.push({ pos: f.pos, r: FLAME_R, k: 0.8 });
    for (const v of sim.level.vats) out.push({ pos: v, r: v.r + 2.6, k: 0.85 });
    for (const e of sim.enemies) if (e.kind === 'gloom' && e.spawnT <= 0) out.push({ pos: e.pos, r: GLOOM_R, k: -0.9 });
    return out;
}

/** How lit p is: 0 (dark) … 1 (right by a flame). */
export function lightAt(all: Light[], p: Vec): number {
    let best = 0;
    for (const l of all) {
        if (l.k < 0) continue;
        const d = Math.hypot(p.x - l.pos.x, p.y - l.pos.y);
        if (d >= l.r) continue;
        const f = 1 - (d / l.r) ** 2;
        best = Math.max(best, f * l.k);
    }
    return best * dimming(all, p);
}

/** How much glooms leave of the light at p (1: none near; less inside a gloom). */
function dimming(all: Light[], p: Vec): number {
    let keep = 1;
    for (const l of all) {
        if (l.k >= 0) continue;
        const d = Math.hypot(p.x - l.pos.x, p.y - l.pos.y);
        if (d < l.r) keep *= 1 + l.k * (1 - (d / l.r) ** 2);
    }
    return keep;
}

/** How lit p is for something that only freezes while SEEN: your own flame counts only in front of
 *  you (where you're looking); every other light counts all round. */
export function seenLightAt(all: Light[], p: Vec, aim: number): number {
    let best = 0;
    for (const l of all) {
        const d = Math.hypot(p.x - l.pos.x, p.y - l.pos.y);
        if (d >= l.r || l.k < 0) continue;
        if (l.you) {
            const a = Math.atan2(p.y - l.pos.y, p.x - l.pos.x);
            if (Math.abs(Math.atan2(Math.sin(a - aim), Math.cos(a - aim))) > GAZE_HALF) continue;
        }
        best = Math.max(best, (1 - (d / l.r) ** 2) * l.k);
    }
    return best * dimming(all, p);
}

export const GAZE_HALF = 1.2; // radians either side of where you aim

/** The direction out of the light from p (away from the strongest source). */
export function awayFromLight(all: Light[], p: Vec): number {
    let best: Light | null = null;
    let bestF = 0;
    for (const l of all) {
        const d = Math.hypot(p.x - l.pos.x, p.y - l.pos.y);
        const f = d < l.r ? (1 - d / l.r) * l.k : 0;
        if (f > bestF) { bestF = f; best = l; }
    }
    return best ? Math.atan2(p.y - best.pos.y, p.x - best.pos.x) : Math.random() * Math.PI * 2;
}
