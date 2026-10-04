// Puzzle Islands — drawing of the environment mechanics (emitters, beams, mirrors,
// receivers, plates, switch gates). Tiles are drawn on top of the base floor / hedge.
import { DX, DY, hasBox, tileAt, type Level, type State } from './Map19Rules';
import { envOf, laserGateOpen, pulsePhase, traceBeam, type Beam, type Emitter } from './Map19Env';

/** Laser channel colours: a gate and its receivers share one. */
const CHANNEL_COLORS = ['#36d6ff', '#7dff6a', '#ffb3f0'];

export const ENV_COLORS = { beam: '#ff3b6b', beamCore: '#fff0f4', pulse: '#ff8a3b', laserGate: '#36d6ff', plateGate: '#ffa94d', metal: '#c9d3e0' };

const cx = (L: Level, i: number, c: number) => ((i % L.w) + 0.5) * c;
const cy = (L: Level, i: number, c: number) => (Math.floor(i / L.w) + 0.5) * c;

/** Overlay for one env tile (base already drawn underneath). Returns false if `cell` isn't one. */
export function drawEnvTile(ctx: CanvasRenderingContext2D, L: Level, s: State, cell: number, c: number, t: number): boolean {
    const b = L.base[cell];
    const px = (cell % L.w) * c;
    const py = Math.floor(cell / L.w) * c;
    if (b === 'Q') {
        const i = L.emitters.findIndex((e) => e.cell === cell);
        drawEmitter(ctx, L.emitters[i], px, py, c, t, L.emitters[i].slot, L.emitters[i].slot && !((s.powered >> i) & 1));
    }
    else if (b === 'O') drawReceiver(ctx, px, py, c, envOf(L, s).beams.some((x) => x.lit === cell), t, CHANNEL_COLORS[L.channel.get(cell) ?? 0]);
    else if (b === '_') drawPlate(ctx, px, py, c, hasBox(s, cell));
    else if (b === 'H') drawSwitchGate(ctx, px, py, c, CHANNEL_COLORS[L.channel.get(cell) ?? 0], laserGateOpen(L, s, cell));
    else if (b === 'J') drawSwitchGate(ctx, px, py, c, ENV_COLORS.plateGate, envOf(L, s).plateOpen);
    else if (b === 'D' && tileAt(L, s, cell) === '.') drawDebris(ctx, px, py, c);
    else if (b === 'D') drawWoodDoor(ctx, px, py, c, isWallRun(L, cell - 1) || isWallRun(L, cell + 1));
    else if (b === 'F') {
        // Run direction: along neighbouring wooden walls, else between hedges.
        const x = cell % L.w;
        const is = (n: number, ch: string) => n >= 0 && n < L.base.length && L.base[n] === ch;
        const side = (ch: string) => (x > 0 && is(cell - 1, ch)) || (x < L.w - 1 && is(cell + 1, ch));
        const vert = (ch: string) => is(cell - L.w, ch) || is(cell + L.w, ch);
        drawWoodWall(ctx, px, py, c, side('F') || (!vert('F') && side('#')));
    }
    else return false;
    return true;
}

/**
 * Beams: always-on ones solid; pulsing ones solid while ON, a thin flicker in the warning
 * window, nothing while off. `beat` undefined (thumbnails) → always-on only.
 */
export function drawBeams(ctx: CanvasRenderingContext2D, L: Level, s: State, c: number, beat: number | undefined, t: number) {
    for (const b of envOf(L, s).beams) drawBeam(ctx, L, b, c, ENV_COLORS.beam, 1, t);
    if (beat === undefined) return;
    const phase = pulsePhase(beat);
    if (phase === 'off') return;
    for (const e of L.emitters) {
        if (!e.pulse) continue;
        const b = traceBeam(L, s, e);
        if (phase === 'on') drawBeam(ctx, L, b, c, ENV_COLORS.pulse, 1, t);
        else drawBeam(ctx, L, b, c, ENV_COLORS.pulse, 0.25 + 0.4 * Math.abs(Math.sin(t * 22)), t, true);
    }
}

function drawBeam(ctx: CanvasRenderingContext2D, L: Level, b: Beam, c: number, col: string, alpha: number, t: number, warn = false) {
    const pts: [number, number][] = [[cx(L, b.emitter.cell, c) + DX[b.emitter.dir] * c * 0.35, cy(L, b.emitter.cell, c) + DY[b.emitter.dir] * c * 0.35]];
    for (const cell of b.path) pts.push([cx(L, cell, c), cy(L, cell, c)]);
    const last = b.path.length ? b.path[b.path.length - 1] : b.emitter.cell;
    if (b.stop >= 0) pts.push([(cx(L, last, c) + cx(L, b.stop, c)) / 2, (cy(L, last, c) + cy(L, b.stop, c)) / 2]);
    if (pts.length < 2) return;
    const line = () => {
        ctx.beginPath();
        ctx.moveTo(pts[0][0], pts[0][1]);
        for (const [x, y] of pts.slice(1)) ctx.lineTo(x, y);
        ctx.stroke();
    };
    ctx.save();
    ctx.globalAlpha = alpha;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    ctx.strokeStyle = col;
    if (warn) {
        ctx.lineWidth = Math.max(1, c * 0.06);
        ctx.setLineDash([c * 0.14, c * 0.1]);
        line();
    } else {
        ctx.globalAlpha = alpha * (0.45 + 0.1 * Math.sin(t * 30));
        ctx.lineWidth = c * 0.34;
        line();
        ctx.globalAlpha = alpha;
        ctx.lineWidth = c * 0.12;
        ctx.strokeStyle = ENV_COLORS.beamCore;
        line();
    }
    ctx.restore();
}

function drawEmitter(ctx: CanvasRenderingContext2D, e: Emitter, px: number, py: number, c: number, t: number, slot: boolean, unpowered: boolean) {
    const x = px + c / 2;
    const y = py + c / 2;
    ctx.fillStyle = '#4a4f63';
    roundRect(ctx, px + c * 0.14, py + c * 0.14, c * 0.72, c * 0.72, c * 0.16);
    ctx.fill();
    // lens on the firing side
    const lx = x + DX[e.dir] * c * 0.3;
    const ly = y + DY[e.dir] * c * 0.3;
    if (unpowered) { // dead: dark lens + empty battery slot
        ctx.fillStyle = '#20232e';
        circle(ctx, lx, ly, c * 0.13);
        ctx.strokeStyle = '#ffd23f';
        ctx.lineWidth = Math.max(1, c * 0.04);
        ctx.strokeRect(x - DX[e.dir] * c * 0.12 - c * 0.08, y - DY[e.dir] * c * 0.12 - c * 0.12, c * 0.16, c * 0.24);
        return;
    }
    if (slot) { // battery sitting in its slot
        ctx.fillStyle = '#ffd23f';
        ctx.fillRect(x - DX[e.dir] * c * 0.12 - c * 0.08, y - DY[e.dir] * c * 0.12 - c * 0.12, c * 0.16, c * 0.24);
    }
    ctx.fillStyle = e.pulse ? ENV_COLORS.pulse : ENV_COLORS.beam;
    circle(ctx, lx, ly, c * (0.13 + 0.02 * Math.sin(t * 8)));
    ctx.fillStyle = '#fff';
    circle(ctx, lx, ly, c * 0.05);
    if (e.pulse) { // tick marks = "on a timer"
        ctx.fillStyle = ENV_COLORS.pulse;
        for (const k of [-1, 0, 1]) circle(ctx, x - DX[e.dir] * c * 0.12 + DY[e.dir] * k * c * 0.14, y - DY[e.dir] * c * 0.12 + DX[e.dir] * k * c * 0.14, c * 0.035);
    }
}

/** Mirror box (pushable): metal base with a / or \ blade. */
export function drawMirror(ctx: CanvasRenderingContext2D, px: number, py: number, c: number, backslash: boolean) {
    ctx.fillStyle = 'rgba(0,0,0,0.2)';
    circle(ctx, px + c / 2, py + c * 0.56, c * 0.38);
    ctx.fillStyle = '#6b7a8f';
    circle(ctx, px + c / 2, py + c / 2, c * 0.36);
    ctx.lineCap = 'round';
    const [ax, ay, bx, by] = backslash ? [0.2, 0.2, 0.8, 0.8] : [0.2, 0.8, 0.8, 0.2];
    ctx.strokeStyle = ENV_COLORS.metal;
    ctx.lineWidth = c * 0.16;
    ctx.beginPath(); ctx.moveTo(px + c * ax, py + c * ay); ctx.lineTo(px + c * bx, py + c * by); ctx.stroke();
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = c * 0.05;
    ctx.beginPath(); ctx.moveTo(px + c * ax, py + c * ay); ctx.lineTo(px + c * bx, py + c * by); ctx.stroke();
    ctx.lineCap = 'butt';
}

function drawReceiver(ctx: CanvasRenderingContext2D, px: number, py: number, c: number, lit: boolean, t: number, col: string) {
    ctx.fillStyle = '#3a3f52';
    circle(ctx, px + c / 2, py + c / 2, c * 0.34);
    ctx.fillStyle = lit ? col : '#1b1e29';
    circle(ctx, px + c / 2, py + c / 2, c * (lit ? 0.22 + 0.03 * Math.sin(t * 10) : 0.2));
    ctx.strokeStyle = col;
    ctx.lineWidth = Math.max(1, c * 0.05);
    ctx.beginPath(); ctx.arc(px + c / 2, py + c / 2, c * 0.3, 0, Math.PI * 2); ctx.stroke();
}

function drawPlate(ctx: CanvasRenderingContext2D, px: number, py: number, c: number, pressed: boolean) {
    ctx.fillStyle = pressed ? ENV_COLORS.plateGate : '#8a8f9e';
    roundRect(ctx, px + c * 0.14, py + c * 0.14, c * 0.72, c * 0.72, c * 0.12);
    ctx.fill();
    ctx.fillStyle = pressed ? '#ffd29a' : '#b4b9c6';
    roundRect(ctx, px + c * 0.22, py + c * (pressed ? 0.24 : 0.2), c * 0.56, c * 0.5, c * 0.1);
    ctx.fill();
}

/** Bomb door: riveted iron slab with a bomb sign — only a blast opens it. */
const isWallRun = (L: Level, n: number) => n >= 0 && n < L.base.length && (L.base[n] === '#' || L.base[n] === 'F');

/**
 * Wooden door in a doorway: vertical planks, iron bands, a ring handle and cracks — it is
 * shut for everyone (enemies can't open it); a bomb blast next to it smashes it.
 * `inRow`: the wall runs left-right (door seen from above / below).
 */
function drawWoodDoor(ctx: CanvasRenderingContext2D, px: number, py: number, c: number, inRow: boolean) {
    ctx.fillStyle = '#6b4424'; // frame
    roundRect(ctx, px + c * 0.04, py + c * 0.04, c * 0.92, c * 0.92, c * 0.14);
    ctx.fill();
    for (let k = 0; k < 4; k++) { // planks across the opening
        ctx.fillStyle = k % 2 ? '#c08a4e' : '#a8743f';
        if (inRow) ctx.fillRect(px + c * (0.11 + k * 0.2), py + c * 0.11, c * 0.18, c * 0.78);
        else ctx.fillRect(px + c * 0.11, py + c * (0.11 + k * 0.2), c * 0.78, c * 0.18);
    }
    ctx.fillStyle = '#4a4f5c'; // iron bands
    if (inRow) { ctx.fillRect(px + c * 0.1, py + c * 0.25, c * 0.8, c * 0.07); ctx.fillRect(px + c * 0.1, py + c * 0.68, c * 0.8, c * 0.07); }
    else { ctx.fillRect(px + c * 0.25, py + c * 0.1, c * 0.07, c * 0.8); ctx.fillRect(px + c * 0.68, py + c * 0.1, c * 0.07, c * 0.8); }
    ctx.strokeStyle = '#ffd23f'; // ring handle
    ctx.lineWidth = Math.max(1, c * 0.04);
    ctx.beginPath(); ctx.arc(px + c * 0.5, py + c * 0.5, c * 0.07, 0, Math.PI * 2); ctx.stroke();
    ctx.strokeStyle = 'rgba(40,20,5,0.55)'; // cracks: "this will break"
    ctx.beginPath(); ctx.moveTo(px + c * 0.36, py + c * 0.13); ctx.lineTo(px + c * 0.42, py + c * 0.3); ctx.lineTo(px + c * 0.35, py + c * 0.42); ctx.stroke();
}

/** Thin wooden wall section of a room wall: a plank fence along its run (lasers shine through the gaps). */
function drawWoodWall(ctx: CanvasRenderingContext2D, px: number, py: number, c: number, horizontal: boolean) {
    ctx.fillStyle = 'rgba(0,0,0,0.18)';
    if (horizontal) ctx.fillRect(px, py + c * 0.56, c, c * 0.14);
    else ctx.fillRect(px + c * 0.36, py + c * 0.1, c * 0.34, c * 0.9);
    for (let k = 0; k < 3; k++) {
        ctx.fillStyle = k % 2 ? '#c99a5b' : '#b5824a';
        if (horizontal) ctx.fillRect(px + c * (0.02 + k * 0.33), py + c * 0.22, c * 0.29, c * 0.4);
        else ctx.fillRect(px + c * 0.32, py + c * (0.02 + k * 0.33), c * 0.36, c * 0.29);
    }
    ctx.fillStyle = '#7d5230'; // rail
    if (horizontal) ctx.fillRect(px, py + c * 0.38, c, c * 0.07);
    else ctx.fillRect(px + c * 0.47, py, c * 0.07, c);
}

function drawDebris(ctx: CanvasRenderingContext2D, px: number, py: number, c: number) {
    ctx.fillStyle = '#b5824a';
    for (const [ax, ay, w, h] of [[0.15, 0.2, 0.18, 0.08], [0.6, 0.3, 0.22, 0.07], [0.3, 0.7, 0.16, 0.09], [0.7, 0.72, 0.12, 0.08]]) {
        ctx.fillRect(px + c * ax, py + c * ay, c * w, c * h);
    }
}

function drawSwitchGate(ctx: CanvasRenderingContext2D, px: number, py: number, c: number, col: string, open: boolean) {
    ctx.strokeStyle = col;
    ctx.lineWidth = Math.max(1, c * (open ? 0.05 : 0.1));
    ctx.globalAlpha = open ? 0.45 : 1;
    ctx.setLineDash(open ? [c * 0.1, c * 0.1] : []);
    ctx.strokeRect(px + c * 0.1, py + c * 0.1, c * 0.8, c * 0.8);
    if (!open) {
        for (const k of [0.32, 0.5, 0.68]) {
            ctx.beginPath(); ctx.moveTo(px + c * k, py + c * 0.12); ctx.lineTo(px + c * k, py + c * 0.88); ctx.stroke();
        }
    }
    ctx.setLineDash([]);
    ctx.globalAlpha = 1;
}

function circle(ctx: CanvasRenderingContext2D, x: number, y: number, r: number) {
    ctx.beginPath();
    ctx.arc(x, y, r, 0, Math.PI * 2);
    ctx.fill();
}

function roundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number) {
    ctx.beginPath();
    ctx.roundRect(x, y, w, h, r);
}
