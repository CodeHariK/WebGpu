// Enemy Arena — a ranged enemy in a fight (the sentry / spitter):
//   hide   go to the nearest cover marker the player can't see from where it was last seen, that
//          is within SHOOT_RANGE of it, and no other shooter is using; wait HIDE seconds there
//   peek   walk toward the player until it can see it
//   aim    stand, a laser line toward the player (it tracks for the first 70%, then locks: step
//          aside to dodge), then the shot (a ray: hits if the player is on the line)
//   → back to hiding. With cover off it just keeps KEEP_RANGE and shoots on a timer.
import { add, dist, lerp, norm, scale, sub, len, type V } from './Map20Math';
import { clear, raycast } from './Map20World';
import { PLAYER_RADIUS, type Enemy, type Sim } from './Map20State';
import { steer } from './Map20Steering';
import { face, hitPlayer, setState } from './Map20Enemy';

const HIDE = 1.8; // seconds behind cover between shots
const SHOOT_RANGE = 13;
const KEEP_RANGE = 8; // no cover: stays this far away
const PEEK_GIVE_UP = 4; // seconds peeking without a shot → hide again
const SHOT_LENGTH = 18;

export function updateRanged(sim: Sim, e: Enemy, dt: number, sees: boolean): void {
    if (e.state === 'aim') return aim(sim, e, dt);
    const target = e.lastSeen ?? sim.player.pos;
    face(e, target, dt);
    const s = sim.settings;
    if (!s.cover) {
        const keep = add(target, scale(norm(sub(e.pos, target)), KEEP_RANGE));
        steer(sim, e, keep, s.enemySpeed * 0.7, dt, { arrive: 1 });
        if (sees && e.timer > HIDE) startAim(sim, e);
        return;
    }
    if (e.timer < HIDE) { // hiding
        if (!e.goal || e.timer < dt * 1.5) e.goal = pickCover(sim, e, target) ?? e.pos;
        steer(sim, e, e.goal, s.enemySpeed * 0.8, dt, { arrive: 0.6 });
        return;
    }
    steer(sim, e, target, s.enemySpeed * 0.5, dt, { keepAway: 4 }); // peeking out
    if (sees && dist(e.pos, sim.player.pos) < SHOOT_RANGE) startAim(sim, e);
    else if (e.timer > HIDE + PEEK_GIVE_UP) e.timer = 0;
}

function pickCover(sim: Sim, e: Enemy, from: V): V | null {
    let best: V | null = null;
    let bestDistance = Infinity;
    for (const spot of sim.world.markers.cover) {
        if (dist(spot, from) > SHOOT_RANGE || clear(sim.world, spot, from)) continue; // too far, or not hidden
        const used = sim.enemies.some((o) => o !== e && o.kind === 'ranged' && o.state !== 'dead' && o.goal && dist(o.goal, spot) < 1);
        if (used) continue;
        const d = dist(spot, e.pos);
        if (d < bestDistance) {
            bestDistance = d;
            best = spot;
        }
    }
    return best;
}

function startAim(sim: Sim, e: Enemy): void {
    e.aim = { ...sim.player.pos };
    setState(e, 'aim');
}

function aim(sim: Sim, e: Enemy, dt: number): void {
    steer(sim, e, null, 0, dt);
    const time = sim.settings.windup ? Math.max(0.6, sim.settings.windupTime * 2) : 0.1;
    if (e.timer < time * 0.7) e.aim = lerp(e.aim, sim.player.pos, 1 - Math.exp(-12 * dt)); // tracking, then locked
    face(e, e.aim, dt, 20);
    if (e.timer < time) return;
    const dir = norm(sub(e.aim, e.pos));
    const end = raycast(sim.world, e.pos, add(e.pos, scale(dir, SHOT_LENGTH))).point;
    const hit = distToSegment(sim.player.pos, e.pos, end) < PLAYER_RADIUS + 0.15;
    if (hit) hitPlayer(sim, dir);
    sim.shots.push({ from: { ...e.pos }, to: end, age: 0, hit });
    e.goal = null;
    setState(e, 'fight'); // back behind cover
}

function distToSegment(p: V, a: V, b: V): number {
    const ab = sub(b, a);
    const l2 = len(ab) ** 2;
    const t = l2 > 0 ? Math.max(0, Math.min(1, ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / l2)) : 0;
    return dist(p, add(a, scale(ab, t)));
}
