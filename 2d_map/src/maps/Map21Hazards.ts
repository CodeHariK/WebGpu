// Combat Sandbox — the place fights back (and can be used): conveyor belts, crushers, gas vents,
// sludge pipes, toxic pools, lifts and tremors. Everything here hurts toys as well as the player,
// so the trick is to stay out of it and knock them into it.
//
//   belts     push whatever stands on them (not you while dashing)
//   crushers  open … a red warning … SLAM: crushes anyone inside
//   vents     hiss (a warning ring) then burst toxic gas for a second
//   pipes     shoot one and it bursts, spilling a new sludge pool at its spill point
//   pools     hurt you; melt toys
//   lifts     where toys arrive (DiscoSim picks them); shoot one to wreck it
//   tremors   every so often the crates marked `slides` lurch a couple of metres
import { flies, hitPlayer, RADIUS } from './Map21Enemies';
import { addShake, banner, burst, floater, ring } from './Map21Juice';
import type { Arena, Sim, Vec } from './Map21State';
import { collide, inRect, WALL } from './Map21World';

export const CRUSH_WARN = 0.9; // seconds of red warning before a slam
export const CRUSH_SLAM = 0.3; // seconds it stays down
export const VENT_WARN = 0.9;
export const VENT_BURST = 1.1;
const SPILL_R = 1.7;

type Crusher = Arena['crushers'][number];
type Vent = Arena['vents'][number];
type Pipe = Arena['pipes'][number];

export function crusherState(c: Crusher): 'open' | 'warn' | 'slam' {
    const t = c.t % c.period;
    if (t > c.period - CRUSH_SLAM) return 'slam';
    if (t > c.period - CRUSH_SLAM - CRUSH_WARN) return 'warn';
    return 'open';
}

export function ventState(v: Vent): 'idle' | 'warn' | 'burst' {
    const t = v.t % v.period;
    if (t < VENT_BURST) return 'burst';
    if (t > v.period - VENT_WARN) return 'warn';
    return 'idle';
}

const grounded = (sim: Sim) => sim.enemies.filter((e) => e.spawnT <= 0 && !flies(e));

/** Standing in glue? (slows you and toys right down; you can't dash out). */
export function inGlue(a: Arena, p: Vec): boolean {
    return a.pools.some((pool) => pool.glue && Math.hypot(p.x - pool.pos.x, p.y - pool.pos.y) < pool.r * pool.grow);
}

export function stepHazards(sim: Sim, dt: number): void {
    const a = sim.arena;
    if (a.beltsOn) { // until the terminal breaks
        belts(sim, dt);
        for (const c of a.crushers) crusher(sim, c, dt);
    }
    for (const v of a.vents) vent(sim, v, dt);
    pools(sim, dt);
    for (const lift of a.lifts) lift.busy = Math.max(0, lift.busy - dt);
    if (a.core) a.core.flash = Math.max(0, a.core.flash - dt);
    tremor(sim, dt);
}

function belts(sim: Sim, dt: number): void {
    const p = sim.player;
    for (const c of sim.arena.def.conveyors) {
        if (p.dashT <= 0 && inRect(p.pos, c)) {
            p.pos.x += c.dir.x * c.speed * dt;
            p.pos.y += c.dir.y * c.speed * dt;
        }
        for (const e of grounded(sim)) {
            if (!inRect(e.pos, c)) continue;
            e.pos.x += c.dir.x * c.speed * dt;
            e.pos.y += c.dir.y * c.speed * dt;
        }
        for (const b of sim.arena.cover) if (b.rides) rideCrate(sim, b, c, dt);
    }
}

// A parts crate on a belt: carried along; at the end it goes into the press and a fresh one
// appears at the start of the line.
function rideCrate(sim: Sim, b: Arena['cover'][number], c: Sim['arena']['def']['conveyors'][number], dt: number): void {
    if (!inRect({ x: b.x + b.w / 2, y: b.y + b.h / 2 }, c)) return;
    b.x += c.dir.x * c.speed * dt;
    b.y += c.dir.y * c.speed * dt;
    const past = c.dir.x > 0 ? b.x + b.w > c.x + c.w : c.dir.x < 0 ? b.x < c.x : c.dir.y > 0 ? b.y + b.h > c.y + c.h : b.y < c.y;
    if (!past) return;
    burst(sim, { x: b.x + b.w / 2, y: b.y + b.h / 2 }, 8, '#b45309', 'square', 5, NaN, 0.15);
    if (c.dir.x > 0) b.x = c.x + 0.1;
    else if (c.dir.x < 0) b.x = c.x + c.w - b.w - 0.1;
    else if (c.dir.y > 0) b.y = c.y + 0.1;
    else b.y = c.y + c.h - b.h - 0.1;
    burst(sim, { x: b.x + b.w / 2, y: b.y + b.h / 2 }, 6, '#a8a29e', 'smoke', 1.5, NaN, 0.4);
}

function crusher(sim: Sim, c: Crusher, dt: number): void {
    const before = crusherState(c);
    c.t += dt;
    const now = crusherState(c);
    if (before === 'slam' || now !== 'slam') return;
    // SLAM: everyone inside is crushed.
    addShake(sim, 0.25);
    burst(sim, { x: c.x + c.w / 2, y: c.y + c.h / 2 }, 14, '#94a3b8', 'square', 7, NaN, 0.2);
    const p = sim.player;
    if (inRect(p.pos, c)) hitPlayer(sim, 3, Math.atan2(p.pos.y - (c.y + c.h / 2), p.pos.x - (c.x + c.w / 2)), 18);
    for (const e of grounded(sim)) {
        if (!inRect(e.pos, c)) continue;
        e.hp -= 12;
        e.flash = 0.1;
        floater(sim, e.pos, 'CRUNCH', '#fde047', 1.2);
    }
}

function vent(sim: Sim, v: Vent, dt: number): void {
    if (v.off) return;
    const before = ventState(v);
    v.t += dt;
    const now = ventState(v);
    if (now === 'burst') {
        if (before !== 'burst') ring(sim, v.pos, v.r * 1.4, 'rgba(163,230,53,0.8)', 0.35);
        if (Math.random() < 0.7) burst(sim, v.pos, 1, 'rgba(163,230,53,0.55)', 'smoke', 2.5, NaN, 0.6);
        const p = sim.player;
        if (Math.hypot(p.pos.x - v.pos.x, p.pos.y - v.pos.y) < v.r && p.dashT <= 0) hitPlayer(sim, 1, Math.random() * 6.28, 4);
        for (const e of grounded(sim)) if (Math.hypot(e.pos.x - v.pos.x, e.pos.y - v.pos.y) < v.r) e.hp -= 5 * dt;
    } else if (now === 'warn' && Math.random() < 0.25) {
        burst(sim, v.pos, 1, 'rgba(190,242,100,0.5)', 'dot', 1.5, -Math.PI / 2, 0.12);
    }
}

function pools(sim: Sim, dt: number): void {
    const p = sim.player;
    for (const pool of sim.arena.pools) {
        pool.grow = Math.min(1, pool.grow + dt);
        if (pool.glue) continue; // glue only holds you (Map21Player, Map21Enemies)
        const r = pool.r * pool.grow;
        if (Math.hypot(p.pos.x - pool.pos.x, p.pos.y - pool.pos.y) < r && p.dashT <= 0) hitPlayer(sim, 1, Math.random() * 6.28, 3);
        for (const e of grounded(sim)) {
            if (Math.hypot(e.pos.x - pool.pos.x, e.pos.y - pool.pos.y) < r) {
                e.hp -= 4 * dt;
                if (Math.random() < 0.3) burst(sim, e.pos, 1, '#84cc16', 'dot', 2, -Math.PI / 2, 0.2);
            }
        }
    }
}

// --- pipes ---

export function pipeDistance(pipe: Pipe, p: Vec): number {
    const dx = pipe.b.x - pipe.a.x;
    const dy = pipe.b.y - pipe.a.y;
    const k = Math.max(0, Math.min(1, ((p.x - pipe.a.x) * dx + (p.y - pipe.a.y) * dy) / (dx * dx + dy * dy)));
    return Math.hypot(p.x - (pipe.a.x + dx * k), p.y - (pipe.a.y + dy * k));
}

export function breakPipe(sim: Sim, pipe: Pipe): void {
    if (pipe.broken) return;
    pipe.broken = true;
    sim.arena.pools.push({ pos: { ...pipe.spill }, r: pipe.glue ? SPILL_R * 1.2 : SPILL_R, grow: 0, glue: pipe.glue });
    burst(sim, pipe.spill, 20, pipe.glue ? '#fef3c7' : '#84cc16', 'dot', 7, NaN, 0.25);
    addShake(sim, 0.2);
    floater(sim, pipe.spill, pipe.glue ? 'GLUE!' : 'SPILL!', pipe.glue ? '#fef3c7' : '#bef264', 1.1);
}

// --- lifts and the core ---

export function wreckLift(sim: Sim, lift: Arena['lifts'][number]): void {
    lift.broken = true;
    burst(sim, lift.pos, 26, '#94a3b8', 'square', 9, NaN, 0.25);
    burst(sim, lift.pos, 12, '#fb923c', 'spark', 10, NaN, 0.12);
    addShake(sim, 0.5);
    floater(sim, lift.pos, 'LIFT DOWN', '#fde047', 1.3);
}

// --- tremors ---

function tremor(sim: Sim, dt: number): void {
    const a = sim.arena;
    if (a.def.tremor <= 0 || sim.phase !== 'fight') return;
    a.tremorT -= dt;
    if (a.tremorT < 1 && a.tremorT + dt >= 1) banner(sim, 'TREMOR!', 'the crates are about to slide');
    if (a.tremorT < 1) addShake(sim, dt * 0.6);
    if (a.tremorT > 0) return;
    a.tremorT = a.def.tremor;
    addShake(sim, 0.8);
    const dir = [{ x: 1, y: 0 }, { x: -1, y: 0 }, { x: 0, y: 1 }, { x: 0, y: -1 }][Math.floor(Math.random() * 4)];
    for (const c of a.cover) {
        if (!c.slides) continue;
        const d = 1.5 + Math.random();
        c.x = Math.max(WALL + 0.5, Math.min(a.w - WALL - 0.5 - c.w, c.x + dir.x * d));
        c.y = Math.max(WALL + 3, Math.min(a.h - WALL - 0.5 - c.h, c.y + dir.y * d));
        burst(sim, { x: c.x + c.w / 2, y: c.y + c.h / 2 }, 6, '#a8a29e', 'smoke', 1.5, NaN, 0.4);
    }
    collide(a, sim.player.pos, 0.5);
    for (const e of sim.enemies) collide(a, e.pos, RADIUS[e.kind]);
}
