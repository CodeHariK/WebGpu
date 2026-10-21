// Lockpick — drawing the people: you, the guards, sleepers, the watcher, the hound — and, over the
// dark, their sight cones and alert marks (? filling up, ! when they know). Pure drawing.
import { coneHalf, sightRange } from './Map23Guards';
import { opaque } from './Map23Grid';
import type { Guard, Sim } from './Map23State';

const TAU = Math.PI * 2;

export function drawActors(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const g of sim.guards) {
        if (g.kind === 'hound') hound(ctx, sim, g);
        else if (g.kind === 'sleeper' && g.state === 'sleep') sleeper(ctx, sim, g);
        else guard(ctx, g);
    }
    player(ctx, sim);
}

function player(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const p = sim.player;
    if (p.hidden >= 0 || sim.phase === 'escaped') return;
    const { x, y } = p.pos;
    const crouch = p.mode === 'sneak';
    const r = crouch ? 0.26 : 0.3;
    ctx.globalAlpha = crouch ? 0.8 : 1;
    ctx.fillStyle = 'rgba(0,0,0,0.4)';
    ctx.beginPath(); ctx.ellipse(x, y + 0.08, r, r * 0.6, 0, 0, TAU); ctx.fill();
    ctx.fillStyle = sim.phase === 'caught' ? '#f87171' : '#3f6212'; // a hooded cloak
    ctx.beginPath(); ctx.arc(x, y, r, 0, TAU); ctx.fill();
    ctx.fillStyle = '#65a30d';
    ctx.beginPath(); ctx.arc(x + Math.cos(p.facing) * r * 0.25, y + Math.sin(p.facing) * r * 0.25, r * 0.6, 0, TAU); ctx.fill(); // hood
    ctx.fillStyle = '#fef3c7';
    ctx.beginPath(); ctx.arc(x + Math.cos(p.facing) * r * 0.55, y + Math.sin(p.facing) * r * 0.55, 0.06, 0, TAU); ctx.fill(); // face
    ctx.fillStyle = '#a16207'; // a little sack on the back
    ctx.beginPath(); ctx.arc(x - Math.cos(p.facing) * r * 0.7, y - Math.sin(p.facing) * r * 0.7, r * 0.4, 0, TAU); ctx.fill();
    ctx.globalAlpha = 1;
}

function guard(ctx: CanvasRenderingContext2D, g: Guard): void {
    const { x, y } = g.pos;
    const watcher = g.kind === 'watcher';
    ctx.fillStyle = 'rgba(0,0,0,0.4)';
    ctx.beginPath(); ctx.ellipse(x, y + 0.1, 0.4, 0.25, 0, 0, TAU); ctx.fill();
    ctx.save();
    ctx.translate(x, y);
    ctx.rotate(g.facing);
    ctx.fillStyle = watcher ? '#4c1d95' : '#7f1d1d'; // tabard
    ctx.beginPath(); ctx.ellipse(0, 0, 0.32, 0.4, 0, 0, TAU); ctx.fill();
    ctx.fillStyle = '#9ca3af'; // helmet
    ctx.beginPath(); ctx.arc(0.04, 0, 0.22, 0, TAU); ctx.fill();
    ctx.fillStyle = '#4b5563';
    ctx.fillRect(0.12, -0.08, 0.1, 0.16); // visor
    if (watcher) { ctx.fillStyle = '#fde68a'; ctx.beginPath(); ctx.arc(0.45, 0.2, 0.1, 0, TAU); ctx.fill(); } // its lantern
    else { ctx.fillStyle = '#78716c'; ctx.fillRect(0.1, 0.25, 0.5, 0.06); } // a spear
    ctx.restore();
}

function sleeper(ctx: CanvasRenderingContext2D, sim: Sim, g: Guard): void {
    const { x, y } = g.home;
    ctx.fillStyle = '#44403c'; // the bunk
    ctx.fillRect(x - 0.45, y - 0.8, 0.9, 1.6);
    ctx.fillStyle = '#7f1d1d';
    ctx.fillRect(x - 0.38, y - 0.3, 0.76, 1);
    ctx.fillStyle = '#9ca3af';
    ctx.beginPath(); ctx.arc(x, y - 0.5, 0.2, 0, TAU); ctx.fill();
    ctx.fillStyle = 'rgba(203,213,225,0.8)';
    ctx.font = '700 0.4px Inter, sans-serif';
    const k = (sim.time * 0.5 + x) % 1;
    ctx.globalAlpha = 1 - k;
    ctx.fillText('z', x + 0.3 + k * 0.3, y - 0.8 - k * 0.6);
    ctx.globalAlpha = 1;
}

function hound(ctx: CanvasRenderingContext2D, sim: Sim, g: Guard): void {
    ctx.save();
    ctx.translate(g.pos.x, g.pos.y);
    ctx.rotate(g.facing);
    const run = g.state === 'chase' ? Math.sin(sim.time * 20) * 0.06 : 0;
    ctx.fillStyle = 'rgba(0,0,0,0.4)';
    ctx.beginPath(); ctx.ellipse(0, 0.1, 0.5, 0.22, 0, 0, TAU); ctx.fill();
    ctx.fillStyle = '#8b6b4a';
    ctx.beginPath(); ctx.ellipse(-0.05, run, 0.45, 0.2, 0, 0, TAU); ctx.fill(); // body
    ctx.beginPath(); ctx.ellipse(0.4, 0, 0.18, 0.14, 0, 0, TAU); ctx.fill(); // head
    ctx.fillStyle = '#292524';
    ctx.beginPath(); ctx.ellipse(0.38, -0.13, 0.07, 0.05, 0, 0, TAU); ctx.ellipse(0.38, 0.13, 0.07, 0.05, 0, 0, TAU); ctx.fill(); // ears
    ctx.fillStyle = '#1c1917';
    ctx.beginPath(); ctx.arc(0.57, 0, 0.04, 0, TAU); ctx.fill(); // nose
    ctx.strokeStyle = '#8b6b4a';
    ctx.lineWidth = 0.06;
    ctx.beginPath(); ctx.moveTo(-0.45, 0); ctx.lineTo(-0.65, Math.sin(sim.time * 12) * 0.1); ctx.stroke(); // tail
    ctx.restore();
    if (g.state === 'track') { // nose down: sniff puffs
        ctx.fillStyle = 'rgba(190,242,100,0.5)';
        const k = (sim.time * 2) % 1;
        ctx.beginPath(); ctx.arc(g.pos.x + Math.cos(g.facing) * (0.6 + k * 0.3), g.pos.y + Math.sin(g.facing) * (0.6 + k * 0.3), 0.05 + k * 0.05, 0, TAU); ctx.fill();
    }
}

// --- over the dark: sight cones and alert marks ------------------------------------------------

export function drawCones(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const g of sim.guards) {
        if (g.kind === 'hound' || g.state === 'sleep') continue;
        const half = coneHalf(sim, g);
        const full = sim.settings.sightRange * (g.kind === 'watcher' ? 1.4 : 1);
        const now = sightRange(sim, g); // how far it would see you where you stand
        const col = g.state === 'chase' || g.state === 'alarm' ? '248,113,113' : g.alert > 0.25 ? '251,146,60' : g.kind === 'watcher' ? '253,224,71' : '226,232,240';
        const pts: { x: number; y: number }[] = [];
        const n = 18;
        for (let i = 0; i <= n; i++) {
            const a = g.facing - half + (2 * half * i) / n;
            let d = 0;
            while (d < full) { const nx = g.pos.x + Math.cos(a) * (d + 0.15), ny = g.pos.y + Math.sin(a) * (d + 0.15); if (opaque(sim, Math.floor(nx), Math.floor(ny))) break; d += 0.15; }
            pts.push({ x: g.pos.x + Math.cos(a) * d, y: g.pos.y + Math.sin(a) * d });
        }
        const grad = ctx.createRadialGradient(g.pos.x, g.pos.y, 0, g.pos.x, g.pos.y, full);
        grad.addColorStop(0, `rgba(${col},0.22)`);
        grad.addColorStop(Math.min(0.99, now / full), `rgba(${col},0.12)`);
        grad.addColorStop(Math.min(1, now / full + 0.01), `rgba(${col},0.04)`);
        grad.addColorStop(1, `rgba(${col},0)`);
        ctx.fillStyle = grad;
        ctx.beginPath(); ctx.moveTo(g.pos.x, g.pos.y);
        for (const q of pts) ctx.lineTo(q.x, q.y);
        ctx.closePath(); ctx.fill();
        ctx.strokeStyle = `rgba(${col},0.35)`;
        ctx.lineWidth = 0.03;
        ctx.stroke();
    }
}

export function drawMarks(ctx: CanvasRenderingContext2D, sim: Sim): void {
    ctx.textAlign = 'center';
    for (const g of sim.guards) {
        const x = g.pos.x, y = g.pos.y - 0.85;
        if (g.state === 'chase' || g.state === 'alarm' || (g.kind === 'hound' && g.state === 'bark')) {
            ctx.font = '900 0.7px Georgia, serif';
            ctx.fillStyle = '#ef4444';
            ctx.fillText('!', x, y);
            continue;
        }
        if (g.alert > 0.05 || g.state === 'investigate' || g.state === 'look' || g.state === 'search' || g.state === 'wake') {
            const k = Math.min(1, g.alert);
            ctx.strokeStyle = 'rgba(0,0,0,0.6)';
            ctx.lineWidth = 0.1;
            ctx.beginPath(); ctx.arc(x, y - 0.18, 0.3, 0, TAU); ctx.stroke();
            ctx.strokeStyle = `rgb(251,${200 - 120 * k},60)`;
            ctx.lineWidth = 0.07;
            ctx.beginPath(); ctx.arc(x, y - 0.18, 0.3, -Math.PI / 2, -Math.PI / 2 + TAU * k); ctx.stroke();
            ctx.font = '900 0.45px Georgia, serif';
            ctx.fillStyle = '#fde68a';
            ctx.fillText('?', x, y);
        }
        if (g.kind === 'hound' && g.state === 'track') { ctx.font = '700 0.35px Inter, sans-serif'; ctx.fillStyle = '#bef264'; ctx.fillText('sniff', x, y); }
    }
}
