// Combat Sandbox — one simulation step and the level flow.
//
//   intro → fight → clear → (next level)        fight → over (knocked out: R retries the level)
//
// In a fight, waves come one after another (or on a timer, `delay`), arriving by lift or dropping
// in along the top. Break every objective (and, on the last level, the drill core) and the exit
// opens; walk into it and the level is clear. Toys keep coming while there's work left.
//
// The push-forward loop: bubbles cost joy; broken toys free clowns whose joy spirits refill you —
// more the closer you were. Where the level has gas, it creeps up from the bottom.
import { addShake, addStop, banner, burst, floater, ring, slowMotion, stepJuice } from './Map21Juice';
import { createPlayer, FIZZ_BLAST, FIZZ_DAMAGE, hurtEnemy, PLAYER_RADIUS, stepPlayer } from './Map21Player';
import { atExit, coreShielded, objectivesLeft, shotHitsObjective, stepObjectives, toyDrop } from './Map21Objectives';
import { ARRIVE_TIME, hitPlayer, MOUSE_BLAST, RADIUS, spawnEnemy, stepEnemies } from './Map21Enemies';
import { batBomb, bombPos, explode, stepBombs } from './Map21Blast';
import { breakPipe, pipeDistance, stepHazards, wreckLift } from './Map21Hazards';
import { shotHitsArm, stepArms } from './Map21Arms';
import { stepSprayers } from './Map21Paint';
import { stepStamps } from './Map21Stamps';
import { shotCutsCable, spawnRig } from './Map21Bots';
import { THEMES } from './Map21Themes';
import type { Settings } from './Map21Settings';
import type { Enemy, Input, Shot, Sim } from './Map21State';
import { collide, inRect, makeArena, solidAt, WALL } from './Map21World';

const SLOW_SCALE = 0.3;
const SPIRIT_JOY = 3;
const SPIRITS_PER_HEART = 6;
const LOW_COVER_REACH = 2.0; // a shot fired this close to a low block flies over it
const DUCK_REACH = 1.6; // someone this close to a low block is crouched behind it
const JOY_TRICKLE = 1.5; // seconds per joy while nearly empty (never stuck with nothing)
const INTRO_TIME = 2.4;
const CLEAR_TIME = 3.5;
const REPEAT = 18; // seconds between extra waves once the list runs out (while there's work left)
const REPEAT_CAP = 4; // …and only while fewer toys than this are about
const HP_PER_LEVEL = 0.12; // toys get this much tougher per level

export function createSim(settings: Settings, theme = 0, level = 0): Sim {
    const levels = THEMES[theme].levels;
    const def = levels[Math.max(0, Math.min(levels.length - 1, level))];
    const sim: Sim = {
        settings, view: { x: def.start.x, y: def.start.y - 4, span: settings.viewSpan },
        arena: makeArena(def), theme, level: levels.indexOf(def), phase: 'intro', phaseT: 0, time: 0,
        player: createPlayer(def.start, settings.playerHp, settings.maxJoy),
        enemies: [], shots: [], bombs: [], spirits: [], pickups: [], particles: [], floaters: [],
        juice: { shake: 0, stop: 0, slow: 0, kick: { x: 0, y: 0 } },
        marked: false, gasY: def.h + 2, wave: 0, zonesEntered: [], waveT: 0, nextId: 1,
        stats: { kills: 0, perfects: 0, hitsTaken: 0, deaths: 0, bestStreak: 0, streak: 0, collected: 0 },
        banner: null,
    };
    banner(sim, `${sim.level + 1}. ${def.name.toUpperCase()}`, def.tagline);
    return sim;
}

export function step(sim: Sim, input: Input, dt: number): void {
    stepJuice(sim, dt);
    stepView(sim, input, dt);
    if (sim.juice.stop > 0) { // hit-stop: the world holds its breath
        sim.juice.stop -= dt;
        return;
    }
    let gdt = dt;
    if (sim.juice.slow > 0) {
        sim.juice.slow -= dt;
        gdt = dt * SLOW_SCALE;
    }
    sim.phaseT += gdt;
    if (sim.phase === 'over') return;
    sim.time += gdt;
    stepPlayer(sim, input, gdt);
    stepEnemies(sim, gdt);
    stepHazards(sim, gdt);
    stepArms(sim, gdt);
    stepSprayers(sim, gdt);
    stepStamps(sim, gdt);
    stepShots(sim, gdt);
    stepBombs(sim, gdt);
    stepObjectives(sim, gdt);
    killDead(sim);
    stepSpirits(sim, gdt);
    trickleJoy(sim, gdt);
    stepGas(sim, gdt);
    stepFlow(sim, gdt);
    if (sim.player.hp <= 0 && sim.phase === 'fight') {
        sim.phase = 'over';
        sim.phaseT = 0;
        sim.stats.deaths++;
        banner(sim, 'KNOCKED OUT', 'R to try this level again');
        sim.banner!.life = 1e9;
        addShake(sim, 1);
    }
}

// The camera follows the player, leaning toward the aim, kept inside the arena.
function stepView(sim: Sim, input: Input, dt: number): void {
    const v = sim.view;
    const p = sim.player;
    const a = sim.arena;
    v.span += (Math.min(sim.settings.viewSpan, Math.max(a.w, a.h) + 2) - v.span) * Math.min(1, dt * 4);
    const ax = input.mouse.x - p.pos.x;
    const ay = input.mouse.y - p.pos.y;
    const len = Math.max(1, Math.hypot(ax, ay));
    const lead = Math.min(1, len / 8) * sim.settings.aimLead;
    const k = Math.min(1, dt * 6);
    v.x += (p.pos.x + (ax / len) * lead - v.x) * k;
    v.y += (p.pos.y + (ay / len) * lead - v.y) * k;
    const half = v.span / 2;
    v.x = a.w > v.span ? Math.max(half, Math.min(a.w - half, v.x)) : a.w / 2;
    v.y = a.h + 2 > v.span ? Math.max(half, Math.min(a.h + 2 - half, v.y)) : a.h / 2;
}

/** Screen fraction (0 … 1 across the canvas) → world metres under the current camera. */
export function screenToWorld(sim: Sim, fx: number, fy: number): { x: number; y: number } {
    const v = sim.view;
    return { x: v.x - v.span / 2 + fx * v.span, y: v.y - v.span / 2 + fy * v.span };
}

// --- level flow --------------------------------------------------------------------------------

function stepFlow(sim: Sim, dt: number): void {
    const def = sim.arena.def;
    if (sim.phase === 'intro') {
        if (sim.phaseT > INTRO_TIME) { sim.phase = 'fight'; sim.phaseT = 0; }
        return;
    }
    if (sim.phase === 'clear') return;
    if (sim.phase === 'over') return;
    sim.waveT += dt;
    const core = sim.arena.core;
    const work = objectivesLeft(sim.arena) > 0 || (core !== null && core.hp > 0);
    const liftsGone = sim.arena.lifts.length > 0 && sim.arena.lifts.every((l) => l.broken);
    enterZones(sim);
    if (!liftsGone) {
        // Walked on to a later station: the waves still waiting back there are dropped.
        const furthest = sim.zonesEntered.lastIndexOf(true);
        while (sim.wave < def.waves.length && (def.waves[sim.wave].zone ?? Infinity) < furthest) sim.wave++;
        if (sim.wave < def.waves.length) {
            const next = def.waves[sim.wave];
            const zoneOpen = next.zone === undefined || sim.zonesEntered[next.zone];
            const firstInZone = next.zone !== undefined && (sim.wave === 0 || def.waves[sim.wave - 1].zone !== next.zone);
            const due = firstInZone ? true : sim.wave === 0 ? sim.waveT > 0.5 : next.delay !== undefined ? sim.waveT > next.delay : sim.enemies.length === 0 && sim.waveT > 1.5;
            if (zoneOpen && (due || (sim.wave > 0 && sim.enemies.length === 0 && sim.waveT > 2))) startWave(sim, next.toys, next.zone);
        } else if (work && !def.zones && sim.waveT > REPEAT && sim.enemies.length < REPEAT_CAP) {
            startWave(sim, def.waves[core ? def.waves.length - 1 : 0].toys);
        }
    }
    if (atExit(sim)) {
        sim.phase = 'clear';
        sim.phaseT = 0;
        const last = sim.level === THEMES[sim.theme].levels.length - 1;
        banner(sim, def.core ? 'THE DRILL IS DOWN!' : 'LEVEL CLEAR', last ? 'that was the last level — R to play it again' : 'next level coming up');
        if (last) sim.banner!.life = 1e9;
        sim.gasY = def.h + 2;
    }
}

// Zoned levels: walking into a station for the first time names it (and lets its waves start).
function enterZones(sim: Sim): void {
    (sim.arena.def.zones ?? []).forEach((z, i) => {
        if (sim.zonesEntered[i] || !inRect(sim.player.pos, z.rect)) return;
        sim.zonesEntered[i] = true;
        banner(sim, z.name, z.sub);
    });
}

/** True when the level is clear and the next one should load (the UI does it). */
export function readyForNext(sim: Sim): boolean {
    return sim.phase === 'clear' && sim.phaseT > CLEAR_TIME && sim.level < THEMES[sim.theme].levels.length - 1;
}

function startWave(sim: Sim, toys: Sim['enemies'][number]['kind'][], zone?: number): void {
    if (sim.wave > 0 && sim.enemies.length === 0 && sim.arena.def.gas) { // cleared the last one: the gas falls back
        sim.gasY = Math.min(sim.arena.h + 2, sim.gasY + sim.settings.gasPushBack);
    }
    sim.wave++;
    sim.waveT = 0;
    const hpScale = 1 + HP_PER_LEVEL * sim.level;
    const lifts = sim.arena.lifts.filter((l) => !l.broken);
    const a = sim.arena;
    const spawns = zone !== undefined ? a.def.zones![zone].spawns : [];
    toys.forEach((kind, i) => {
        let at;
        if (spawns.length > 0) { // a station's spawn points
            const s = spawns[i % spawns.length];
            at = { x: s.x + (Math.random() - 0.5) * 1.5, y: s.y + (Math.random() - 0.5) * 1.5 };
            collide(a, at, RADIUS[kind]);
        } else if (lifts.length > 0) {
            const lift = lifts[i % lifts.length];
            lift.busy = ARRIVE_TIME;
            at = { x: lift.pos.x + ((i % 3) - 1) * 0.9, y: lift.pos.y + 0.6 + Math.floor(i / 3) * 0.8 };
        } else {
            const slots = Math.max(1, toys.length);
            at = { x: WALL + 2 + ((a.w - 2 * WALL - 4) * (i + 0.5)) / slots + (Math.random() - 0.5), y: WALL + 2 + Math.random() };
        }
        if (kind === 'rigger') spawnRig(sim, at, hpScale);
        else spawnEnemy(sim, kind, at, hpScale);
    });
    if (sim.wave > 1) floater(sim, sim.player.pos, `wave ${sim.wave}`, '#e2e8f0', 1);
}

// --- shots -------------------------------------------------------------------------------------

function stepShots(sim: Sim, dt: number): void {
    for (const shot of sim.shots) {
        shot.pos.x += shot.vel.x * dt;
        shot.pos.y += shot.vel.y * dt;
        shot.life -= dt;
        if (shot.friendly && hitThings(sim, shot)) continue;
        if (blocked(sim, shot)) {
            shot.life = 0;
            if (shot.friendly) ring(sim, shot.pos, 0.35, 'rgba(160,240,255,0.8)', 0.18);
            else burst(sim, shot.pos, 3, '#fb923c', 'spark', 4, NaN, 0.08);
            continue;
        }
        if (shot.friendly) hitEnemies(sim, shot);
        else hitThePlayer(sim, shot);
    }
    for (const shot of sim.shots) if (shot.big && shot.life <= 0) fizzBurst(sim, shot.pos);
    sim.shots = sim.shots.filter((s) => s.life > 0);
}

// A fizz bubble bursts: a blast that hurts toys and machines (and the core), never you.
function fizzBurst(sim: Sim, at: { x: number; y: number }): void {
    explode(sim, at, FIZZ_BLAST, 0, FIZZ_DAMAGE);
    ring(sim, at, FIZZ_BLAST * 0.7, 'rgba(196,181,253,0.95)', 0.25);
    burst(sim, at, 16, 'rgba(196,181,253,0.9)', 'dot', 8, NaN, 0.25);
    const core = sim.arena.core;
    if (core && core.hp > 0 && Math.hypot(at.x - core.pos.x, at.y - core.pos.y) < core.r + FIZZ_BLAST) hurtCore(sim, FIZZ_DAMAGE * 2, at);
}

// A bubble meeting a bomb in the air, a pipe, a lift or the drill core. True if it stopped.
function hitThings(sim: Sim, shot: Shot): boolean {
    const a = sim.arena;
    for (const b of sim.bombs) {
        if (b.batted) continue;
        const bp = bombPos(b);
        if (Math.hypot(bp.pos.x - shot.pos.x, bp.pos.y - shot.pos.y) < 0.6) {
            batBomb(sim, b);
            floater(sim, bp.pos, 'BATTED!', '#a5f3fc', 1.1);
            shot.life = 0;
            return true;
        }
    }
    for (const pipe of a.pipes) {
        if (pipe.broken || pipeDistance(pipe, shot.pos) > 0.45) continue;
        shot.life = 0;
        if (--pipe.hp <= 0) breakPipe(sim, pipe);
        else burst(sim, shot.pos, 3, '#bef264', 'dot', 4, NaN, 0.12);
        return true;
    }
    for (const lift of a.lifts) {
        if (lift.broken || Math.abs(shot.pos.x - lift.pos.x) > 1.6 || Math.abs(shot.pos.y - lift.pos.y) > 1.2) continue;
        shot.life = 0;
        lift.hp -= shot.damage;
        burst(sim, shot.pos, 2, '#cbd5e1', 'spark', 4, NaN, 0.1);
        if (lift.hp <= 0) wreckLift(sim, lift);
        return true;
    }
    if (shotCutsCable(sim, shot) || shotHitsArm(sim, shot) || shotHitsObjective(sim, shot)) return true;
    const core = a.core;
    if (core && core.hp > 0 && Math.hypot(shot.pos.x - core.pos.x, shot.pos.y - core.pos.y) < core.r) {
        shot.life = 0;
        if (!shot.big) hurtCore(sim, shot.damage, shot.pos);
        return true;
    }
    return false;
}

// The drill core takes a hit — unless its shield pumps are still running.
function hurtCore(sim: Sim, damage: number, at: { x: number; y: number }): void {
    const core = sim.arena.core!;
    if (coreShielded(sim.arena)) {
        ring(sim, at, 0.5, 'rgba(147,197,253,0.9)', 0.15);
        if (Math.random() < 0.15) floater(sim, at, 'SHIELDED — break the pumps', '#93c5fd', 0.9);
        return;
    }
    core.hp -= damage;
    core.flash = 0.08;
    floater(sim, at, String(damage), '#fde047', 0.9);
    addShake(sim, 0.04);
    if (core.hp > 0) return;
    explode(sim, core.pos, core.r + 2, 0, 20);
    addShake(sim, 1);
    addStop(sim, 6);
    for (let i = 0; i < 12; i++) breakToySpirits(sim, core.pos, 3);
    floater(sim, core.pos, 'THE CORE BREAKS!', '#fde047', 1.6);
}

// High blocks stop everything. A low crate only stops a shot when someone is crouched right
// behind it: a rifleman ducking between bursts ('rest'), or the player (against bolts fired from
// further than LOW_COVER_REACH).
function blocked(sim: Sim, shot: Shot): boolean {
    const a = sim.arena;
    if (!solidAt(a, shot.pos, true)) return false;
    if (solidAt(a, shot.pos, false)) return true;
    const block = a.cover.find((b) => b.low && shot.pos.x >= b.x && shot.pos.x <= b.x + b.w && shot.pos.y >= b.y && shot.pos.y <= b.y + b.h);
    if (!block) return true;
    const near = (p: { x: number; y: number }) => {
        const nx = Math.max(block.x, Math.min(p.x, block.x + block.w));
        const ny = Math.max(block.y, Math.min(p.y, block.y + block.h));
        return Math.hypot(p.x - nx, p.y - ny);
    };
    if (shot.friendly) return sim.enemies.some((e) => e.kind === 'rifleman' && e.state === 'rest' && near(e.pos) < DUCK_REACH);
    return near(sim.player.pos) < DUCK_REACH && near(shot.origin) > LOW_COVER_REACH;
}

function hitEnemies(sim: Sim, shot: Shot): void {
    for (const e of sim.enemies) {
        if (e.spawnT > 0 || e.hp <= 0) continue;
        if (Math.hypot(e.pos.x - shot.pos.x, e.pos.y - shot.pos.y) > RADIUS[e.kind] + shot.radius) continue;
        shot.life = 0;
        hurtEnemy(sim, e, shot.damage, Math.atan2(shot.vel.y, shot.vel.x), 4);
        ring(sim, shot.pos, 0.4, 'rgba(160,240,255,0.9)', 0.15);
        return;
    }
}

// An enemy bolt reaching the player: a hit — or, mid-dash, a PERFECT dodge.
function hitThePlayer(sim: Sim, shot: Shot): void {
    const p = sim.player;
    if (Math.hypot(p.pos.x - shot.pos.x, p.pos.y - shot.pos.y) > PLAYER_RADIUS + shot.radius) return;
    if (p.iframes > 0) {
        if (p.perfectArmed) {
            p.perfectArmed = false;
            sim.stats.perfects++;
            p.joy = Math.min(sim.settings.maxJoy, p.joy + 5);
            slowMotion(sim, 0.35);
            floater(sim, p.pos, 'PERFECT!', '#fde047', 1.5);
            ring(sim, p.pos, 2.2, 'rgba(253,224,71,0.9)', 0.3);
        }
        return; // the bolt flies on through
    }
    shot.life = 0;
    hitPlayer(sim, shot.damage, Math.atan2(shot.vel.y, shot.vel.x), 6);
}

// --- deaths and joy ----------------------------------------------------------------------------

function killDead(sim: Sim): void {
    const dead = sim.enemies.filter((e) => e.hp <= 0);
    if (dead.length === 0) return;
    sim.enemies = sim.enemies.filter((e) => e.hp > 0);
    for (const e of dead) breakToy(sim, e);
}

// A toy breaks: the clown inside escapes as joy spirits (more if you were close). Mice go bang.
function breakToy(sim: Sim, e: Enemy): void {
    const d = Math.hypot(e.pos.x - sim.player.pos.x, e.pos.y - sim.player.pos.y);
    breakToySpirits(sim, e.pos, e.kind === 'mouse' ? 1 : d < 4 ? 6 : d < 9 ? 4 : 2);
    sim.stats.kills++;
    sim.stats.streak++;
    sim.stats.bestStreak = Math.max(sim.stats.bestStreak, sim.stats.streak);
    toyDrop(sim, e.pos);
    if (e.kind === 'mouse') {
        explode(sim, e.pos, MOUSE_BLAST, 1, 2);
        return;
    }
    burst(sim, e.pos, 18, e.kind === 'riot' ? '#94a3b8' : '#a3a380', 'square', 9, NaN, 0.22);
    burst(sim, e.pos, 8, '#fde047', 'spark', 7, NaN, 0.12);
    ring(sim, e.pos, 1.8, 'rgba(253,224,71,0.8)', 0.3);
    addShake(sim, 0.3);
    addStop(sim, 3);
    if (d < 4) floater(sim, e.pos, 'CLOSE! +joy', '#fde047', 1.1);
    if (sim.stats.streak > 1 && sim.stats.streak % 3 === 0) floater(sim, sim.player.pos, `${sim.stats.streak} streak!`, '#f472b6', 1.2);
}

function breakToySpirits(sim: Sim, at: { x: number; y: number }, count: number): void {
    for (let i = 0; i < count; i++) {
        const a = Math.random() * Math.PI * 2;
        sim.spirits.push({ pos: { ...at }, vel: { x: Math.cos(a) * 4, y: Math.sin(a) * 4 }, wait: 0.35 + i * 0.04 });
    }
}

function stepSpirits(sim: Sim, dt: number): void {
    const p = sim.player;
    for (const s of sim.spirits) {
        s.wait -= dt;
        if (s.wait > 0) {
            s.pos.x += s.vel.x * dt;
            s.pos.y += s.vel.y * dt;
            s.vel.x *= Math.exp(-dt * 6);
            s.vel.y *= Math.exp(-dt * 6);
            continue;
        }
        const dx = p.pos.x - s.pos.x;
        const dy = p.pos.y - s.pos.y;
        const d = Math.hypot(dx, dy);
        if (d < 0.5) {
            s.wait = -999; // collected
            p.joy = Math.min(sim.settings.maxJoy, p.joy + SPIRIT_JOY);
            sim.stats.collected++;
            if (sim.stats.collected % SPIRITS_PER_HEART === 0 && p.hp < sim.settings.playerHp) {
                p.hp++;
                floater(sim, p.pos, '+♥', '#f472b6', 1.2);
            }
            continue;
        }
        const speed = 16 + Math.min(20, -s.wait * 20);
        s.pos.x += (dx / d) * Math.min(d, speed * dt);
        s.pos.y += (dy / d) * Math.min(d, speed * dt);
    }
    sim.spirits = sim.spirits.filter((s) => s.wait > -999);
}

let trickle = 0;
function trickleJoy(sim: Sim, dt: number): void {
    if (sim.player.joy >= 5) return;
    trickle += dt;
    if (trickle >= JOY_TRICKLE) {
        trickle = 0;
        sim.player.joy++;
    }
}

// The gas line creeps up from the bottom on gassy levels; below it you choke.
function stepGas(sim: Sim, dt: number): void {
    if (!sim.arena.def.gas || sim.phase !== 'fight') return;
    sim.gasY = Math.max(6, sim.gasY - sim.settings.gasSpeed * dt);
    if (sim.player.pos.y > sim.gasY) hitPlayer(sim, 1, -Math.PI / 2, 2);
}

/** Testing: clear the field, break everything and stand at the open exit. */
export function skipWaves(sim: Sim): void {
    const a = sim.arena;
    sim.enemies = [];
    sim.shots = [];
    sim.bombs = [];
    sim.wave = a.def.waves.length;
    for (const o of a.objectives) { o.broken = true; o.hp = 0; }
    if (a.core) a.core.hp = 0;
    sim.player.pos = { x: a.def.exit.x + a.def.exit.w / 2, y: a.def.exit.y + a.def.exit.h / 2 };
    collide(a, sim.player.pos, PLAYER_RADIUS);
}
