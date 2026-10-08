// Enemy Arena — drawing. World units are metres; one transform scales them to the canvas.
//   cone        each enemy's view, coloured by its state (see STATE_COLOR)
//   bar         the detection meter over its head while it fills
//   gold ring   holds an attack token        red ring grows   wind-up (the telegraph)
//   red line    a ranged enemy aiming (dashed = still tracking you, solid = locked: dodge now)
//   cyan dots   the slot ring (filled = taken, red x = blocked)    green diamonds  cover markers
//   white rings noises        magenta x  where the fight thinks you are
import { add, fromAngle, scale, type V } from './Map20Math';
import { SIZE } from './Map20World';
import { ENEMY_RADIUS, PLAYER_RADIUS, type Enemy, type EnemyState, type Sim } from './Map20State';
import { SWING } from './Map20Player';
import { fighting } from './Map20Group';

export const STATE_COLOR: Record<EnemyState, string> = {
    patrol: '#64748b', suspicious: '#facc15', fight: '#f97316', windup: '#ef4444', lunge: '#ef4444',
    recover: '#fb923c', aim: '#ef4444', stagger: '#a78bfa', retreat: '#38bdf8', search: '#fde047',
    return: '#94a3b8', dead: '#334155',
};

export type View = { markers: boolean; slots: boolean; cones: boolean; patrols: boolean };

export function drawSim(ctx: CanvasRenderingContext2D, sim: Sim, view: View): void {
    const s = ctx.canvas.width / SIZE;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.fillStyle = '#0b1220';
    ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
    ctx.setTransform(s, 0, 0, s, 0, 0);
    ctx.lineWidth = 0.06;
    drawFloor(ctx);
    if (view.patrols) for (const e of sim.enemies) drawPatrol(ctx, e);
    if (view.cones) for (const e of sim.enemies) if (e.state !== 'dead') drawCone(ctx, sim, e);
    for (const b of sim.world.walls) {
        ctx.fillStyle = '#334155';
        ctx.fillRect(b.x, b.y, b.w, b.h);
        ctx.strokeStyle = '#475569';
        ctx.strokeRect(b.x, b.y, b.w, b.h);
    }
    if (view.markers) for (const c of sim.world.markers.cover) diamond(ctx, c, 0.22, '#22c55e');
    if (view.slots && sim.settings.slots && sim.enemies.some(fighting)) drawSlots(ctx, sim);
    drawNoises(ctx, sim);
    drawLastSeen(ctx, sim);
    for (const shot of sim.shots) line(ctx, shot.from, shot.to, shot.hit ? '#f43f5e' : '#fef3c7', 0.08, 1 - shot.age / 0.4);
    for (const e of sim.enemies) drawEnemy(ctx, sim, e);
    drawPlayer(ctx, sim);
}

function drawFloor(ctx: CanvasRenderingContext2D): void {
    ctx.strokeStyle = 'rgba(148,163,184,0.06)';
    for (let i = 0; i <= SIZE; i += 2) {
        line(ctx, { x: i, y: 0 }, { x: i, y: SIZE }, 'rgba(148,163,184,0.06)', 0.03);
        line(ctx, { x: 0, y: i }, { x: SIZE, y: i }, 'rgba(148,163,184,0.06)', 0.03);
    }
}

function drawPatrol(ctx: CanvasRenderingContext2D, e: Enemy): void {
    if (e.patrol.length < 2 || e.state === 'dead') return;
    ctx.setLineDash([0.2, 0.3]);
    ctx.strokeStyle = 'rgba(100,116,139,0.35)';
    ctx.beginPath();
    e.patrol.forEach((p, i) => (i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y)));
    ctx.closePath();
    ctx.stroke();
    ctx.setLineDash([]);
}

function drawCone(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    const half = (sim.settings.viewAngle * Math.PI) / 360;
    ctx.fillStyle = STATE_COLOR[e.state] + '22';
    ctx.beginPath();
    ctx.moveTo(e.pos.x, e.pos.y);
    ctx.arc(e.pos.x, e.pos.y, sim.settings.viewRange, e.facing - half, e.facing + half);
    ctx.closePath();
    ctx.fill();
}

function drawSlots(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const slot of sim.slots) {
        if (!slot.valid) {
            line(ctx, add(slot.pos, { x: -0.15, y: -0.15 }), add(slot.pos, { x: 0.15, y: 0.15 }), '#f43f5e', 0.05);
            line(ctx, add(slot.pos, { x: -0.15, y: 0.15 }), add(slot.pos, { x: 0.15, y: -0.15 }), '#f43f5e', 0.05);
            continue;
        }
        circle(ctx, slot.pos, 0.22, slot.taken >= 0 ? '#22d3ee' : null, '#22d3ee');
    }
}

function drawNoises(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const n of sim.noises) {
        const t = Math.min(1, n.age / 0.3);
        ctx.globalAlpha = 1 - n.age / 0.8;
        circle(ctx, n.pos, n.radius * t, null, '#e2e8f0', 0.04);
        ctx.globalAlpha = 1;
    }
}

function drawLastSeen(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const shown: V[] = [];
    for (const e of sim.enemies) {
        if (!e.lastSeen || !(fighting(e) || e.state === 'search')) continue;
        if (shown.some((p) => Math.hypot(p.x - e.lastSeen!.x, p.y - e.lastSeen!.y) < 0.3)) continue;
        shown.push(e.lastSeen);
        const p = e.lastSeen;
        line(ctx, add(p, { x: -0.3, y: -0.3 }), add(p, { x: 0.3, y: 0.3 }), '#e879f9', 0.07);
        line(ctx, add(p, { x: -0.3, y: 0.3 }), add(p, { x: 0.3, y: -0.3 }), '#e879f9', 0.07);
    }
}

function drawEnemy(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    const color = STATE_COLOR[e.state];
    if (e.state === 'dead') {
        circle(ctx, e.pos, ENEMY_RADIUS * 0.8, '#1e293b', '#334155');
        return;
    }
    if (e.state === 'aim') {
        const locked = e.timer >= (sim.settings.windup ? Math.max(0.6, sim.settings.windupTime * 2) : 0.1) * 0.7;
        ctx.setLineDash(locked ? [] : [0.25, 0.2]);
        line(ctx, e.pos, add(e.pos, scale(fromAngle(e.facing), 14)), '#ef4444', locked ? 0.07 : 0.04);
        ctx.setLineDash([]);
    }
    if (e.state === 'windup') {
        const t = Math.min(1, e.timer / Math.max(0.05, sim.settings.windupTime));
        circle(ctx, e.pos, ENEMY_RADIUS + 0.15 + t * 0.5, null, '#ef4444', 0.1);
    }
    if (e.hasToken) circle(ctx, e.pos, ENEMY_RADIUS + 0.18, null, '#fbbf24', 0.08);
    if (e.kind === 'ranged') square(ctx, e.pos, ENEMY_RADIUS, color);
    else circle(ctx, e.pos, ENEMY_RADIUS, color, '#0f172a');
    line(ctx, e.pos, add(e.pos, scale(fromAngle(e.facing), ENEMY_RADIUS + 0.25)), '#f8fafc', 0.08);
    for (let i = 0; i < e.hp; i++) circle(ctx, { x: e.pos.x - 0.3 + i * 0.3, y: e.pos.y + 0.75 }, 0.08, '#f8fafc', null);
    if (e.meter > 0 && e.meter < 1) {
        ctx.fillStyle = '#1e293b';
        ctx.fillRect(e.pos.x - 0.5, e.pos.y - 1.0, 1, 0.16);
        ctx.fillStyle = e.meter > 0.5 ? '#f97316' : '#facc15';
        ctx.fillRect(e.pos.x - 0.5, e.pos.y - 1.0, e.meter, 0.16);
    }
    text(ctx, e.state, { x: e.pos.x, y: e.pos.y - 1.15 }, color, 0.42);
    if (e.barkLeft > 0) text(ctx, e.bark, { x: e.pos.x, y: e.pos.y - 1.7 }, '#f8fafc', 0.6);
}

function drawPlayer(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const p = sim.player;
    if (p.swingLeft > 0) {
        ctx.fillStyle = 'rgba(96,165,250,0.35)';
        ctx.beginPath();
        ctx.moveTo(p.pos.x, p.pos.y);
        ctx.arc(p.pos.x, p.pos.y, SWING.reach, p.facing - SWING.arc, p.facing + SWING.arc);
        ctx.closePath();
        ctx.fill();
    }
    if (p.sneaking) ctx.setLineDash([0.15, 0.12]);
    circle(ctx, p.pos, PLAYER_RADIUS, p.hurtLeft > 0 ? '#f43f5e' : '#60a5fa', '#e0f2fe', 0.07);
    ctx.setLineDash([]);
    line(ctx, p.pos, add(p.pos, scale(fromAngle(p.facing), 0.8)), '#e0f2fe', 0.08);
}

// --- primitives ----------------------------------------------------------------------------------

function line(ctx: CanvasRenderingContext2D, a: V, b: V, color: string, width = 0.06, alpha = 1): void {
    ctx.globalAlpha = Math.max(0, alpha);
    ctx.strokeStyle = color;
    ctx.lineWidth = width;
    ctx.beginPath();
    ctx.moveTo(a.x, a.y);
    ctx.lineTo(b.x, b.y);
    ctx.stroke();
    ctx.globalAlpha = 1;
}

function circle(ctx: CanvasRenderingContext2D, c: V, r: number, fill: string | null, stroke: string | null, width = 0.05): void {
    ctx.beginPath();
    ctx.arc(c.x, c.y, Math.max(0, r), 0, Math.PI * 2);
    if (fill) {
        ctx.fillStyle = fill;
        ctx.fill();
    }
    if (stroke) {
        ctx.strokeStyle = stroke;
        ctx.lineWidth = width;
        ctx.stroke();
    }
}

function square(ctx: CanvasRenderingContext2D, c: V, half: number, fill: string): void {
    ctx.fillStyle = fill;
    ctx.fillRect(c.x - half, c.y - half, half * 2, half * 2);
    ctx.strokeStyle = '#0f172a';
    ctx.lineWidth = 0.05;
    ctx.strokeRect(c.x - half, c.y - half, half * 2, half * 2);
}

function diamond(ctx: CanvasRenderingContext2D, c: V, r: number, color: string): void {
    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.moveTo(c.x, c.y - r);
    ctx.lineTo(c.x + r, c.y);
    ctx.lineTo(c.x, c.y + r);
    ctx.lineTo(c.x - r, c.y);
    ctx.closePath();
    ctx.fill();
}

function text(ctx: CanvasRenderingContext2D, str: string, at: V, color: string, size: number): void {
    ctx.fillStyle = color;
    ctx.font = `600 ${size}px Inter, sans-serif`;
    ctx.textAlign = 'center';
    ctx.fillText(str, at.x, at.y);
}
