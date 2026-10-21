// Wick — drawing a frame: the valley (Map22RenderWorld), then everyone in it, then the DARK laid
// over the top with holes cut where the light is, then what you can see in the dark anyway (eyes),
// then the HUD. Pure drawing: reads the Sim, changes nothing.
import { RISE_TIME, TELL_TIME, weeperFrozen } from './Map22Enemies';
import { type Light, lights, playerRadius } from './Map22Light';
import type { Enemy, Sim, Vec } from './Map22State';
import { LEVELS } from './Map22Levels';
import { drawCandles, drawFogWalls, drawGround, drawGusts, drawLandmark, drawLanterns, drawProps, drawVats, drawWater } from './Map22RenderWorld';
import { drawBlobs, drawCreature } from './Map22RenderCreatures';

const TAU = Math.PI * 2;
let darkCanvas: HTMLCanvasElement | null = null;
const DARK_RES = 256; // the dark is drawn small and stretched: soft edges for free

export function drawSim(ctx: CanvasRenderingContext2D, sim: Sim, mouse: Vec): void {
    const w = ctx.canvas.width;
    const v = sim.view;
    const k = w / v.span;
    const sh = sim.shake * sim.shake * 0.5;
    const ox = (Math.random() - 0.5) * sh, oy = (Math.random() - 0.5) * sh;
    ctx.setTransform(k, 0, 0, k, (ox - (v.x - v.span / 2)) * k, (oy - (v.y - v.span / 2)) * k);

    drawWater(ctx, sim);
    drawGround(ctx, sim);
    drawVats(ctx, sim);
    drawEmbers(ctx, sim);
    drawBlobs(ctx, sim);
    drawProps(ctx, sim);
    drawLandmark(ctx, sim);
    drawCandles(ctx, sim);
    drawLanterns(ctx, sim);
    for (const e of sim.enemies) drawEnemy(ctx, sim, e);
    drawPlayer(ctx, sim);
    drawFlames(ctx, sim);
    drawParticles(ctx, sim);
    drawFogWalls(ctx, sim);
    drawGusts(ctx, sim);

    ctx.setTransform(1, 0, 0, 1, 0, 0);
    const all = lights(sim);
    drawDark(ctx, sim, all);
    drawGlow(ctx, sim, all, k, ox, oy);
    ctx.setTransform(k, 0, 0, k, (ox - (v.x - v.span / 2)) * k, (oy - (v.y - v.span / 2)) * k);
    drawEyes(ctx, sim);
    drawFloaters(ctx, sim);
    drawCrosshair(ctx, mouse);
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    drawHud(ctx, sim);
}

// --- the dark --------------------------------------------------------------------------------

function drawDark(ctx: CanvasRenderingContext2D, sim: Sim, all: Light[]): void {
    if (!darkCanvas) { darkCanvas = document.createElement('canvas'); darkCanvas.width = DARK_RES; darkCanvas.height = DARK_RES; }
    const d = darkCanvas.getContext('2d')!;
    const v = sim.view;
    const k = DARK_RES / v.span;
    d.globalCompositeOperation = 'source-over';
    d.clearRect(0, 0, DARK_RES, DARK_RES);
    d.fillStyle = 'rgba(2,3,9,0.95)';
    d.fillRect(0, 0, DARK_RES, DARK_RES);
    d.globalCompositeOperation = 'destination-out';
    for (const l of all) {
        if (l.k < 0) continue;
        const x = (l.pos.x - (v.x - v.span / 2)) * k;
        const y = (l.pos.y - (v.y - v.span / 2)) * k;
        const r = l.r * k * (1 + Math.sin(sim.time * 11 + l.pos.x) * 0.015); // a flicker
        const g = d.createRadialGradient(x, y, 0, x, y, r);
        g.addColorStop(0, `rgba(0,0,0,${l.k})`);
        g.addColorStop(0.55, `rgba(0,0,0,${0.85 * l.k})`);
        g.addColorStop(1, 'rgba(0,0,0,0)');
        d.fillStyle = g;
        d.beginPath(); d.arc(x, y, r, 0, TAU); d.fill();
    }
    d.globalCompositeOperation = 'source-over'; // glooms put the dark back
    for (const l of all) {
        if (l.k >= 0) continue;
        const x = (l.pos.x - (v.x - v.span / 2)) * k;
        const y = (l.pos.y - (v.y - v.span / 2)) * k;
        const r = l.r * k;
        const g = d.createRadialGradient(x, y, 0, x, y, r);
        g.addColorStop(0, `rgba(2,3,9,${-l.k})`);
        g.addColorStop(1, 'rgba(2,3,9,0)');
        d.fillStyle = g;
        d.beginPath(); d.arc(x, y, r, 0, TAU); d.fill();
    }
    ctx.imageSmoothingEnabled = true;
    ctx.drawImage(darkCanvas, 0, 0, ctx.canvas.width, ctx.canvas.height);
}

// Warm light added on top of the lit places.
function drawGlow(ctx: CanvasRenderingContext2D, sim: Sim, all: Light[], k: number, ox: number, oy: number): void {
    const v = sim.view;
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    for (const l of all) {
        if (l.k < 0) continue;
        const x = (l.pos.x + ox - (v.x - v.span / 2)) * k;
        const y = (l.pos.y + oy - (v.y - v.span / 2)) * k;
        const r = l.r * k * 0.8;
        const g = ctx.createRadialGradient(x, y, 0, x, y, r);
        g.addColorStop(0, 'rgba(255,170,80,0.22)');
        g.addColorStop(1, 'rgba(255,140,60,0)');
        ctx.fillStyle = g;
        ctx.beginPath(); ctx.arc(x, y, r, 0, TAU); ctx.fill();
    }
    ctx.restore();
}

// In the dark you still see their eyes.
function drawEyes(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const e of sim.enemies) {
        if (e.kind === 'moth' || e.kind === 'hand' || e.kind === 'leech') continue;
        if (e.kind === 'gloom') { // its rim shows in the dark
            ctx.strokeStyle = 'rgba(167,139,250,0.7)';
            ctx.lineWidth = 0.05;
            ctx.beginPath(); ctx.arc(e.pos.x, e.pos.y - 1, 0.42, 0, TAU); ctx.stroke();
            continue;
        }
        const rising = e.spawnT > 0 ? 1 - e.spawnT / RISE_TIME : 1;
        const up = e.kind === 'weeper' ? 2.1 : e.kind === 'snuffer' ? 0.95 : 0.55;
        const x = e.pos.x, y = e.pos.y - up * rising;
        if (e.kind === 'weeper') { // a pale face, hands over it, faintly
            ctx.fillStyle = `rgba(226,232,240,${weeperFrozen(e) ? 0 : 0.22 * rising})`;
            ctx.beginPath(); ctx.ellipse(x, y, 0.28, 0.36, 0, 0, TAU); ctx.fill();
            continue;
        }
        const tell = e.state === 'tell' ? Math.min(1, e.t / TELL_TIME) : 0;
        const a = e.facing;
        const ex = Math.cos(a) * 0.12, ey = Math.sin(a) * 0.08;
        ctx.fillStyle = tell > 0 ? `rgb(255,${200 - 160 * tell},${200 - 160 * tell})` : `rgba(196,181,253,${0.85 * rising})`;
        for (const s of [-1, 1]) { ctx.beginPath(); ctx.ellipse(x + ex + s * 0.14, y + ey, 0.07 + tell * 0.04, 0.05 + tell * 0.03, 0, 0, TAU); ctx.fill(); }
    }
}

// --- you -------------------------------------------------------------------------------------

function drawPlayer(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const p = sim.player;
    if (sim.phase === 'snuffed') return;
    const { x, y } = p.pos;
    const s = sim.settings;
    const melt = Math.max(0, p.wax) / s.maxWax;
    const h = 0.35 + 0.75 * melt; // the candle gets shorter as the wax goes
    const hurt = p.hurtT > 0 && Math.sin(sim.time * 40) > 0;
    ctx.fillStyle = 'rgba(0,0,0,0.4)';
    ctx.beginPath(); ctx.ellipse(x, y + 0.05, 0.42, 0.18, 0, 0, TAU); ctx.fill();
    ctx.fillStyle = hurt ? '#fecaca' : '#efe3c4';
    ctx.fillRect(x - 0.3, y - h, 0.6, h);
    ctx.fillStyle = '#f7efd9';
    ctx.beginPath(); ctx.ellipse(x, y - h, 0.3, 0.12, 0, 0, TAU); ctx.fill();
    ctx.fillStyle = '#e3d3ad'; // drips down the side
    ctx.fillRect(x - 0.3, y - h, 0.1, h * 0.5);
    ctx.fillRect(x + 0.12, y - h, 0.08, h * 0.3);
    ctx.fillStyle = '#3b2f24'; // little face
    ctx.fillRect(x - 0.13, y - h * 0.55, 0.06, 0.08);
    ctx.fillRect(x + 0.07, y - h * 0.55, 0.06, 0.08);
    // The wick and the flame, leaning the way you aim.
    ctx.strokeStyle = '#1c1917';
    ctx.lineWidth = 0.04;
    ctx.beginPath(); ctx.moveTo(x, y - h); ctx.lineTo(x, y - h - 0.12); ctx.stroke();
    const flick = Math.sin(sim.time * 17) * 0.03;
    const lean = Math.cos(p.aim) * 0.08;
    const fh = 0.35 + 0.25 * melt + (p.flareT > 0 ? 0.4 : 0);
    ctx.fillStyle = '#fb923c';
    ctx.beginPath(); ctx.moveTo(x - 0.12, y - h - 0.12); ctx.quadraticCurveTo(x + lean, y - h - 0.12 - fh * 1.4, x + 0.12, y - h - 0.12); ctx.fill();
    ctx.fillStyle = '#fde68a';
    ctx.beginPath(); ctx.ellipse(x + lean * 0.5 + flick, y - h - 0.2, 0.06, 0.12 + fh * 0.2, 0, 0, TAU); ctx.fill();
    if (p.kindle > 0) { // kindling a lantern
        ctx.strokeStyle = '#fde047';
        ctx.lineWidth = 0.07;
        ctx.beginPath(); ctx.arc(x, y - h / 2, 0.7, -Math.PI / 2, -Math.PI / 2 + TAU * Math.min(1, p.kindle / 0.7)); ctx.stroke();
    }
}

// --- the things in the dark --------------------------------------------------------------------

function drawEnemy(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    const rising = e.spawnT > 0 ? 1 - e.spawnT / RISE_TIME : 1;
    ctx.globalAlpha = rising;
    if (e.kind === 'shade') drawShade(ctx, sim, e);
    else if (e.kind === 'moth') drawMoth(ctx, sim, e);
    else if (e.kind === 'weeper') drawWeeper(ctx, e);
    else drawCreature(ctx, sim, e);
    ctx.globalAlpha = 1;
}

// A shade: a ragged blob of dark with smoky tendrils, burning orange at the edges in light.
function drawShade(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    const { x, y } = e.pos;
    const r = 0.55;
    const lunge = e.state === 'lunge';
    ctx.fillStyle = e.flash > 0 ? '#e2e8f0' : '#0e0b17';
    ctx.beginPath();
    for (let i = 0; i <= 14; i++) {
        const a = (i / 14) * TAU;
        const rr = r * (1 + 0.25 * Math.sin(a * 3 + sim.time * 6 + e.id) + (lunge ? 0.2 : 0));
        const px = x + Math.cos(a) * rr * (lunge ? 1.4 : 1), py = y - 0.4 + Math.sin(a) * rr;
        if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
    }
    ctx.fill();
    for (let i = 0; i < 3; i++) { // trailing tendrils
        const a = e.facing + Math.PI + (i - 1) * 0.5;
        ctx.strokeStyle = 'rgba(14,11,23,0.8)';
        ctx.lineWidth = 0.12;
        ctx.beginPath(); ctx.moveTo(x, y - 0.3);
        ctx.quadraticCurveTo(x + Math.cos(a) * 0.6, y - 0.3 + Math.sin(a) * 0.6 + Math.sin(sim.time * 5 + i) * 0.2, x + Math.cos(a) * 1.1, y - 0.2 + Math.sin(a) * 1.1);
        ctx.stroke();
    }
    if (e.lit > 0.3) { // burning
        ctx.strokeStyle = `rgba(249,115,22,${Math.min(1, e.lit)})`;
        ctx.lineWidth = 0.06;
        ctx.beginPath(); ctx.arc(x, y - 0.4, r * 1.1, 0, TAU); ctx.stroke();
    }
}

function drawMoth(ctx: CanvasRenderingContext2D, sim: Sim, e: Enemy): void {
    const { x, y } = e.pos;
    const flap = Math.abs(Math.sin(sim.time * 30 + e.id));
    ctx.save();
    ctx.translate(x, y - 0.6);
    ctx.rotate(e.facing + Math.PI / 2);
    ctx.fillStyle = e.state === 'cling' ? '#a8a29e' : '#d6cfc4';
    for (const s of [-1, 1]) { ctx.beginPath(); ctx.ellipse(s * 0.15 * flap, 0, 0.16 * flap + 0.04, 0.12, s * 0.4, 0, TAU); ctx.fill(); }
    ctx.fillStyle = '#57534e';
    ctx.fillRect(-0.03, -0.1, 0.06, 0.2);
    ctx.restore();
}

// The weeper: tall and thin, hands over its face. Frozen in light it's grey stone; moving in the
// dark it's a dark smear (and you mostly see its face).
function drawWeeper(ctx: CanvasRenderingContext2D, e: Enemy): void {
    const { x, y } = e.pos;
    const frozen = weeperFrozen(e);
    const body = e.flash > 0 ? '#fff' : frozen ? '#8b8f94' : '#191622';
    ctx.fillStyle = 'rgba(0,0,0,0.4)';
    ctx.beginPath(); ctx.ellipse(x, y + 0.05, 0.4, 0.15, 0, 0, TAU); ctx.fill();
    ctx.fillStyle = body;
    ctx.beginPath(); ctx.moveTo(x - 0.35, y); ctx.lineTo(x - 0.18, y - 1.8); ctx.lineTo(x + 0.18, y - 1.8); ctx.lineTo(x + 0.35, y); ctx.fill(); // robe
    ctx.beginPath(); ctx.ellipse(x, y - 2.1, 0.26, 0.34, 0, 0, TAU); ctx.fill(); // head
    ctx.fillStyle = frozen ? '#6b7076' : '#2a2535'; // hands over the face
    ctx.fillRect(x - 0.24, y - 2.15, 0.2, 0.28);
    ctx.fillRect(x + 0.04, y - 2.15, 0.2, 0.28);
    if (frozen) { // cracks, and how close it is to breaking
        ctx.strokeStyle = 'rgba(30,30,30,0.6)';
        ctx.lineWidth = 0.03;
        ctx.beginPath(); ctx.moveTo(x - 0.1, y - 1.5); ctx.lineTo(x + 0.05, y - 1.1); ctx.lineTo(x - 0.05, y - 0.7); ctx.stroke();
        ctx.fillStyle = 'rgba(0,0,0,0.5)';
        ctx.fillRect(x - 0.5, y - 2.65, 1, 0.1);
        ctx.fillStyle = '#e2e8f0';
        ctx.fillRect(x - 0.5, y - 2.65, Math.max(0, e.hp / e.maxHp), 0.1);
    } else {
        ctx.strokeStyle = 'rgba(25,22,34,0.5)'; // smear behind it as it moves
        ctx.lineWidth = 0.4;
        ctx.beginPath(); ctx.moveTo(x, y - 1); ctx.lineTo(x - Math.cos(e.facing) * 0.8, y - 1 - Math.sin(e.facing) * 0.8); ctx.stroke();
    }
}

// --- flames, embers, particles -------------------------------------------------------------------

function drawFlames(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const f of sim.flames) {
        const a = Math.atan2(f.vel.y, f.vel.x);
        ctx.fillStyle = 'rgba(251,146,60,0.5)';
        ctx.beginPath(); ctx.moveTo(f.pos.x - Math.cos(a) * 0.7, f.pos.y - 0.4 - Math.sin(a) * 0.7); ctx.lineTo(f.pos.x + Math.cos(a + 1.6) * 0.15, f.pos.y - 0.4 + Math.sin(a + 1.6) * 0.15); ctx.lineTo(f.pos.x + Math.cos(a - 1.6) * 0.15, f.pos.y - 0.4 + Math.sin(a - 1.6) * 0.15); ctx.fill();
        ctx.fillStyle = '#fde68a';
        ctx.beginPath(); ctx.arc(f.pos.x, f.pos.y - 0.4, 0.15 + Math.sin(sim.time * 30) * 0.02, 0, TAU); ctx.fill();
    }
}

function drawEmbers(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const e of sim.embers) {
        const k = e.life / e.maxLife;
        const flick = 0.75 + 0.25 * Math.sin(sim.time * 14 + e.pos.x * 3);
        ctx.fillStyle = `rgba(120,53,15,${0.6 * k + 0.2})`;
        ctx.beginPath(); ctx.ellipse(e.pos.x, e.pos.y, 0.35, 0.18, 0, 0, TAU); ctx.fill();
        ctx.fillStyle = `rgba(251,146,60,${flick * (0.4 + 0.6 * k)})`;
        for (const [dx, dy] of [[-0.12, 0], [0.1, -0.04], [0, 0.05]]) { ctx.beginPath(); ctx.arc(e.pos.x + dx, e.pos.y + dy, 0.07, 0, TAU); ctx.fill(); }
        if (k > 0.3) { // a little flame still going
            ctx.fillStyle = `rgba(253,224,71,${flick * k})`;
            ctx.beginPath(); ctx.ellipse(e.pos.x, e.pos.y - 0.2, 0.08, 0.18 * k, 0, 0, TAU); ctx.fill();
        }
    }
}

function drawParticles(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const p of sim.particles) {
        const k = p.life / p.maxLife;
        ctx.globalAlpha = Math.min(1, k * 1.5);
        if (p.kind === 'ring') {
            ctx.strokeStyle = p.color;
            ctx.lineWidth = 0.08 * k + 0.02;
            ctx.beginPath(); ctx.arc(p.pos.x, p.pos.y, p.size * (1 - k * k), 0, TAU); ctx.stroke();
        } else if (p.kind === 'smoke') {
            ctx.globalAlpha = k * 0.7;
            ctx.fillStyle = p.color;
            ctx.beginPath(); ctx.arc(p.pos.x, p.pos.y, p.size * (1.6 - k), 0, TAU); ctx.fill();
        } else {
            ctx.fillStyle = p.color;
            ctx.beginPath(); ctx.arc(p.pos.x, p.pos.y, p.size * (p.kind === 'spark' ? k : 1), 0, TAU); ctx.fill();
        }
    }
    ctx.globalAlpha = 1;
}

function drawFloaters(ctx: CanvasRenderingContext2D, sim: Sim): void {
    ctx.textAlign = 'center';
    for (const f of sim.floaters) {
        ctx.globalAlpha = Math.min(1, f.life * 2);
        const size = 0.5 * f.size;
        ctx.font = `800 ${size}px Inter, sans-serif`;
        ctx.lineWidth = size * 0.2;
        ctx.strokeStyle = 'rgba(2,3,9,0.9)';
        ctx.strokeText(f.text, f.pos.x, f.pos.y);
        ctx.fillStyle = f.color;
        ctx.fillText(f.text, f.pos.x, f.pos.y);
    }
    ctx.globalAlpha = 1;
}

function drawCrosshair(ctx: CanvasRenderingContext2D, m: Vec): void {
    ctx.strokeStyle = 'rgba(253,230,138,0.7)';
    ctx.lineWidth = 0.05;
    ctx.beginPath(); ctx.arc(m.x, m.y, 0.28, 0, TAU); ctx.stroke();
    ctx.beginPath(); ctx.arc(m.x, m.y, 0.05, 0, TAU); ctx.stroke();
}

// --- HUD (screen pixels) ---------------------------------------------------------------------

function drawHud(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const w = ctx.canvas.width;
    const p = sim.player;
    const s = sim.settings;
    // The wax: a candle that melts down.
    const melt = Math.max(0, p.wax) / s.maxWax;
    const ch = 70;
    ctx.fillStyle = 'rgba(0,0,0,0.5)';
    ctx.fillRect(14, 14, 26, ch + 30);
    ctx.fillStyle = '#efe3c4';
    ctx.fillRect(19, 24 + ch * (1 - melt), 16, ch * melt + 4);
    ctx.fillStyle = melt < 0.25 ? '#f87171' : '#fb923c';
    ctx.beginPath(); ctx.ellipse(27, 18 + ch * (1 - melt), 4, 8, 0, 0, TAU); ctx.fill();
    ctx.font = '700 12px Inter, sans-serif';
    ctx.textAlign = 'left';
    ctx.fillStyle = '#fde68a';
    ctx.fillText(`WAX ${Math.max(0, Math.round(p.wax))}`, 48, 28);
    ctx.fillStyle = p.flareCd <= 0 ? '#fde047' : 'rgba(253,224,71,0.35)';
    ctx.fillText(p.flareCd <= 0 ? 'FLARE ready (right click / E)' : 'flare…', 48, 46);
    ctx.fillStyle = '#a8a29e';
    ctx.fillText(`light ${playerRadius(sim).toFixed(1)} m`, 48, 64);
    // Where you are and what's left to do.
    const here = sim.clearings.find((c) => c.entered && !c.done && c.def.waves.length > 0);
    ctx.textAlign = 'right';
    ctx.font = '800 15px Inter, sans-serif';
    ctx.fillStyle = '#e2e8f0';
    ctx.fillText(`${LEVELS.indexOf(sim.level) + 1}. ${sim.level.name}`, w - 14, 26);
    ctx.font = '600 12px Inter, sans-serif';
    if (here) {
        const i = sim.clearings.indexOf(here);
        const ls = sim.lanterns.filter((l) => l.clearing === i);
        ctx.fillStyle = '#fde68a';
        ctx.fillText(`${here.def.name.toLowerCase()} · lanterns ${ls.filter((l) => l.lit).length}/${ls.length} · waves ${here.wave}/${here.def.waves.length}`, w - 14, 44);
    } else {
        const lit = sim.lanterns.filter((l) => l.lit).length;
        ctx.fillStyle = '#a8a29e';
        ctx.fillText(`lanterns lit ${lit}/${sim.lanterns.length} · shades ${sim.stats.shades} · weepers ${sim.stats.weepers}`, w - 14, 44);
    }
    if (sim.banner) {
        const b = sim.banner;
        ctx.globalAlpha = Math.min(1, b.life * 1.5);
        ctx.textAlign = 'center';
        const y = ctx.canvas.height * 0.32;
        ctx.font = '900 40px Georgia, serif';
        ctx.lineWidth = 7;
        ctx.strokeStyle = 'rgba(2,3,9,0.9)';
        ctx.strokeText(b.text, w / 2, y);
        ctx.fillStyle = b.text.startsWith('SNUFFED') ? '#a8a29e' : '#fde68a';
        ctx.fillText(b.text, w / 2, y);
        if (b.sub) {
            ctx.font = 'italic 600 17px Georgia, serif';
            ctx.lineWidth = 5;
            ctx.strokeText(b.sub, w / 2, y + 30);
            ctx.fillStyle = '#e7e5e4';
            ctx.fillText(b.sub, w / 2, y + 30);
        }
        ctx.globalAlpha = 1;
    }
}
