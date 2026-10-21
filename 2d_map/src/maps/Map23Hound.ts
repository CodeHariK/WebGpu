// Lockpick — the hound. It doesn't see far (a few metres, all round, light or dark), but it SMELLS:
// wherever you walk you leave a trail of scent for a while. The hound wanders its kennel route;
// when it crosses your trail it follows it to you. When it finds you it barks — every guard who
// hears comes to look — and gives chase. Barrels don't fool it: it sniffs you out and barks at the
// barrel. Water washes your scent away: wade through it and the trail ends there.
import { centre, findPath, lineOfSight } from './Map23Grid';
import { followPath, GUARD_R, setState } from './Map23Guards';
import { makeNoise } from './Map23Noise';
import type { Guard, Sim, Vec } from './Map23State';

const SCENT_LIFE = 14; // seconds your trail lasts
const SNIFF_REACH = 2.6; // it notices a trail this close
const NOSE_SIGHT = 3.2; // it notices YOU this close (any light)
const BARK = 14;
const CHASE_TIME = 5;

const dist = (a: Vec, b: Vec) => Math.hypot(a.x - b.x, a.y - b.y);

export function forgetOldScent(sim: Sim): void {
    sim.scent = sim.scent.filter((s) => sim.time - s.t < SCENT_LIFE);
}

// The freshest scent near the hound that's fresher than `after` (or null).
function nextScent(sim: Sim, g: Guard, after: number): Vec & { t: number } | null {
    let best: (Vec & { t: number }) | null = null;
    for (const s of sim.scent) {
        if (s.t <= after || dist(s.pos, g.pos) > SNIFF_REACH) continue;
        if (!best || s.t > best.t) best = { ...s.pos, t: s.t };
    }
    return best;
}

export function stepHound(sim: Sim, g: Guard, dt: number): void {
    const s = sim.settings;
    const p = sim.player;
    g.t += dt;
    const near = dist(g.pos, p.pos) < NOSE_SIGHT && lineOfSight(sim, g.pos, p.pos);
    const hiddenHere = p.hidden >= 0 && dist(centre(sim.barrels[p.hidden]), g.pos) < 1.6;
    if ((near && p.hidden < 0) || hiddenHere) {
        if (g.state !== 'bark' && g.state !== 'chase') {
            setState(g, 'bark');
            makeNoise(sim, g.pos, BARK, true);
            sim.floaters.push({ pos: { ...g.pos }, text: hiddenHere ? 'WOOF! (at the barrel)' : 'WOOF!', color: '#f87171', life: 1.3 });
            sim.stats.spotted++;
            if (hiddenHere) sim.exposed = p.hidden;
        }
    }
    switch (g.state) {
        case 'patrol': {
            const trail = nextScent(sim, g, sim.time - SCENT_LIFE);
            if (trail) { g.alert = trail.t; setState(g, 'track'); break; }
            if (!g.path.length) g.path = findPath(sim, g.pos, g.route[g.routeI]);
            if (followPath(sim, g, s.patrolSpeed * 1.1, dt)) g.routeI = (g.routeI + 1) % g.route.length;
            break;
        }
        case 'track': { // nose down, from one bit of trail to the next fresher one
            const next = nextScent(sim, g, g.alert);
            if (!next) { if (g.t > 2) setState(g, 'patrol'); break; }
            const d = dist(g.pos, next);
            const step = Math.min(d, s.houndSpeed * 0.8 * dt);
            if (d > 0.01) { g.pos.x += ((next.x - g.pos.x) / d) * step; g.pos.y += ((next.y - g.pos.y) / d) * step; g.facing = Math.atan2(next.y - g.pos.y, next.x - g.pos.x); }
            if (d < 0.3) { g.alert = next.t; g.t = 0; }
            break;
        }
        case 'bark':
            g.facing = Math.atan2(p.pos.y - g.pos.y, p.pos.x - g.pos.x);
            if (g.t > 0.7) { if (p.hidden >= 0) { setState(g, 'patrol'); break; } setState(g, 'chase'); }
            break;
        case 'chase': {
            if (g.repath <= 0) { g.path = findPath(sim, g.pos, p.pos); g.repath = 0.3; }
            g.repath -= dt;
            followPath(sim, g, s.houndSpeed, dt);
            if (p.hidden < 0 && dist(g.pos, p.pos) < GUARD_R + 0.35 && sim.phase === 'play') { sim.phase = 'caught'; sim.phaseT = 0; }
            if (g.t > CHASE_TIME && !near) { g.path = []; setState(g, 'patrol'); }
            break;
        }
    }
}
