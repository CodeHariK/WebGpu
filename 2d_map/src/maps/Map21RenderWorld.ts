// Combat Sandbox — drawing the place: floor plates and grime, the factory wall with its pipes and
// lamps (the Assembly Plant swaps those three for its own look: Map21RenderPlant), pools, belts, crushers, vents, sludge pipes, lifts, the drill core, cover, lamp light and
// the gas. Pure drawing in world metres (the caller sets the camera transform).
import { crusherState, ventState, VENT_WARN } from './Map21Hazards';
import { drawPlantFloor, drawPlantLamps, drawPlantWalls } from './Map21RenderPlant';
import type { Arena, Sim } from './Map21State';
import { THEMES } from './Map21Themes';
import { WALL } from './Map21World';

const HAZARD = '#facc15';
const isPlant = (sim: Sim) => THEMES[sim.theme].id === 'plant';

export function drawFloor(ctx: CanvasRenderingContext2D, sim: Sim): void {
    if (isPlant(sim)) { drawPlantFloor(ctx, sim); return; }
    const a = sim.arena;
    const t = sim.time;
    ctx.fillStyle = '#0b0f0e';
    ctx.fillRect(-40, -40, a.w + 80, a.h + 80);
    ctx.fillStyle = a.def.floor;
    ctx.fillRect(WALL, WALL, a.w - 2 * WALL, a.h - 2 * WALL);
    // Floor plates: 2 m squares with a faint bevel and rivets at the corners.
    for (let x = WALL; x < a.w - WALL; x += 2) {
        for (let y = WALL; y < a.h - WALL; y += 2) {
            const shade = ((x * 7 + y * 13) % 5) * 0.006;
            ctx.fillStyle = `rgba(255,255,255,${0.012 + shade})`;
            ctx.fillRect(x + 0.04, y + 0.04, 1.92, 1.92);
            ctx.fillStyle = 'rgba(0,0,0,0.25)';
            ctx.fillRect(x, y + 1.94, 2, 0.06);
            ctx.fillRect(x + 1.94, y, 0.06, 2);
            ctx.fillStyle = 'rgba(255,255,255,0.08)';
            for (const [rx, ry] of [[0.2, 0.2], [1.8, 0.2], [0.2, 1.8], [1.8, 1.8]]) ctx.fillRect(x + rx - 0.04, y + ry - 0.04, 0.08, 0.08);
        }
    }
    drawDecals(ctx, a);
    // Hazard stripes along the top edge, where toys come in.
    ctx.save();
    ctx.beginPath();
    ctx.rect(WALL, WALL, a.w - 2 * WALL, 0.5);
    ctx.clip();
    ctx.fillStyle = HAZARD;
    for (let x = -2 + ((t * 0.5) % 1.2); x < a.w; x += 1.2) {
        ctx.beginPath();
        ctx.moveTo(x, WALL + 0.5); ctx.lineTo(x + 0.5, WALL); ctx.lineTo(x + 0.9, WALL); ctx.lineTo(x + 0.4, WALL + 0.5);
        ctx.fill();
    }
    ctx.restore();
}

// Oil stains, scorch marks, cracks and bolts, placed once per level (Arena.decals).
function drawDecals(ctx: CanvasRenderingContext2D, a: Arena): void {
    for (const d of a.decals) {
        ctx.save();
        ctx.translate(d.pos.x, d.pos.y);
        ctx.rotate(d.rot);
        if (d.kind === 0) { // oil stain
            ctx.fillStyle = 'rgba(0,0,0,0.18)';
            ctx.beginPath(); ctx.ellipse(0, 0, d.r, d.r * 0.6, 0, 0, Math.PI * 2); ctx.fill();
        } else if (d.kind === 1) { // scorch
            const g = ctx.createRadialGradient(0, 0, 0, 0, 0, d.r * 0.8);
            g.addColorStop(0, 'rgba(10,10,10,0.3)');
            g.addColorStop(1, 'rgba(10,10,10,0)');
            ctx.fillStyle = g;
            ctx.beginPath(); ctx.arc(0, 0, d.r * 0.8, 0, Math.PI * 2); ctx.fill();
        } else if (d.kind === 2) { // crack
            ctx.strokeStyle = 'rgba(0,0,0,0.3)';
            ctx.lineWidth = 0.04;
            ctx.beginPath(); ctx.moveTo(-d.r, 0); ctx.lineTo(-d.r * 0.3, 0.15); ctx.lineTo(d.r * 0.2, -0.1); ctx.lineTo(d.r, 0.08); ctx.stroke();
        } else { // a dropped bolt
            ctx.fillStyle = 'rgba(148,163,184,0.35)';
            ctx.fillRect(-0.12, -0.05, 0.24, 0.1);
        }
        ctx.restore();
    }
}

// The outer wall: dark blocks with pipes along it and caged lamps at intervals.
export function drawWalls(ctx: CanvasRenderingContext2D, sim: Sim): void {
    if (isPlant(sim)) { drawPlantWalls(ctx, sim); return; }
    const a = sim.arena;
    ctx.fillStyle = '#141a19';
    ctx.fillRect(0, 0, a.w, WALL);
    ctx.fillRect(0, a.h - WALL, a.w, WALL);
    ctx.fillRect(0, 0, WALL, a.h);
    ctx.fillRect(a.w - WALL, 0, WALL, a.h);
    ctx.fillStyle = '#2b3433'; // a lighter lip on the inner edge
    ctx.fillRect(WALL - 0.12, WALL - 0.12, a.w - 2 * WALL + 0.24, 0.12);
    ctx.fillRect(WALL - 0.12, WALL, 0.12, a.h - 2 * WALL);
    ctx.fillRect(a.w - WALL, WALL, 0.12, a.h - 2 * WALL);
    // Two pipes running along the top wall.
    for (const [y, c] of [[0.3, '#3f4a48'], [0.62, '#4b3a2c']] as const) {
        ctx.fillStyle = c;
        ctx.fillRect(0, y, a.w, 0.22);
        ctx.fillStyle = 'rgba(255,255,255,0.12)';
        ctx.fillRect(0, y + 0.03, a.w, 0.04);
        for (let x = 2; x < a.w; x += 4) { ctx.fillStyle = '#1f2625'; ctx.fillRect(x, y - 0.03, 0.18, 0.28); }
    }
}

// Lamp light: soft warm pools along the side walls (drawn with 'lighter' so they add up).
export function drawLamps(ctx: CanvasRenderingContext2D, sim: Sim): void {
    if (isPlant(sim)) { drawPlantLamps(ctx, sim); return; }
    const a = sim.arena;
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    for (let y = 5; y < a.h - 2; y += 8) {
        for (const x of [WALL + 0.3, a.w - WALL - 0.3]) {
            const flicker = 0.85 + 0.15 * Math.sin(sim.time * 13 + y * 3 + x);
            const g = ctx.createRadialGradient(x, y, 0, x, y, 6);
            g.addColorStop(0, `rgba(255,200,120,${0.18 * flicker})`);
            g.addColorStop(1, 'rgba(255,200,120,0)');
            ctx.fillStyle = g;
            ctx.beginPath(); ctx.arc(x, y, 6, 0, Math.PI * 2); ctx.fill();
        }
    }
    ctx.restore();
    for (let y = 5; y < a.h - 2; y += 8) { // the lamp cages themselves
        for (const x of [WALL - 0.15, a.w - WALL - 0.15]) {
            ctx.fillStyle = '#fde68a';
            ctx.fillRect(x, y - 0.25, 0.3, 0.5);
            ctx.strokeStyle = '#1c1917';
            ctx.lineWidth = 0.05;
            ctx.strokeRect(x, y - 0.25, 0.3, 0.5);
        }
    }
}

// Glue: a thick cream puddle with a glossy highlight and strings stretching off whoever's in it.
function drawGlue(ctx: CanvasRenderingContext2D, sim: Sim, c: { x: number; y: number }, r: number): void {
    ctx.fillStyle = 'rgba(254,243,199,0.75)';
    ctx.beginPath();
    for (let i = 0; i <= 20; i++) {
        const a = (i / 20) * Math.PI * 2;
        const rr = r * (1 + 0.12 * Math.sin(a * 3 + c.x));
        if (i === 0) ctx.moveTo(c.x + Math.cos(a) * rr, c.y + Math.sin(a) * rr);
        else ctx.lineTo(c.x + Math.cos(a) * rr, c.y + Math.sin(a) * rr);
    }
    ctx.fill();
    ctx.strokeStyle = 'rgba(217,119,6,0.45)';
    ctx.lineWidth = 0.06;
    ctx.stroke();
    ctx.fillStyle = 'rgba(255,255,255,0.55)';
    ctx.beginPath(); ctx.ellipse(c.x - r * 0.3, c.y - r * 0.35, r * 0.35, r * 0.12, -0.4, 0, Math.PI * 2); ctx.fill();
    const stuck = [sim.player.pos, ...sim.enemies.map((e) => e.pos)].filter((p) => Math.hypot(p.x - c.x, p.y - c.y) < r);
    ctx.strokeStyle = 'rgba(254,243,199,0.8)';
    ctx.lineWidth = 0.04;
    for (const p of stuck) { // strings
        for (let i = 0; i < 3; i++) {
            const a = sim.time * 0.5 + i * 2.1;
            ctx.beginPath(); ctx.moveTo(p.x, p.y); ctx.lineTo(p.x + Math.cos(a) * 0.6, p.y + 0.2 + Math.sin(a) * 0.3); ctx.stroke();
        }
    }
}

export function drawPools(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const t = sim.time;
    for (const pool of sim.arena.pools) {
        const r = pool.r * pool.grow;
        if (r < 0.05) continue;
        if (pool.glue) { drawGlue(ctx, sim, pool.pos, r); continue; }
        const g = ctx.createRadialGradient(pool.pos.x, pool.pos.y, 0.1, pool.pos.x, pool.pos.y, r * 1.6);
        g.addColorStop(0, 'rgba(190,242,100,0.95)');
        g.addColorStop(0.6, 'rgba(132,204,22,0.75)');
        g.addColorStop(1, 'rgba(132,204,22,0)');
        ctx.fillStyle = g;
        ctx.beginPath(); ctx.arc(pool.pos.x, pool.pos.y, r * 1.6, 0, Math.PI * 2); ctx.fill();
        for (let i = 0; i < 3; i++) {
            const a = t * 1.3 + i * 2.1 + pool.pos.x;
            const rr = ((t * 0.7 + i * 0.33) % 1) * 0.25;
            ctx.strokeStyle = 'rgba(236,252,203,0.7)';
            ctx.lineWidth = 0.04;
            ctx.beginPath(); ctx.arc(pool.pos.x + Math.cos(a) * r * 0.5, pool.pos.y + Math.sin(a) * r * 0.5, rr, 0, Math.PI * 2); ctx.stroke();
        }
    }
}

// Belts: dark rubber with chevrons scrolling the way they carry.
export function drawBelts(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const c of sim.arena.def.conveyors) {
        ctx.fillStyle = '#181b1e';
        ctx.fillRect(c.x, c.y, c.w, c.h);
        ctx.fillStyle = '#2d3238';
        ctx.fillRect(c.x, c.y, c.w, 0.18);
        ctx.fillRect(c.x, c.y + c.h - 0.18, c.w, 0.18);
        ctx.save();
        ctx.beginPath(); ctx.rect(c.x, c.y + 0.2, c.w, c.h - 0.4); ctx.clip();
        const along = c.dir.x !== 0;
        const len = along ? c.w : c.h;
        const shift = sim.arena.beltsOn ? (sim.time * c.speed) % 1.5 : 0;
        ctx.strokeStyle = sim.arena.beltsOn ? 'rgba(250,204,21,0.35)' : 'rgba(120,120,120,0.2)';
        ctx.lineWidth = 0.12;
        for (let s = -1.5; s < len + 1.5; s += 1.5) {
            const o = s + shift * (along ? Math.sign(c.dir.x) : Math.sign(c.dir.y));
            ctx.beginPath();
            if (along) {
                const x = c.x + o;
                const tip = Math.sign(c.dir.x) * 0.5;
                ctx.moveTo(x, c.y + 0.6); ctx.lineTo(x + tip, c.y + c.h / 2); ctx.lineTo(x, c.y + c.h - 0.6);
            } else {
                const y = c.y + o;
                const tip = Math.sign(c.dir.y) * 0.5;
                ctx.moveTo(c.x + 0.6, y); ctx.lineTo(c.x + c.w / 2, y + tip); ctx.lineTo(c.x + c.w - 0.6, y);
            }
            ctx.stroke();
        }
        ctx.restore();
    }
}

// Crushers: a heavy block hanging over a pit plate; red warning, then down.
export function drawCrushers(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const c of sim.arena.crushers) {
        const state = crusherState(c);
        ctx.fillStyle = state === 'warn' ? `rgba(239,68,68,${0.25 + 0.25 * Math.sin(sim.time * 30)})` : 'rgba(0,0,0,0.3)';
        ctx.fillRect(c.x, c.y, c.w, c.h);
        ctx.strokeStyle = HAZARD;
        ctx.lineWidth = 0.1;
        ctx.setLineDash([0.3, 0.2]);
        ctx.strokeRect(c.x + 0.05, c.y + 0.05, c.w - 0.1, c.h - 0.1);
        ctx.setLineDash([]);
        const lift = state === 'slam' ? 0 : state === 'warn' ? 0.5 : 1.2; // how high the block hangs
        ctx.fillStyle = 'rgba(0,0,0,0.35)';
        ctx.fillRect(c.x + 0.2 + lift * 0.3, c.y + 0.3 + lift * 0.6, c.w - 0.4, c.h - 0.4);
        ctx.fillStyle = state === 'slam' ? '#94a3b8' : '#64748b';
        ctx.fillRect(c.x + 0.2, c.y + 0.2 - lift, c.w - 0.4, c.h - 0.4);
        ctx.fillStyle = '#475569';
        ctx.fillRect(c.x + 0.2, c.y + c.h - 0.5 - lift, c.w - 0.4, 0.3);
    }
}

export function drawVents(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const v of sim.arena.vents) {
        const state = ventState(v);
        ctx.fillStyle = '#111514';
        ctx.beginPath(); ctx.arc(v.pos.x, v.pos.y, 0.8, 0, Math.PI * 2); ctx.fill();
        ctx.strokeStyle = '#3f4a48';
        ctx.lineWidth = 0.08;
        for (let i = -2; i <= 2; i++) { // grate slats
            ctx.beginPath(); ctx.moveTo(v.pos.x - 0.6, v.pos.y + i * 0.25); ctx.lineTo(v.pos.x + 0.6, v.pos.y + i * 0.25); ctx.stroke();
        }
        if (v.off) { // its valve is broken: capped
            ctx.strokeStyle = '#57534e';
            ctx.lineWidth = 0.14;
            ctx.beginPath(); ctx.arc(v.pos.x, v.pos.y, 0.8, 0, Math.PI * 2); ctx.stroke();
            continue;
        }
        if (state === 'warn') {
            const k = 1 - ((v.t % v.period) - (v.period - VENT_WARN)) / VENT_WARN;
            ctx.strokeStyle = `rgba(163,230,53,${0.4 + 0.4 * Math.sin(sim.time * 25)})`;
            ctx.lineWidth = 0.08;
            ctx.beginPath(); ctx.arc(v.pos.x, v.pos.y, v.r * (0.4 + 0.6 * (1 - k)), 0, Math.PI * 2); ctx.stroke();
        } else if (state === 'burst') {
            const g = ctx.createRadialGradient(v.pos.x, v.pos.y, 0, v.pos.x, v.pos.y, v.r);
            g.addColorStop(0, 'rgba(190,242,100,0.7)');
            g.addColorStop(1, 'rgba(132,204,22,0.1)');
            ctx.fillStyle = g;
            ctx.beginPath(); ctx.arc(v.pos.x, v.pos.y, v.r, 0, Math.PI * 2); ctx.fill();
        }
    }
}

// Sludge pipes along the walls: thick green-brown tubes with joints; a dripping crack once hit.
export function drawPipes(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const p of sim.arena.pipes) {
        ctx.strokeStyle = p.glue ? (p.broken ? '#57534e' : '#d6b25e') : p.broken ? '#3f3f2a' : '#5b6b2a';
        ctx.lineWidth = 0.55;
        ctx.lineCap = 'round';
        ctx.beginPath(); ctx.moveTo(p.a.x, p.a.y); ctx.lineTo(p.b.x, p.b.y); ctx.stroke();
        ctx.strokeStyle = 'rgba(255,255,255,0.15)';
        ctx.lineWidth = 0.1;
        ctx.beginPath(); ctx.moveTo(p.a.x - 0.1, p.a.y - 0.1); ctx.lineTo(p.b.x - 0.1, p.b.y - 0.1); ctx.stroke();
        ctx.lineCap = 'butt';
        const len = Math.hypot(p.b.x - p.a.x, p.b.y - p.a.y);
        for (let s = 0; s <= len; s += 2) { // joints
            const k = s / len;
            ctx.fillStyle = '#2f3a17';
            ctx.beginPath(); ctx.arc(p.a.x + (p.b.x - p.a.x) * k, p.a.y + (p.b.y - p.a.y) * k, 0.36, 0, Math.PI * 2); ctx.fill();
        }
        // A target marker so it reads as shootable.
        const mid = { x: (p.a.x + p.b.x) / 2, y: (p.a.y + p.b.y) / 2 };
        if (!p.broken) {
            ctx.strokeStyle = p.glue ? `rgba(254,243,199,${0.5 + 0.3 * Math.sin(sim.time * 4)})` : `rgba(190,242,100,${0.5 + 0.3 * Math.sin(sim.time * 4)})`;
            ctx.lineWidth = 0.06;
            ctx.beginPath(); ctx.arc(mid.x, mid.y, 0.55, 0, Math.PI * 2); ctx.stroke();
        } else {
            ctx.strokeStyle = p.glue ? 'rgba(254,243,199,0.7)' : 'rgba(132,204,22,0.7)'; // the gush toward the spill point
            ctx.lineWidth = 0.3;
            ctx.beginPath(); ctx.moveTo(mid.x, mid.y); ctx.lineTo(p.spill.x, p.spill.y); ctx.stroke();
        }
    }
}

// Lifts: a platform with chevrons, a frame and a health bar; wrecked: a scorched hole.
export function drawLifts(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const l of sim.arena.lifts) {
        const x = l.pos.x - 1.6;
        const y = l.pos.y - 1.2;
        if (l.broken) {
            ctx.fillStyle = '#0a0a0a';
            ctx.fillRect(x, y, 3.2, 2.4);
            ctx.strokeStyle = '#7c2d12';
            ctx.lineWidth = 0.08;
            ctx.strokeRect(x, y, 3.2, 2.4);
            continue;
        }
        ctx.fillStyle = l.busy > 0 ? '#52525b' : '#3f3f46';
        ctx.fillRect(x, y, 3.2, 2.4);
        ctx.strokeStyle = HAZARD;
        ctx.lineWidth = 0.12;
        ctx.strokeRect(x + 0.06, y + 0.06, 3.08, 2.28);
        ctx.strokeStyle = 'rgba(250,204,21,0.5)';
        ctx.lineWidth = 0.1;
        for (let i = 0; i < 3; i++) { // chevrons pointing down: they come out here
            const cy = y + 0.6 + i * 0.5 + (l.busy > 0 ? ((sim.time * 4) % 0.5) : 0);
            ctx.beginPath(); ctx.moveTo(l.pos.x - 0.6, cy); ctx.lineTo(l.pos.x, cy + 0.35); ctx.lineTo(l.pos.x + 0.6, cy); ctx.stroke();
        }
        if (l.hp < 30) {
            ctx.fillStyle = 'rgba(0,0,0,0.5)';
            ctx.fillRect(x, y - 0.35, 3.2, 0.15);
            ctx.fillStyle = '#f87171';
            ctx.fillRect(x, y - 0.35, (3.2 * l.hp) / 30, 0.15);
        }
    }
}

// The drill core: a big round machine with a spinning bit and glowing vents.
export function drawCore(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const c = sim.arena.core;
    if (!c) return;
    if (c.hp <= 0) {
        ctx.fillStyle = '#0a0a0a';
        ctx.beginPath(); ctx.arc(c.pos.x, c.pos.y, c.r * 0.9, 0, Math.PI * 2); ctx.fill();
        return;
    }
    const white = c.flash > 0;
    ctx.fillStyle = 'rgba(0,0,0,0.4)';
    ctx.beginPath(); ctx.arc(c.pos.x + 0.3, c.pos.y + 0.5, c.r, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = white ? '#fff' : '#57534e';
    ctx.beginPath(); ctx.arc(c.pos.x, c.pos.y, c.r, 0, Math.PI * 2); ctx.fill();
    ctx.strokeStyle = HAZARD;
    ctx.lineWidth = 0.18;
    ctx.setLineDash([0.4, 0.3]);
    ctx.beginPath(); ctx.arc(c.pos.x, c.pos.y, c.r - 0.2, 0, Math.PI * 2); ctx.stroke();
    ctx.setLineDash([]);
    const spin = sim.time * 3;
    ctx.fillStyle = white ? '#fff' : '#a8a29e';
    for (let i = 0; i < 3; i++) { // the bit's blades
        const a = spin + (i * Math.PI * 2) / 3;
        ctx.beginPath();
        ctx.moveTo(c.pos.x, c.pos.y);
        ctx.lineTo(c.pos.x + Math.cos(a) * c.r * 0.8, c.pos.y + Math.sin(a) * c.r * 0.8);
        ctx.lineTo(c.pos.x + Math.cos(a + 0.5) * c.r * 0.6, c.pos.y + Math.sin(a + 0.5) * c.r * 0.6);
        ctx.fill();
    }
    const heat = 1 - c.hp / c.maxHp;
    ctx.fillStyle = `rgb(${200 + heat * 55},${120 - heat * 80},40)`;
    ctx.beginPath(); ctx.arc(c.pos.x, c.pos.y, 0.5 + heat * 0.4, 0, Math.PI * 2); ctx.fill();
    // Its health, as a ring.
    ctx.strokeStyle = 'rgba(0,0,0,0.5)';
    ctx.lineWidth = 0.25;
    ctx.beginPath(); ctx.arc(c.pos.x, c.pos.y, c.r + 0.4, 0, Math.PI * 2); ctx.stroke();
    ctx.strokeStyle = '#f87171';
    ctx.beginPath(); ctx.arc(c.pos.x, c.pos.y, c.r + 0.4, -Math.PI / 2, -Math.PI / 2 + (Math.PI * 2 * c.hp) / c.maxHp); ctx.stroke();
}

// Cover colours per theme: the factory's orange crates and steel pillars; the plant's teal plastic
// bins, cardboard parts crates (the ones riding the belts) and pale machine cabinets.
function coverColors(sim: Sim, b: Arena['cover'][number]): { side: string; top: string; stripe: string } {
    if (!isPlant(sim)) return b.low ? { side: '#b45309', top: '#f59e0b', stripe: HAZARD } : { side: '#64748b', top: '#94a3b8', stripe: HAZARD };
    if (b.rides) return { side: '#8a5a2b', top: '#d6a35a', stripe: HAZARD };
    return b.low ? { side: '#0e7490', top: '#2dd4bf', stripe: HAZARD } : { side: '#6b6577', top: '#d4d0dc', stripe: '#f0abfc' };
}

// Blocks as little extruded boxes: a front face below a lighter top.
export function drawCover(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const b of sim.arena.cover) {
        const height = b.low ? 0.35 : 0.8;
        ctx.fillStyle = 'rgba(0,0,0,0.35)';
        ctx.fillRect(b.x + 0.2, b.y + 0.3, b.w, b.h);
        const c = coverColors(sim, b);
        ctx.fillStyle = c.side;
        ctx.fillRect(b.x, b.y, b.w, b.h);
        ctx.fillStyle = c.top;
        ctx.fillRect(b.x, b.y - height, b.w, b.h);
        if (!b.low) {
            ctx.fillStyle = c.stripe;
            ctx.fillRect(b.x, b.y + b.h - height - 0.12, b.w, 0.1);
        } else {
            ctx.strokeStyle = 'rgba(120,53,15,0.8)';
            ctx.lineWidth = 0.05;
            ctx.strokeRect(b.x + 0.08, b.y - height + 0.08, b.w - 0.16, b.h - 0.16);
            if (b.slides) { // sliding crates have wheels showing
                ctx.fillStyle = '#1c1917';
                ctx.fillRect(b.x + 0.15, b.y + b.h - 0.1, 0.25, 0.12);
                ctx.fillRect(b.x + b.w - 0.4, b.y + b.h - 0.1, 0.25, 0.12);
            }
        }
    }
}

export function drawGas(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const a = sim.arena;
    const t = sim.time;
    if (sim.gasY >= a.h) return;
    ctx.fillStyle = 'rgba(132,204,22,0.28)';
    ctx.beginPath();
    ctx.moveTo(-2, a.h + 2);
    for (let x = -2; x <= a.w + 2; x += 0.5) ctx.lineTo(x, sim.gasY + Math.sin(x * 0.8 + t * 2) * 0.35 + Math.sin(x * 0.3 - t) * 0.3);
    ctx.lineTo(a.w + 2, a.h + 2);
    ctx.fill();
    ctx.fillStyle = 'rgba(190,242,100,0.18)';
    for (let i = 0; i < 14; i++) {
        const x = (i * 2.9 + t * 0.6) % a.w;
        ctx.beginPath(); ctx.arc(x, sim.gasY + 1 + (i % 3), 1 + (i % 2) * 0.6, 0, Math.PI * 2); ctx.fill();
    }
}
