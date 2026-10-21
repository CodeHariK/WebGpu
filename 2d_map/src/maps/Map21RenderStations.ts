// Combat Sandbox — drawing a production route (Toy Assembly Plant): the inner walls between
// stations, each station's name painted big on its floor, the set dressing (cages of captured
// clowns, stuffing piles, stacked boxes), the paint sprayers, stamp rows, and you when painted.
// Pure drawing: reads the Sim, changes nothing.
import { SPRAY_HALF, SPRAY_TIME, SPRAY_WARN, sprayColor, sprayState } from './Map21Paint';
import type { PropDef } from './Map21Levels';
import { headState, stampHead } from './Map21Stamps';
import type { Sim } from './Map21State';

const TAU = Math.PI * 2;

// --- floor: station names and props ------------------------------------------------------------

export function drawStations(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const def = sim.arena.def;
    (def.zones ?? []).forEach((z, i) => {
        const r = z.rect;
        // A coloured band along the station's edge, like floor tape, and its number and name.
        const hue = ['#f472b6', '#facc15', '#22d3ee', '#a78bfa', '#4ade80'][i % 5];
        ctx.strokeStyle = hue;
        ctx.globalAlpha = 0.35;
        ctx.lineWidth = 0.18;
        ctx.strokeRect(r.x + 0.35, r.y + 0.35, r.w - 0.7, r.h - 0.7);
        ctx.globalAlpha = 0.16;
        ctx.fillStyle = hue;
        ctx.textAlign = 'center';
        const size = Math.min(2.6, r.h * 0.28);
        ctx.font = `900 ${size}px Inter, sans-serif`;
        ctx.fillText(`${String(i + 1).padStart(2, '0')} ${z.name}`, r.x + r.w / 2, r.y + r.h / 2 + size * 0.35);
        ctx.globalAlpha = 1;
    });
    for (const p of def.props ?? []) drawProp(ctx, sim, p);
}

function drawProp(ctx: CanvasRenderingContext2D, sim: Sim, p: PropDef): void {
    if (p.kind === 'cage') drawCage(ctx, sim, p);
    else if (p.kind === 'fluff') drawFluff(ctx, p);
    else drawBoxes(ctx, p);
}

// A cage of captured clowns: a dark floor, clown faces peeking, bars over them.
function drawCage(ctx: CanvasRenderingContext2D, sim: Sim, p: PropDef): void {
    ctx.fillStyle = 'rgba(0,0,0,0.4)';
    ctx.fillRect(p.x + 0.15, p.y + 0.25, p.w, p.h);
    ctx.fillStyle = '#1e1a24';
    ctx.fillRect(p.x, p.y, p.w, p.h);
    const n = Math.max(1, Math.floor(p.w / 0.9));
    for (let i = 0; i < n; i++) {
        const cx = p.x + (i + 0.5) * (p.w / n);
        const cy = p.y + p.h * 0.55 + Math.sin(sim.time * 2 + i * 1.7 + p.y) * 0.06; // fidgeting
        ctx.fillStyle = '#fff7ed';
        ctx.beginPath(); ctx.arc(cx, cy, 0.3, 0, TAU); ctx.fill();
        ctx.fillStyle = '#ef4444';
        ctx.beginPath(); ctx.arc(cx, cy + 0.04, 0.08, 0, TAU); ctx.fill(); // red nose
        ctx.fillStyle = ['#f472b6', '#facc15', '#22d3ee'][i % 3]; // a tuft of hair
        ctx.beginPath(); ctx.arc(cx - 0.2, cy - 0.22, 0.12, 0, TAU); ctx.arc(cx + 0.2, cy - 0.22, 0.12, 0, TAU); ctx.fill();
        ctx.fillStyle = '#0f0a14';
        ctx.fillRect(cx - 0.12, cy - 0.1, 0.05, 0.05);
        ctx.fillRect(cx + 0.07, cy - 0.1, 0.05, 0.05);
    }
    ctx.strokeStyle = '#9ca3af';
    ctx.lineWidth = 0.07;
    for (let x = p.x + 0.2; x < p.x + p.w; x += 0.32) { ctx.beginPath(); ctx.moveTo(x, p.y); ctx.lineTo(x, p.y + p.h); ctx.stroke(); }
    ctx.strokeStyle = '#6b7280';
    ctx.lineWidth = 0.12;
    ctx.strokeRect(p.x, p.y, p.w, p.h);
}

// A pile of stuffing: overlapping soft white puffs.
function drawFluff(ctx: CanvasRenderingContext2D, p: PropDef): void {
    const n = Math.round(p.w * p.h * 2.5);
    for (let i = 0; i < n; i++) {
        const fx = p.x + ((i * 0.618) % 1) * p.w;
        const fy = p.y + ((i * 0.381 + 0.17) % 1) * p.h;
        const r = 0.25 + ((i * 0.27) % 1) * 0.25;
        ctx.fillStyle = i % 4 === 0 ? 'rgba(203,213,225,0.85)' : 'rgba(248,250,252,0.85)';
        ctx.beginPath(); ctx.arc(fx, fy, r, 0, TAU); ctx.fill();
    }
}

// Packed toys waiting to ship: a row of cardboard boxes with tape and a "this way up" mark.
function drawBoxes(ctx: CanvasRenderingContext2D, p: PropDef): void {
    for (let x = p.x; x + 1.1 <= p.x + p.w + 0.01; x += 1.2) {
        ctx.fillStyle = 'rgba(0,0,0,0.35)';
        ctx.fillRect(x + 0.12, p.y + 0.2, 1.1, p.h);
        ctx.fillStyle = '#9a6b3a';
        ctx.fillRect(x, p.y, 1.1, p.h);
        ctx.fillStyle = '#c9965c';
        ctx.fillRect(x, p.y - 0.3, 1.1, p.h - 0.2);
        ctx.fillStyle = '#e8d3a8';
        ctx.fillRect(x + 0.48, p.y - 0.3, 0.14, p.h - 0.2);
    }
}

// --- inner walls ---------------------------------------------------------------------------------

export function drawInnerWalls(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const w of sim.arena.def.walls ?? []) {
        ctx.fillStyle = 'rgba(0,0,0,0.4)';
        ctx.fillRect(w.x + 0.25, w.y + 0.35, w.w, w.h);
        ctx.fillStyle = '#231c2e';
        ctx.fillRect(w.x, w.y, w.w, w.h);
        ctx.fillStyle = '#3a3048';
        ctx.fillRect(w.x, w.y - 0.9, w.w, w.h); // its top, drawn up (it's tall)
        ctx.fillStyle = '#f0abfc';
        ctx.fillRect(w.x, w.y - 0.9, w.w, 0.08);
        ctx.fillRect(w.x, w.y + w.h - 0.98, w.w, 0.08);
    }
}

// --- paint sprayers -----------------------------------------------------------------------------

export function drawSprayers(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const s of sim.arena.sprayers) {
        const col = sprayColor(sim, s);
        const state = s.off ? 'idle' : sprayState(s);
        const a = Math.atan2(s.dir.y, s.dir.x);
        ctx.save();
        ctx.translate(s.pos.x, s.pos.y);
        ctx.rotate(a);
        if (state === 'spray') { // the band of paint
            const k = (s.t % s.period) / SPRAY_TIME;
            const g = ctx.createLinearGradient(0, 0, s.len, 0);
            g.addColorStop(0, col);
            g.addColorStop(1, 'rgba(255,255,255,0)');
            ctx.fillStyle = g;
            ctx.globalAlpha = 0.55 * Math.min(1, (1 - k) * 4);
            ctx.fillRect(0, -SPRAY_HALF, s.len, SPRAY_HALF * 2);
            ctx.globalAlpha = 0.8;
            for (let i = 0; i < 10; i++) { // droplets
                const d = ((sim.time * 9 + i * 1.3) % s.len);
                ctx.fillStyle = col;
                ctx.beginPath(); ctx.arc(d, Math.sin(i * 2.3 + sim.time * 5) * SPRAY_HALF * 0.8, 0.1, 0, TAU); ctx.fill();
            }
            ctx.globalAlpha = 1;
        } else if (state === 'warn') { // dashed outline of where it's about to go, and drips
            const k = 1 - ((s.t % s.period) - (s.period - SPRAY_WARN)) / SPRAY_WARN;
            ctx.strokeStyle = col;
            ctx.globalAlpha = 0.4 + 0.4 * Math.sin(sim.time * 25);
            ctx.lineWidth = 0.07;
            ctx.setLineDash([0.4, 0.3]);
            ctx.strokeRect(0, -SPRAY_HALF, s.len * (1 - k * 0.5), SPRAY_HALF * 2);
            ctx.setLineDash([]);
            ctx.globalAlpha = 1;
        } else if (!s.off) { // a faint stain where it always sprays
            ctx.fillStyle = col;
            ctx.globalAlpha = 0.08;
            ctx.fillRect(0, -SPRAY_HALF, s.len, SPRAY_HALF * 2);
            ctx.globalAlpha = 1;
        }
        // The nozzle on the wall.
        ctx.fillStyle = s.off ? '#3f3f46' : '#52525b';
        ctx.fillRect(-0.5, -0.45, 0.8, 0.9);
        ctx.fillStyle = s.off ? '#27272a' : col;
        ctx.fillRect(0.2, -0.2, 0.35, 0.4);
        ctx.restore();
    }
}

/** You, while painted: drips of paint and a colour ring that every toy can see. */
export function drawPainted(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const p = sim.player;
    if (p.paintT <= 0) return;
    const col = '#f472b6';
    ctx.strokeStyle = col;
    ctx.globalAlpha = 0.5 + 0.4 * Math.sin(sim.time * 10);
    ctx.lineWidth = 0.12;
    ctx.beginPath(); ctx.arc(p.pos.x, p.pos.y, 0.85, 0, TAU); ctx.stroke();
    ctx.globalAlpha = 1;
    ctx.fillStyle = col;
    for (let i = 0; i < 4; i++) {
        const dy = ((sim.time * 1.5 + i * 0.25) % 1) * 0.6;
        ctx.beginPath(); ctx.arc(p.pos.x - 0.3 + i * 0.2, p.pos.y + 0.2 + dy, 0.07, 0, TAU); ctx.fill();
    }
}

// --- stamp rows ---------------------------------------------------------------------------------

/** Stamp heads: a hazard-edged plate on the floor under each; a heavy head hanging over it that
 *  glows red and drops as its turn comes, then slams (white flash) — a wave running down the row. */
export function drawStamps(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const s of sim.arena.stamps) {
        for (let i = 0; i < s.n; i++) {
            const h = stampHead(s, i);
            const state = headState(s, i);
            const pulse = 0.5 + 0.5 * Math.sin(sim.time * 30);
            // The plate.
            ctx.fillStyle = state === 'warn' ? `rgba(239,68,68,${0.35 + 0.3 * pulse})` : state === 'slam' ? 'rgba(255,255,255,0.5)' : '#1c1a22';
            ctx.fillRect(h.x + 0.05, h.y, h.w - 0.1, h.h);
            ctx.strokeStyle = '#facc15';
            ctx.lineWidth = 0.08;
            ctx.setLineDash([0.3, 0.2]);
            ctx.strokeRect(h.x + 0.12, h.y + 0.12, h.w - 0.24, h.h - 0.24);
            ctx.setLineDash([]);
            if (s.off) continue; // stopped: heads parked up out of the way
            // The head: inset, hanging higher the further it is from its turn.
            const lift = state === 'slam' ? 0 : state === 'warn' ? 0.35 : 0.9;
            const inset = 0.25;
            const hx = h.x + inset;
            const hw = h.w - inset * 2;
            const hy = h.y + inset;
            const hh = h.h - inset * 2;
            ctx.fillStyle = 'rgba(0,0,0,0.4)';
            ctx.fillRect(hx + lift * 0.4, hy + lift * 0.6, hw, hh);
            ctx.fillStyle = state === 'slam' ? '#e2e8f0' : state === 'warn' ? `rgb(${120 + 100 * pulse},70,80)` : '#3f3a4a';
            ctx.fillRect(hx, hy - lift, hw, hh);
            ctx.fillStyle = state === 'warn' ? '#fca5a5' : '#5b5568';
            ctx.fillRect(hx, hy - lift, hw, 0.18);
            ctx.fillStyle = '#facc15';
            ctx.fillRect(hx, hy - lift + hh - 0.14, hw, 0.14);
            ctx.fillStyle = '#f0abfc'; // the stamp's face: a toy-shaped mould
            ctx.beginPath(); ctx.arc(h.x + h.w / 2, h.y + h.h / 2 - lift, Math.min(hw, hh) * 0.28, 0, TAU); ctx.fill();
        }
    }
}
