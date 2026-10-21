// Lockpick — the keep's guards. Each is a small state machine with eyes and ears.
//
//   EYES   a cone in front (see Settings). How far it reaches depends on the light where YOU are:
//          lit, they see you across the room; in the dark only up close. Sneaking makes you
//          harder to see, running easier. Hidden in a barrel you can't be seen at all.
//   ALERT  while a guard sees you its alert fills (faster the closer you are). A little: it stops
//          and stares (?). Full: it knows (!) and chases. Out of sight it cools down again.
//   EARS   it goes to look at noises it hears (pebbles, footsteps on loud floors, doors, splashes).
//
//   guard    walks a patrol route. Suspicious → stares; lost you → goes to where it last saw you
//            and looks round; heard something → goes to look; knows → chases (and catches you if
//            it reaches you). Passing a doused torch, it walks over and lights it again.
//   sleeper  asleep on a bunk (Zz). A noise near it wakes it: it goes to look, then back to bed.
//   watcher  stands still and sweeps a long, narrow lantern beam. If it spots you it rings the
//            alarm — every guard comes running to where you were.
import { centre, doorAt, findPath, lightAt, lineOfSight, tileAt } from './Map23Grid';
import { freshNoises, hears, makeNoise } from './Map23Noise';
import type { Guard, Sim, Vec } from './Map23State';

export const GUARD_R = 0.35;
const LOOK_TIME = 3;
const SEARCH_TIME = 4;
const RELIGHT_REACH = 6;
const INVESTIGATE_SPEED = 2.6;

const dist = (a: Vec, b: Vec) => Math.hypot(a.x - b.x, a.y - b.y);
const angleTo = (a: Vec, b: Vec) => Math.atan2(b.y - a.y, b.x - a.x);
const angleDiff = (a: number, b: number) => Math.abs(Math.atan2(Math.sin(a - b), Math.cos(a - b)));

export function setState(g: Guard, state: string): void {
    g.state = state;
    g.t = 0;
}

// --- eyes --------------------------------------------------------------------------------------

/** How far this guard can see you right now (depends on the light where you stand). */
export function sightRange(sim: Sim, g: Guard): number {
    const s = sim.settings;
    const p = sim.player;
    let r = s.darkSight + (s.sightRange - s.darkSight) * lightAt(sim, p.pos);
    if (p.mode === 'sneak') r *= 0.7;
    if (p.mode === 'run' && p.moving) r *= 1.2;
    if (g.kind === 'watcher') r *= 1.4;
    return r;
}

export const coneHalf = (sim: Sim, g: Guard) => (sim.settings.sightAngle * Math.PI) / 180 * (g.kind === 'watcher' ? 0.45 : 1);

/** 0: it doesn't see you; up to 1: right in front of it. */
export function sees(sim: Sim, g: Guard): number {
    const p = sim.player;
    if (p.hidden >= 0 || sim.phase !== 'play' || g.state === 'sleep' || g.state === 'wake') return 0;
    const d = dist(g.pos, p.pos);
    const range = sightRange(sim, g);
    const close = d < 1.1;
    if (!close && (d > range || angleDiff(angleTo(g.pos, p.pos), g.facing) > coneHalf(sim, g))) return 0;
    if (!lineOfSight(sim, g.pos, p.pos)) return 0;
    return Math.max(0.2, 1 - d / Math.max(range, 1.1));
}

// --- moving ------------------------------------------------------------------------------------

/** Walk along g.path; opens doors on the way (a locked one shuts again behind it). */
export function followPath(sim: Sim, g: Guard, speed: number, dt: number): boolean {
    let budget = speed * dt;
    while (budget > 0 && g.path.length) {
        const next = g.path[0];
        const door = doorAt(sim, Math.floor(next.x), Math.floor(next.y));
        if (door && !door.open) { door.open = true; door.shutT = door.lock ? 1.6 : 0; sim.lightDirty = true; makeNoise(sim, next, 3); }
        const d = dist(g.pos, next);
        g.facing = angleTo(g.pos, next);
        if (d <= budget) { g.pos = { ...next }; g.path.shift(); budget -= d; } else { g.pos.x += ((next.x - g.pos.x) / d) * budget; g.pos.y += ((next.y - g.pos.y) / d) * budget; budget = 0; }
    }
    return g.path.length === 0;
}

function goTo(sim: Sim, g: Guard, target: Vec): void {
    g.goal = { ...target };
    g.path = findPath(sim, g.pos, target);
}

// Look round: turn slowly through a wide arc.
function lookAround(g: Guard, dt: number): void {
    g.facing += Math.sin(g.t * 1.6) * 1.8 * dt;
}

// --- the guard's mind --------------------------------------------------------------------------

export function stepGuard(sim: Sim, g: Guard, dt: number): void {
    const s = sim.settings;
    g.t += dt;
    g.repath -= dt;
    if (g.kind === 'watcher') { watcher(sim, g, dt); return; }
    if (g.kind === 'sleeper' && g.state === 'sleep') { sleep(sim, g); return; }
    if (g.state === 'wake') { if (g.t > 0.9) setState(g, 'investigate'); return; }

    // Eyes.
    const seen = sees(sim, g);
    if (seen > 0) {
        g.alert = Math.min(1.5, g.alert + s.noticeRate * (0.35 + seen) * dt);
        g.lastSeen = { ...sim.player.pos };
    } else if (g.state !== 'chase') g.alert = Math.max(0, g.alert - 0.22 * dt);
    if (g.alert >= 1 && g.state !== 'chase') { setState(g, 'chase'); sim.stats.spotted++; sim.floaters.push({ pos: { ...g.pos }, text: '!', color: '#f87171', life: 1 }); }

    // Ears (not while chasing: it's busy).
    if (g.state !== 'chase') {
        for (const n of freshNoises(sim)) {
            if (!hears(sim, g.pos, n)) continue;
            g.alert = Math.max(g.alert, n.loud ? 0.6 : 0.3);
            goTo(sim, g, n.pos);
            setState(g, 'investigate');
        }
    }

    switch (g.state) {
        case 'patrol': {
            if (g.alert > 0.25 && seen > 0) { setState(g, 'suspicious'); break; }
            const torch = sim.torches.find((t) => !t.lit && dist(t.light, g.pos) < RELIGHT_REACH && lineOfSight(sim, g.pos, t.light));
            if (torch) { goTo(sim, g, { x: torch.light.x, y: torch.light.y + (torch.light.y < g.pos.y ? 0.4 : -0.4) }); setState(g, 'relight'); break; }
            if (g.t < 0.8) { lookAround(g, dt); break; } // pause at each waypoint
            if (!g.path.length) goTo(sim, g, g.route[g.routeI]);
            if (followPath(sim, g, s.patrolSpeed, dt)) { g.routeI = (g.routeI + 1) % g.route.length; g.t = 0; }
            break;
        }
        case 'suspicious':
            if (g.lastSeen) g.facing = angleTo(g.pos, g.lastSeen);
            if (seen === 0 && g.t > 1.2) { if (g.lastSeen && g.alert > 0.2) { goTo(sim, g, g.lastSeen); setState(g, 'investigate'); } else returnToRoute(sim, g); }
            break;
        case 'investigate':
            if (!g.path.length && g.goal && dist(g.pos, g.goal) > 0.6) goTo(sim, g, g.goal);
            if (followPath(sim, g, INVESTIGATE_SPEED, dt)) setState(g, 'look');
            break;
        case 'look':
            lookAround(g, dt);
            if (g.t > LOOK_TIME) returnToRoute(sim, g);
            break;
        case 'chase': {
            const p = sim.player;
            if (seen > 0 && g.repath <= 0) { goTo(sim, g, p.pos); g.repath = 0.3; }
            if (seen === 0 && g.lastSeen && (!g.goal || dist(g.goal, g.lastSeen) > 0.5)) goTo(sim, g, g.lastSeen);
            const arrived = followPath(sim, g, s.chaseSpeed, dt);
            if (p.hidden < 0 && dist(g.pos, p.pos) < GUARD_R + 0.35 && sim.phase === 'play') { sim.phase = 'caught'; sim.phaseT = 0; break; }
            if (arrived && seen === 0) {
                // Saw you climb into a barrel? It checks the barrels right there.
                if (p.hidden >= 0 && dist(centre(sim.barrels[p.hidden]), g.pos) < 1.8) { sim.phase = 'caught'; sim.phaseT = 0; break; }
                g.alert = 0.7;
                setState(g, 'search');
            }
            break;
        }
        case 'search':
            lookAround(g, dt);
            if (g.t > SEARCH_TIME) returnToRoute(sim, g);
            break;
        case 'relight': {
            const torch = sim.torches.find((t) => !t.lit && dist(t.light, g.pos) < RELIGHT_REACH + 1);
            if (!torch) { returnToRoute(sim, g); break; }
            if (followPath(sim, g, s.patrolSpeed * 1.2, dt) && g.t > 1.2) {
                g.facing = angleTo(g.pos, torch.light);
                torch.lit = true;
                sim.lightDirty = true;
                sim.floaters.push({ pos: { ...torch.light }, text: 'relit', color: '#fdba74', life: 1 });
                returnToRoute(sim, g);
            }
            break;
        }
        case 'return':
            if (followPath(sim, g, s.patrolSpeed, dt)) {
                if (g.kind === 'sleeper') { setState(g, 'sleep'); g.alert = 0; break; }
                setState(g, 'patrol');
            }
            break;
    }
}

function returnToRoute(sim: Sim, g: Guard): void {
    if (g.kind === 'sleeper') { goTo(sim, g, g.home); setState(g, 'return'); return; }
    let best = 0;
    g.route.forEach((w, i) => { if (dist(w, g.pos) < dist(g.route[best], g.pos)) best = i; });
    g.routeI = best;
    goTo(sim, g, g.route[best]);
    setState(g, 'return');
}

// --- sleeper -----------------------------------------------------------------------------------

function sleep(sim: Sim, g: Guard): void {
    for (const n of freshNoises(sim)) {
        if (!hears(sim, g.pos, n, true)) continue;
        g.alert = 0.4;
        goTo(sim, g, n.pos);
        setState(g, 'wake');
        sim.floaters.push({ pos: { ...g.pos }, text: 'huh?', color: '#fde68a', life: 1 });
        return;
    }
}

// --- watcher -----------------------------------------------------------------------------------

function watcher(sim: Sim, g: Guard, dt: number): void {
    const [a0, a1] = g.sweep;
    const seen = sees(sim, g);
    if (seen > 0) {
        g.alert = Math.min(1.5, g.alert + sim.settings.noticeRate * 1.4 * (0.35 + seen) * dt);
        g.lastSeen = { ...sim.player.pos };
        g.facing += Math.max(-1.2 * dt, Math.min(1.2 * dt, Math.atan2(Math.sin(angleTo(g.pos, sim.player.pos) - g.facing), Math.cos(angleTo(g.pos, sim.player.pos) - g.facing))));
    } else {
        g.alert = Math.max(0, g.alert - 0.3 * dt);
        const mid = (a0 + a1) / 2, half = (a1 - a0) / 2;
        g.facing = mid + Math.sin(sim.time * 0.45 + g.id) * half;
    }
    if (g.alert >= 1 && g.state !== 'alarm') {
        setState(g, 'alarm');
        sim.stats.spotted++;
        makeNoise(sim, sim.player.pos, 40, true); // "THERE!" — everyone comes to where you were
        sim.floaters.push({ pos: { ...g.pos }, text: 'THERE! (alarm)', color: '#f87171', life: 1.6 });
    }
    if (g.state === 'alarm' && g.t > 4 && seen === 0) { g.alert = 0.4; setState(g, 'sweep'); }
}

/** The tile a guard stands on (for drawing and checks). */
export const guardTile = (sim: Sim, g: Guard) => tileAt(sim, g.pos);
