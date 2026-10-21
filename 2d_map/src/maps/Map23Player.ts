// Lockpick — you, the thief. No weapon: you get out by not being seen.
//
//   WASD         walk (some noise on creaky boards, glass and water)
//   hold Shift   sneak: slow, nearly silent, and harder to see
//   hold Space   run: fast, and loud everywhere
//   click        throw a pebble where you point: it lands with a clatter guards go to look at
//   E            use what's next to you: open/close a door (unlock it with the right key), douse a
//                torch (the room goes dark — guards will come and relight it), climb into a barrel
//                to hide (E again to climb out), pick up a key or pebbles
import { centre, collide, floorAt, solid, tileAt } from './Map23Grid';
import { makeNoise } from './Map23Noise';
import { CREAK, GLASS, type Input, type Player, PLATE, type Sim, type Vec, WATER } from './Map23State';

export const PLAYER_R = 0.3;
const USE_REACH = 1.6;

export function createPlayer(at: Vec, pebbles: number, keys: Player['keys']): Player {
    return { pos: centre(at), facing: -Math.PI / 2, hidden: -1, pebbles, keys: [...keys], stepT: 0, scentT: 0, moving: false, mode: 'walk' };
}

export function stepPlayer(sim: Sim, input: Input, dt: number): void {
    const p = sim.player;
    const s = sim.settings;
    if (input.use) { interact(sim); input.use = false; }
    if (input.throw) { throwPebble(sim, input.mouse); input.throw = false; }
    if (p.hidden >= 0) { p.moving = false; return; }
    let mx = (input.right ? 1 : 0) - (input.left ? 1 : 0);
    let my = (input.down ? 1 : 0) - (input.up ? 1 : 0);
    const len = Math.hypot(mx, my);
    p.moving = len > 0;
    p.mode = input.sneak ? 'sneak' : input.run ? 'run' : 'walk';
    if (!p.moving) return;
    mx /= len; my /= len;
    p.facing = Math.atan2(my, mx);
    const floor = floorAt(sim, p.pos);
    const speed = (p.mode === 'sneak' ? s.sneakSpeed : p.mode === 'run' ? s.runSpeed : s.walkSpeed) * (floor === WATER ? s.waterSlow : 1);
    p.pos.x += mx * speed * dt;
    p.pos.y += my * speed * dt;
    collide(sim, p.pos, PLAYER_R);
    footsteps(sim, dt);
}

// Footsteps: how loud depends on how you move and what's underfoot.
function footsteps(sim: Sim, dt: number): void {
    const p = sim.player;
    p.stepT -= dt;
    if (p.stepT > 0) return;
    p.stepT = p.mode === 'run' ? 0.28 : p.mode === 'sneak' ? 0.6 : 0.42;
    const floor = floorAt(sim, p.pos);
    if (floor === PLATE) { alarm(sim, p.pos); return; }
    const table: Record<'walk' | 'sneak' | 'run', number[]> = { // [floor, creak, glass, water]
        walk: [1.2, 5, 6.5, 4],
        sneak: [0, 1.5, 3.5, 2],
        run: [6, 8, 9, 7],
    };
    const r = table[p.mode][floor === CREAK ? 1 : floor === GLASS ? 2 : floor === WATER ? 3 : 0];
    if (r > 0) makeNoise(sim, p.pos, r);
}

/** An alarm plate: a bell rings and every guard in the keep comes to look. */
export function alarm(sim: Sim, at: Vec): void {
    makeNoise(sim, at, 40, true);
    sim.floaters.push({ pos: { ...at }, text: 'CLANG! the alarm bell', color: '#f87171', life: 1.6 });
}

// --- using things ------------------------------------------------------------------------------

function interact(sim: Sim): void {
    const p = sim.player;
    if (p.hidden >= 0) { leaveBarrel(sim); return; }
    // Items first.
    for (const it of sim.items) {
        if (it.taken || Math.hypot(it.pos.x - p.pos.x, it.pos.y - p.pos.y) > USE_REACH) continue;
        it.taken = true;
        if (it.kind === 'key' && it.key) { p.keys.push(it.key); say(sim, it.key === 'gate' ? 'the GATE KEY — now get out' : 'an IRON KEY', '#fde047'); }
        else { p.pebbles += 3; say(sim, '+3 pebbles', '#cbd5e1'); }
        return;
    }
    // Then the nearest thing to use.
    const near = (q: Vec) => Math.hypot(q.x - p.pos.x, q.y - p.pos.y);
    const door = sim.doors.filter((d) => near(centre(d)) < USE_REACH).sort((a, b) => near(centre(a)) - near(centre(b)))[0];
    const torch = sim.torches.filter((t) => near(t.light) < USE_REACH + 0.3)[0];
    const barrel = sim.barrels.findIndex((b) => near(centre(b)) < USE_REACH);
    if (door) { operateDoor(sim, door); return; }
    if (torch) {
        torch.lit = !torch.lit;
        sim.lightDirty = true;
        makeNoise(sim, torch.light, torch.lit ? 1.5 : 2.5);
        say(sim, torch.lit ? 'lit' : 'doused — it’s dark', torch.lit ? '#fdba74' : '#94a3b8');
        return;
    }
    if (barrel >= 0) { p.hidden = barrel; p.pos = centre(sim.barrels[barrel]); say(sim, 'hiding', '#a5b4fc'); makeNoise(sim, p.pos, 1); }
}

function operateDoor(sim: Sim, d: Sim['doors'][number]): void {
    const p = sim.player;
    if (!d.open && d.lock) {
        if (!p.keys.includes(d.lock)) { say(sim, `locked — needs the ${d.lock.toUpperCase()} key`, '#fca5a5'); return; }
        say(sim, 'unlocked', '#fde047');
        d.lock = null;
    }
    if (d.open && tileAt(sim, p.pos).x === d.x && tileAt(sim, p.pos).y === d.y) return; // standing in it
    d.open = !d.open;
    sim.lightDirty = true;
    makeNoise(sim, centre(d), 3);
}

function leaveBarrel(sim: Sim): void {
    const p = sim.player;
    const b = sim.barrels[p.hidden];
    p.hidden = -1;
    for (const [ox, oy] of [[0, 1], [0, -1], [1, 0], [-1, 0]]) {
        if (solid(sim, b.x + ox, b.y + oy)) continue;
        p.pos = centre({ x: b.x + ox, y: b.y + oy });
        break;
    }
    makeNoise(sim, p.pos, 1.5);
}

function throwPebble(sim: Sim, target: Vec): void {
    const p = sim.player;
    if (p.pebbles <= 0) { say(sim, 'no pebbles', '#94a3b8'); return; }
    const d = Math.hypot(target.x - p.pos.x, target.y - p.pos.y);
    const k = Math.min(1, sim.settings.throwRange / Math.max(0.1, d));
    const to = { x: p.pos.x + (target.x - p.pos.x) * k, y: p.pos.y + (target.y - p.pos.y) * k };
    // It stops at the first wall in the way.
    const n = Math.ceil(d * k / 0.2);
    let land = { ...p.pos };
    for (let i = 1; i <= n; i++) {
        const q = { x: p.pos.x + ((to.x - p.pos.x) * i) / n, y: p.pos.y + ((to.y - p.pos.y) * i) / n };
        if (solid(sim, Math.floor(q.x), Math.floor(q.y))) break;
        land = q;
    }
    p.pebbles--;
    sim.pebbles.push({ from: { ...p.pos }, to: land, t: 0, flight: 0.25 + Math.hypot(land.x - p.pos.x, land.y - p.pos.y) * 0.04 });
}

export function stepPebbles(sim: Sim, dt: number): void {
    for (const pb of sim.pebbles) {
        pb.t += dt;
        if (pb.t >= pb.flight) makeNoise(sim, pb.to, sim.settings.pebbleNoise);
    }
    sim.pebbles = sim.pebbles.filter((pb) => pb.t < pb.flight);
}

function say(sim: Sim, text: string, color: string): void {
    sim.floaters.push({ pos: { x: sim.player.pos.x, y: sim.player.pos.y - 0.6 }, text, color, life: 1.4 });
}
