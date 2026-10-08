// Enemy Arena — context steering: how an enemy gets where it's going without a path finder.
// Each frame it scores DIRS directions round itself:
//   interest  how much a direction points at the goal (dot product, negatives = 0)
//   danger    a wall close ahead that way (one short ray per direction), a squadmate that way
//             (separation), the player that way when it must keep its distance
// and walks the direction with the best interest − danger. Cheap, no grid, slides round boxes and
// round each other. (A short A* would only be needed for mazes; this arena has none.)
import { add, dist, dot, fromAngle, len, norm, scale, sub, type V } from './Map20Math';
import { pushOut, raycast } from './Map20World';
import { ENEMY_RADIUS, type Enemy, type Sim } from './Map20State';

const DIRS = 16;
const LOOK = 1.4; // metres of wall look-ahead
const SPACE = 1.4; // metres kept from squadmates
const DANGER = 2.0; // how much danger outweighs interest
const SMOOTH = 10; // 1/s: how fast the velocity follows the chosen direction

export type SteerOptions = {
    keepAway?: number; // stay at least this far from the player (waiting on the ring)
    arrive?: number; // slow down inside this distance of the goal
};

/** Pick a direction toward `goal` (null = stand, but still make room) and move at up to `speed`. */
export function steer(sim: Sim, e: Enemy, goal: V | null, speed: number, dt: number, opts: SteerOptions = {}): void {
    const want = goal ? norm(sub(goal, e.pos)) : { x: 0, y: 0 };
    let best = -Infinity;
    let bestDir: V = { x: 0, y: 0 };
    for (let i = 0; i < DIRS; i++) {
        const dir = fromAngle((i / DIRS) * Math.PI * 2);
        const score = Math.max(0, dot(dir, want)) - DANGER * danger(sim, e, dir, opts);
        if (score > best) {
            best = score;
            bestDir = dir;
        }
    }
    let target = best > 0.05 ? scale(bestDir, speed) : { x: 0, y: 0 };
    if (goal && opts.arrive) target = scale(target, Math.min(1, dist(goal, e.pos) / opts.arrive));
    const blend = 1 - Math.exp(-SMOOTH * dt);
    e.vel = add(e.vel, scale(sub(target, e.vel), blend));
    e.pos = pushOut(sim.world, add(e.pos, scale(e.vel, dt)), ENEMY_RADIUS);
}

function danger(sim: Sim, e: Enemy, dir: V, opts: SteerOptions): number {
    let d = 0;
    const wall = raycast(sim.world, e.pos, add(e.pos, scale(dir, LOOK)));
    if (wall.hit) d = Math.max(d, 1 - wall.t);
    for (const o of sim.enemies) {
        if (o === e || o.state === 'dead') continue;
        const away = sub(o.pos, e.pos);
        const gap = len(away);
        if (gap < SPACE) d = Math.max(d, Math.max(0, dot(dir, norm(away))) * (1 - gap / SPACE));
    }
    if (opts.keepAway) {
        const toPlayer = sub(sim.player.pos, e.pos);
        const gap = len(toPlayer);
        if (gap < opts.keepAway) d = Math.max(d, Math.max(0, dot(dir, norm(toPlayer))) * (1 - gap / opts.keepAway + 0.3));
    }
    return d;
}
