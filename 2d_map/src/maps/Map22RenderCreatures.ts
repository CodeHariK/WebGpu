// Wick — drawing the second set of creatures (snuffer, drowned hand, gloom, wax leech) and the wax
// blobs knocked off you. Pure drawing in world metres.
import { leechRadius, RISE_TELL, SNUFF_TIME } from './Map22Creatures';
import type { Enemy, Sim } from './Map22State';

const TAU = Math.PI * 2;

export function drawBlobs(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const b of sim.blobs) {
        const r = 0.1 + Math.min(0.12, b.wax * 0.03);
        ctx.fillStyle = '#e9dcb8';
        ctx.beginPath(); ctx.ellipse(b.pos.x, b.pos.y, r * 1.3, r, 0, 0, TAU); ctx.fill();
        ctx.fillStyle = 'rgba(255,255,255,0.6)';
        ctx.beginPath(); ctx.arc(b.pos.x - r * 0.4, b.pos.y - r * 0.3, r * 0.3, 0, TAU); ctx.fill();
    }
}

export function drawCreature(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    if (e.kind === 'snuffer') snuffer(ctx, sim, e);
    else if (e.kind === 'hand') hand(ctx, sim, e);
    else if (e.kind === 'gloom') gloom(ctx, sim, e);
    else if (e.kind === 'leech') leech(ctx, sim, e);
}

// A hunched cloaked thing; its head is a long brass snuffer cone. Snuffing, the cone tips down
// over the light and a ring round it fills.
function snuffer(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    const { x, y } = e.pos;
    const bob = Math.sin(sim.time * 3 + e.id) * 0.08;
    const white = e.flash > 0;
    ctx.fillStyle = 'rgba(0,0,0,0.35)';
    ctx.beginPath(); ctx.ellipse(x, y + 0.1, 0.45, 0.16, 0, 0, TAU); ctx.fill();
    ctx.fillStyle = white ? '#fff' : '#1c1a20'; // cloak
    ctx.beginPath(); ctx.moveTo(x - 0.45, y); ctx.quadraticCurveTo(x - 0.3, y - 1.1 + bob, x, y - 1.2 + bob); ctx.quadraticCurveTo(x + 0.3, y - 1.1 + bob, x + 0.45, y); ctx.fill();
    const snuffing = e.state === 'snuff' || e.state === 'hold';
    const tip = snuffing ? 0.9 : 0.2; // how far the cone tips forward
    ctx.save();
    ctx.translate(x, y - 1.15 + bob);
    ctx.rotate(Math.PI + tip * (Math.cos(e.facing) >= 0 ? -1 : 1));
    ctx.fillStyle = white ? '#fff' : '#a16207';
    ctx.beginPath(); ctx.moveTo(-0.22, 0); ctx.lineTo(0, -0.95); ctx.lineTo(0.22, 0); ctx.fill();
    ctx.fillStyle = '#713f12';
    ctx.fillRect(-0.22, -0.06, 0.44, 0.08);
    ctx.restore();
    if (e.state === 'snuff' && e.target) { // the ring filling round the light it's putting out
        const k = Math.min(1, e.t / SNUFF_TIME);
        ctx.strokeStyle = 'rgba(168,162,158,0.9)';
        ctx.lineWidth = 0.09;
        ctx.beginPath(); ctx.arc(e.target.x, e.target.y, 0.9, -Math.PI / 2, -Math.PI / 2 + TAU * k); ctx.stroke();
    }
}

// A drowned hand: ripples while it waits; pale fingers breaking the surface (the tell); an arm out
// of the water gripping you.
function hand(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    const home = e.home!;
    const near = Math.hypot(sim.player.pos.x - home.x, sim.player.pos.y - home.y) < 5;
    const woods = sim.level.ground === 'woods';
    if (e.state === 'under') { // faint rings (or a twitch in the undergrowth): something is down there
        if (!near) return;
        if (woods) {
            ctx.strokeStyle = 'rgba(120,90,60,0.4)';
            ctx.lineWidth = 0.06;
            const tw = Math.sin(sim.time * 3 + home.x) * 0.15;
            ctx.beginPath(); ctx.moveTo(home.x - 0.5, home.y); ctx.quadraticCurveTo(home.x, home.y - 0.3 + tw, home.x + 0.5, home.y); ctx.stroke();
            return;
        }
        ctx.strokeStyle = 'rgba(148,163,184,0.25)';
        ctx.lineWidth = 0.04;
        for (let i = 0; i < 2; i++) {
            const k = (sim.time * 0.4 + i * 0.5) % 1;
            ctx.globalAlpha = 1 - k;
            ctx.beginPath(); ctx.ellipse(home.x, home.y, 0.3 + k * 1.2, (0.3 + k * 1.2) * 0.5, 0, 0, TAU); ctx.stroke();
        }
        ctx.globalAlpha = 1;
        return;
    }
    const up = e.state === 'rise' ? Math.min(1, e.t / RISE_TELL) : 1;
    const tip = e.state === 'hold' ? e.pos : e.state === 'grope' ? e.pos : home;
    ctx.strokeStyle = 'rgba(148,163,184,0.6)'; // the water parting
    ctx.lineWidth = 0.05;
    ctx.beginPath(); ctx.ellipse(home.x, home.y, 0.6 + up * 0.4, 0.3 + up * 0.2, 0, 0, TAU); ctx.stroke();
    ctx.strokeStyle = e.flash > 0 ? '#fff' : woods ? '#4a3524' : '#8a9a8c'; // the arm (a root, in the woods)
    ctx.lineCap = 'round';
    ctx.lineWidth = 0.28;
    ctx.beginPath(); ctx.moveTo(home.x, home.y); ctx.lineTo(home.x + (tip.x - home.x) * up, home.y + (tip.y - home.y) * up - 0.4 * up); ctx.stroke();
    const hx = home.x + (tip.x - home.x) * up, hy = home.y + (tip.y - home.y) * up - 0.4 * up;
    ctx.lineWidth = 0.08; // fingers
    const open = e.state === 'hold' ? 0.3 : 0.9;
    for (let i = -2; i <= 2; i++) {
        const a = -Math.PI / 2 + i * 0.35 * open;
        ctx.beginPath(); ctx.moveTo(hx, hy); ctx.lineTo(hx + Math.cos(a) * 0.4 * up, hy + Math.sin(a) * 0.4 * up); ctx.stroke();
    }
    ctx.lineCap = 'butt';
}

// A ball of un-light: black, a violet rim, motes being drawn into it.
function gloom(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    const { x } = e.pos;
    const y = e.pos.y - 1;
    ctx.fillStyle = e.flash > 0 ? '#e9d5ff' : '#000';
    ctx.beginPath(); ctx.arc(x, y, 0.4, 0, TAU); ctx.fill();
    ctx.strokeStyle = 'rgba(167,139,250,0.8)';
    ctx.lineWidth = 0.05;
    ctx.stroke();
    ctx.fillStyle = 'rgba(196,181,253,0.7)';
    for (let i = 0; i < 6; i++) {
        const k = (sim.time * 0.6 + i / 6) % 1;
        const a = i * 1.7 + sim.time * 0.3;
        const r = 1.6 * (1 - k) + 0.4;
        ctx.beginPath(); ctx.arc(x + Math.cos(a) * r, y + Math.sin(a) * r, 0.04, 0, TAU); ctx.fill();
    }
}

// A wax leech: a pale fat slug; the more it has swallowed, the bigger and more yellow.
function leech(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    const r = leechRadius(e);
    const fill = Math.min(1, e.eaten / 5);
    ctx.save();
    ctx.translate(e.pos.x, e.pos.y);
    ctx.rotate(e.facing);
    const squirm = Math.sin(sim.time * 10 + e.id) * 0.08;
    ctx.fillStyle = e.flash > 0 ? '#fff' : `rgb(${120 + 110 * fill},${110 + 100 * fill},${100 + 40 * fill})`;
    for (let i = 0; i < 3; i++) {
        ctx.beginPath(); ctx.ellipse(-i * r * 0.8, Math.sin(i + sim.time * 10) * squirm, r * (1 - i * 0.18), r * 0.75 * (1 - i * 0.18), 0, 0, TAU); ctx.fill();
    }
    ctx.fillStyle = '#3f2a1d';
    ctx.beginPath(); ctx.arc(r * 0.6, 0, r * 0.25, 0, TAU); ctx.fill(); // mouth
    ctx.restore();
}
