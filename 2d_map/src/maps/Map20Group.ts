// Enemy Arena — the group tricks (the whole "squad AI"):
//   slot ring   slotCount fixed spots on a circle round the player; a slot is usable if it isn't
//               in a wall and the player could reach it in a straight line (one ray). Each melee
//               fighter takes the nearest free one (greedy, a bonus for keeping its own).
//   tokens      only `tokens` enemies attack at once; a returned token rests tokenRest seconds.
//               Whoever is behind the player gets to go first, then the nearest.
//   shout       one spots you → everyone within shoutRadius knows where you are.
//   share       while anyone in the fight sees you, everyone in the fight knows where you are.
import { angleDiff, angleOf, dist, fromAngle, add, scale, sub } from './Map20Math';
import { blocked, clear } from './Map20World';
import { ENEMY_RADIUS, type Enemy, type Sim } from './Map20State';

const KEEP_BONUS = 1.5; // metres: how much an enemy prefers its current slot
const AT_SLOT = 0.8; // metres from its slot that count as "in place"

export const fighting = (e: Enemy) => e.state === 'fight' || e.state === 'windup' || e.state === 'lunge' || e.state === 'recover' || e.state === 'aim';

export function updateSlots(sim: Sim): void {
    const { slotCount, slotRadius } = sim.settings;
    const p = sim.player.pos;
    sim.slots = [];
    for (let i = 0; i < slotCount; i++) {
        const pos = add(p, scale(fromAngle((i / slotCount) * Math.PI * 2), slotRadius));
        const valid = !blocked(sim.world, pos, ENEMY_RADIUS) && clear(sim.world, p, pos);
        sim.slots.push({ pos, valid, taken: -1 });
    }
}

/** Greedy: repeatedly give the cheapest (enemy, slot) pair; cost = distance − keep bonus. */
export function assignSlots(sim: Sim): void {
    const fighters = sim.enemies.filter((e) => e.kind === 'melee' && fighting(e));
    for (const e of sim.enemies) if (!fighters.includes(e)) e.slot = -1;
    if (!sim.settings.slots) {
        for (const e of fighters) e.slot = -1;
        return;
    }
    const free = fighters.slice();
    while (free.length) {
        let best = { cost: Infinity, e: null as Enemy | null, i: -1 };
        for (const e of free) {
            sim.slots.forEach((s, i) => {
                if (!s.valid || s.taken >= 0) return;
                const cost = dist(e.pos, s.pos) - (e.slot === i ? KEEP_BONUS : 0);
                if (cost < best.cost) best = { cost, e, i };
            });
        }
        if (!best.e) break; // more fighters than slots: the rest wait outside (slot −1)
        best.e.slot = best.i;
        sim.slots[best.i].taken = best.e.id;
        free.splice(free.indexOf(best.e), 1);
    }
    for (const e of free) e.slot = -1;
}

export const inPlace = (sim: Sim, e: Enemy): boolean =>
    e.slot < 0 ? dist(e.pos, sim.player.pos) < sim.settings.slotRadius + 1 : dist(e.pos, sim.slots[e.slot].pos) < AT_SLOT;

// --- tokens ------------------------------------------------------------------------------------

export function updateTokens(sim: Sim, dt: number): void {
    sim.tokens.resting = sim.tokens.resting.map((t) => t - dt).filter((t) => t > 0);
    sim.tokens.holders = sim.tokens.holders.filter((id) => sim.enemies.some((e) => e.id === id && e.hasToken));
}

function requestToken(sim: Sim, e: Enemy): boolean {
    if (!sim.settings.takeTurns) return true;
    if (sim.tokens.holders.length + sim.tokens.resting.length >= sim.settings.tokens) return false;
    sim.tokens.holders.push(e.id);
    e.hasToken = true;
    return true;
}

export function releaseToken(sim: Sim, e: Enemy): void {
    if (!e.hasToken) return;
    e.hasToken = false;
    sim.tokens.holders = sim.tokens.holders.filter((id) => id !== e.id);
    sim.tokens.resting.push(sim.settings.tokenRest);
}

/** Melee fighters in place ask for a token, best shot first: behind the player, then nearest. */
export function handOutAttacks(sim: Sim, start: (e: Enemy) => void): void {
    const p = sim.player;
    const ready = sim.enemies.filter((e) => e.kind === 'melee' && e.state === 'fight' && e.timer > 0.4 && inPlace(sim, e));
    const priority = (e: Enemy) => {
        const behind = Math.abs(angleDiff(p.facing, angleOf(sub(e.pos, p.pos)))) > Math.PI / 2 ? 10 : 0;
        return behind - dist(e.pos, p.pos);
    };
    ready.sort((a, b) => priority(b) - priority(a));
    for (const e of ready) {
        if (!requestToken(sim, e)) break;
        start(e);
    }
}

// --- knowing where the player is -------------------------------------------------------------------

/** Everyone within shoutRadius of `caller` (not already fighting) is told where you are. */
export function shout(sim: Sim, caller: Enemy, alert: (e: Enemy) => void): void {
    if (!sim.settings.shout) return;
    for (const e of sim.enemies) {
        if (e === caller || e.state === 'dead' || fighting(e)) continue;
        if (dist(e.pos, caller.pos) <= sim.settings.shoutRadius) alert(e);
    }
}

/** While anyone in the fight sees you, everyone in the fight knows where you are. */
export function shareSighting(sim: Sim, seers: Set<number>): void {
    if (seers.size === 0) return;
    for (const e of sim.enemies) {
        if (!fighting(e)) continue;
        e.lastSeen = { ...sim.player.pos };
        e.unseen = 0;
    }
}
