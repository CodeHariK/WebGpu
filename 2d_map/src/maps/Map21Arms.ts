// Combat Sandbox — grabber arms (Assembly Plant). A claw rides a gantry rail over an assembly line:
//
//   patrol   slides back and forth along its rail, high up (bubbles fly under it)
//   lower    stopped over someone: a red reticle shrinks on the floor, then the claw drops
//   carry    if they were still under it, it lifts them and carries them to its press, drops them
//            in, and the press slams a moment later (a toy is crushed; you'd better dash out)
//   recover  back up, then patrol again
//   jam      shot enough while low (or you wriggled free): sparks, drops what it holds, stuck a while
//   off      its terminal is broken
//
// Held, you can't move: dash (Space) three times to wriggle free, or shoot the claw.
import { flies } from './Map21Enemies';
import { CRUSH_SLAM } from './Map21Hazards';
import { addShake, burst, floater, ring } from './Map21Juice';
import type { Arm, Enemy, Shot, Sim, Vec } from './Map21State';
import { ARM_HP } from './Map21World';

const REACH = 0.9; // grabs things this close (along the line) under the claw, anywhere across the belt
const LOWER_TIME = 0.6; // the warning before the claw drops
const RECOVER_TIME = 0.7;
const JAM_TIME = 3;
const WRIGGLE_JAM = 1.4;
const CARRY_SPEED = 6;
const HUNT_SPEED = 6.5; // sliding over someone on the line
const FOLLOW_SPEED = 4; // tracking them while it drops (faster than the belt; slower than you)
const LINE_HALF = 1.6; // on the line: this close to the rail (the belt is 3 m wide)
const STRUGGLES = 3;
const PRESS_DELAY = 0.35; // dropped in the press: this long to dash out before it slams
export const CLAW_HIT = 0.75; // shot radius on a lowered claw

const grabbable = (e: Enemy) => e.spawnT <= 0 && !e.held && e.hp > 0 && !flies(e) && e.kind !== 'turret'; // turrets are bolted down

export const clawPos = (arm: Arm): Vec => ({ x: arm.x, y: arm.def.y });
/** How high the claw hangs: 1 up at patrol, 0 down on the floor. */
export function clawHeight(arm: Arm): number {
    if (arm.state === 'lower') return Math.max(0, 1 - arm.t / LOWER_TIME);
    if (arm.state === 'carry') return 0.35;
    if (arm.state === 'recover') return Math.min(1, arm.t / RECOVER_TIME);
    if (arm.state === 'jam') return 0.5;
    return 1;
}
export const armLowered = (arm: Arm) => arm.state === 'lower' || arm.state === 'carry';
export const LOWER_SECONDS = LOWER_TIME;

export function stepArms(sim: Sim, dt: number): void {
    const a = sim.arena;
    for (const arm of a.arms) {
        arm.t += dt;
        arm.flash = Math.max(0, arm.flash - dt);
        if (!a.armsOn && arm.state !== 'off') { release(sim, arm); arm.state = 'off'; arm.t = 0; }
        switch (arm.state) {
            case 'patrol': patrol(sim, arm, dt); break;
            case 'lower': lower(sim, arm, dt); break;
            case 'carry': carry(sim, arm, dt); break;
            case 'recover': if (arm.t > RECOVER_TIME) setState(arm, 'patrol'); break;
            case 'jam': if (arm.t > JAM_TIME) { arm.hp = ARM_HP; setState(arm, 'recover'); } break;
            case 'off': break;
        }
    }
}

function setState(arm: Arm, state: Arm['state']): void {
    arm.state = state;
    arm.t = 0;
}

function patrol(sim: Sim, arm: Arm, dt: number): void {
    const d = arm.def;
    // Anyone on its stretch of the line? Hurry over them, then drop.
    const prey = nearestPrey(sim, arm);
    if (prey) {
        const dx = prey.pos.x - arm.x;
        if (Math.abs(dx) < 0.3) {
            arm.who = prey.who;
            setState(arm, 'lower');
            return;
        }
        arm.x += Math.sign(dx) * Math.min(Math.abs(dx), HUNT_SPEED * dt);
        arm.dir = Math.sign(dx);
        return;
    }
    arm.x += arm.dir * d.speed * dt;
    if (arm.x > d.x1) { arm.x = d.x1; arm.dir = -1; }
    if (arm.x < d.x0) { arm.x = d.x0; arm.dir = 1; }
}

// The closest thing standing on this arm's stretch of the line that no other claw is after.
function nearestPrey(sim: Sim, arm: Arm): { pos: Vec; who: 'player' | number } | null {
    const d = arm.def;
    const busy = (who: 'player' | number) => sim.arena.arms.some((o) => o !== arm && o.who === who);
    const onLine = (p: Vec) => Math.abs(p.y - d.y) < LINE_HALF && p.x > d.x0 - 0.5 && p.x < d.x1 + 0.5;
    let best: { pos: Vec; who: 'player' | number } | null = null;
    let bestD = Infinity;
    const p = sim.player;
    if (!p.held && onLine(p.pos) && !busy('player')) { best = { pos: p.pos, who: 'player' }; bestD = Math.abs(p.pos.x - arm.x); }
    for (const e of sim.enemies) {
        if (!grabbable(e) || !onLine(e.pos) || busy(e.id)) continue;
        const k = Math.abs(e.pos.x - arm.x);
        if (k < bestD) { bestD = k; best = { pos: e.pos, who: e.id }; }
    }
    return best;
}

// The claw comes down: whoever is still under it when it lands is caught.
function lower(sim: Sim, arm: Arm, dt: number): void {
    const target = arm.who === 'player' ? sim.player.pos : sim.enemies.find((e) => e.id === arm.who)?.pos;
    if (target && arm.t < LOWER_TIME * 0.8) { // it follows them along the line as it drops
        const dx = target.x - arm.x;
        arm.x += Math.sign(dx) * Math.min(Math.abs(dx), FOLLOW_SPEED * dt);
    }
    if (arm.t < LOWER_TIME) return;
    const c = clawPos(arm);
    burst(sim, c, 6, '#94a3b8', 'square', 4, NaN, 0.12);
    addShake(sim, 0.12);
    const p = sim.player;
    if (!p.held && p.dashT <= 0 && under(arm, p.pos)) {
        arm.who = 'player';
        p.held = true;
        p.struggle = 0;
        p.vel = { x: 0, y: 0 };
        floater(sim, p.pos, 'GRABBED! dash ×3', '#f87171', 1.3);
        addShake(sim, 0.4);
        setState(arm, 'carry');
        return;
    }
    const toy = sim.enemies.find((e) => grabbable(e) && under(arm, e.pos));
    if (toy) {
        arm.who = toy.id;
        toy.held = true;
        floater(sim, toy.pos, 'SNATCHED', '#fde047', 1);
        setState(arm, 'carry');
        return;
    }
    arm.who = null; // missed
    setState(arm, 'recover');
}

function carry(sim: Sim, arm: Arm, dt: number): void {
    const press = sim.arena.crushers[arm.def.press];
    const target = press.x + press.w / 2;
    const held = holding(sim, arm);
    if (!held) { arm.who = null; setState(arm, 'recover'); return; } // it broke in the claw
    if (arm.who === 'player' && sim.player.struggle >= STRUGGLES) { // wriggled free
        release(sim, arm);
        floater(sim, sim.player.pos, 'FREE!', '#a5f3fc', 1.2);
        sim.player.hurtT = Math.max(sim.player.hurtT, 0.4);
        setState(arm, 'jam');
        arm.t = JAM_TIME - WRIGGLE_JAM;
        return;
    }
    const step = Math.sign(target - arm.x) * Math.min(Math.abs(target - arm.x), CARRY_SPEED * dt);
    arm.x += step;
    held.x = arm.x;
    held.y = arm.def.y;
    if (Math.abs(target - arm.x) > 0.05) return;
    // Over the press: drop it in and slam.
    release(sim, arm);
    press.t = press.period - CRUSH_SLAM - PRESS_DELAY;
    floater(sim, { x: target, y: arm.def.y - 1 }, 'INTO THE PRESS', '#fb923c', 1.1);
    setState(arm, 'recover');
}

// Where the held thing is (a reference to its position), or null if it's gone.
function holding(sim: Sim, arm: Arm): Vec | null {
    if (arm.who === 'player') return sim.player.held ? sim.player.pos : null;
    if (arm.who === null) return null;
    const toy = sim.enemies.find((e) => e.id === arm.who && e.hp > 0);
    return toy ? toy.pos : null;
}

function release(sim: Sim, arm: Arm): void {
    if (arm.who === 'player') {
        sim.player.held = false;
        sim.player.struggle = 0;
    } else if (arm.who !== null) {
        const toy = sim.enemies.find((e) => e.id === arm.who);
        if (toy) { toy.held = false; toy.stagger = Math.max(toy.stagger, 0.8); }
    }
    arm.who = null;
}

/** A friendly shot meeting a lowered claw: six of them jam it (it drops what it holds). */
export function shotHitsArm(sim: Sim, shot: Shot): boolean {
    for (const arm of sim.arena.arms) {
        if (!armLowered(arm) || dist(shot.pos, clawPos(arm)) > CLAW_HIT + shot.radius) continue;
        if (arm.who === 'player' && dist(shot.origin, clawPos(arm)) < 0.2) continue; // your own muzzle, from inside it
        shot.life = 0;
        arm.hp -= shot.big ? ARM_HP : shot.damage;
        arm.flash = 0.08;
        burst(sim, shot.pos, 3, '#e2e8f0', 'spark', 5, NaN, 0.1);
        if (arm.hp <= 0) {
            release(sim, arm);
            setState(arm, 'jam');
            ring(sim, clawPos(arm), 1.4, 'rgba(250,204,21,0.9)', 0.25);
            burst(sim, clawPos(arm), 14, '#facc15', 'spark', 8, NaN, 0.15);
            floater(sim, clawPos(arm), 'JAMMED', '#facc15', 1.2);
            addShake(sim, 0.25);
        }
        return true;
    }
    return false;
}

const dist = (p: Vec, q: Vec) => Math.hypot(p.x - q.x, p.y - q.y);
const under = (arm: Arm, p: Vec) => Math.abs(p.x - arm.x) < REACH && Math.abs(p.y - arm.def.y) < LINE_HALF;
