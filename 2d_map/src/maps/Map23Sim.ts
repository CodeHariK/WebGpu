// Lockpick — one simulation step and the flow of the level.
//
//   play → caught (a guard or the hound reached you: back to the last door you came through,
//          keeping your keys) → play … → escaped (through the gate with the gate key)
import { buildTiles, centre, collide, computeLight, tileAt } from './Map23Grid';
import { stepGuard } from './Map23Guards';
import { forgetOldScent, stepHound } from './Map23Hound';
import type { LevelDef } from './Map23Level';
import { ageNoises } from './Map23Noise';
import { createPlayer, PLAYER_R, stepPebbles, stepPlayer } from './Map23Player';
import type { Settings } from './Map23Settings';
import { type Guard, type Input, type Sim, type Vec, WATER } from './Map23State';

const CAUGHT_TIME = 2;
const START_PEBBLES = 2;

function makeGuards(sim: Sim): Guard[] {
    return sim.level.guards.map((d) => {
        const at = centre(d.at);
        return {
            id: sim.nextId++, kind: d.kind, pos: at, facing: Math.PI / 2,
            state: d.kind === 'sleeper' ? 'sleep' : d.kind === 'watcher' ? 'sweep' : 'patrol',
            t: 0, route: (d.route ?? [d.at]).map(centre), routeI: 0, path: [], goal: null, alert: 0, lastSeen: null,
            home: at, sweep: d.sweep ?? [0, Math.PI], repath: 0,
        };
    });
}

export function createSim(settings: Settings, level: LevelDef): Sim {
    const sim: Sim = {
        settings, level, w: level.w, h: level.h, tiles: buildTiles(level),
        doors: level.doors.map((d) => ({ x: d.x, y: d.y, open: false, lock: d.lock ?? null, shutT: 0 })),
        barrels: level.barrels.map((b) => ({ ...b })),
        torches: level.torches.map((t) => ({ x: t.x, y: t.y, light: { x: t.x + 0.5 + t.dir.x * 0.9, y: t.y + 0.5 + t.dir.y * 0.9 }, lit: true })),
        light: [], lightDirty: true,
        items: [
            ...level.keys.map((k) => ({ pos: centre(k), kind: 'key' as const, key: k.key, taken: false })),
            ...level.pebbles.map((p) => ({ pos: centre(p), kind: 'pebbles' as const, taken: false })),
        ],
        time: 0, phase: 'play', phaseT: 0,
        player: createPlayer(level.start, START_PEBBLES, []), checkpoint: { ...level.start },
        guards: [], noises: [], pebbles: [], scent: [], floaters: [],
        view: { x: level.start.x, y: level.start.y, span: settings.viewSpan },
        banner: null, stats: { caught: 0, spotted: 0, noises: 0, time: 0 }, nextId: 1, exposed: -1, roomName: '',
    };
    sim.guards = makeGuards(sim);
    sim.light = computeLight(sim);
    sim.lightDirty = false;
    banner(sim, level.name.toUpperCase(), 'find the keys · stay out of sight · get out');
    return sim;
}

export function banner(sim: Sim, text: string, sub = ''): void {
    sim.banner = { text, sub, life: 2.6 };
}

export function step(sim: Sim, input: Input, dt: number): void {
    sim.time += dt;
    sim.phaseT += dt;
    stepView(sim, dt);
    for (const f of sim.floaters) { f.life -= dt; f.pos.y -= dt * 0.6; }
    sim.floaters = sim.floaters.filter((f) => f.life > 0);
    if (sim.banner) { sim.banner.life -= dt; if (sim.banner.life <= 0) sim.banner = null; }
    if (sim.lightDirty) { sim.light = computeLight(sim); sim.lightDirty = false; }
    if (sim.phase === 'caught') { if (sim.phaseT > CAUGHT_TIME) restart(sim); return; }
    if (sim.phase === 'escaped') return;
    sim.stats.time += dt;
    stepPlayer(sim, input, dt);
    layScent(sim, dt);
    stepPebbles(sim, dt);
    for (const g of sim.guards) {
        if (g.kind === 'hound') stepHound(sim, g, dt);
        else stepGuard(sim, g, dt);
        if (g.kind !== 'watcher') collide(sim, g.pos, 0.3);
    }
    exposedBarrel(sim);
    shutDoors(sim, dt);
    ageNoises(sim, dt);
    forgetOldScent(sim);
    checkpoints(sim);
    if ((sim.phase as Sim['phase']) === 'caught') { sim.stats.caught++; banner(sim, 'CAUGHT', 'back to the last door you came through'); }
    if (sim.player.pos.x > sim.level.outside.x + 0.2) { sim.phase = 'escaped'; sim.phaseT = 0; banner(sim, 'ESCAPED', `out in ${Math.round(sim.stats.time)} s · seen ${sim.stats.spotted}× · caught ${sim.stats.caught}× · R to try again`); sim.banner!.life = 1e9; }
}

// The camera follows you.
function stepView(sim: Sim, dt: number): void {
    const v = sim.view;
    const k = Math.min(1, dt * 5);
    v.span += (sim.settings.viewSpan - v.span) * k;
    v.x += (sim.player.pos.x - v.x) * k;
    v.y += (sim.player.pos.y - v.y) * k;
}

/** Screen fraction (0 … 1) → world metres under the camera. */
export function screenToWorld(sim: Sim, fx: number, fy: number): Vec {
    const v = sim.view;
    return { x: v.x - v.span / 2 + fx * v.span, y: v.y - v.span / 2 + fy * v.span };
}

// You leave a scent trail for the hound (not in water).
function layScent(sim: Sim, dt: number): void {
    const p = sim.player;
    const t = tileAt(sim, p.pos);
    if (sim.tiles[t.y * sim.w + t.x] === WATER) { sim.scent = sim.scent.filter((s) => Math.hypot(s.pos.x - p.pos.x, s.pos.y - p.pos.y) > 1.2); return; }
    p.scentT -= dt;
    if (p.scentT > 0 || !p.moving) return;
    p.scentT = 0.35;
    sim.scent.push({ pos: { ...p.pos }, t: sim.time });
}

// The hound barked at your barrel: the first guard to get there drags you out.
function exposedBarrel(sim: Sim): void {
    const p = sim.player;
    if (p.hidden < 0 || p.hidden !== sim.exposed) { if (p.hidden < 0) sim.exposed = -1; return; }
    const b = centre(sim.barrels[p.hidden]);
    if (sim.guards.some((g) => (g.kind === 'guard' || (g.kind === 'sleeper' && g.state !== 'sleep')) && Math.hypot(g.pos.x - b.x, g.pos.y - b.y) < 1.6)) {
        sim.phase = 'caught';
        sim.phaseT = 0;
    }
}

// Locked doors a guard opened shut themselves again behind it.
function shutDoors(sim: Sim, dt: number): void {
    for (const d of sim.doors) {
        if (!d.open || d.shutT <= 0) continue;
        d.shutT -= dt;
        const someoneIn = sim.guards.some((g) => tileAt(sim, g.pos).x === d.x && tileAt(sim, g.pos).y === d.y) || (tileAt(sim, sim.player.pos).x === d.x && tileAt(sim, sim.player.pos).y === d.y);
        if (d.shutT <= 0 && !someoneIn) { d.open = false; sim.lightDirty = true; } else if (d.shutT <= 0) d.shutT = 0.3;
    }
}

// Walking through a door saves where you come back to if caught; entering a room shows its name.
function checkpoints(sim: Sim): void {
    const t = tileAt(sim, sim.player.pos);
    if (sim.doors.some((d) => d.x === t.x && d.y === t.y)) sim.checkpoint = { ...t };
    const room = sim.level.rooms.find((r) => t.x >= r.x && t.x < r.x + r.w && t.y >= r.y && t.y < r.y + r.h);
    if (room && room.name !== sim.roomName) { sim.roomName = room.name; banner(sim, room.name); }
}

function restart(sim: Sim): void {
    const keys = sim.player.keys;
    const pebbles = Math.max(sim.player.pebbles, START_PEBBLES);
    sim.player = createPlayer(sim.checkpoint, pebbles, keys);
    collide(sim, sim.player.pos, PLAYER_R);
    sim.guards = makeGuards(sim);
    sim.noises = [];
    sim.pebbles = [];
    sim.scent = [];
    sim.exposed = -1;
    sim.phase = 'play';
    sim.phaseT = 0;
}
