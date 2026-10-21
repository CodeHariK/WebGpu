// Wick — one simulation step and the flow of the level.
//
//   play → snuffed (your wax ran out: back to the last lantern you lit) → play … → won
//
// Walk into a clearing and it wakes: shapes rise out of the water round it in waves. It's done when
// its lanterns are lit and its waves are beaten; then the fog wall across the path ahead parts.
// Out on the paths, away from lantern light, the dark sends a shade every so often.
import { banner, burst, floater, ring, shake, stepFx } from './Map22Fx';
import { hurtEnemy, RADIUS, spawnEnemy, stepEnemies, weeperFrozen } from './Map22Enemies';
import type { LevelDef } from './Map22Level';
import { LEVELS } from './Map22Levels';
import { lights } from './Map22Light';
import { createPlayer, lightLantern, stepPlayer } from './Map22Player';
import type { Settings } from './Map22Settings';
import type { Clearing, Input, Sim, Vec } from './Map22State';
import { groundDepth, samplePaths, waterNear } from './Map22World';
import { gustLight, stepWind } from './Map22Wind';

const SNUFF_TIME = 2.2;
const RESPAWN_WAX = 60;
const FLAME_DAMAGE = 2.5;
const AMBIENT_CAP = 3;

export function createSim(settings: Settings, level: LevelDef): Sim {
    const clearings: Clearing[] = level.clearings.map((def) => ({ def, entered: false, done: false, wave: 0, waveT: 0 }));
    const sim: Sim = {
        settings, level, segs: samplePaths(level), view: { x: level.start.x, y: level.start.y, span: settings.viewSpan },
        time: 0, phase: 'play', phaseT: 0, player: createPlayer(level.start, settings.maxWax), checkpoint: { ...level.start },
        enemies: [], flames: [], embers: [], blobs: [],
        lanterns: [
            ...level.clearings.flatMap((c, i) => c.lanterns.map((pos) => ({ pos: { ...pos }, lit: false, litT: 0, clearing: i }))),
            ...level.lanterns.map((pos) => ({ pos: { ...pos }, lit: false, litT: 0, clearing: -1 })),
        ],
        candles: level.candles.map((pos) => ({ pos: { ...pos }, taken: false })),
        clearings, particles: [], floaters: [], shake: 0, stop: 0, windLight: 1, ambientT: settings.ambientEvery, nextId: 1,
        banner: null, stats: { shades: 0, moths: 0, weepers: 0, snuffed: 0, others: 0 },
    };
    for (const at of level.hands) { const h = spawnEnemy(sim, 'hand', at, -1); h.home = { ...at }; h.spawnT = 0; }
    banner(sim, level.name.toUpperCase(), 'keep your flame alive · light the lanterns');
    return sim;
}

export function step(sim: Sim, input: Input, dt: number): void {
    stepFx(sim, dt);
    stepView(sim, input, dt);
    if (sim.stop > 0) { sim.stop -= dt; return; }
    sim.time += dt;
    sim.phaseT += dt;
    if (sim.phase === 'snuffed') { if (sim.phaseT > SNUFF_TIME) relight(sim); return; }
    stepPlayer(sim, input, dt);
    stepWind(sim, dt);
    sim.windLight += (gustLight(sim) - sim.windLight) * Math.min(1, dt * 4);
    if (sim.player.flareT === 1) flareHits(sim);
    const all = lights(sim);
    stepEnemies(sim, all, dt);
    if (sim.player.held && !sim.enemies.some((e) => e.id === sim.player.held)) { sim.player.held = 0; sim.player.struggle = 0; }
    stepFlames(sim, dt);
    stepEmbers(sim, dt);
    for (const l of sim.lanterns) if (l.lit) l.litT += dt;
    if (sim.phase === 'play') {
        stepClearings(sim, dt);
        stepAmbient(sim, dt);
    }
    if (sim.player.wax <= 0 && sim.phase === 'play') snuff(sim);
}

// The camera follows you, leaning toward where you aim.
function stepView(sim: Sim, input: Input, dt: number): void {
    const v = sim.view;
    const p = sim.player.pos;
    v.span += (sim.settings.viewSpan - v.span) * Math.min(1, dt * 3);
    const ax = input.mouse.x - p.x;
    const ay = input.mouse.y - p.y;
    const len = Math.max(1, Math.hypot(ax, ay));
    const lead = Math.min(1, len / 8) * 2.5;
    const k = Math.min(1, dt * 5);
    v.x += (p.x + (ax / len) * lead - v.x) * k;
    v.y += (p.y + (ay / len) * lead - v.y) * k;
}

/** Screen fraction (0 … 1) → world metres under the camera. */
export function screenToWorld(sim: Sim, fx: number, fy: number): Vec {
    const v = sim.view;
    return { x: v.x - v.span / 2 + fx * v.span, y: v.y - v.span / 2 + fy * v.span };
}

// --- flames and embers -------------------------------------------------------------------------

function stepFlames(sim: Sim, dt: number): void {
    for (const f of sim.flames) {
        f.pos.x += f.vel.x * dt;
        f.pos.y += f.vel.y * dt;
        f.life -= dt;
        if (Math.random() < 0.5) burst(sim, f.pos, 1, '#fdba74', 'spark', 1, NaN, 0.08);
        for (const e of sim.enemies) {
            if (e.spawnT > 0 || Math.hypot(e.pos.x - f.pos.x, e.pos.y - f.pos.y) > RADIUS[e.kind] + 0.3) continue;
            hurtEnemy(sim, e, FLAME_DAMAGE, Math.atan2(f.vel.y, f.vel.x));
            f.life = 0;
            break;
        }
        for (const l of sim.lanterns) {
            if (l.lit || Math.hypot(l.pos.x - f.pos.x, l.pos.y - f.pos.y) > 0.6) continue;
            lightLantern(sim, l);
            f.life = -1; // used up: no ember
        }
        if (f.life <= 0 && f.life > -1) land(sim, f.pos);
    }
    sim.flames = sim.flames.filter((f) => f.life > 0);
}

// Where a flame comes down: on the ground it keeps burning (an ember); on the water it hisses out.
function land(sim: Sim, at: Vec): void {
    if (groundDepth(sim.segs, sim.clearings, at) > -0.3) {
        sim.embers.push({ pos: { ...at }, life: sim.settings.emberTime, maxLife: sim.settings.emberTime });
        burst(sim, at, 5, '#fb923c', 'spark', 3, NaN, 0.09);
    } else {
        burst(sim, at, 6, 'rgba(203,213,225,0.5)', 'smoke', 1.5, NaN, 0.25);
    }
}

function stepEmbers(sim: Sim, dt: number): void {
    for (const e of sim.embers) {
        e.life -= dt;
        if (Math.random() < 0.15) burst(sim, e.pos, 1, '#fdba74', 'spark', 1.2, -Math.PI / 2, 0.06);
    }
    sim.embers = sim.embers.filter((e) => e.life > 0);
}

// The flare's burst: shades burn and are thrown back, a frozen weeper cracks, moths are gone,
// unlit lanterns in reach catch.
function flareHits(sim: Sim): void {
    const p = sim.player;
    const r = sim.settings.flareRadius;
    for (const e of sim.enemies) {
        const d = Math.hypot(e.pos.x - p.pos.x, e.pos.y - p.pos.y);
        if (d > r || e.spawnT > 0) continue;
        const a = Math.atan2(e.pos.y - p.pos.y, e.pos.x - p.pos.x);
        if (e.kind === 'moth') { e.hp = 0; continue; }
        if (e.kind === 'weeper') { e.lit = 1; if (weeperFrozen(e)) hurtEnemy(sim, e, 4, a); continue; }
        hurtEnemy(sim, e, 4, a);
        if (e.kind !== 'shade') continue;
        e.vel.x += Math.cos(a) * 10;
        e.vel.y += Math.sin(a) * 10;
        e.state = 'retreat';
        e.t = 0;
    }
    for (const l of sim.lanterns) if (!l.lit && Math.hypot(l.pos.x - p.pos.x, l.pos.y - p.pos.y) < r * 0.6) lightLantern(sim, l);
}

// --- clearings ---------------------------------------------------------------------------------

function stepClearings(sim: Sim, dt: number): void {
    const p = sim.player.pos;
    sim.clearings.forEach((c, i) => {
        if (c.done) return;
        const inside = Math.hypot(p.x - c.def.pos.x, p.y - c.def.pos.y) < c.def.r;
        if (!c.entered) {
            if (!inside) return;
            c.entered = true;
            banner(sim, c.def.name, c.def.sub);
        }
        c.waveT += dt;
        const alive = sim.enemies.filter((e) => e.clearing === i).length;
        if (c.wave < c.def.waves.length && (c.wave === 0 ? c.waveT > 1 : alive <= 1 && c.waveT > 2.5)) {
            for (const kind of c.def.waves[c.wave]) spawnEnemy(sim, kind, edgeOf(sim, c), i);
            c.wave++;
            c.waveT = 0;
            if (c.wave > 1) floater(sim, p, 'more rise from the water', '#a5b4fc', 0.9);
        }
        const lit = sim.lanterns.every((l) => l.clearing !== i || l.lit);
        if (lit && c.wave >= c.def.waves.length && alive === 0) finish(sim, c);
    });
}

// A point on the water just off a clearing's edge (where its waves rise).
function edgeOf(sim: Sim, c: Clearing): Vec {
    for (let i = 0; i < 30; i++) {
        const a = Math.random() * Math.PI * 2;
        const q = { x: c.def.pos.x + Math.cos(a) * (c.def.r + 1.5), y: c.def.pos.y + Math.sin(a) * (c.def.r + 1.5) };
        if (groundDepth(sim.segs, sim.clearings, q) < -0.8) return q;
    }
    return waterNear(sim.segs, sim.clearings, c.def.pos, 1);
}

function finish(sim: Sim, c: Clearing): void {
    c.done = true;
    if (c.def.end) {
        sim.phase = 'won';
        sim.phaseT = 0;
        const last = LEVELS.indexOf(sim.level) === LEVELS.length - 1;
        banner(sim, sim.level.winText, last ? 'the last light is lit · R to play it again' : 'the bell rings out · next level coming up');
        sim.banner!.life = 1e9;
        for (const e of sim.enemies) e.hp = 0;
        return;
    }
    if (c.def.gates.length) {
        banner(sim, 'THE FOG PARTS', 'the way on is open');
        for (const g of c.def.gates) {
            burst(sim, g, 30, 'rgba(203,213,225,0.6)', 'smoke', 4, NaN, 0.5);
            ring(sim, g, g.r + 2, 'rgba(226,232,240,0.6)', 0.6);
        }
        shake(sim, 0.3);
    }
}

// --- the dark itself ---------------------------------------------------------------------------

function stepAmbient(sim: Sim, dt: number): void {
    const s = sim.settings;
    if (s.ambientEvery <= 0) return;
    const p = sim.player.pos;
    const safe = sim.lanterns.some((l) => l.lit && Math.hypot(l.pos.x - p.x, l.pos.y - p.y) < s.lanternRadius);
    const fighting = sim.clearings.some((c) => c.entered && !c.done && c.def.waves.length > 0);
    if (safe || fighting) return;
    sim.ambientT -= dt;
    if (sim.ambientT > 0) return;
    sim.ambientT = s.ambientEvery * (0.7 + Math.random() * 0.6);
    if (sim.enemies.filter((e) => e.clearing === -1 && e.kind !== 'hand').length >= AMBIENT_CAP) return;
    const roll = Math.random();
    spawnEnemy(sim, roll < 0.25 ? 'moth' : roll < 0.4 ? 'leech' : 'shade', waterNear(sim.segs, sim.clearings, p, 1), -1);
}

// --- going out, and coming back ----------------------------------------------------------------

function snuff(sim: Sim): void {
    sim.phase = 'snuffed';
    sim.phaseT = 0;
    sim.stats.snuffed++;
    sim.player.wax = 0;
    banner(sim, 'SNUFFED OUT', 'back to the last lantern you lit');
    burst(sim, sim.player.pos, 20, 'rgba(120,113,108,0.8)', 'smoke', 2, -Math.PI / 2, 0.4);
    shake(sim, 0.6);
}

function relight(sim: Sim): void {
    const c = sim.checkpoint;
    sim.player = createPlayer(c, RESPAWN_WAX);
    sim.flames = [];
    // The dark round the lantern lets go; clinging moths fall off.
    sim.enemies = sim.enemies.filter((e) => e.kind === 'hand' || (Math.hypot(e.pos.x - c.x, e.pos.y - c.y) > 12 && e.state !== 'cling'));
    for (const e of sim.enemies) if (e.kind === 'hand') { e.state = 'under'; e.t = 0; }
    sim.blobs = [];
    sim.phase = 'play';
    sim.phaseT = 0;
    sim.ambientT = sim.settings.ambientEvery;
    ring(sim, c, 3, 'rgba(253,224,71,0.8)', 0.5);
}
