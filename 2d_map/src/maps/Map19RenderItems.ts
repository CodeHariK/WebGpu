// Puzzle Islands — drawing of pickups (hearts, batteries, bomb ammo), player bombs, blast
// flames and the melee swipe.
import { DX, DY, type ItemKind, type Level } from './Map19Rules';
import { BLAST_SHOW, type Blast, type Bomb, type BombKind } from './Map19Bombs';

export const BOMB_COLOR: Record<BombKind, string> = { time: '#ff5a3c', throw: '#5ad67a', remote: '#4aa8ff' };

const cx = (L: Level, i: number, c: number) => ((i % L.w) + 0.5) * c;
const cy = (L: Level, i: number, c: number) => (Math.floor(i / L.w) + 0.5) * c;

export function drawItem(ctx: CanvasRenderingContext2D, L: Level, cell: number, kind: ItemKind, c: number, t: number) {
    const x = cx(L, cell, c);
    const y = cy(L, cell, c) + Math.sin(t * 3 + cell) * c * 0.05;
    glow(ctx, x, y, c * 0.36, kind === 'heart' ? 'rgba(255,77,109,0.25)' : 'rgba(255,255,255,0.18)');
    if (kind === 'heart') {
        ctx.fillStyle = '#ff4d6d';
        heart(ctx, x, y, c * 0.2);
        return;
    }
    drawBombShape(ctx, x, y, c * 0.8, kind, t, false);
    ctx.fillStyle = '#fff';
    ctx.font = `bold ${Math.round(c * 0.26)}px sans-serif`;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText('+', x + c * 0.26, y - c * 0.22);
}

export function drawBattery(ctx: CanvasRenderingContext2D, L: Level, cell: number, c: number, t: number) {
    const x = cx(L, cell, c);
    const y = cy(L, cell, c) + Math.sin(t * 3 + cell) * c * 0.05;
    glow(ctx, x, y, c * 0.36, 'rgba(255,220,60,0.3)');
    ctx.fillStyle = '#2b2f3d';
    ctx.beginPath(); ctx.roundRect(x - c * 0.14, y - c * 0.24, c * 0.28, c * 0.46, c * 0.06); ctx.fill();
    ctx.fillRect(x - c * 0.06, y - c * 0.3, c * 0.12, c * 0.07);
    ctx.fillStyle = '#ffd23f';
    ctx.beginPath(); ctx.roundRect(x - c * 0.1, y - c * 0.02, c * 0.2, c * 0.2, c * 0.04); ctx.fill();
    ctx.fillStyle = '#ffd23f';
    ctx.beginPath(); // bolt
    ctx.moveTo(x + c * 0.03, y - c * 0.2); ctx.lineTo(x - c * 0.06, y - c * 0.05); ctx.lineTo(x + c * 0.01, y - c * 0.05);
    ctx.lineTo(x - c * 0.03, y + c * 0.05); ctx.lineTo(x + c * 0.07, y - c * 0.09); ctx.lineTo(x, y - c * 0.09); ctx.closePath(); ctx.fill();
}

/** A bomb on the ground or in flight (arc from `from` to `cell`). */
export function drawBomb(ctx: CanvasRenderingContext2D, L: Level, b: Bomb, c: number, t: number, flightTime: number, fuseTime: number) {
    let x = cx(L, b.cell, c);
    let y = cy(L, b.cell, c);
    if (b.flight > 0) {
        const k = 1 - b.flight / flightTime;
        x = cx(L, b.from, c) + (x - cx(L, b.from, c)) * k;
        y = cy(L, b.from, c) + (y - cy(L, b.from, c)) * k - Math.sin(k * Math.PI) * c * 0.8;
    }
    const urgent = b.kind === 'time' && b.fuse < 0.8;
    const pulse = b.kind === 'time' ? 1 + 0.08 * Math.sin(t * (urgent ? 40 : 14)) : 1;
    drawBombShape(ctx, x, y, c * pulse, b.kind, t, true);
    if (b.kind === 'time' && b.flight <= 0) { // fuse ring
        ctx.strokeStyle = urgent ? '#ffffff' : BOMB_COLOR.time;
        ctx.lineWidth = Math.max(1, c * 0.05);
        ctx.beginPath(); ctx.arc(x, y, c * 0.4, -Math.PI / 2, -Math.PI / 2 + (Math.PI * 2 * Math.max(0, b.fuse)) / fuseTime); ctx.stroke();
    }
}

function drawBombShape(ctx: CanvasRenderingContext2D, x: number, y: number, c: number, kind: BombKind, t: number, lit: boolean) {
    ctx.fillStyle = 'rgba(0,0,0,0.25)';
    ctx.beginPath(); ctx.ellipse(x, y + c * 0.24, c * 0.22, c * 0.07, 0, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = '#23252e';
    circle(ctx, x, y, c * 0.24);
    ctx.fillStyle = BOMB_COLOR[kind];
    circle(ctx, x, y, c * 0.11); // colour band = type
    ctx.fillStyle = 'rgba(255,255,255,0.35)';
    circle(ctx, x - c * 0.08, y - c * 0.09, c * 0.05);
    ctx.strokeStyle = '#b08a5a';
    ctx.lineWidth = Math.max(1, c * 0.04);
    ctx.beginPath(); ctx.moveTo(x + c * 0.12, y - c * 0.18); ctx.quadraticCurveTo(x + c * 0.22, y - c * 0.34, x + c * 0.3, y - c * 0.3); ctx.stroke();
    if (kind === 'remote') { // antenna light
        ctx.fillStyle = Math.sin(t * 8) > 0 ? '#9fe0ff' : BOMB_COLOR.remote;
        circle(ctx, x + c * 0.3, y - c * 0.3, c * 0.05);
    } else if (lit) { // spark
        ctx.fillStyle = Math.sin(t * 30) > 0 ? '#fff3a0' : '#ff9a3c';
        circle(ctx, x + c * 0.3, y - c * 0.3, c * (0.04 + 0.03 * Math.abs(Math.sin(t * 25))));
    }
}

export function drawBlast(ctx: CanvasRenderingContext2D, L: Level, b: Blast, c: number) {
    const k = b.t / BLAST_SHOW; // 1 → 0
    for (const cell of b.cells) {
        const x = cx(L, cell, c);
        const y = cy(L, cell, c);
        ctx.globalAlpha = Math.min(1, k * 1.4);
        ctx.fillStyle = '#ff7a1a';
        circle(ctx, x, y, c * (0.5 - 0.15 * (1 - k)));
        ctx.fillStyle = '#ffd23f';
        circle(ctx, x, y, c * 0.32 * k);
        ctx.fillStyle = '#fffbe0';
        circle(ctx, x, y, c * 0.15 * k);
    }
    ctx.globalAlpha = 1;
}

export function drawSwing(ctx: CanvasRenderingContext2D, x: number, y: number, c: number, dir: number, k: number) {
    const a = Math.atan2(DY[dir], DX[dir]);
    ctx.strokeStyle = `rgba(255,255,255,${0.9 * k})`;
    ctx.lineWidth = c * 0.12;
    ctx.lineCap = 'round';
    ctx.beginPath(); ctx.arc(x, y, c * 0.62, a - 0.9, a + 0.9); ctx.stroke();
    ctx.lineCap = 'butt';
}

const PAD_COLORS = ['#b46cff', '#3be0c8', '#ff7ac8'];

/** Teleporter pad: glowing swirl ring; both pads of a pair share a colour. */
export function drawPad(ctx: CanvasRenderingContext2D, L: Level, cell: number, c: number, t: number, pair: number) {
    const x = cx(L, cell, c);
    const y = cy(L, cell, c);
    const col = PAD_COLORS[pair % PAD_COLORS.length];
    ctx.fillStyle = 'rgba(30,20,50,0.45)';
    circle(ctx, x, y, c * 0.42);
    ctx.strokeStyle = col;
    ctx.lineWidth = Math.max(1, c * 0.07);
    ctx.beginPath(); ctx.arc(x, y, c * 0.36, 0, Math.PI * 2); ctx.stroke();
    ctx.lineWidth = Math.max(1, c * 0.05);
    for (let k = 0; k < 3; k++) { // swirl
        const a = t * 3 + (k * Math.PI * 2) / 3;
        ctx.beginPath(); ctx.arc(x, y, c * 0.22, a, a + 1.4); ctx.stroke();
    }
    ctx.fillStyle = col;
    circle(ctx, x, y, c * (0.07 + 0.02 * Math.sin(t * 6)));
}

export function drawDeadAlien(ctx: CanvasRenderingContext2D, x: number, y: number, c: number) {
    ctx.fillStyle = 'rgba(80,40,110,0.55)';
    for (let k = 0; k < 6; k++) circle(ctx, x + Math.cos(k * 1.1) * c * 0.2, y + Math.sin(k * 1.7) * c * 0.15, c * (0.09 + 0.03 * (k % 2)));
}

function glow(ctx: CanvasRenderingContext2D, x: number, y: number, r: number, col: string) {
    ctx.fillStyle = col;
    circle(ctx, x, y, r);
}

function heart(ctx: CanvasRenderingContext2D, x: number, y: number, r: number) {
    ctx.beginPath();
    ctx.moveTo(x, y + r);
    ctx.bezierCurveTo(x - r * 1.2, y, x - r * 1.1, y - r * 1.1, x, y - r * 0.4);
    ctx.bezierCurveTo(x + r * 1.1, y - r * 1.1, x + r * 1.2, y, x, y + r);
    ctx.fill();
}

function circle(ctx: CanvasRenderingContext2D, x: number, y: number, r: number) {
    ctx.beginPath();
    ctx.arc(x, y, r, 0, Math.PI * 2);
    ctx.fill();
}
