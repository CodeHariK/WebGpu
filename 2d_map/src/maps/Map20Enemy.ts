// Enemy Arena — one enemy's state machine (melee; ranged fighting is in Map20Ranged).
// See EnemyState in Map20State for what each state means. The flow:
//   patrol ─(meter rising / heard something)→ suspicious ─(meter full)→ fight (+ shout)
//   fight ─(token)→ windup → lunge → recover → fight          (Map20Group hands out tokens)
//   fight ─(unseen loseAfter)→ search ─(searchTime)→ return → patrol
//   fight ─(too far from home)→ return                         (the leash)
//   any ─(you hit it)→ stagger → fight, or retreat when it has 1 hp left
import { add, angleOf, dist, norm, scale, sub, turnToward, len, type V } from './Map20Math';
import { pushOut } from './Map20World';
import { ENEMY_RADIUS, PLAYER_RADIUS, type Enemy, type EnemyState, type Sim } from './Map20State';
import { heard, updateMeter } from './Map20Senses';
import { steer } from './Map20Steering';
import { fighting, releaseToken, shout } from './Map20Group';
import { updateRanged } from './Map20Ranged';

const TURN = 7; // rad/s
const LUNGE_SPEED = 12; // m/s
const LUNGE_TIME = 0.28;
const RECOVER = 0.6;
const STAGGER = 0.45;
const RETREAT = 2.5;
const PATROL_PAUSE = 1.4;
const SUSPICIOUS_LOOK = 0.7; // seconds staring before walking over
const SUSPICIOUS_GIVE_UP = 6;
const GIVE_UP_IGNORE = 3; // seconds a returning enemy ignores you

export function setState(e: Enemy, state: EnemyState): void {
    e.state = state;
    e.timer = 0;
}

export function bark(e: Enemy, text: string): void {
    e.bark = text;
    e.barkLeft = 1.2;
}

export function face(e: Enemy, target: V, dt: number, rate = TURN): void {
    if (dist(target, e.pos) > 0.05) e.facing = turnToward(e.facing, angleOf(sub(target, e.pos)), rate * dt);
}

function faceMovement(e: Enemy, dt: number): void {
    if (len(e.vel) > 0.3) e.facing = turnToward(e.facing, angleOf(e.vel), TURN * dt);
}

/** It knows where you are now: fight. */
export function alert(sim: Sim, e: Enemy): void {
    if (e.state === 'dead' || fighting(e)) return;
    setState(e, 'fight');
    e.meter = 1;
    e.lastSeen = { ...sim.player.pos };
    e.unseen = 0;
    bark(e, '!');
}

/** Its own meter filled: fight, and shout for the others. */
function spotted(sim: Sim, e: Enemy): void {
    alert(sim, e);
    bark(e, 'there!');
    shout(sim, e, (o) => alert(sim, o));
}

function suspect(e: Enemy, at: V): void {
    if (e.state !== 'suspicious') setState(e, 'suspicious');
    e.goal = { ...at };
    bark(e, '?');
}

export function updateEnemy(sim: Sim, e: Enemy, dt: number, sees: boolean): void {
    e.timer += dt;
    e.barkLeft -= dt;
    if (e.state === 'dead') return;
    if (e.kind === 'ranged' && (e.state === 'fight' || e.state === 'aim')) {
        if (!keepFighting(sim, e, sees, dt)) return;
        updateRanged(sim, e, dt, sees);
        return;
    }
    switch (e.state) {
        case 'patrol': return patrol(sim, e, dt, sees);
        case 'suspicious': return suspicious(sim, e, dt, sees);
        case 'fight': return fight(sim, e, dt, sees);
        case 'windup': return windup(sim, e, dt);
        case 'lunge': return lunge(sim, e, dt);
        case 'recover': return recover(sim, e, dt);
        case 'stagger': return stagger(sim, e, dt);
        case 'retreat': return retreat(sim, e, dt, sees);
        case 'search': return search(sim, e, dt, sees);
        case 'return': return goHome(sim, e, dt, sees);
    }
}

// Calm states notice things: the meter, and noises.
function notice(sim: Sim, e: Enemy, sees: boolean, dt: number): boolean {
    if (updateMeter(sim, e, sees, dt)) {
        spotted(sim, e);
        return true;
    }
    if (sees && e.meter > 0.15 && e.state !== 'suspicious') {
        suspect(e, sim.player.pos);
        return true;
    }
    const noise = heard(sim, e);
    if (noise) {
        e.meter = Math.max(e.meter, 0.3);
        suspect(e, noise.pos);
        return true;
    }
    return false;
}

function patrol(sim: Sim, e: Enemy, dt: number, sees: boolean): void {
    if (notice(sim, e, sees, dt)) return;
    const speed = sim.settings.enemySpeed * 0.4;
    if (e.goal) {
        steer(sim, e, e.goal, speed, dt, { arrive: 0.6 });
        faceMovement(e, dt);
        if (dist(e.pos, e.goal) < 0.4) {
            e.goal = null;
            e.timer = 0;
        }
        return;
    }
    steer(sim, e, null, 0, dt); // pause: stand and look about
    e.facing += Math.sin(sim.time * 1.3 + e.id) * 0.9 * dt;
    if (e.timer > PATROL_PAUSE + (e.id % 3) * 0.4 && e.patrol.length > 1) {
        e.patrolIndex = (e.patrolIndex + 1) % e.patrol.length;
        e.goal = e.patrol[e.patrolIndex];
    }
}

function suspicious(sim: Sim, e: Enemy, dt: number, sees: boolean): void {
    if (updateMeter(sim, e, sees, dt)) return spotted(sim, e);
    const noise = heard(sim, e);
    if (noise) {
        e.goal = { ...noise.pos };
        e.timer = 0;
    }
    if (sees) e.goal = { ...sim.player.pos }; // keeps its eyes on you
    const at = e.goal ?? e.pos;
    face(e, at, dt);
    if (e.timer > SUSPICIOUS_LOOK) steer(sim, e, at, sim.settings.enemySpeed * 0.35, dt, { arrive: 1.2 });
    else steer(sim, e, null, 0, dt);
    const settled = e.meter <= 0 && e.timer > 2.5 && dist(e.pos, at) < 1.5;
    if (settled || e.timer > SUSPICIOUS_GIVE_UP) {
        bark(e, 'hm.');
        setState(e, 'return');
    }
}

/** Shared by melee and ranged fighters: track you, give up when lost or too far. False = stopped fighting. */
export function keepFighting(sim: Sim, e: Enemy, sees: boolean, dt: number): boolean {
    if (sees) {
        e.lastSeen = { ...sim.player.pos };
        e.unseen = 0;
    } else e.unseen += dt;
    const s = sim.settings;
    if (s.leash && dist(e.pos, e.home) > s.leashRadius) {
        bark(e, 'tch');
        releaseToken(sim, e);
        setState(e, 'return');
        return false;
    }
    if (e.unseen > s.loseAfter) {
        bark(e, s.search ? 'where…' : 'lost it');
        releaseToken(sim, e);
        e.goal = e.lastSeen;
        setState(e, s.search ? 'search' : 'return');
        return false;
    }
    return true;
}

function fight(sim: Sim, e: Enemy, dt: number, sees: boolean): void {
    if (!keepFighting(sim, e, sees, dt)) return;
    const target = e.lastSeen ?? sim.player.pos;
    const goal = e.slot >= 0 ? sim.slots[e.slot].pos : target;
    steer(sim, e, goal, sim.settings.enemySpeed, dt, { keepAway: sim.settings.slotRadius - 0.5, arrive: 1 });
    face(e, target, dt);
}

/** The group gave it a token: wind up (the telegraph), then lunge. */
export function startWindup(e: Enemy): void {
    setState(e, 'windup');
}

function windup(sim: Sim, e: Enemy, dt: number): void {
    steer(sim, e, null, 0, dt);
    face(e, sim.player.pos, dt, TURN * 2);
    const time = sim.settings.windup ? sim.settings.windupTime : 0.05;
    if (e.timer < time) return;
    e.lungeDir = norm(sub(sim.player.pos, e.pos)); // committed: it goes where you were
    e.hitDone = false;
    setState(e, 'lunge');
}

function lunge(sim: Sim, e: Enemy, dt: number): void {
    e.vel = scale(e.lungeDir, LUNGE_SPEED);
    e.pos = pushOut(sim.world, add(e.pos, scale(e.vel, dt)), ENEMY_RADIUS);
    if (!e.hitDone && dist(e.pos, sim.player.pos) < ENEMY_RADIUS + PLAYER_RADIUS + 0.15) {
        e.hitDone = true;
        hitPlayer(sim, e.lungeDir);
    }
    if (e.timer > LUNGE_TIME) setState(e, 'recover');
}

function recover(sim: Sim, e: Enemy, dt: number): void {
    steer(sim, e, null, 0, dt);
    face(e, sim.player.pos, dt);
    if (e.timer < RECOVER) return;
    releaseToken(sim, e);
    setState(e, 'fight');
}

export function hitPlayer(sim: Sim, from: V): void {
    const p = sim.player;
    if (p.hurtLeft > 0) return; // a moment of mercy after each hit
    p.hp -= 1;
    p.hurtLeft = 0.5;
    p.pos = pushOut(sim.world, add(p.pos, scale(norm(from), 0.8)), PLAYER_RADIUS);
    sim.stats.hitsTaken += 1;
    if (p.hp <= 0) p.hp = 10; // keep playing: the HUD counts hits taken
}

/** The player hit it: knocked back along `push`; dies at 0 hp. */
export function hurtEnemy(sim: Sim, e: Enemy, push: V): void {
    e.hp -= 1;
    releaseToken(sim, e);
    if (e.hp <= 0) {
        setState(e, 'dead');
        sim.stats.kills += 1;
        return;
    }
    const wasFighting = fighting(e);
    e.vel = scale(norm(push), 7);
    e.lastSeen = { ...sim.player.pos };
    e.unseen = 0;
    e.meter = 1;
    setState(e, 'stagger');
    bark(e, wasFighting ? 'ngh' : '!?');
}

function stagger(sim: Sim, e: Enemy, dt: number): void {
    e.vel = scale(e.vel, Math.exp(-6 * dt));
    e.pos = pushOut(sim.world, add(e.pos, scale(e.vel, dt)), ENEMY_RADIUS);
    if (e.timer < STAGGER) return;
    if (sim.settings.retreat && e.hp === 1) {
        bark(e, 'back off');
        setState(e, 'retreat');
        return;
    }
    setState(e, 'fight');
}

function retreat(sim: Sim, e: Enemy, dt: number, sees: boolean): void {
    if (sees) e.lastSeen = { ...sim.player.pos };
    const away = norm(sub(e.pos, sim.player.pos));
    steer(sim, e, add(e.pos, scale(away, 3)), sim.settings.enemySpeed * 0.8, dt);
    face(e, sim.player.pos, dt);
    if (e.timer > RETREAT) setState(e, 'fight');
}

function search(sim: Sim, e: Enemy, dt: number, sees: boolean): void {
    if (updateMeter(sim, e, sees, dt) || (sees && e.meter > 0.5)) return spotted(sim, e); // it's looking for you: quick
    if (e.goal) {
        steer(sim, e, e.goal, sim.settings.enemySpeed * 0.6, dt, { arrive: 0.8 });
        faceMovement(e, dt);
        if (dist(e.pos, e.goal) < 0.8) {
            e.goal = null;
            e.timer = 0;
        }
        return;
    }
    steer(sim, e, null, 0, dt); // there: look around
    e.facing += 2.2 * dt * (e.id % 2 ? 1 : -1);
    if (e.timer > sim.settings.searchTime) {
        bark(e, 'gone.');
        setState(e, 'return');
    }
}

function goHome(sim: Sim, e: Enemy, dt: number, sees: boolean): void {
    // It just gave up: ignore you for a few steps, or it would turn round at once at the leash edge.
    if (e.timer < GIVE_UP_IGNORE) e.meter = 0;
    else if (notice(sim, e, sees, dt)) return;
    steer(sim, e, e.home, sim.settings.enemySpeed * 0.45, dt, { arrive: 0.6 });
    faceMovement(e, dt);
    if (dist(e.pos, e.home) < 0.5) {
        e.meter = 0;
        e.goal = e.patrol[e.patrolIndex];
        setState(e, 'patrol');
    }
}

