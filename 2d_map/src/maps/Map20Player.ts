// Enemy Arena — the player: WASD to move, the mouse to aim, click to swing, Space to shout,
// Shift to sneak. Walking makes footstep noises (not when sneaking); a swing and a shout are loud.
import { add, angleDiff, angleOf, dist, norm, scale, sub, v } from './Map20Math';
import { pushOut } from './Map20World';
import { PLAYER_RADIUS, type Input, type Sim } from './Map20State';
import { hurtEnemy } from './Map20Enemy';

const SNEAK = 0.45; // × speed while sneaking
const STEP_EVERY = 0.4; // seconds between footsteps
const STEP_NOISE = 4; // metres a footstep carries
const SHOUT_NOISE = 12;
const SWING_NOISE = 6;
const SWING_REACH = 1.8;
const SWING_ARC = (60 * Math.PI) / 180; // half angle
const SWING_COOLDOWN = 0.35;

let stepLeft = 0;
let shoutHeld = false;
let attackHeld = false;

export function updatePlayer(sim: Sim, input: Input, dt: number): void {
    const p = sim.player;
    p.hurtLeft = Math.max(0, p.hurtLeft - dt);
    p.attackLeft = Math.max(0, p.attackLeft - dt);
    p.swingLeft = Math.max(0, p.swingLeft - dt);
    p.sneaking = input.sneak;
    p.facing = angleOf(sub(input.mouse, p.pos));
    const move = norm(v((input.right ? 1 : 0) - (input.left ? 1 : 0), (input.down ? 1 : 0) - (input.up ? 1 : 0)));
    const speed = sim.settings.playerSpeed * (input.sneak ? SNEAK : 1);
    p.pos = pushOut(sim.world, add(p.pos, scale(move, speed * dt)), PLAYER_RADIUS);
    const moving = move.x !== 0 || move.y !== 0;
    stepLeft -= dt;
    if (moving && !input.sneak && stepLeft <= 0) {
        stepLeft = STEP_EVERY;
        sim.noises.push({ pos: { ...p.pos }, radius: STEP_NOISE, age: 0 });
    }
    if (input.shout && !shoutHeld) sim.noises.push({ pos: { ...p.pos }, radius: SHOUT_NOISE, age: 0 });
    shoutHeld = input.shout;
    if (input.attack && !attackHeld && p.attackLeft <= 0) swing(sim);
    attackHeld = input.attack;
}

/** Hits every enemy in a short arc in front: knocked back, staggered, 1 damage. */
function swing(sim: Sim): void {
    const p = sim.player;
    p.attackLeft = SWING_COOLDOWN;
    p.swingLeft = 0.15;
    sim.noises.push({ pos: { ...p.pos }, radius: SWING_NOISE, age: 0 });
    for (const e of sim.enemies) {
        if (e.state === 'dead') continue;
        const to = sub(e.pos, p.pos);
        if (dist(e.pos, p.pos) > SWING_REACH || Math.abs(angleDiff(p.facing, angleOf(to))) > SWING_ARC) continue;
        hurtEnemy(sim, e, to);
        sim.stats.hitsGiven += 1;
    }
}

export const SWING = { reach: SWING_REACH, arc: SWING_ARC };
