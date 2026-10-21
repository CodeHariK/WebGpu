// Lockpick — drawing a frame: the dungeon (stone floors, boards, glass, water, plates, walls,
// doors, barrels, torches, items), then the people in it, then the DARK (from the torch light, one
// value per tile, drawn small and stretched so it's soft), then what you need to read in the dark —
// the guards' sight cones, their alert marks, noise rings — and the HUD. Pure drawing.
import { lightAt } from './Map23Grid';
import { drawActors, drawCones, drawMarks } from './Map23RenderActors';
import { NOISE_LIFE } from './Map23Noise';
import { CREAK, GLASS, PLATE, type Sim, type Vec, WALL, WATER } from './Map23State';

const TAU = Math.PI * 2;
let darkCanvas: HTMLCanvasElement | null = null;
const DARK = 0.72; // how dark an unlit place is (0 … 1)

const hash = (x: number, y: number) => { const h = Math.sin(x * 127.1 + y * 311.7) * 43758.5453; return h - Math.floor(h); };

export function drawSim(ctx: CanvasRenderingContext2D, sim: Sim, mouse: Vec): void {
    const w = ctx.canvas.width;
    const v = sim.view;
    const k = w / v.span;
    const toWorld = () => ctx.setTransform(k, 0, 0, k, -(v.x - v.span / 2) * k, -(v.y - v.span / 2) * k);
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.fillStyle = '#050407';
    ctx.fillRect(0, 0, w, ctx.canvas.height);
    toWorld();
    const x0 = Math.max(0, Math.floor(v.x - v.span / 2) - 1), x1 = Math.min(sim.w - 1, Math.ceil(v.x + v.span / 2) + 1);
    const y0 = Math.max(0, Math.floor(v.y - v.span / 2) - 1), y1 = Math.min(sim.h - 1, Math.ceil(v.y + v.span / 2) + 1);
    drawFloors(ctx, sim, x0, x1, y0, y1);
    drawWalls(ctx, sim, x0, x1, y0, y1);
    drawDoors(ctx, sim);
    drawItems(ctx, sim);
    drawBarrels(ctx, sim);
    drawActors(ctx, sim);
    drawTorches(ctx, sim);
    drawPebbles(ctx, sim);
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    drawDark(ctx, sim, k);
    toWorld();
    drawFlames(ctx, sim);
    drawScent(ctx, sim);
    drawYou(ctx, sim);
    if (sim.settings.showCones) drawCones(ctx, sim);
    drawNoises(ctx, sim);
    drawMarks(ctx, sim);
    drawFloaters(ctx, sim);
    drawAim(ctx, sim, mouse);
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    drawHud(ctx, sim);
}

// --- the dungeon -------------------------------------------------------------------------------

function drawFloors(ctx: CanvasRenderingContext2D, sim: Sim, x0: number, x1: number, y0: number, y1: number): void {
    for (let y = y0; y <= y1; y++) {
        for (let x = x0; x <= x1; x++) {
            const t = sim.tiles[y * sim.w + x];
            if (t === WALL) continue;
            const h = hash(x, y);
            if (t === CREAK) { // old boards
                ctx.fillStyle = h < 0.5 ? '#4a3626' : '#433022';
                ctx.fillRect(x, y, 1, 1);
                ctx.fillStyle = 'rgba(0,0,0,0.35)';
                ctx.fillRect(x, y + 0.32, 1, 0.04);
                ctx.fillRect(x, y + 0.65, 1, 0.04);
                if (h > 0.8) { ctx.fillStyle = '#1a120b'; ctx.fillRect(x + 0.3, y + 0.4, 0.25, 0.12); } // a gap
                continue;
            }
            ctx.fillStyle = h < 0.33 ? '#34313a' : h < 0.66 ? '#302d36' : '#38343f'; // stone flags
            ctx.fillRect(x, y, 1, 1);
            ctx.strokeStyle = 'rgba(0,0,0,0.35)';
            ctx.lineWidth = 0.03;
            ctx.strokeRect(x + 0.02, y + 0.02, 0.96, 0.96);
            if (t === GLASS) {
                ctx.fillStyle = 'rgba(186,230,253,0.6)';
                for (let i = 0; i < 4; i++) {
                    const a = hash(x + i, y) * TAU, sx = x + 0.2 + hash(x, y + i) * 0.6, sy = y + 0.2 + hash(x + i * 3, y + i) * 0.6;
                    ctx.beginPath(); ctx.moveTo(sx, sy); ctx.lineTo(sx + Math.cos(a) * 0.15, sy + Math.sin(a) * 0.15); ctx.lineTo(sx + Math.cos(a + 1.8) * 0.09, sy + Math.sin(a + 1.8) * 0.09); ctx.fill();
                }
            } else if (t === WATER) {
                ctx.fillStyle = 'rgba(30,64,90,0.85)';
                ctx.fillRect(x, y, 1, 1);
                ctx.strokeStyle = 'rgba(147,197,253,0.25)';
                const k = (sim.time * 0.3 + h) % 1;
                ctx.beginPath(); ctx.ellipse(x + 0.5, y + 0.5, 0.1 + k * 0.4, (0.1 + k * 0.4) * 0.5, 0, 0, TAU); ctx.stroke();
            } else if (t === PLATE) {
                ctx.fillStyle = '#57534e';
                ctx.fillRect(x + 0.1, y + 0.1, 0.8, 0.8);
                ctx.strokeStyle = '#a8a29e';
                ctx.lineWidth = 0.04;
                ctx.strokeRect(x + 0.18, y + 0.18, 0.64, 0.64);
                ctx.beginPath(); ctx.arc(x + 0.5, y + 0.5, 0.15, 0, TAU); ctx.stroke(); // a bell sign
            }
        }
    }
}

function drawWalls(ctx: CanvasRenderingContext2D, sim: Sim, x0: number, x1: number, y0: number, y1: number): void {
    const wall = (x: number, y: number) => x < 0 || y < 0 || x >= sim.w || y >= sim.h || sim.tiles[y * sim.w + x] === WALL;
    for (let y = y0; y <= y1; y++) {
        for (let x = x0; x <= x1; x++) {
            if (!wall(x, y)) continue;
            const edge = !wall(x, y + 1) || !wall(x, y - 1) || !wall(x + 1, y) || !wall(x - 1, y);
            ctx.fillStyle = edge ? '#211d27' : '#0d0b10';
            ctx.fillRect(x, y, 1, 1);
            if (!wall(x, y + 1)) { // the face you see below it, with bricks
                ctx.fillStyle = '#2d2834';
                ctx.fillRect(x, y + 0.55, 1, 0.45);
                ctx.fillStyle = 'rgba(0,0,0,0.35)';
                ctx.fillRect(x, y + 0.77, 1, 0.03);
                ctx.fillRect(x + ((x % 2) ? 0.3 : 0.7), y + 0.55, 0.03, 0.22);
                ctx.fillRect(x + ((x % 2) ? 0.7 : 0.3), y + 0.77, 0.03, 0.23);
            }
        }
    }
}

function drawDoors(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const d of sim.doors) {
        const exit = sim.level.doors.find((q) => q.x === d.x && q.y === d.y)?.exit;
        const vertical = sim.tiles[d.y * sim.w + d.x - 1] === WALL && sim.tiles[d.y * sim.w + d.x + 1] === WALL; // wall left and right: the door spans x
        ctx.fillStyle = '#1b1712';
        ctx.fillRect(d.x, d.y, 1, 1);
        if (d.open) { // swung against the frame
            ctx.fillStyle = '#5b3d22';
            if (vertical) ctx.fillRect(d.x, d.y, 0.12, 1); else ctx.fillRect(d.x, d.y, 1, 0.12);
            continue;
        }
        ctx.fillStyle = exit ? '#3f3f46' : '#6b4a2b';
        ctx.fillRect(d.x + 0.05, d.y + 0.05, 0.9, 0.9);
        ctx.fillStyle = exit ? '#71717a' : '#4a321c';
        if (exit) for (let i = 0; i < 4; i++) ctx.fillRect(d.x + 0.12 + i * 0.22, d.y + 0.05, 0.08, 0.9); // portcullis bars
        else { ctx.fillRect(d.x + 0.05, d.y + 0.3, 0.9, 0.06); ctx.fillRect(d.x + 0.05, d.y + 0.65, 0.9, 0.06); }
        if (d.lock) {
            ctx.fillStyle = d.lock === 'gate' ? '#fbbf24' : '#cbd5e1';
            ctx.beginPath(); ctx.arc(d.x + 0.5, d.y + 0.45, 0.13, 0, TAU); ctx.fill();
            ctx.fillRect(d.x + 0.45, d.y + 0.45, 0.1, 0.25);
        }
    }
}

function drawBarrels(ctx: CanvasRenderingContext2D, sim: Sim): void {
    sim.barrels.forEach((b, i) => {
        const x = b.x + 0.5, y = b.y + 0.5;
        ctx.fillStyle = 'rgba(0,0,0,0.4)';
        ctx.beginPath(); ctx.ellipse(x + 0.08, y + 0.12, 0.42, 0.3, 0, 0, TAU); ctx.fill();
        ctx.fillStyle = '#6b4423';
        ctx.beginPath(); ctx.arc(x, y, 0.4, 0, TAU); ctx.fill();
        ctx.strokeStyle = '#3f2a16';
        ctx.lineWidth = 0.06;
        ctx.beginPath(); ctx.arc(x, y, 0.34, 0, TAU); ctx.stroke();
        ctx.beginPath(); ctx.arc(x, y, 0.2, 0, TAU); ctx.stroke();
        if (sim.player.hidden === i) { // eyes peeking out
            ctx.fillStyle = '#fef3c7';
            ctx.fillRect(x - 0.12, y - 0.05, 0.07, 0.05);
            ctx.fillRect(x + 0.05, y - 0.05, 0.07, 0.05);
        }
        if (sim.exposed === i) { ctx.strokeStyle = '#f87171'; ctx.lineWidth = 0.06; ctx.beginPath(); ctx.arc(x, y, 0.55, 0, TAU); ctx.stroke(); }
    });
}

function drawItems(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const it of sim.items) {
        if (it.taken) continue;
        const { x, y } = it.pos;
        const bob = Math.sin(sim.time * 3 + x) * 0.05;
        if (it.kind === 'key') {
            ctx.fillStyle = it.key === 'gate' ? '#fbbf24' : '#cbd5e1';
            ctx.beginPath(); ctx.arc(x - 0.15, y + bob, 0.13, 0, TAU); ctx.fill();
            ctx.fillRect(x - 0.05, y - 0.04 + bob, 0.35, 0.08);
            ctx.fillRect(x + 0.18, y + bob, 0.06, 0.12);
        } else {
            ctx.fillStyle = '#a8a29e';
            for (const [dx, dy] of [[-0.1, 0], [0.1, 0.05], [0, -0.1]]) { ctx.beginPath(); ctx.arc(x + dx, y + dy, 0.07, 0, TAU); ctx.fill(); }
        }
    }
}

function drawTorches(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const t of sim.torches) {
        ctx.fillStyle = '#3f3f46';
        ctx.fillRect(t.light.x - 0.06, t.light.y - 0.25, 0.12, 0.35); // the bracket
        if (!t.lit && Math.random() < 0.3) { ctx.fillStyle = 'rgba(148,163,184,0.3)'; ctx.beginPath(); ctx.arc(t.light.x, t.light.y - 0.45 - Math.random() * 0.3, 0.06, 0, TAU); ctx.fill(); }
    }
}

// The bright bits that show over the dark: flames.
function drawFlames(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const t of sim.torches) {
        if (!t.lit) continue;
        const f = 0.9 + 0.1 * Math.sin(sim.time * 17 + t.x);
        ctx.fillStyle = '#fb923c';
        ctx.beginPath(); ctx.ellipse(t.light.x, t.light.y - 0.35, 0.1 * f, 0.18 * f, 0, 0, TAU); ctx.fill();
        ctx.fillStyle = '#fde68a';
        ctx.beginPath(); ctx.ellipse(t.light.x, t.light.y - 0.32, 0.05, 0.1, 0, 0, TAU); ctx.fill();
    }
}

// The hound's view of things: your trail, faintly, when the hound is near.
function drawScent(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const hound = sim.guards.find((g) => g.kind === 'hound');
    if (!hound || Math.hypot(hound.pos.x - sim.player.pos.x, hound.pos.y - sim.player.pos.y) > 10) return;
    for (const s of sim.scent) {
        const age = (sim.time - s.t) / 14;
        ctx.fillStyle = `rgba(190,242,100,${0.45 * (1 - age)})`;
        ctx.beginPath(); ctx.arc(s.pos.x, s.pos.y, 0.09, 0, TAU); ctx.fill();
    }
}

function drawPebbles(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const pb of sim.pebbles) {
        const k = pb.t / pb.flight;
        const x = pb.from.x + (pb.to.x - pb.from.x) * k, y = pb.from.y + (pb.to.y - pb.from.y) * k - Math.sin(k * Math.PI) * 1.2;
        ctx.fillStyle = '#e7e5e4';
        ctx.beginPath(); ctx.arc(x, y, 0.08, 0, TAU); ctx.fill();
    }
}

// A faint ring round you over the dark, so you always know where you are.
function drawYou(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const p = sim.player;
    if (sim.phase === 'escaped') return;
    const b = p.hidden >= 0 ? { x: sim.barrels[p.hidden].x + 0.5, y: sim.barrels[p.hidden].y + 0.5 } : p.pos;
    ctx.strokeStyle = p.hidden >= 0 ? 'rgba(165,180,252,0.6)' : 'rgba(190,242,100,0.45)';
    ctx.lineWidth = 0.04;
    ctx.beginPath(); ctx.arc(b.x, b.y, 0.5, 0, TAU); ctx.stroke();
}

// --- the dark ----------------------------------------------------------------------------------

function drawDark(ctx: CanvasRenderingContext2D, sim: Sim, k: number): void {
    if (!darkCanvas) darkCanvas = document.createElement('canvas');
    if (darkCanvas.width !== sim.w) { darkCanvas.width = sim.w; darkCanvas.height = sim.h; }
    const d = darkCanvas.getContext('2d')!;
    const img = d.createImageData(sim.w, sim.h);
    for (let i = 0; i < sim.w * sim.h; i++) {
        const l = sim.tiles[i] === WALL ? neighbourLight(sim, i) : sim.light[i];
        img.data[i * 4] = 4; img.data[i * 4 + 1] = 3; img.data[i * 4 + 2] = 10;
        img.data[i * 4 + 3] = Math.round(255 * Math.max(0, DARK - l * DARK)); // never pitch black: you can make out the room
    }
    d.putImageData(img, 0, 0);
    const v = sim.view;
    ctx.imageSmoothingEnabled = true;
    ctx.drawImage(darkCanvas, -(v.x - v.span / 2) * k, -(v.y - v.span / 2) * k, sim.w * k, sim.h * k);
}

// Walls take the light of the floor next to them (so lit rooms show their walls).
function neighbourLight(sim: Sim, i: number): number {
    const x = i % sim.w, y = Math.floor(i / sim.w);
    let best = 0;
    for (const [ox, oy] of [[0, 1], [0, -1], [1, 0], [-1, 0]]) {
        const nx = x + ox, ny = y + oy;
        if (nx < 0 || ny < 0 || nx >= sim.w || ny >= sim.h) continue;
        const j = ny * sim.w + nx;
        if (sim.tiles[j] !== WALL) best = Math.max(best, sim.light[j] * 0.8);
    }
    return best;
}

// --- overlays ----------------------------------------------------------------------------------

function drawNoises(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const n of sim.noises) {
        const k = n.t / NOISE_LIFE;
        ctx.strokeStyle = n.loud ? `rgba(248,113,113,${0.7 * (1 - k)})` : `rgba(226,232,240,${0.45 * (1 - k)})`;
        ctx.lineWidth = 0.06;
        ctx.beginPath(); ctx.arc(n.pos.x, n.pos.y, Math.min(n.r, 12) * (0.2 + 0.8 * k), 0, TAU); ctx.stroke();
    }
}

function drawFloaters(ctx: CanvasRenderingContext2D, sim: Sim): void {
    ctx.textAlign = 'center';
    ctx.font = '700 0.45px Inter, sans-serif';
    for (const f of sim.floaters) {
        ctx.globalAlpha = Math.min(1, f.life * 2);
        ctx.lineWidth = 0.1;
        ctx.strokeStyle = 'rgba(0,0,0,0.85)';
        ctx.strokeText(f.text, f.pos.x, f.pos.y);
        ctx.fillStyle = f.color;
        ctx.fillText(f.text, f.pos.x, f.pos.y);
    }
    ctx.globalAlpha = 1;
}

function drawAim(ctx: CanvasRenderingContext2D, sim: Sim, m: Vec): void {
    const p = sim.player;
    const d = Math.hypot(m.x - p.pos.x, m.y - p.pos.y);
    const inRange = d <= sim.settings.throwRange;
    ctx.strokeStyle = inRange ? 'rgba(226,232,240,0.6)' : 'rgba(226,232,240,0.25)';
    ctx.lineWidth = 0.04;
    ctx.beginPath(); ctx.arc(m.x, m.y, 0.25, 0, TAU); ctx.stroke();
    if (p.pebbles > 0 && inRange) { // where the clatter will be heard
        ctx.setLineDash([0.2, 0.2]);
        ctx.strokeStyle = 'rgba(226,232,240,0.08)';
        ctx.beginPath(); ctx.arc(m.x, m.y, sim.settings.pebbleNoise, 0, TAU); ctx.stroke();
        ctx.setLineDash([]);
    }
}

function drawHud(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const w = ctx.canvas.width;
    const p = sim.player;
    ctx.font = '700 13px Inter, sans-serif';
    ctx.textAlign = 'left';
    const lit = lightAt(sim, p.pos);
    const status = p.hidden >= 0 ? 'HIDDEN in a barrel' : lit > 0.45 ? 'IN THE LIGHT — easy to see' : lit > 0.2 ? 'half-lit' : 'in shadow';
    ctx.fillStyle = p.hidden >= 0 ? '#a5b4fc' : lit > 0.45 ? '#fbbf24' : '#94a3b8';
    ctx.fillText(status, 14, 24);
    ctx.fillStyle = p.mode === 'sneak' ? '#86efac' : p.mode === 'run' ? '#f87171' : '#e2e8f0';
    ctx.fillText(p.mode === 'sneak' ? 'SNEAKING (quiet)' : p.mode === 'run' ? 'RUNNING (loud)' : 'walking', 14, 42);
    ctx.fillStyle = '#e7e5e4';
    ctx.fillText(`pebbles ${p.pebbles}`, 14, 60);
    const keys = p.keys.map((k) => (k === 'gate' ? 'GATE key' : 'iron key')).join(' · ') || 'no keys';
    ctx.fillStyle = '#fde047';
    ctx.fillText(keys, 14, 78);
    ctx.textAlign = 'right';
    ctx.fillStyle = '#e2e8f0';
    ctx.font = '800 15px Georgia, serif';
    ctx.fillText(sim.roomName || sim.level.name, w - 14, 24);
    ctx.font = '600 12px Inter, sans-serif';
    ctx.fillStyle = '#a8a29e';
    const goal = !p.keys.includes('iron') ? 'find the iron key (the barracks)' : !p.keys.includes('gate') ? 'the gate key is in the treasury' : 'get to the gate in the exit hall';
    ctx.fillText(goal, w - 14, 42);
    ctx.fillText(`seen ${sim.stats.spotted} · caught ${sim.stats.caught} · ${Math.round(sim.stats.time)} s`, w - 14, 58);
    if (sim.banner) {
        const b = sim.banner;
        ctx.globalAlpha = Math.min(1, b.life * 1.5);
        ctx.textAlign = 'center';
        const y = ctx.canvas.height * 0.3;
        ctx.font = '900 36px Georgia, serif';
        ctx.lineWidth = 6;
        ctx.strokeStyle = 'rgba(0,0,0,0.9)';
        ctx.strokeText(b.text, w / 2, y);
        ctx.fillStyle = b.text === 'CAUGHT' ? '#f87171' : b.text === 'ESCAPED' ? '#86efac' : '#e7e5e4';
        ctx.fillText(b.text, w / 2, y);
        if (b.sub) {
            ctx.font = 'italic 600 16px Georgia, serif';
            ctx.lineWidth = 4;
            ctx.strokeText(b.sub, w / 2, y + 28);
            ctx.fillStyle = '#d6d3d1';
            ctx.fillText(b.sub, w / 2, y + 28);
        }
        ctx.globalAlpha = 1;
    }
}
