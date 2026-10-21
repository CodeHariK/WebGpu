// Combat Sandbox — the Toy Assembly Plant's look: clean pastel tiles with painted walkways, toy
// parts dropped everywhere (doll eyes, springs, screws, paint splats), pink-trimmed walls with
// shelves of finished toys staring down, and cold strip lights. Same layers as the factory
// (floor, walls, lamps) so Map21RenderWorld can pick one or the other.
import type { Arena, Sim } from './Map21State';
import { WALL } from './Map21World';

const TAU = Math.PI * 2;
const PAINT = ['rgba(244,114,182,0.22)', 'rgba(103,232,249,0.2)', 'rgba(253,224,71,0.2)', 'rgba(167,139,250,0.22)'];

export function drawPlantFloor(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const a = sim.arena;
    ctx.fillStyle = '#120d16';
    ctx.fillRect(-40, -40, a.w + 80, a.h + 80);
    ctx.fillStyle = a.def.floor;
    ctx.fillRect(WALL, WALL, a.w - 2 * WALL, a.h - 2 * WALL);
    // 1 m tiles, every other one a touch lighter, with grout lines.
    for (let x = WALL; x < a.w - WALL; x += 1) {
        for (let y = WALL; y < a.h - WALL; y += 1) {
            if ((Math.floor(x) + Math.floor(y)) % 2 === 0) {
                ctx.fillStyle = 'rgba(255,255,255,0.025)';
                ctx.fillRect(x, y, 1, 1);
            }
        }
    }
    ctx.strokeStyle = 'rgba(0,0,0,0.22)';
    ctx.lineWidth = 0.03;
    ctx.beginPath();
    for (let x = WALL; x <= a.w - WALL; x += 1) { ctx.moveTo(x, WALL); ctx.lineTo(x, a.h - WALL); }
    for (let y = WALL; y <= a.h - WALL; y += 1) { ctx.moveTo(WALL, y); ctx.lineTo(a.w - WALL, y); }
    ctx.stroke();
    drawWalkways(ctx, a);
    drawParts(ctx, a);
}

// Painted walkway edges (white dashes) beside every belt, and a stencilled arrow on the floor.
function drawWalkways(ctx: CanvasRenderingContext2D, a: Arena): void {
    ctx.strokeStyle = 'rgba(241,245,249,0.35)';
    ctx.lineWidth = 0.1;
    ctx.setLineDash([0.6, 0.4]);
    for (const c of a.def.conveyors) {
        ctx.beginPath();
        ctx.moveTo(c.x, c.y - 0.5); ctx.lineTo(c.x + c.w, c.y - 0.5);
        ctx.moveTo(c.x, c.y + c.h + 0.5); ctx.lineTo(c.x + c.w, c.y + c.h + 0.5);
        ctx.stroke();
    }
    ctx.setLineDash([]);
    ctx.fillStyle = 'rgba(241,245,249,0.12)';
    const ax = a.w / 2;
    const ay = a.h - WALL - 5;
    ctx.beginPath(); // "this way up", painted big
    ctx.moveTo(ax, ay - 1.6); ctx.lineTo(ax + 1.2, ay - 0.4); ctx.lineTo(ax + 0.45, ay - 0.4); ctx.lineTo(ax + 0.45, ay + 1.2);
    ctx.lineTo(ax - 0.45, ay + 1.2); ctx.lineTo(ax - 0.45, ay - 0.4); ctx.lineTo(ax - 1.2, ay - 0.4);
    ctx.fill();
}

// The decals, read as dropped toy parts: paint splats, doll eyes, springs and screws.
function drawParts(ctx: CanvasRenderingContext2D, a: Arena): void {
    for (const d of a.decals) {
        ctx.save();
        ctx.translate(d.pos.x, d.pos.y);
        ctx.rotate(d.rot);
        if (d.kind === 0) { // paint splat: a blob and a few drops
            ctx.fillStyle = PAINT[Math.floor(d.rot * 10) % PAINT.length];
            ctx.beginPath(); ctx.ellipse(0, 0, d.r * 0.8, d.r * 0.55, 0, 0, TAU); ctx.fill();
            for (let i = 0; i < 3; i++) { ctx.beginPath(); ctx.arc(d.r * (0.9 + i * 0.2), (i - 1) * 0.25, 0.08, 0, TAU); ctx.fill(); }
        } else if (d.kind === 1) { // a doll's eye, staring up
            ctx.fillStyle = 'rgba(241,245,249,0.55)';
            ctx.beginPath(); ctx.arc(0, 0, 0.16, 0, TAU); ctx.fill();
            ctx.fillStyle = 'rgba(59,130,246,0.7)';
            ctx.beginPath(); ctx.arc(0.03, 0, 0.09, 0, TAU); ctx.fill();
            ctx.fillStyle = 'rgba(0,0,0,0.8)';
            ctx.beginPath(); ctx.arc(0.04, 0, 0.045, 0, TAU); ctx.fill();
        } else if (d.kind === 2) { // a wind-up spring
            ctx.strokeStyle = 'rgba(203,213,225,0.4)';
            ctx.lineWidth = 0.03;
            ctx.beginPath();
            for (let i = 0; i <= 24; i++) {
                const t = i / 24;
                const x = -0.3 + t * 0.6;
                const y = Math.sin(t * TAU * 4) * 0.08;
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.stroke();
        } else { // a screw
            ctx.fillStyle = 'rgba(148,163,184,0.45)';
            ctx.fillRect(-0.12, -0.03, 0.2, 0.06);
            ctx.beginPath(); ctx.arc(0.1, 0, 0.06, 0, TAU); ctx.fill();
        }
        ctx.restore();
    }
}

// Walls: dark plum blocks with a pink trim, and along the top a shelf of finished toys watching.
export function drawPlantWalls(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const a = sim.arena;
    ctx.fillStyle = '#2a2236';
    ctx.fillRect(0, 0, a.w, WALL);
    ctx.fillRect(0, a.h - WALL, a.w, WALL);
    ctx.fillRect(0, 0, WALL, a.h);
    ctx.fillRect(a.w - WALL, 0, WALL, a.h);
    ctx.fillStyle = '#f0abfc'; // trim
    ctx.fillRect(WALL - 0.1, WALL - 0.1, a.w - 2 * WALL + 0.2, 0.1);
    ctx.fillRect(WALL - 0.1, WALL, 0.1, a.h - 2 * WALL);
    ctx.fillRect(a.w - WALL, WALL, 0.1, a.h - 2 * WALL);
    ctx.fillRect(WALL - 0.1, a.h - WALL, a.w - 2 * WALL + 0.2, 0.1);
    // The shelf of finished toys: little heads in a row; their eyes follow you.
    const p = sim.player.pos;
    for (let x = 1.6; x < a.w - 1.2; x += 1.1) {
        const tint = ['#a3a380', '#94a3b8', '#c084fc', '#f9a8d4'][Math.floor(x * 3) % 4];
        ctx.fillStyle = tint;
        ctx.beginPath(); ctx.arc(x, 0.5, 0.32, 0, TAU); ctx.fill();
        const look = Math.atan2(p.y - 0.5, p.x - x);
        ctx.fillStyle = '#0f0a14';
        for (const s of [-1, 1]) {
            ctx.beginPath(); ctx.arc(x + s * 0.12 + Math.cos(look) * 0.04, 0.48 + Math.sin(look) * 0.04, 0.05, 0, TAU); ctx.fill();
        }
    }
}

// Cold strip lights along the side walls (long, flickering now and then).
export function drawPlantLamps(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const a = sim.arena;
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    for (let y = 4; y < a.h - 2; y += 7) {
        for (const x of [WALL + 0.3, a.w - WALL - 0.3]) {
            const glitch = Math.sin(sim.time * 7 + y * 5) > 0.97 ? 0.3 : 1; // a tube about to go
            const g = ctx.createRadialGradient(x, y, 0, x, y, 5.5);
            g.addColorStop(0, `rgba(170,220,255,${0.16 * glitch})`);
            g.addColorStop(1, 'rgba(170,220,255,0)');
            ctx.fillStyle = g;
            ctx.beginPath(); ctx.ellipse(x, y, 4, 6, 0, 0, TAU); ctx.fill();
        }
    }
    ctx.restore();
    for (let y = 4; y < a.h - 2; y += 7) {
        for (const x of [WALL - 0.12, a.w - WALL - 0.12]) {
            ctx.fillStyle = '#e0f2fe';
            ctx.fillRect(x, y - 1, 0.24, 2);
            ctx.strokeStyle = '#1e1b24';
            ctx.lineWidth = 0.04;
            ctx.strokeRect(x, y - 1, 0.24, 2);
        }
    }
}
