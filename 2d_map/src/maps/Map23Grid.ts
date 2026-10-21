// Lockpick — the tile grid: building it from the level, what's solid, what blocks sight, lines of
// sight, tile paths for the guards (they open doors on the way), the light from the torches, and
// pushing a circle out of walls.
import type { LevelDef, Rect } from './Map23Level';
import { CREAK, FLOOR, GLASS, PLATE, type Sim, type Tile, type Vec, WALL, WATER } from './Map23State';

export const idx = (sim: Sim, x: number, y: number) => y * sim.w + x;
export const tileAt = (_sim: Sim, p: Vec) => ({ x: Math.floor(p.x), y: Math.floor(p.y) });
export const centre = (t: Vec) => ({ x: t.x + 0.5, y: t.y + 0.5 });

export function buildTiles(level: LevelDef): Tile[] {
    const tiles: Tile[] = new Array(level.w * level.h).fill(WALL);
    const fill = (rect: Rect, t: Tile) => {
        for (let y = rect.y; y < rect.y + rect.h; y++) for (let x = rect.x; x < rect.x + rect.w; x++) tiles[y * level.w + x] = t;
    };
    for (const rm of [...level.rooms, ...level.halls]) fill(rm, FLOOR);
    for (const d of level.doors) tiles[d.y * level.w + d.x] = FLOOR;
    tiles[level.outside.y * level.w + level.outside.x] = FLOOR;
    for (const c of level.creak) fill(c, CREAK);
    for (const g of level.glass) fill(g, GLASS);
    for (const w of level.water) fill(w, WATER);
    for (const p of level.plates) tiles[p.y * level.w + p.x] = PLATE;
    for (const p of level.pillars) tiles[p.y * level.w + p.x] = WALL;
    return tiles;
}

export const doorAt = (sim: Sim, x: number, y: number) => sim.doors.find((d) => d.x === x && d.y === y) ?? null;
const barrelAt = (sim: Sim, x: number, y: number) => sim.barrels.some((b) => b.x === x && b.y === y);

/** Can't walk through: walls, shut doors, barrels, off the map. */
export function solid(sim: Sim, x: number, y: number): boolean {
    if (x < 0 || y < 0 || x >= sim.w || y >= sim.h) return true;
    if (sim.tiles[idx(sim, x, y)] === WALL) return true;
    const d = doorAt(sim, x, y);
    if (d && !d.open) return true;
    return barrelAt(sim, x, y);
}

/** Blocks sight and light: walls and shut doors (barrels are low: you see over them). */
export function opaque(sim: Sim, x: number, y: number): boolean {
    if (x < 0 || y < 0 || x >= sim.w || y >= sim.h) return true;
    if (sim.tiles[idx(sim, x, y)] === WALL) return true;
    const d = doorAt(sim, x, y);
    return !!d && !d.open;
}

/** A clear line from a to b (no wall or shut door in between). */
export function lineOfSight(sim: Sim, a: Vec, b: Vec): boolean {
    const dx = b.x - a.x, dy = b.y - a.y;
    const n = Math.ceil(Math.hypot(dx, dy) / 0.2);
    for (let i = 1; i < n; i++) {
        const x = Math.floor(a.x + (dx * i) / n), y = Math.floor(a.y + (dy * i) / n);
        if (opaque(sim, x, y)) return false;
    }
    return true;
}

/** The tile path from a to b (tile centres), walking through doors (guards open them). */
export function findPath(sim: Sim, from: Vec, to: Vec): Vec[] {
    const s = tileAt(sim, from), g = tileAt(sim, to);
    const passable = (x: number, y: number) => {
        if (x < 0 || y < 0 || x >= sim.w || y >= sim.h) return false;
        if (sim.tiles[idx(sim, x, y)] === WALL || barrelAt(sim, x, y)) return false;
        const d = doorAt(sim, x, y);
        return !d || !d.lock || d.lock !== 'gate'; // they can't open the way out
    };
    const start = idx(sim, s.x, s.y), goal = idx(sim, g.x, g.y);
    const prev = new Map<number, number>([[start, -1]]);
    const queue = [start];
    while (queue.length) {
        const k = queue.shift()!;
        if (k === goal) break;
        const x = k % sim.w, y = Math.floor(k / sim.w);
        for (const [ox, oy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
            const nx = x + ox, ny = y + oy;
            const nk = idx(sim, nx, ny);
            if (prev.has(nk) || !passable(nx, ny)) continue;
            prev.set(nk, k);
            queue.push(nk);
        }
    }
    if (!prev.has(goal)) return [];
    const out: Vec[] = [];
    for (let k = goal; k !== -1 && k !== start; k = prev.get(k)!) out.push(centre({ x: k % sim.w, y: Math.floor(k / sim.w) }));
    return out.reverse();
}

/** Light per tile from the lit torches (blocked by walls and shut doors), plus a little ambient. */
export function computeLight(sim: Sim): number[] {
    const out = new Array(sim.w * sim.h).fill(0.05);
    const R = sim.settings.torchRadius;
    for (const t of sim.torches) {
        if (!t.lit) continue;
        for (let y = Math.floor(t.light.y - R); y <= t.light.y + R; y++) {
            for (let x = Math.floor(t.light.x - R); x <= t.light.x + R; x++) {
                if (x < 0 || y < 0 || x >= sim.w || y >= sim.h || sim.tiles[idx(sim, x, y)] === WALL) continue;
                const c = centre({ x, y });
                const d = Math.hypot(c.x - t.light.x, c.y - t.light.y);
                if (d > R || !lineOfSight(sim, t.light, c)) continue;
                const k = idx(sim, x, y);
                out[k] = Math.max(out[k], 1 - (d / R) ** 1.5);
            }
        }
    }
    return out;
}

export const lightAt = (sim: Sim, p: Vec) => {
    const t = tileAt(sim, p);
    return t.x < 0 || t.y < 0 || t.x >= sim.w || t.y >= sim.h ? 0 : sim.light[idx(sim, t.x, t.y)] ?? 0;
};

export const floorAt = (sim: Sim, p: Vec): Tile => {
    const t = tileAt(sim, p);
    return t.x < 0 || t.y < 0 || t.x >= sim.w || t.y >= sim.h ? WALL : sim.tiles[idx(sim, t.x, t.y)];
};

/** Push a circle out of solid tiles. */
export function collide(sim: Sim, pos: Vec, r: number): void {
    const tx = Math.floor(pos.x), ty = Math.floor(pos.y);
    for (let y = ty - 1; y <= ty + 1; y++) {
        for (let x = tx - 1; x <= tx + 1; x++) {
            if (!solid(sim, x, y)) continue;
            const nx = Math.max(x, Math.min(pos.x, x + 1)), ny = Math.max(y, Math.min(pos.y, y + 1));
            const dx = pos.x - nx, dy = pos.y - ny;
            const d = Math.hypot(dx, dy);
            if (d >= r) continue;
            if (d > 1e-4) { pos.x = nx + (dx / d) * r; pos.y = ny + (dy / d) * r; }
            else pos.y = y - r;
        }
    }
}

