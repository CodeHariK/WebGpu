// Combat Sandbox — drawing the Assembly Plant's bots: the rigger drone and its cables, the rig bots
// on the end of them, and the springer (with a red ring where it will land). Pure drawing.
// drawRigger / drawRigbot / drawSpringer are called by Map21Render with the canvas already moved to
// the bot's position (and lifted for height).
import { LAND_R, PULL_TELL, riggerOf } from './Map21Bots';
import type { Enemy, Sim } from './Map21State';

const TAU = Math.PI * 2;
const STEEL = '#64748b';
const DARK = '#1e293b';

/** Cables from each rigger to its bots (on the floor layer, under the bodies). */
export function drawCables(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const bot of sim.enemies) {
        if (bot.kind !== 'rigbot' || bot.spawnT > 0) continue;
        const owner = riggerOf(sim, bot);
        if (!owner || owner.spawnT > 0) continue;
        const pulling = owner.state === 'pull';
        const k = pulling ? Math.min(1, owner.t / PULL_TELL) : 0;
        const from = { x: owner.pos.x, y: owner.pos.y - 0.9 }; // the drone hangs up high
        const sag = Math.min(1.2, Math.hypot(bot.pos.x - from.x, bot.pos.y - from.y) * 0.08) * (1 - k);
        const mid = { x: (from.x + bot.pos.x) / 2, y: (from.y + bot.pos.y) / 2 + sag };
        ctx.strokeStyle = pulling ? `rgba(248,113,113,${0.5 + 0.5 * k})` : 'rgba(203,213,225,0.7)';
        ctx.lineWidth = pulling ? 0.08 + 0.06 * k : 0.06;
        ctx.beginPath(); ctx.moveTo(from.x, from.y); ctx.quadraticCurveTo(mid.x, mid.y, bot.pos.x, bot.pos.y - 0.3); ctx.stroke();
        if (pulling) { // the bot's sight line during the pull
            const p = sim.player.pos;
            ctx.strokeStyle = `rgba(248,113,113,${0.15 + 0.5 * k})`;
            ctx.lineWidth = 0.04;
            ctx.setLineDash([0.25, 0.2]);
            ctx.beginPath(); ctx.moveTo(bot.pos.x, bot.pos.y); ctx.lineTo(p.x, p.y); ctx.stroke();
            ctx.setLineDash([]);
        }
    }
}

/** A springer mid-hop: a red ring on the floor where it will land. */
export function drawLandings(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const e of sim.enemies) {
        if (e.kind !== 'springer' || e.state !== 'hop' || !e.hop) continue;
        const pulse = 0.5 + 0.4 * Math.sin(sim.time * 30);
        ctx.strokeStyle = `rgba(248,113,113,${pulse})`;
        ctx.lineWidth = 0.08;
        ctx.beginPath(); ctx.arc(e.hop.to.x, e.hop.to.y, LAND_R, 0, TAU); ctx.stroke();
        ctx.fillStyle = 'rgba(248,113,113,0.12)';
        ctx.fill();
    }
}

// The rigger: a flat drone with two cable spools; its eye goes red during a pull.
export function drawRigger(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy, r: number, white: boolean): void {
    const pulling = e.state === 'pull';
    ctx.fillStyle = white ? '#fff' : '#475569';
    ctx.fillRect(-r * 1.4, -r * 0.5, r * 2.8, r);
    for (const s of [-1, 1]) { // spools, spinning while it reels
        ctx.save();
        ctx.translate(s * r * 1.2, 0);
        ctx.rotate(sim.time * (pulling ? 20 : 3) * s);
        ctx.fillStyle = white ? '#fff' : DARK;
        ctx.beginPath(); ctx.arc(0, 0, r * 0.45, 0, TAU); ctx.fill();
        ctx.strokeStyle = '#cbd5e1';
        ctx.lineWidth = 0.05;
        ctx.beginPath(); ctx.moveTo(-r * 0.35, 0); ctx.lineTo(r * 0.35, 0); ctx.stroke();
        ctx.restore();
    }
    ctx.fillStyle = white ? '#fff' : STEEL;
    ctx.beginPath(); ctx.arc(0, 0, r * 0.6, 0, TAU); ctx.fill();
    ctx.fillStyle = pulling ? '#ef4444' : '#38bdf8';
    ctx.beginPath(); ctx.arc(Math.cos(e.facing) * r * 0.25, Math.sin(e.facing) * r * 0.25, 0.14, 0, TAU); ctx.fill();
    ctx.strokeStyle = 'rgba(226,232,240,0.5)'; // rotors
    ctx.lineWidth = 0.05;
    const a = sim.time * 35;
    for (const s of [-1, 1]) {
        ctx.beginPath(); ctx.moveTo(s * r * 1.2 + Math.cos(a) * 0.5, -r * 0.9 + Math.sin(a) * 0.15); ctx.lineTo(s * r * 1.2 - Math.cos(a) * 0.5, -r * 0.9 - Math.sin(a) * 0.15); ctx.stroke();
    }
}

// A rig bot: a boxy walker with an antenna hook where its cable clips on; its gun glows in a pull.
export function drawRigbot(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy, r: number, white: boolean): void {
    const step = Math.sin(sim.time * 10 + e.id) * 0.08;
    ctx.fillStyle = white ? '#fff' : DARK; // legs
    ctx.fillRect(-r * 0.6, r * 0.4 + step, r * 0.35, r * 0.5);
    ctx.fillRect(r * 0.25, r * 0.4 - step, r * 0.35, r * 0.5);
    ctx.fillStyle = white ? '#fff' : '#57534e';
    ctx.fillRect(-r * 0.8, -r * 0.7, r * 1.6, r * 1.2);
    ctx.fillStyle = white ? '#fff' : '#78716c';
    ctx.fillRect(-r * 0.8, -r * 0.7, r * 1.6, r * 0.3);
    ctx.save();
    ctx.rotate(e.facing);
    ctx.fillStyle = e.state === 'aim' ? `rgb(248,${113 + 60 * Math.sin(sim.time * 30)},113)` : '#334155';
    ctx.fillRect(r * 0.3, -0.08, r * 0.9, 0.16); // gun
    ctx.restore();
    ctx.fillStyle = '#ef4444';
    ctx.fillRect(-r * 0.5, -r * 0.35, r, 0.1); // visor slit
    ctx.strokeStyle = '#cbd5e1';
    ctx.lineWidth = 0.05;
    ctx.beginPath(); ctx.moveTo(0, -r * 0.7); ctx.lineTo(0, -r * 1.1); ctx.arc(0, -r * 1.2, 0.1, Math.PI / 2, Math.PI * 2.2); ctx.stroke();
}

// The springer: a dome bot on a big coil spring; squashed between hops, stretched mid-air.
export function drawSpringer(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy, r: number, white: boolean): void {
    const k = e.state === 'hop' ? 1.4 : 0.7 + 0.15 * Math.sin(sim.time * 14); // spring length
    ctx.strokeStyle = white ? '#fff' : '#a8a29e';
    ctx.lineWidth = 0.07;
    ctx.beginPath();
    for (let i = 0; i <= 16; i++) {
        const t = i / 16;
        const x = Math.sin(t * TAU * 3) * r * 0.5;
        const y = r * 0.2 + t * r * k;
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    ctx.stroke();
    ctx.fillStyle = white ? '#fff' : '#9a3412';
    ctx.beginPath(); ctx.arc(0, 0, r, Math.PI, 0); ctx.lineTo(r, r * 0.3); ctx.lineTo(-r, r * 0.3); ctx.fill();
    ctx.fillStyle = '#fde047';
    ctx.beginPath(); ctx.arc(Math.cos(e.facing) * r * 0.35, -r * 0.35, 0.12, 0, TAU); ctx.fill();
    if (e.state === 'rest' && e.t < 0.5) { // just landed: recoiling — open
        ctx.strokeStyle = 'rgba(253,224,71,0.8)';
        ctx.lineWidth = 0.05;
        ctx.beginPath(); ctx.arc(0, 0, r + 0.2, 0, TAU); ctx.stroke();
    }
}
