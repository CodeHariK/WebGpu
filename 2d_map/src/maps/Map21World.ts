// Combat Sandbox — the arena geometry: building a level's runtime Arena from its LevelDef, circles
// against blocks, where shots stop, and line of sight. Everything takes the Arena, because levels
// differ (and tremors move crates).
import type { LevelDef } from './Map21Levels';
import type { Arena, Rect, Vec } from './Map21State';

export const WALL = 1; // the border wall's thickness

const LIFT_HP = 30;
const SUPPLY_HP = 3;
export const ARM_HP = 6; // bubbles to jam a lowered claw
export const OBJECTIVE_R = 0.9; // objectives are drums / cabinets this wide
export const SUPPLY_HALF = 0.45; // supply boxes are squares this half-size

export function makeArena(def: LevelDef): Arena {
    let seed = def.name.length * 9973 + def.w * 31 + def.h;
    const rand = () => {
        seed = (seed * 16807) % 2147483647;
        return seed / 2147483647;
    };
    const decals = Array.from({ length: Math.round(def.w * def.h * 0.09) }, () => ({
        pos: { x: WALL + rand() * (def.w - 2 * WALL), y: WALL + rand() * (def.h - 2 * WALL) },
        r: 0.2 + rand() * 1.2, kind: Math.floor(rand() * 4), rot: rand() * Math.PI * 2,
    }));
    return {
        def, w: def.w, h: def.h,
        cover: def.cover.map((c) => ({ ...c })),
        pools: def.pools.map((p) => ({ pos: { ...p.pos }, r: p.r, grow: 1, glue: p.glue })),
        crushers: def.crushers.map((c) => ({ ...c, t: c.phase })),
        vents: def.vents.map((v) => ({ ...v, t: v.phase, off: false })),
        pipes: def.pipes.map((p) => ({ ...p, hp: 6, broken: false })),
        lifts: def.lifts.map((pos) => ({ pos: { ...pos }, hp: LIFT_HP, broken: false, busy: 0 })),
        core: def.core ? { pos: { ...def.core.pos }, r: def.core.r, hp: def.core.hp, maxHp: def.core.hp, flash: 0 } : null,
        objectives: def.objectives.map((o) => ({ ...o, pos: { ...o.pos }, maxHp: o.hp, broken: false, flash: 0 })),
        doors: def.doors.map((d) => ({ ...d, open: 0 })),
        supplies: [
            ...def.supplies.map((pos) => ({ pos: { ...pos }, hp: SUPPLY_HP, broken: false, flash: 0, trap: false })),
            ...(def.traps ?? []).map((pos) => ({ pos: { ...pos }, hp: SUPPLY_HP, broken: false, flash: 0, trap: true })),
        ],
        exitOpen: false,
        arms: def.arms.map((d) => ({ def: d, x: (d.x0 + d.x1) / 2, dir: 1, state: 'patrol' as const, t: 0, who: null, hp: ARM_HP, flash: 0 })),
        armsOn: true,
        sprayers: (def.sprayers ?? []).map((s) => ({ ...s, t: s.phase, off: false })),
        stamps: (def.stamps ?? []).map((s) => ({ ...s, t: 0, off: false })),
        beltsOn: true,
        tremorT: def.tremor,
        decals,
    };
}

// Where a hider can stand: just off each side of every block (recomputed: crates slide).
export function coverSpots(arena: Arena): { pos: Vec; block: Rect }[] {
    return arena.cover.flatMap((b) => {
        const cx = b.x + b.w / 2;
        const cy = b.y + b.h / 2;
        const gap = 0.75;
        return [
            { pos: { x: cx, y: b.y - gap }, block: b },
            { pos: { x: cx, y: b.y + b.h + gap }, block: b },
            { pos: { x: b.x - gap, y: cy }, block: b },
            { pos: { x: b.x + b.w + gap, y: cy }, block: b },
        ];
    });
}

// Everything solid that is a box: cover, inner walls, shut doors, unbroken supply boxes.
export function solidRects(arena: Arena): { x: number; y: number; w: number; h: number; low: boolean }[] {
    const out: { x: number; y: number; w: number; h: number; low: boolean }[] = [...arena.cover];
    for (const w of arena.def.walls ?? []) out.push({ ...w, low: false });
    for (const d of arena.doors) if (d.open === 0) out.push({ x: d.x, y: d.y, w: d.w, h: d.h, low: false });
    for (const s of arena.supplies) if (!s.broken) out.push({ x: s.pos.x - SUPPLY_HALF, y: s.pos.y - SUPPLY_HALF, w: SUPPLY_HALF * 2, h: SUPPLY_HALF * 2, low: false });
    return out;
}

// Everything solid that is a circle: the drill core and the objectives still standing.
export function solidCircles(arena: Arena): { pos: Vec; r: number }[] {
    const out: { pos: Vec; r: number }[] = [];
    if (arena.core && arena.core.hp > 0) out.push(arena.core);
    for (const o of arena.objectives) if (!o.broken) out.push({ pos: o.pos, r: OBJECTIVE_R });
    return out;
}

// Pushing circles out of boxes.
function pushOutOfRect(pos: Vec, radius: number, b: { x: number; y: number; w: number; h: number }): Vec | null {
    const nx = Math.max(b.x, Math.min(pos.x, b.x + b.w));
    const ny = Math.max(b.y, Math.min(pos.y, b.y + b.h));
    const dx = pos.x - nx;
    const dy = pos.y - ny;
    const d2 = dx * dx + dy * dy;
    if (d2 >= radius * radius) return null;
    const d = Math.sqrt(d2);
    if (d > 1e-4) {
        pos.x = nx + (dx / d) * radius;
        pos.y = ny + (dy / d) * radius;
        return { x: dx / d, y: dy / d };
    }
    const left = pos.x - b.x, right = b.x + b.w - pos.x, top = pos.y - b.y, bottom = b.y + b.h - pos.y;
    const m = Math.min(left, right, top, bottom);
    if (m === left) { pos.x = b.x - radius; return { x: -1, y: 0 }; }
    if (m === right) { pos.x = b.x + b.w + radius; return { x: 1, y: 0 }; }
    if (m === top) { pos.y = b.y - radius; return { x: 0, y: -1 }; }
    pos.y = b.y + b.h + radius;
    return { x: 0, y: 1 };
}

// Push a circle out of everything solid; returns the push direction if it hit (for slams).
export function collide(arena: Arena, pos: Vec, radius: number): Vec | null {
    let hit: Vec | null = null;
    const lo = WALL + radius;
    if (pos.x < lo) { pos.x = lo; hit = { x: 1, y: 0 }; }
    if (pos.x > arena.w - lo) { pos.x = arena.w - lo; hit = { x: -1, y: 0 }; }
    if (pos.y < lo) { pos.y = lo; hit = { x: 0, y: 1 }; }
    if (pos.y > arena.h - lo) { pos.y = arena.h - lo; hit = { x: 0, y: -1 }; }
    for (const b of solidRects(arena)) hit = pushOutOfRect(pos, radius, b) ?? hit;
    for (const c of solidCircles(arena)) {
        const dx = pos.x - c.pos.x;
        const dy = pos.y - c.pos.y;
        const d = Math.hypot(dx, dy);
        if (d < c.r + radius && d > 1e-4) {
            pos.x = c.pos.x + (dx / d) * (c.r + radius);
            pos.y = c.pos.y + (dy / d) * (c.r + radius);
            hit = { x: dx / d, y: dy / d };
        }
    }
    return hit;
}

// Is the point inside a block (or outside the arena)? Shots stop there.
export function solidAt(arena: Arena, p: Vec, lowToo: boolean): boolean {
    if (p.x < WALL || p.y < WALL || p.x > arena.w - WALL || p.y > arena.h - WALL) return true;
    if (solidCircles(arena).some((c) => Math.hypot(p.x - c.pos.x, p.y - c.pos.y) < c.r)) return true;
    return solidRects(arena).some((b) => (lowToo || !b.low) && p.x >= b.x && p.x <= b.x + b.w && p.y >= b.y && p.y <= b.y + b.h);
}

// Can a shot fly from a to b? High blocks stop it; low blocks only when `lowToo`.
export function clearLine(arena: Arena, a: Vec, b: Vec, lowToo = false): boolean {
    const dx = b.x - a.x;
    const dy = b.y - a.y;
    const steps = Math.ceil(Math.hypot(dx, dy) / 0.3);
    for (let i = 1; i < steps; i++) {
        if (solidAt(arena, { x: a.x + (dx * i) / steps, y: a.y + (dy * i) / steps }, lowToo)) return false;
    }
    return true;
}

export const inRect = (p: Vec, r: { x: number; y: number; w: number; h: number }) => p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h;
