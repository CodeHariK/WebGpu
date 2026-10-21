// Wick — drawing the valley: black water with lily pads and slow ripples, the muddy paths and their
// plank bridges, the clearings, set dressing scattered along the banks (dead trees, reeds,
// gravestones, stumps, crooked fence posts), lanterns, spare candles, fog walls and the chapel.
// Pure drawing in world metres (the caller sets the camera transform). The set dressing is placed
// once per level from a fixed seed, off the ground only.
import type { LevelDef } from './Map22Level';
import type { Seg, Sim, Vec } from './Map22State';
import { groundDepth } from './Map22World';
import { gustState } from './Map22Wind';

const TAU = Math.PI * 2;
const BRIDGE_W = 1.2; // pieces narrower than this (half-width) are plank bridges

type Prop = { pos: Vec; kind: 'tree' | 'reeds' | 'grave' | 'stump' | 'post' | 'pad' | 'pine' | 'shroom' | 'thorn'; size: number; rot: number };
const propCache = new WeakMap<LevelDef, Prop[]>();

function propsFor(sim: Sim): Prop[] {
    const cached = propCache.get(sim.level);
    if (cached) return cached;
    let seed = 1337;
    const rand = () => { seed = (seed * 16807) % 2147483647; return seed / 2147483647; };
    const out: Prop[] = [];
    for (const s of sim.segs) {
        const len = Math.hypot(s.b.x - s.a.x, s.b.y - s.a.y);
        const nx = -(s.b.y - s.a.y) / (len || 1), ny = (s.b.x - s.a.x) / (len || 1);
        for (const side of [-1, 1]) {
            if (rand() > len * (sim.level.ground === 'woods' ? 0.9 : 0.55)) continue;
            const t = rand();
            const w = s.wa + (s.wb - s.wa) * t;
            const bridge = w < BRIDGE_W;
            const off = w + 0.5 + rand() * (bridge ? 1.5 : 3.5);
            const pos = { x: s.a.x + (s.b.x - s.a.x) * t + nx * off * side, y: s.a.y + (s.b.y - s.a.y) * t + ny * off * side };
            if (groundDepth(sim.segs, sim.clearings, pos) > -0.35) continue;
            const roll = rand();
            const woods = sim.level.ground === 'woods';
            const kind: Prop['kind'] = woods
                ? (off > w + 2.2 ? (roll < 0.6 ? 'pine' : roll < 0.85 ? 'tree' : 'thorn') : roll < 0.3 ? 'thorn' : roll < 0.5 ? 'shroom' : roll < 0.7 ? 'stump' : roll < 0.85 ? 'post' : 'grave')
                : bridge ? (roll < 0.7 ? 'reeds' : 'pad') : off > w + 2.5 ? (roll < 0.55 ? 'tree' : roll < 0.8 ? 'pad' : 'reeds') : roll < 0.3 ? 'grave' : roll < 0.5 ? 'stump' : roll < 0.75 ? 'post' : 'reeds';
            out.push({ pos, kind, size: 0.7 + rand() * 0.7, rot: rand() * TAU });
        }
    }
    for (const c of sim.clearings) { // gravestones round the clearings' rims
        for (let i = 0; i < 7; i++) {
            const a = rand() * TAU;
            const pos = { x: c.def.pos.x + Math.cos(a) * (c.def.r + 0.8), y: c.def.pos.y + Math.sin(a) * (c.def.r + 0.8) };
            if (groundDepth(sim.segs, sim.clearings, pos) < -0.35) out.push({ pos, kind: rand() < 0.6 ? 'grave' : 'tree', size: 0.8 + rand() * 0.5, rot: rand() * TAU });
        }
    }
    out.sort((p, q) => p.pos.y - q.pos.y);
    propCache.set(sim.level, out);
    return out;
}

// --- water and ground --------------------------------------------------------------------------

export function drawWater(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const v = sim.view;
    if (sim.level.ground === 'woods') { drawWoods(ctx, sim); return; }
    ctx.fillStyle = '#05080d';
    ctx.fillRect(v.x - v.span, v.y - v.span, v.span * 2, v.span * 2);
    // Slow ripples drifting over the dark water.
    ctx.strokeStyle = 'rgba(94,114,140,0.12)';
    ctx.lineWidth = 0.05;
    for (let i = 0; i < 26; i++) {
        const x = Math.floor(v.x / 6) * 6 + ((i * 7.3) % 30) - 15;
        const y = Math.floor(v.y / 6) * 6 + ((i * 11.7) % 30) - 15;
        const k = (sim.time * 0.25 + i * 0.37) % 1;
        ctx.globalAlpha = 1 - k;
        ctx.beginPath(); ctx.ellipse(x, y, 0.4 + k * 2, (0.4 + k * 2) * 0.5, 0, 0, TAU); ctx.stroke();
    }
    ctx.globalAlpha = 1;
}

// The black wood: dark undergrowth with fallen leaves and the shapes of trunks.
function drawWoods(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const v = sim.view;
    ctx.fillStyle = '#060a07';
    ctx.fillRect(v.x - v.span, v.y - v.span, v.span * 2, v.span * 2);
    const x0 = Math.floor((v.x - v.span) / 2) * 2, y0 = Math.floor((v.y - v.span) / 2) * 2;
    for (let x = x0; x < v.x + v.span; x += 2) {
        for (let y = y0; y < v.y + v.span; y += 2) {
            const h = Math.sin(x * 12.9898 + y * 78.233) * 43758.5453;
            const r = h - Math.floor(h);
            ctx.fillStyle = r < 0.33 ? '#0b120c' : r < 0.66 ? '#0a0f0b' : '#110f0a';
            ctx.beginPath(); ctx.arc(x + r * 1.5, y + (1 - r) * 1.5, 0.6 + r * 0.8, 0, TAU); ctx.fill();
        }
    }
}

function strokeSegs(ctx: CanvasRenderingContext2D, segs: Seg[], extra: number, scale: number, color: string, bridges: boolean | null): void {
    ctx.strokeStyle = color;
    ctx.lineCap = 'round';
    for (const s of segs) {
        const w = (s.wa + s.wb) / 2;
        if (bridges !== null && (w < BRIDGE_W) !== bridges) continue;
        ctx.lineWidth = (w * scale + extra) * 2;
        ctx.beginPath(); ctx.moveTo(s.a.x, s.a.y); ctx.lineTo(s.b.x, s.b.y); ctx.stroke();
    }
    ctx.lineCap = 'butt';
}

export function drawGround(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const segs = sim.segs;
    const cl = sim.clearings.map((c) => c.def);
    // Muddy bank, then the ground, then the worn track down the middle.
    strokeSegs(ctx, segs, 0.55, 1, '#121510', false);
    for (const c of cl) { ctx.fillStyle = '#121510'; ctx.beginPath(); ctx.arc(c.pos.x, c.pos.y, c.r + 0.55, 0, TAU); ctx.fill(); }
    strokeSegs(ctx, segs, 0, 1, '#2a2a1f', false);
    for (const c of cl) { ctx.fillStyle = '#2a2a1f'; ctx.beginPath(); ctx.arc(c.pos.x, c.pos.y, c.r, 0, TAU); ctx.fill(); }
    strokeSegs(ctx, segs, 0, 0.45, '#383226', false);
    for (const c of cl) { // cobbles in the clearings
        ctx.fillStyle = '#383226';
        ctx.beginPath(); ctx.arc(c.pos.x, c.pos.y, c.r * 0.7, 0, TAU); ctx.fill();
        ctx.strokeStyle = 'rgba(0,0,0,0.25)';
        ctx.lineWidth = 0.04;
        for (let r = 1.2; r < c.r * 0.7; r += 1.1) { ctx.beginPath(); ctx.arc(c.pos.x, c.pos.y, r, 0, TAU); ctx.stroke(); }
    }
    drawBridges(ctx, segs);
    drawGrass(ctx, segs);
}

// Plank bridges over the narrow stretches: boards across, rope posts at the sides.
function drawBridges(ctx: CanvasRenderingContext2D, segs: Seg[]): void {
    for (const s of segs) {
        const w = (s.wa + s.wb) / 2;
        if (w >= BRIDGE_W) continue;
        const len = Math.hypot(s.b.x - s.a.x, s.b.y - s.a.y);
        const a = Math.atan2(s.b.y - s.a.y, s.b.x - s.a.x);
        ctx.save();
        ctx.translate(s.a.x, s.a.y);
        ctx.rotate(a);
        ctx.fillStyle = '#3d2f20';
        ctx.fillRect(-0.05, -w - 0.1, len + 0.1, (w + 0.1) * 2);
        ctx.strokeStyle = '#1f170f';
        ctx.lineWidth = 0.05;
        for (let x = 0; x < len; x += 0.35) { ctx.beginPath(); ctx.moveTo(x, -w - 0.1); ctx.lineTo(x, w + 0.1); ctx.stroke(); }
        ctx.fillStyle = '#2a1f14';
        ctx.fillRect(0, -w - 0.2, 0.15, 0.15);
        ctx.fillRect(0, w + 0.05, 0.15, 0.15);
        ctx.restore();
    }
}

// Tufts of dead grass along the edges.
function drawGrass(ctx: CanvasRenderingContext2D, segs: Seg[]): void {
    ctx.strokeStyle = 'rgba(120,113,80,0.35)';
    ctx.lineWidth = 0.04;
    segs.forEach((s, i) => {
        const w = (s.wa + s.wb) / 2;
        if (w < BRIDGE_W) return;
        const len = Math.hypot(s.b.x - s.a.x, s.b.y - s.a.y) || 1;
        const nx = -(s.b.y - s.a.y) / len, ny = (s.b.x - s.a.x) / len;
        for (const side of [-1, 1]) {
            const t = ((i * 0.618) % 1);
            const x = s.a.x + (s.b.x - s.a.x) * t + nx * (w - 0.15) * side;
            const y = s.a.y + (s.b.y - s.a.y) * t + ny * (w - 0.15) * side;
            for (let k = -1; k <= 1; k++) { ctx.beginPath(); ctx.moveTo(x + k * 0.08, y); ctx.lineTo(x + k * 0.15, y - 0.3); ctx.stroke(); }
        }
    });
}

// --- set dressing ------------------------------------------------------------------------------

export function drawProps(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const v = sim.view;
    for (const p of propsFor(sim)) {
        if (Math.abs(p.pos.x - v.x) > v.span || Math.abs(p.pos.y - v.y) > v.span) continue;
        if (p.kind === 'tree') tree(ctx, p);
        else if (p.kind === 'reeds') reeds(ctx, sim, p);
        else if (p.kind === 'grave') grave(ctx, p);
        else if (p.kind === 'stump') stump(ctx, p);
        else if (p.kind === 'post') post(ctx, p);
        else if (p.kind === 'pine') pine(ctx, p);
        else if (p.kind === 'shroom') shroom(ctx, sim, p);
        else if (p.kind === 'thorn') thorn(ctx, p);
        else pad(ctx, p);
    }
}

// A dead tree: trunk standing up out of the water, bare crooked branches.
function tree(ctx: CanvasRenderingContext2D, p: Prop): void {
    const h = 2.2 * p.size;
    ctx.fillStyle = 'rgba(0,0,0,0.35)';
    ctx.beginPath(); ctx.ellipse(p.pos.x + 0.4, p.pos.y + 0.1, 0.8 * p.size, 0.3 * p.size, 0, 0, TAU); ctx.fill();
    ctx.strokeStyle = '#1c1915';
    ctx.lineCap = 'round';
    ctx.lineWidth = 0.28 * p.size;
    ctx.beginPath(); ctx.moveTo(p.pos.x, p.pos.y); ctx.lineTo(p.pos.x + 0.1, p.pos.y - h); ctx.stroke();
    ctx.lineWidth = 0.09 * p.size;
    for (let i = 0; i < 5; i++) {
        const y = p.pos.y - h * (0.45 + i * 0.12);
        const s = (i % 2 ? 1 : -1) * (0.6 + ((p.rot * (i + 1)) % 0.6)) * p.size;
        ctx.beginPath(); ctx.moveTo(p.pos.x + 0.05, y); ctx.lineTo(p.pos.x + s, y - 0.35 * p.size); ctx.lineTo(p.pos.x + s * 1.3, y - 0.2 * p.size); ctx.stroke();
    }
    ctx.lineCap = 'butt';
}

function reeds(ctx: CanvasRenderingContext2D, sim: Sim, p: Prop): void {
    ctx.strokeStyle = '#3f4a2c';
    ctx.lineWidth = 0.05;
    for (let i = 0; i < 6; i++) {
        const x = p.pos.x + (i - 2.5) * 0.12;
        const sway = Math.sin(sim.time * 1.3 + p.rot + i) * 0.08;
        ctx.beginPath(); ctx.moveTo(x, p.pos.y); ctx.lineTo(x + sway, p.pos.y - (0.6 + (i % 3) * 0.25) * p.size); ctx.stroke();
    }
    ctx.fillStyle = '#4a3a24';
    ctx.fillRect(p.pos.x - 0.05, p.pos.y - 0.85 * p.size, 0.1, 0.25); // a bulrush head
}

function grave(ctx: CanvasRenderingContext2D, p: Prop): void {
    const w = 0.55 * p.size, h = 0.8 * p.size;
    const tilt = (p.rot - Math.PI) * 0.06;
    ctx.save();
    ctx.translate(p.pos.x, p.pos.y);
    ctx.rotate(tilt);
    ctx.fillStyle = 'rgba(0,0,0,0.35)';
    ctx.fillRect(-w / 2 + 0.15, -0.05, w, 0.25);
    ctx.fillStyle = '#4b5052';
    ctx.beginPath(); ctx.moveTo(-w / 2, 0); ctx.lineTo(-w / 2, -h + w / 2); ctx.arc(0, -h + w / 2, w / 2, Math.PI, 0); ctx.lineTo(w / 2, 0); ctx.fill();
    ctx.fillStyle = '#34393b';
    ctx.fillRect(-w * 0.25, -h * 0.55, w * 0.5, 0.06);
    ctx.fillRect(-0.03, -h * 0.75, 0.06, h * 0.4); // a cross cut in it
    ctx.restore();
}

function stump(ctx: CanvasRenderingContext2D, p: Prop): void {
    ctx.fillStyle = '#2b2116';
    ctx.fillRect(p.pos.x - 0.25 * p.size, p.pos.y - 0.4 * p.size, 0.5 * p.size, 0.4 * p.size);
    ctx.fillStyle = '#4a3a28';
    ctx.beginPath(); ctx.ellipse(p.pos.x, p.pos.y - 0.4 * p.size, 0.25 * p.size, 0.12 * p.size, 0, 0, TAU); ctx.fill();
}

function post(ctx: CanvasRenderingContext2D, p: Prop): void {
    ctx.save();
    ctx.translate(p.pos.x, p.pos.y);
    ctx.rotate((p.rot - Math.PI) * 0.08);
    ctx.fillStyle = '#2f2418';
    ctx.fillRect(-0.06, -0.9 * p.size, 0.12, 0.9 * p.size);
    ctx.fillRect(-0.06, -0.7 * p.size, 0.6, 0.07); // a broken rail
    ctx.restore();
}

// A dark pine: stacked jagged tiers.
function pine(ctx: CanvasRenderingContext2D, p: Prop): void {
    const h = 2.6 * p.size;
    ctx.fillStyle = 'rgba(0,0,0,0.35)';
    ctx.beginPath(); ctx.ellipse(p.pos.x + 0.4, p.pos.y + 0.1, 0.9 * p.size, 0.3 * p.size, 0, 0, TAU); ctx.fill();
    ctx.fillStyle = '#1f1710';
    ctx.fillRect(p.pos.x - 0.08 * p.size, p.pos.y - 0.5 * p.size, 0.16 * p.size, 0.5 * p.size);
    for (let i = 0; i < 3; i++) {
        const y = p.pos.y - 0.4 * p.size - i * h * 0.27;
        const w = (0.85 - i * 0.22) * p.size;
        ctx.fillStyle = i % 2 ? '#0f1a12' : '#132017';
        ctx.beginPath(); ctx.moveTo(p.pos.x - w, y); ctx.lineTo(p.pos.x, y - h * 0.38); ctx.lineTo(p.pos.x + w, y); ctx.fill();
    }
}

// Pale mushrooms that glow a little (you see them in the dark).
function shroom(ctx: CanvasRenderingContext2D, sim: Sim, p: Prop): void {
    for (let i = 0; i < 3; i++) {
        const x = p.pos.x + (i - 1) * 0.22 * p.size, y = p.pos.y + (i % 2) * 0.1;
        ctx.fillStyle = '#d6d3c8';
        ctx.fillRect(x - 0.03, y - 0.22 * p.size, 0.06, 0.22 * p.size);
        ctx.fillStyle = `rgba(167,243,208,${0.55 + 0.2 * Math.sin(sim.time * 2 + p.rot + i)})`;
        ctx.beginPath(); ctx.ellipse(x, y - 0.22 * p.size, 0.12 * p.size, 0.07 * p.size, 0, Math.PI, 0); ctx.fill();
    }
}

// A thorn bush: a tangle of dark spiky strokes.
function thorn(ctx: CanvasRenderingContext2D, p: Prop): void {
    ctx.strokeStyle = '#2b1f1a';
    ctx.lineWidth = 0.05;
    for (let i = 0; i < 9; i++) {
        const a = p.rot + i * 0.7;
        const r = (0.4 + (i % 3) * 0.2) * p.size;
        ctx.beginPath(); ctx.moveTo(p.pos.x, p.pos.y - 0.2); ctx.quadraticCurveTo(p.pos.x + Math.cos(a) * r * 0.5, p.pos.y - 0.2 + Math.sin(a) * r * 0.3 - 0.3, p.pos.x + Math.cos(a) * r, p.pos.y - 0.2 + Math.sin(a) * r * 0.5); ctx.stroke();
    }
}

function pad(ctx: CanvasRenderingContext2D, p: Prop): void {
    ctx.fillStyle = '#1d2b1c';
    ctx.beginPath(); ctx.arc(p.pos.x, p.pos.y, 0.35 * p.size, p.rot, p.rot + TAU * 0.88); ctx.lineTo(p.pos.x, p.pos.y); ctx.fill();
}

// --- lanterns, candles, fog walls, the chapel ------------------------------------------------

export function drawLanterns(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const l of sim.lanterns) {
        const { x, y } = l.pos;
        ctx.fillStyle = 'rgba(0,0,0,0.4)';
        ctx.beginPath(); ctx.ellipse(x + 0.3, y + 0.1, 0.5, 0.2, 0, 0, TAU); ctx.fill();
        ctx.fillStyle = '#231a12'; // the post and its arm
        ctx.fillRect(x - 0.08, y - 1.9, 0.16, 1.9);
        ctx.fillRect(x - 0.08, y - 1.9, 0.5, 0.1);
        const lx = x + 0.4, ly = y - 1.6;
        ctx.fillStyle = l.lit ? `rgb(255,${200 + 30 * Math.sin(sim.time * 9 + x)},120)` : '#1a1f24';
        ctx.fillRect(lx - 0.2, ly - 0.15, 0.4, 0.45);
        ctx.strokeStyle = '#0c0a08';
        ctx.lineWidth = 0.06;
        ctx.strokeRect(lx - 0.2, ly - 0.15, 0.4, 0.45);
        ctx.beginPath(); ctx.moveTo(lx - 0.25, ly - 0.15); ctx.lineTo(lx, ly - 0.35); ctx.lineTo(lx + 0.25, ly - 0.15); ctx.stroke();
        if (!l.lit) { // unlit: a faint ring on the ground says "light me"
            ctx.strokeStyle = `rgba(253,230,138,${0.15 + 0.12 * Math.sin(sim.time * 3 + x)})`;
            ctx.setLineDash([0.25, 0.2]);
            ctx.beginPath(); ctx.arc(x, y, 1.4, 0, TAU); ctx.stroke();
            ctx.setLineDash([]);
        }
    }
}

export function drawCandles(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const c of sim.candles) {
        if (c.taken) continue;
        const { x, y } = c.pos;
        ctx.fillStyle = '#efe6cf';
        ctx.fillRect(x - 0.1, y - 0.45, 0.2, 0.45);
        ctx.fillStyle = `rgba(253,186,116,${0.8 + 0.2 * Math.sin(sim.time * 12 + x)})`;
        ctx.beginPath(); ctx.ellipse(x, y - 0.58, 0.06, 0.12, 0, 0, TAU); ctx.fill();
    }
}

/** Fog walls across the path out of a clearing that isn't done: a churning grey bank. */
export function drawFogWalls(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const c of sim.clearings) {
        if (c.done) continue;
        for (const g of c.def.gates) for (let i = 0; i < 16; i++) {
            const a = sim.time * (0.3 + (i % 3) * 0.1) + i * 1.7;
            const r = g.r * (0.3 + ((i * 0.37) % 0.7));
            const x = g.x + Math.cos(a) * r, y = g.y + Math.sin(a) * r * 0.8;
            const grad = ctx.createRadialGradient(x, y, 0, x, y, 1.6);
            grad.addColorStop(0, 'rgba(148,163,184,0.35)');
            grad.addColorStop(1, 'rgba(148,163,184,0)');
            ctx.fillStyle = grad;
            ctx.beginPath(); ctx.arc(x, y, 1.6, 0, TAU); ctx.fill();
        }
    }
}

// The chapel at the end of the path: stone walls, a pitched roof, a bell tower. Its windows glow
// once its lanterns are lit.
export function drawChapel(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const { x, y } = sim.level.chapel;
    const lit = sim.phase === 'won';
    ctx.fillStyle = 'rgba(0,0,0,0.4)';
    ctx.fillRect(x - 4.5, y + 1.6, 9, 1);
    ctx.fillStyle = '#2b2d33';
    ctx.fillRect(x - 4, y - 2, 8, 3.6);
    ctx.fillStyle = '#1a1b20';
    ctx.beginPath(); ctx.moveTo(x - 4.4, y - 2); ctx.lineTo(x, y - 4.6); ctx.lineTo(x + 4.4, y - 2); ctx.fill();
    ctx.fillStyle = '#2b2d33'; // bell tower
    ctx.fillRect(x + 2.2, y - 6.5, 1.6, 4.6);
    ctx.fillStyle = '#1a1b20';
    ctx.beginPath(); ctx.moveTo(x + 2, y - 6.5); ctx.lineTo(x + 3, y - 8); ctx.lineTo(x + 4, y - 6.5); ctx.fill();
    ctx.fillStyle = lit ? '#fbbf24' : '#3b3f47';
    ctx.beginPath(); ctx.arc(x + 3, y - 5.4, 0.35, 0, TAU); ctx.fill(); // the bell
    for (const dx of [-2.6, -0.8, 1]) { // arched windows
        const wx = x + dx;
        ctx.fillStyle = lit ? 'rgba(251,191,36,0.9)' : '#14161b';
        ctx.beginPath(); ctx.moveTo(wx, y + 0.6); ctx.lineTo(wx, y - 0.6); ctx.arc(wx + 0.35, y - 0.6, 0.35, Math.PI, 0); ctx.lineTo(wx + 0.7, y + 0.6); ctx.fill();
    }
    ctx.fillStyle = '#120f0c';
    ctx.fillRect(x + 1.4 - 0.5, y + 0.2, 0.8, 1.4); // the door
}

// The bell tower at the top of the Wax Works: a tall stone tower, a bell in its open belfry.
export function drawBellTower(ctx: CanvasRenderingContext2D, sim: Sim): void {
    const { x, y } = sim.level.chapel;
    const won = sim.phase === 'won';
    ctx.fillStyle = 'rgba(0,0,0,0.4)';
    ctx.fillRect(x - 2, y + 1.6, 4, 0.8);
    ctx.fillStyle = '#2b2d33';
    ctx.fillRect(x - 1.6, y - 5, 3.2, 6.6);
    ctx.fillStyle = '#1a1b20';
    ctx.beginPath(); ctx.moveTo(x - 2, y - 5); ctx.lineTo(x, y - 7.5); ctx.lineTo(x + 2, y - 5); ctx.fill();
    ctx.fillStyle = '#0d0e11'; // the open belfry
    ctx.fillRect(x - 1.1, y - 4.6, 2.2, 1.6);
    const swing = won ? Math.sin(sim.time * 4) * 0.5 : 0;
    ctx.save();
    ctx.translate(x, y - 4.5);
    ctx.rotate(swing);
    ctx.fillStyle = won ? '#fbbf24' : '#6b5a2e';
    ctx.beginPath(); ctx.moveTo(-0.5, 1.2); ctx.quadraticCurveTo(-0.45, 0.1, 0, 0.1); ctx.quadraticCurveTo(0.45, 0.1, 0.5, 1.2); ctx.fill();
    ctx.restore();
    ctx.fillStyle = '#120f0c';
    ctx.fillRect(x - 0.4, y + 0.2, 0.8, 1.4);
}

export function drawLandmark(ctx: CanvasRenderingContext2D, sim: Sim): void {
    if (sim.level.landmark === 'belltower') drawBellTower(ctx, sim);
    else drawChapel(ctx, sim);
}

// Wax vats: an iron rim round a pool of warm wax, slow bubbles, a little steam.
export function drawVats(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const v of sim.level.vats) {
        ctx.fillStyle = '#1c1917';
        ctx.beginPath(); ctx.arc(v.x, v.y, v.r + 0.2, 0, TAU); ctx.fill();
        const g = ctx.createRadialGradient(v.x, v.y, 0, v.x, v.y, v.r);
        g.addColorStop(0, '#fde68a');
        g.addColorStop(1, '#d97706');
        ctx.fillStyle = g;
        ctx.beginPath(); ctx.arc(v.x, v.y, v.r, 0, TAU); ctx.fill();
        for (let i = 0; i < 4; i++) {
            const k = (sim.time * 0.5 + i * 0.27 + v.x) % 1;
            const a = i * 1.9 + v.y;
            ctx.strokeStyle = `rgba(255,251,235,${0.7 * (1 - k)})`;
            ctx.lineWidth = 0.03;
            ctx.beginPath(); ctx.arc(v.x + Math.cos(a) * v.r * 0.5, v.y + Math.sin(a) * v.r * 0.5, 0.05 + k * 0.15, 0, TAU); ctx.stroke();
        }
    }
}

// Gusts: leaves skittering across the area as it gathers (the tell), then streaks while it blows.
export function drawGusts(ctx: CanvasRenderingContext2D, sim: Sim): void {
    for (const g of sim.level.gusts) {
        const state = gustState(g, sim.time);
        if (state === 'calm') continue;
        const l = Math.hypot(g.dir.x, g.dir.y) || 1;
        const dx = g.dir.x / l, dy = g.dir.y / l;
        const n = state === 'warn' ? 10 : 26;
        for (let i = 0; i < n; i++) {
            const lane = ((i * 0.618) % 1) * 2 - 1; // across the gust
            const along = ((sim.time * (state === 'blow' ? 1.6 : 0.5) + i * 0.37) % 1) * 2 - 1;
            const x = g.pos.x + dx * along * g.r - dy * lane * g.r * 0.9;
            const y = g.pos.y + dy * along * g.r + dx * lane * g.r * 0.9;
            if (Math.hypot(x - g.pos.x, y - g.pos.y) > g.r) continue;
            if (state === 'blow') {
                ctx.strokeStyle = 'rgba(203,213,225,0.35)';
                ctx.lineWidth = 0.05;
                ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(x - dx * 1.4, y - dy * 1.4); ctx.stroke();
            }
            ctx.fillStyle = i % 2 ? '#7c4a1e' : '#92400e'; // a leaf
            ctx.save();
            ctx.translate(x, y);
            ctx.rotate(sim.time * 6 + i);
            ctx.beginPath(); ctx.ellipse(0, 0, 0.12, 0.06, 0, 0, TAU); ctx.fill();
            ctx.restore();
        }
    }
}
