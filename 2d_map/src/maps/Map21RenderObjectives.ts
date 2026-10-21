// Combat Sandbox — drawing the objectives (the machines you break), locked doors, supply crates,
// pickups on the floor and the exit gate. Pure drawing: reads the Sim, changes nothing.
import { coreShielded, KEY_COLOR } from './Map21Objectives';
import type { Arena, Pickup, Sim, Vec } from './Map21State';
import { OBJECTIVE_R, SUPPLY_HALF } from './Map21World';

type Objective = Arena['objectives'][number];
const TAU = Math.PI * 2;

function disc(ctx: CanvasRenderingContext2D, x: number, y: number, r: number, color: string): void {
    ctx.fillStyle = color;
    ctx.beginPath(); ctx.arc(x, y, r, 0, TAU); ctx.fill();
}

// --- the exit --------------------------------------------------------------------------------

export function drawExit(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const a = sim.arena;
    const e = a.def.exit;
    const open = a.exitOpen;
    const t = sim.time;
    ctx.fillStyle = open ? '#052e16' : '#1c1917';
    ctx.fillRect(e.x, e.y, e.w, e.h);
    const vertical = e.h > e.w; // in a side wall
    const inward = vertical ? (e.x < a.w / 2 ? 1 : -1) : (e.y < a.h / 2 ? 1 : -1); // toward the floor
    if (open) {
        const g = 0.35 + 0.25 * Math.sin(t * 5);
        ctx.fillStyle = `rgba(74,222,128,${g})`;
        if (vertical) ctx.fillRect(e.x + (inward > 0 ? e.w : -1.6), e.y, 1.6, e.h);
        else ctx.fillRect(e.x, e.y + (inward > 0 ? e.h : -1.6), e.w, 1.6);
        ctx.strokeStyle = '#86efac';
        ctx.lineWidth = 0.14;
        for (let i = 0; i < 3; i++) { // chevrons pointing out of the arena
            const s = ((t * 1.5 + i / 3) % 1) * 1.6;
            ctx.beginPath();
            if (vertical) {
                const x = e.x + (inward > 0 ? e.w + 1.6 - s : -1.6 + s);
                ctx.moveTo(x, e.y + 0.6); ctx.lineTo(x - inward * 0.45, e.y + e.h / 2); ctx.lineTo(x, e.y + e.h - 0.6);
            } else {
                const y = e.y + (inward > 0 ? e.h + 1.6 - s : -1.6 + s);
                ctx.moveTo(e.x + 0.6, y); ctx.lineTo(e.x + e.w / 2, y - inward * 0.45); ctx.lineTo(e.x + e.w - 0.6, y);
            }
            ctx.stroke();
        }
    } else { // a shutter with hazard stripes and a red lamp
        ctx.save();
        ctx.beginPath(); ctx.rect(e.x, e.y, e.w, e.h); ctx.clip();
        ctx.strokeStyle = '#facc15';
        ctx.lineWidth = 0.22;
        for (let s = -4; s < 8; s += 0.7) {
            ctx.beginPath(); ctx.moveTo(e.x + s, e.y); ctx.lineTo(e.x + s + 1, e.y + e.h + 1); ctx.stroke();
        }
        ctx.restore();
        ctx.strokeStyle = '#44403c';
        ctx.lineWidth = 0.1;
        ctx.strokeRect(e.x, e.y, e.w, e.h);
    }
    const lamp = { x: e.x + e.w / 2, y: e.y + e.h / 2 };
    disc(ctx, lamp.x, lamp.y, 0.22, open ? '#4ade80' : Math.sin(t * 4) > 0 ? '#ef4444' : '#7f1d1d');
    ctx.textAlign = 'center';
    ctx.font = '800 0.5px Inter, sans-serif';
    ctx.fillStyle = open ? '#bbf7d0' : 'rgba(254,202,202,0.8)';
    const lx = vertical ? e.x + (inward > 0 ? e.w + 1.1 : -1.1) : lamp.x;
    const ly = vertical ? lamp.y - e.h / 2 - 0.3 : e.y + (inward > 0 ? e.h + 0.8 : -0.4);
    ctx.fillText('EXIT', lx, ly);
}

// --- doors -----------------------------------------------------------------------------------

export function drawDoors(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const d of sim.arena.doors) {
        if (d.open >= 1) {
            ctx.fillStyle = 'rgba(0,0,0,0.35)';
            ctx.fillRect(d.x, d.y, d.w, d.h); // the empty track
            continue;
        }
        const col = KEY_COLOR[d.key];
        const w = d.w * (1 - d.open); // it slides away to the left
        ctx.fillStyle = 'rgba(0,0,0,0.4)';
        ctx.fillRect(d.x + 0.15, d.y + 0.25, w, d.h);
        ctx.fillStyle = '#334155';
        ctx.fillRect(d.x, d.y - 0.6, w, d.h + 0.6);
        ctx.fillStyle = col;
        ctx.fillRect(d.x, d.y - 0.6, w, 0.18);
        ctx.fillRect(d.x, d.y + d.h - 0.18, w, 0.18);
        if (d.open > 0) continue;
        const c = { x: d.x + d.w / 2, y: d.y - 0.1 }; // the lock
        disc(ctx, c.x, c.y, 0.42, '#0f172a');
        ctx.strokeStyle = col;
        ctx.lineWidth = 0.09;
        ctx.beginPath(); ctx.arc(c.x, c.y - 0.08, 0.15, Math.PI, 0); ctx.stroke();
        ctx.fillStyle = col;
        ctx.fillRect(c.x - 0.2, c.y - 0.08, 0.4, 0.3);
        if (sim.player.keys.includes(d.key)) { // you have the key: it glows
            ctx.strokeStyle = `rgba(255,255,255,${0.4 + 0.4 * Math.sin(sim.time * 6)})`;
            ctx.lineWidth = 0.07;
            ctx.strokeRect(d.x - 0.1, d.y - 0.7, d.w + 0.2, d.h + 0.8);
        }
    }
}

// --- supply crates ---------------------------------------------------------------------------

export function drawSupplies(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const h = SUPPLY_HALF;
    for (const s of sim.arena.supplies) {
        // A trap crate rattles now and then: something is in there.
        const rattle = s.trap && !s.broken && (sim.time + s.pos.x * 0.7) % 2.6 < 0.35 ? Math.sin(sim.time * 70) * 0.06 : 0;
        const x = s.pos.x + rattle;
        const y = s.pos.y;
        if (s.broken) {
            ctx.fillStyle = 'rgba(80,55,30,0.6)';
            for (let i = 0; i < 4; i++) ctx.fillRect(x - h + (i % 2) * 0.5, y - h * 0.4 + Math.floor(i / 2) * 0.35, 0.35, 0.1);
            continue;
        }
        const white = s.flash > 0;
        ctx.fillStyle = 'rgba(0,0,0,0.35)';
        ctx.fillRect(x - h + 0.15, y - h + 0.25, h * 2, h * 2);
        ctx.fillStyle = white ? '#fff' : '#7c5a32';
        ctx.fillRect(x - h, y - h, h * 2, h * 2);
        ctx.fillStyle = white ? '#fff' : '#b08850';
        ctx.fillRect(x - h, y - h - 0.35, h * 2, h * 2);
        ctx.strokeStyle = '#5b4023';
        ctx.lineWidth = 0.07;
        ctx.strokeRect(x - h + 0.06, y - h - 0.29, h * 2 - 0.12, h * 2 - 0.12);
        ctx.beginPath(); ctx.moveTo(x - h + 0.08, y - h - 0.27); ctx.lineTo(x + h - 0.08, y + h - 0.43); ctx.stroke();
        ctx.fillStyle = '#e0f2fe'; // a little "+" sticker: there's something inside
        ctx.fillRect(x - 0.05, y - 0.5, 0.1, 0.3);
        ctx.fillRect(x - 0.15, y - 0.4, 0.3, 0.1);
    }
}

// --- objectives ------------------------------------------------------------------------------

export function drawObjectives(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const a = sim.arena;
    // Shield pumps feed the core: blue beams while they run.
    if (a.core && a.core.hp > 0 && coreShielded(a)) {
        for (const o of a.objectives) {
            if (o.stops !== 'shield' || o.broken) continue;
            ctx.strokeStyle = `rgba(96,165,250,${0.35 + 0.2 * Math.sin(sim.time * 8 + o.pos.x)})`;
            ctx.lineWidth = 0.18;
            ctx.setLineDash([0.5, 0.3]);
            ctx.lineDashOffset = -sim.time * 3;
            ctx.beginPath(); ctx.moveTo(o.pos.x, o.pos.y); ctx.lineTo(a.core.pos.x, a.core.pos.y); ctx.stroke();
            ctx.setLineDash([]);
            ctx.lineDashOffset = 0;
        }
    }
    for (const o of a.objectives) {
        if (o.broken) { drawWreck(ctx, sim, o.pos); continue; }
        drawObjective(ctx, sim, o);
    }
}

/** The drill core's shield bubble (drawn over the core). */
export function drawCoreShield(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const c = sim.arena.core;
    if (!c || c.hp <= 0 || !coreShielded(sim.arena)) return;
    const r = c.r + 0.8 + Math.sin(sim.time * 3) * 0.08;
    ctx.fillStyle = 'rgba(96,165,250,0.12)';
    ctx.beginPath(); ctx.arc(c.pos.x, c.pos.y, r, 0, TAU); ctx.fill();
    ctx.strokeStyle = 'rgba(147,197,253,0.7)';
    ctx.lineWidth = 0.1;
    ctx.stroke();
    ctx.strokeStyle = 'rgba(191,219,254,0.35)';
    ctx.lineWidth = 0.05;
    for (let i = 0; i < 6; i++) { // hex shimmer
        const ang = sim.time * 0.5 + (i * TAU) / 6;
        ctx.beginPath(); ctx.arc(c.pos.x + Math.cos(ang) * r * 0.55, c.pos.y + Math.sin(ang) * r * 0.55, r * 0.35, 0, TAU); ctx.stroke();
    }
}

function drawWreck(ctx: CanvasRenderingContext2D, sim: Sim, p: Vec): void {
    disc(ctx, p.x, p.y, OBJECTIVE_R * 1.1, 'rgba(10,10,10,0.75)');
    ctx.fillStyle = '#3f3f46';
    for (let i = 0; i < 5; i++) {
        const ang = i * 1.3 + p.x;
        ctx.fillRect(p.x + Math.cos(ang) * 0.5 - 0.12, p.y + Math.sin(ang) * 0.5 - 0.1, 0.24, 0.2);
    }
    const flick = 0.5 + 0.5 * Math.sin(sim.time * 13 + p.y);
    disc(ctx, p.x, p.y, 0.18 + flick * 0.06, `rgba(251,146,60,${0.4 + 0.4 * flick})`);
}

function drawObjective(ctx: CanvasRenderingContext2D, sim: Sim, o: Objective): void {
    const { x, y } = o.pos;
    const r = OBJECTIVE_R;
    const t = sim.time;
    const white = o.flash > 0;
    // "Break me": a slow amber target ring on the floor.
    ctx.strokeStyle = `rgba(251,191,36,${0.35 + 0.25 * Math.sin(t * 3 + x)})`;
    ctx.lineWidth = 0.07;
    ctx.setLineDash([0.35, 0.25]);
    ctx.beginPath(); ctx.arc(x, y, r + 0.45, 0, TAU); ctx.stroke();
    ctx.setLineDash([]);
    ctx.fillStyle = 'rgba(0,0,0,0.4)';
    ctx.beginPath(); ctx.ellipse(x + 0.2, y + 0.3, r, r * 0.9, 0, 0, TAU); ctx.fill();
    const lift = 0.45; // drawn standing up a little, like the cover
    const top = y - lift;
    if (o.kind === 'generator') {
        ctx.fillStyle = white ? '#fff' : '#3f3f46';
        ctx.fillRect(x - r, top, r * 2, lift + 0.2);
        disc(ctx, x, top, r, white ? '#fff' : '#52525b');
        ctx.strokeStyle = '#facc15';
        ctx.lineWidth = 0.12;
        ctx.setLineDash([0.25, 0.25]);
        ctx.beginPath(); ctx.arc(x, top, r - 0.12, 0, TAU); ctx.stroke();
        ctx.setLineDash([]);
        ctx.fillStyle = '#1f2937';
        for (let i = 0; i < 4; i++) { // the fan
            const ang = t * 9 + (i * TAU) / 4;
            ctx.beginPath(); ctx.moveTo(x, top);
            ctx.arc(x, top, r * 0.6, ang, ang + 0.6); ctx.fill();
        }
        disc(ctx, x, top, 0.18, `rgb(255,${150 + 60 * Math.sin(t * 12)},60)`);
    } else if (o.kind === 'terminal') {
        ctx.fillStyle = white ? '#fff' : '#334155';
        ctx.fillRect(x - r, top - 0.3, r * 2, r * 1.5 + 0.3);
        ctx.fillStyle = '#022c22';
        ctx.fillRect(x - r + 0.15, top - 0.15, r * 2 - 0.3, r * 0.9);
        ctx.fillStyle = '#4ade80';
        for (let i = 0; i < 4; i++) { // scrolling readout
            const w = 0.3 + ((Math.floor(t * 4) + i * 7) % 5) * 0.2;
            ctx.fillRect(x - r + 0.25, top - 0.05 + i * 0.18, Math.min(w, r * 2 - 0.5), 0.07);
        }
        ctx.fillStyle = Math.sin(t * 6) > 0 ? '#ef4444' : '#7f1d1d';
        ctx.fillRect(x + r - 0.35, top + r * 0.95, 0.18, 0.12);
    } else if (o.kind === 'valve') {
        ctx.fillStyle = white ? '#fff' : '#4d5a2a';
        ctx.fillRect(x - 0.3, top, 0.6, lift + 0.3);
        disc(ctx, x, top, 0.35, white ? '#fff' : '#5b6b2a');
        ctx.strokeStyle = white ? '#fff' : '#dc2626';
        ctx.lineWidth = 0.16;
        ctx.beginPath(); ctx.arc(x, top, r * 0.8, 0, TAU); ctx.stroke();
        for (let i = 0; i < 4; i++) { // spokes, turning slowly
            const ang = t * 0.8 + (i * TAU) / 4;
            ctx.beginPath(); ctx.moveTo(x, top); ctx.lineTo(x + Math.cos(ang) * r * 0.8, top + Math.sin(ang) * r * 0.8); ctx.stroke();
        }
        disc(ctx, x, top, 0.12, '#fca5a5');
    } else if (o.kind === 'locker') {
        ctx.fillStyle = white ? '#fff' : '#475569';
        ctx.fillRect(x - r * 0.8, top - 0.4, r * 1.6, r * 1.8 + 0.2);
        ctx.fillStyle = '#64748b';
        ctx.fillRect(x - r * 0.8 + 0.1, top - 0.3, r * 1.6 - 0.2, 0.25);
        ctx.strokeStyle = '#1e293b';
        ctx.lineWidth = 0.05;
        for (let i = 0; i < 3; i++) { ctx.beginPath(); ctx.moveTo(x - 0.4, top + 0.1 + i * 0.15); ctx.lineTo(x + 0.4, top + 0.1 + i * 0.15); ctx.stroke(); }
        if (o.drops) drawKeyShape(ctx, x, top + 0.75, KEY_COLOR[o.drops], 0.8);
    } else { // pump
        const bob = Math.sin(t * 5 + x) * 0.15;
        ctx.fillStyle = white ? '#fff' : '#3730a3';
        ctx.fillRect(x - r, top, r * 2, lift + 0.25);
        disc(ctx, x, top, r, white ? '#fff' : '#4338ca');
        disc(ctx, x, top, r * 0.65, '#1e1b4b');
        ctx.fillStyle = white ? '#fff' : '#a5b4fc'; // the piston
        ctx.fillRect(x - 0.2, top - 0.5 - bob, 0.4, 0.6 + bob);
        disc(ctx, x, top - 0.5 - bob, 0.25, o.stops === 'shield' ? '#60a5fa' : '#c7d2fe');
    }
    // Health bar once it's been hit.
    if (o.hp < o.maxHp) {
        ctx.fillStyle = 'rgba(0,0,0,0.6)';
        ctx.fillRect(x - 0.9, y - r - 1.1, 1.8, 0.18);
        ctx.fillStyle = '#fbbf24';
        ctx.fillRect(x - 0.9, y - r - 1.1, (1.8 * Math.max(0, o.hp)) / o.maxHp, 0.18);
    }
}

// --- pickups ---------------------------------------------------------------------------------

export function drawPickups(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const k of sim.pickups) drawPickup(ctx, sim, k);
}

function drawPickup(ctx: CanvasRenderingContext2D, sim: Sim, k: Pickup): void {
    const bob = Math.sin(sim.time * 5 + k.pos.x * 3) * 0.08 - 0.15;
    const { x } = k.pos;
    const y = k.pos.y + bob;
    ctx.fillStyle = 'rgba(0,0,0,0.3)';
    ctx.beginPath(); ctx.ellipse(k.pos.x, k.pos.y + 0.2, 0.3, 0.12, 0, 0, TAU); ctx.fill();
    const glow = k.kind === 'joy' ? '253,224,71' : k.kind === 'heart' ? '244,114,182' : k.kind === 'fizz' ? '196,181,253' : k.color === 'red' ? '248,113,113' : '96,165,250';
    disc(ctx, x, y, 0.5 + 0.06 * Math.sin(sim.time * 8), `rgba(${glow},0.22)`);
    if (k.kind === 'joy') { // a jar of joy
        ctx.fillStyle = '#e2e8f0';
        ctx.fillRect(x - 0.18, y - 0.32, 0.36, 0.1);
        ctx.fillStyle = '#fde047';
        ctx.fillRect(x - 0.22, y - 0.22, 0.44, 0.48);
        ctx.fillStyle = '#422006';
        ctx.fillRect(x - 0.1, y - 0.08, 0.05, 0.05);
        ctx.fillRect(x + 0.05, y - 0.08, 0.05, 0.05);
        ctx.strokeStyle = '#422006';
        ctx.lineWidth = 0.04;
        ctx.beginPath(); ctx.arc(x, y + 0.02, 0.1, 0.2, Math.PI - 0.2); ctx.stroke();
    } else if (k.kind === 'heart') {
        ctx.font = '700 0.7px Inter, sans-serif';
        ctx.textAlign = 'center';
        ctx.fillStyle = '#f472b6';
        ctx.fillText('♥', x, y + 0.22);
    } else if (k.kind === 'fizz') { // a soda can with a bubble on top
        ctx.fillStyle = '#7c3aed';
        ctx.fillRect(x - 0.17, y - 0.25, 0.34, 0.5);
        ctx.fillStyle = '#ddd6fe';
        ctx.fillRect(x - 0.17, y - 0.05, 0.34, 0.1);
        disc(ctx, x + 0.12, y - 0.38, 0.12, 'rgba(196,181,253,0.9)');
    } else {
        drawKeyShape(ctx, x, y, KEY_COLOR[k.color ?? 'red'], 1.2);
    }
}

function drawKeyShape(ctx: CanvasRenderingContext2D, x: number, y: number, color: string, s: number): void {
    ctx.strokeStyle = color;
    ctx.fillStyle = color;
    ctx.lineWidth = 0.08 * s;
    ctx.beginPath(); ctx.arc(x - 0.15 * s, y, 0.13 * s, 0, TAU); ctx.stroke();
    ctx.fillRect(x - 0.03 * s, y - 0.04 * s, 0.35 * s, 0.08 * s);
    ctx.fillRect(x + 0.2 * s, y, 0.06 * s, 0.12 * s);
    ctx.fillRect(x + 0.3 * s, y, 0.05 * s, 0.09 * s);
}

/** A key icon for the HUD (screen pixels). */
export function drawHudKey(ctx: CanvasRenderingContext2D, x: number, y: number, color: string): void {
    drawKeyShape(ctx, x, y, color, 30);
}
