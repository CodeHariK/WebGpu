// Puzzle Islands — cute top-down drawing of a level state (also used for thumbnails).
import { BOX, boxCell, boxKind, isMirrorBox, DX, DY, LOCK_COLORS, type Level, type State } from './Map19Rules';
import { BULLET_STEP, ENEMY_SPEC, PLAYER_HEALTH, type Bullet, type Enemy } from './Map19Enemies';
import { drawBeams, drawEnvTile, drawMirror } from './Map19RenderEnv';
import { drawBattery, drawBlast, drawBomb, drawItem, drawPad, drawSwing } from './Map19RenderItems';
import { FLIGHT, FUSE, type Blast, type Bomb } from './Map19Bombs';
import type { ItemKind } from './Map19Rules';

export const COLORS = {
    forest: '#1d3b2a',
    tree: '#2a5a3a',
    treeLight: '#3a7a4c',
    floorA: '#a3d67c',
    floorB: '#97cc70',
    hedge: '#2f7d3b',
    hedgeTop: '#4fae5b',
    crate: '#c98d52',
    crateDark: '#7d5230',
    gold: '#ffcc33',
    dino: '#5ccf5a',
    door: '#7a4a2a',
};

export type DrawOptions = {
    cell: number;
    hasKey?: boolean;
    time?: number; // seconds, animates keys / beams
    ghost?: number[]; // cells to highlight (e.g. the solver's next push)
    enemies?: Enemy[];
    bullets?: Bullet[];
    health?: number; // hearts over the player (omit = none)
    hurt?: number; // seconds left of the red hurt flash
    beat?: number; // beat clock for pulsing lasers (omit = draw always-on beams only)
    floorTint?: (string | undefined)[]; // per-cell floor colour overlay (e.g. one colour per dungeon room)
    items?: { cell: number; kind: ItemKind }[]; // omit = the level's starting items
    bombs?: Bomb[];
    blasts?: Blast[];
    facing?: number;
    swing?: number; // seconds left of the melee swipe
};

const hash = (x: number, y: number) => {
    const h = Math.sin(x * 127.1 + y * 311.7) * 43758.5453;
    return h - Math.floor(h);
};

export function drawLevel(ctx: CanvasRenderingContext2D, L: Level, s: State, o: DrawOptions) {
    const c = o.cell;
    const t = o.time ?? 0;
    ctx.save();
    ctx.fillStyle = COLORS.forest;
    ctx.fillRect(0, 0, L.w * c, L.h * c);
    for (let y = 0; y < L.h; y++) {
        for (let x = 0; x < L.w; x++) {
            const i = y * L.w + x;
            const gate = L.gates.get(i);
            if (gate !== undefined) {
                drawGate(ctx, x * c, y * c, c, gate, (s.keys & (1 << gate)) !== 0, (x + y) % 2 === 0);
                continue;
            }
            const b = L.base[i];
            const envUnder = b === 'Q' || b === 'O' ? '#' : b === '_' || b === 'H' || b === 'J' || b === 'D' || b === 'F' ? '.' : null;
            drawCell(ctx, envUnder ?? b, x * c, y * c, c, x, y, L.door.includes(i), o.hasKey ?? false);
            const tint = o.floorTint?.[i];
            if (tint && (envUnder ?? b) !== '#') { // per-room floor colour (dungeon rooms)
                ctx.globalAlpha = (x + y) % 2 ? 0.3 : 0.38;
                ctx.fillStyle = tint;
                ctx.fillRect(x * c, y * c, c, c);
                ctx.globalAlpha = 1;
            }
            if (envUnder) drawEnvTile(ctx, L, s, i, c, t);
        }
    }
    L.padPairs.forEach(([a, b], k) => { drawPad(ctx, L, a, c, t, k); drawPad(ctx, L, b, c, t, k); });
    L.golds.forEach((g, i) => { if (!((s.gold >> i) & 1)) drawKey(ctx, cx(L, g, c), cy(L, g, c) + Math.sin(t * 3 + i) * c * 0.05, c, COLORS.gold); });
    for (const [cell, color] of L.keys) {
        if (!(s.keys & (1 << color))) drawKey(ctx, cx(L, cell, c), cy(L, cell, c) + Math.sin(t * 3 + color) * c * 0.05, c, LOCK_COLORS[color]);
    }
    L.batteries.forEach((b, i) => { if (!((s.batteries >> i) & 1)) drawBattery(ctx, L, b, c, t); });
    for (const it of o.items ?? L.items) drawItem(ctx, L, it.cell, it.kind, c, t);
    for (const b of s.boxes) {
        const px = (boxCell(b) % L.w) * c;
        const py = Math.floor(boxCell(b) / L.w) * c;
        if (boxKind(b) === BOX.metal) drawMetalCrate(ctx, px, py, c);
        else if (isMirrorBox(b)) drawMirror(ctx, px, py, c, boxKind(b) === BOX.mirrorBack);
        else drawCrate(ctx, px, py, c);
    }
    drawBeams(ctx, L, s, c, o.beat, t);
    for (const g of o.ghost ?? []) {
        ctx.strokeStyle = 'rgba(255,255,255,0.9)';
        ctx.setLineDash([c * 0.12, c * 0.08]);
        ctx.lineWidth = Math.max(1, c * 0.06);
        ctx.strokeRect((g % L.w) * c + 2, Math.floor(g / L.w) * c + 2, c - 4, c - 4);
        ctx.setLineDash([]);
    }
    const glide = (e: Enemy) => {
        const k = Math.min(1, Math.max(0, 1 - e.stepTimer / e.stepPeriod));
        return [cx(L, e.prev, c) + (cx(L, e.cell, c) - cx(L, e.prev, c)) * k, cy(L, e.prev, c) + (cy(L, e.cell, c) - cy(L, e.prev, c)) * k];
    };
    for (const e of o.enemies ?? []) {
        const [x, y] = glide(e);
        drawRanges(ctx, x, y, c, e);
    }
    for (const e of o.enemies ?? []) {
        // Continuous walk: glide from the previous cell to the current one over the whole
        // step period (logic stays on cells, so collisions remain exact).
        const [gx, gy] = glide(e);
        const lunge = e.mode === 'charge' || e.mode === 'beam' ? 0 : e.actTimer * c * 1.2; // bite: lunge at you
        const x = gx + DX[e.dir] * lunge;
        const y = gy + DY[e.dir] * lunge;
        if (e.beamLen > 0) drawBeam(ctx, cx(L, e.cell, c), cy(L, e.cell, c), e.beamAngle, e.beamLen * c, c, e.mode === 'beam', t);
        drawEnemy(ctx, x, y, c, e, t);
        if (e.mode !== 'patrol') drawAlert(ctx, x, y - c * 0.55, c, ENEMY_SPEC[e.kind].color);
    }
    for (const b of o.bombs ?? []) drawBomb(ctx, L, b, c, t, FLIGHT, FUSE);
    for (const b of o.blasts ?? []) drawBlast(ctx, L, b, c);
    for (const b of o.bullets ?? []) {
        const k = 1 - Math.max(0, Math.min(1, b.timer / BULLET_STEP));
        const x = cx(L, b.prev, c) + (cx(L, b.cell, c) - cx(L, b.prev, c)) * k;
        const y = cy(L, b.prev, c) + (cy(L, b.cell, c) - cy(L, b.prev, c)) * k;
        ctx.fillStyle = '#fff3a0';
        circle(ctx, x, y, c * 0.12);
        ctx.fillStyle = ENEMY_SPEC.yellow.color;
        circle(ctx, x, y, c * 0.08);
    }
    if (s.player >= 0) {
        const x = cx(L, s.player, c), y = cy(L, s.player, c);
        drawDino(ctx, x, y, c, o.facing ?? 0);
        if (o.swing && o.swing > 0) drawSwing(ctx, x, y, c, o.facing ?? 0, o.swing / 0.18);
        if (o.hurt && o.hurt > 0) {
            ctx.globalAlpha = Math.min(0.7, o.hurt * 2);
            ctx.fillStyle = '#ff3030';
            circle(ctx, x, y, c * 0.38);
            ctx.globalAlpha = 1;
        }
        if (o.health !== undefined) drawHearts(ctx, x, y - c * 0.62, c, o.health);
    }
    ctx.restore();
}

const cx = (L: Level, i: number, c: number) => ((i % L.w) + 0.5) * c;
const cy = (L: Level, i: number, c: number) => (Math.floor(i / L.w) + 0.5) * c;

function drawCell(ctx: CanvasRenderingContext2D, k: string, px: number, py: number, c: number, x: number, y: number, isDoor: boolean, hasKey: boolean) {
    if (isDoor) {
        ctx.fillStyle = COLORS.hedge;
        ctx.fillRect(px, py, c, c);
        ctx.fillStyle = COLORS.door;
        roundRect(ctx, px + c * 0.12, py + c * 0.08, c * 0.76, c * 0.84, c * 0.3);
        ctx.fill();
        ctx.fillStyle = hasKey ? '#7dff9a' : COLORS.gold;
        circle(ctx, px + c * 0.5, py + c * 0.55, c * 0.09);
        return;
    }
    switch (k) {
        case '.':
        case 'K':
            ctx.fillStyle = (x + y) % 2 ? COLORS.floorA : COLORS.floorB;
            ctx.fillRect(px, py, c, c);
            if (hash(x, y) > 0.82) { // tiny flower
                ctx.fillStyle = hash(y, x) > 0.5 ? '#fff7a8' : '#ffb3d1';
                circle(ctx, px + c * (0.2 + 0.6 * hash(x + 3, y)), py + c * (0.2 + 0.6 * hash(x, y + 5)), c * 0.05);
            }
            break;
        case '#':
            ctx.fillStyle = COLORS.hedge;
            roundRect(ctx, px + 1, py + 1, c - 2, c - 2, c * 0.28);
            ctx.fill();
            ctx.fillStyle = COLORS.hedgeTop;
            roundRect(ctx, px + c * 0.15, py + c * 0.1, c * 0.7, c * 0.45, c * 0.22);
            ctx.fill();
            ctx.fillStyle = 'rgba(0,0,0,0.18)';
            circle(ctx, px + c * (0.3 + 0.4 * hash(x, y)), py + c * 0.72, c * 0.06);
            break;
        case ' ':
            // Outside: forest floor with a tree here and there.
            if (hash(x, y) > 0.55) {
                ctx.fillStyle = COLORS.tree;
                circle(ctx, px + c * 0.5, py + c * 0.5, c * 0.42);
                ctx.fillStyle = COLORS.treeLight;
                circle(ctx, px + c * 0.4, py + c * 0.4, c * 0.2);
            }
            break;
    }
}

// Locked gate: coloured bars with a keyhole; once you hold its key it is drawn open.
function drawGate(ctx: CanvasRenderingContext2D, px: number, py: number, c: number, color: number, open: boolean, alt: boolean) {
    ctx.fillStyle = alt ? COLORS.floorA : COLORS.floorB;
    ctx.fillRect(px, py, c, c);
    const col = LOCK_COLORS[color];
    if (open) {
        ctx.strokeStyle = col;
        ctx.globalAlpha = 0.45;
        ctx.lineWidth = Math.max(1, c * 0.08);
        ctx.strokeRect(px + c * 0.1, py + c * 0.1, c * 0.8, c * 0.8);
        ctx.globalAlpha = 1;
        return;
    }
    ctx.fillStyle = '#3b2a1e';
    ctx.fillRect(px + c * 0.06, py + c * 0.06, c * 0.88, c * 0.88);
    ctx.fillStyle = col;
    for (let k = 0; k < 4; k++) ctx.fillRect(px + c * (0.12 + k * 0.21), py + c * 0.1, c * 0.12, c * 0.8);
    ctx.fillStyle = '#3b2a1e';
    circle(ctx, px + c * 0.5, py + c * 0.5, c * 0.16);
    ctx.fillStyle = col;
    circle(ctx, px + c * 0.5, py + c * 0.46, c * 0.06);
    ctx.fillRect(px + c * 0.48, py + c * 0.48, c * 0.04, c * 0.12);
}

function drawCrate(ctx: CanvasRenderingContext2D, px: number, py: number, c: number) {
    const m = c * 0.08;
    ctx.fillStyle = COLORS.crate;
    roundRect(ctx, px + m, py + m, c - 2 * m, c - 2 * m, c * 0.1);
    ctx.fill();
    ctx.strokeStyle = COLORS.crateDark;
    ctx.lineWidth = Math.max(1, c * 0.07);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(px + m * 2, py + m * 2);
    ctx.lineTo(px + c - m * 2, py + c - m * 2);
    ctx.moveTo(px + c - m * 2, py + m * 2);
    ctx.lineTo(px + m * 2, py + c - m * 2);
    ctx.stroke();
}

/** Metal crate: grey steel box with rivets — pushes like a crate, bombs can't break it. */
function drawMetalCrate(ctx: CanvasRenderingContext2D, px: number, py: number, c: number) {
    const m = c * 0.08;
    const g = ctx.createLinearGradient(px, py, px + c, py + c);
    g.addColorStop(0, '#d9e2ea');
    g.addColorStop(1, '#8796a5');
    ctx.fillStyle = g;
    roundRect(ctx, px + m, py + m, c - 2 * m, c - 2 * m, c * 0.12);
    ctx.fill();
    ctx.strokeStyle = '#4f5d6b';
    ctx.lineWidth = Math.max(1, c * 0.07);
    ctx.stroke();
    ctx.strokeStyle = 'rgba(79,93,107,0.6)';
    ctx.lineWidth = Math.max(1, c * 0.05);
    ctx.strokeRect(px + c * 0.28, py + c * 0.28, c * 0.44, c * 0.44);
    ctx.fillStyle = '#4f5d6b';
    for (const [rx, ry] of [[0.2, 0.2], [0.8, 0.2], [0.2, 0.8], [0.8, 0.8]]) circle(ctx, px + c * rx, py + c * ry, c * 0.045);
}

function drawKey(ctx: CanvasRenderingContext2D, x: number, y: number, c: number, color: string) {
    ctx.fillStyle = 'rgba(255,255,255,0.22)';
    circle(ctx, x, y, c * 0.4);
    ctx.strokeStyle = color;
    ctx.fillStyle = color;
    ctx.lineWidth = c * 0.09;
    ctx.beginPath();
    ctx.arc(x - c * 0.12, y, c * 0.13, 0, Math.PI * 2);
    ctx.moveTo(x, y);
    ctx.lineTo(x + c * 0.28, y);
    ctx.moveTo(x + c * 0.2, y);
    ctx.lineTo(x + c * 0.2, y + c * 0.1);
    ctx.stroke();
}

// Enemies: one body shape, coloured per kind, plus a small kind badge (horns / cannon / dish).
function drawEnemy(ctx: CanvasRenderingContext2D, x: number, y: number, c: number, e: Enemy, t: number) {
    const col = ENEMY_SPEC[e.kind].color;
    const bob = Math.sin(t * 5 + e.cell) * c * 0.03;
    ctx.fillStyle = col;
    ctx.beginPath();
    ctx.ellipse(x, y + bob, c * 0.33, c * 0.29, 0, 0, Math.PI * 2);
    ctx.fill();
    ctx.strokeStyle = 'rgba(0,0,0,0.35)';
    ctx.lineWidth = Math.max(1, c * 0.04);
    ctx.stroke();
    const fx = DX[e.dir] * c * 0.12; // eyes look where it's heading
    const fy = DY[e.dir] * c * 0.08;
    for (const s of [-1, 1]) {
        ctx.fillStyle = 'white';
        circle(ctx, x + s * c * 0.11 + fx * 0.5, y - c * 0.05 + bob + fy * 0.5, c * 0.08);
        ctx.fillStyle = '#1a1a22';
        circle(ctx, x + s * c * 0.11 + fx, y - c * 0.05 + bob + fy, c * 0.04);
    }
    ctx.strokeStyle = '#1a1a22';
    ctx.lineWidth = Math.max(1, c * 0.05);
    ctx.beginPath();
    if (e.kind === 'red') { // angry horns
        ctx.moveTo(x - c * 0.22, y - c * 0.24 + bob); ctx.lineTo(x - c * 0.3, y - c * 0.38 + bob);
        ctx.moveTo(x + c * 0.22, y - c * 0.24 + bob); ctx.lineTo(x + c * 0.3, y - c * 0.38 + bob);
    } else if (e.kind === 'yellow') { // cannon barrel toward its facing
        ctx.moveTo(x, y + bob); ctx.lineTo(x + DX[e.dir] * c * 0.42, y + bob + DY[e.dir] * c * 0.42);
    } else if (e.kind === 'blue') { // antenna dish
        ctx.moveTo(x, y - c * 0.28 + bob); ctx.lineTo(x, y - c * 0.42 + bob);
        ctx.stroke();
        ctx.fillStyle = '#bfe3ff';
        circle(ctx, x, y - c * 0.44 + bob, c * 0.06);
        return;
    } else { // patroller: shell line
        ctx.moveTo(x, y - c * 0.28 + bob); ctx.lineTo(x, y + c * 0.28 + bob);
    }
    ctx.stroke();
}

// "!" over an enemy that has noticed you (chasing / shooting / charging).
function drawAlert(ctx: CanvasRenderingContext2D, x: number, y: number, c: number, col: string) {
    ctx.fillStyle = col;
    ctx.font = `bold ${Math.round(c * 0.4)}px sans-serif`;
    ctx.textAlign = 'center';
    ctx.fillText('!', x, y);
}

// Range circles: faint filled disc = chase (notices you inside), dashed ring = attack reach.
function drawRanges(ctx: CanvasRenderingContext2D, x: number, y: number, c: number, e: Enemy) {
    const spec = ENEMY_SPEC[e.kind];
    const hot = e.mode !== 'patrol';
    if (spec.chase > 0) {
        ctx.globalAlpha = hot ? 0.13 : 0.06;
        ctx.fillStyle = spec.color;
        circle(ctx, x, y, spec.chase * c);
        ctx.globalAlpha = hot ? 0.55 : 0.3;
        ctx.strokeStyle = spec.color;
        ctx.lineWidth = Math.max(1, c * 0.04);
        ctx.beginPath(); ctx.arc(x, y, spec.chase * c, 0, Math.PI * 2); ctx.stroke();
    }
    if (e.kind === 'yellow') { // sentry: a sight line straight ahead only
        const len = spec.reach * c;
        ctx.globalAlpha = hot ? 0.9 : 0.45;
        ctx.strokeStyle = spec.color;
        ctx.lineWidth = Math.max(1, c * 0.06);
        ctx.setLineDash([c * 0.18, c * 0.12]);
        ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(x + DX[e.dir] * len, y + DY[e.dir] * len); ctx.stroke();
        ctx.setLineDash([]);
        ctx.globalAlpha = hot ? 0.18 : 0.08; // narrow band = the lane it covers
        ctx.fillStyle = spec.color;
        const w = c * 0.5;
        if (DX[e.dir]) ctx.fillRect(Math.min(x, x + DX[e.dir] * len), y - w / 2, len, w);
        else ctx.fillRect(x - w / 2, Math.min(y, y + DY[e.dir] * len), w, len);
        ctx.globalAlpha = 1;
        return;
    }
    if (spec.reach > 0) {
        ctx.globalAlpha = hot ? 0.9 : 0.5;
        ctx.strokeStyle = spec.color;
        ctx.lineWidth = Math.max(1, c * 0.06);
        ctx.setLineDash([c * 0.18, c * 0.12]);
        ctx.beginPath(); ctx.arc(x, y, spec.reach * c, 0, Math.PI * 2); ctx.stroke();
        ctx.setLineDash([]);
    }
    ctx.globalAlpha = 1;
}

// Blue laser: blinking thin warning line while charging, thick bright beam when firing.
function drawBeam(ctx: CanvasRenderingContext2D, x0: number, y0: number, angle: number, len: number, c: number, firing: boolean, t: number) {
    const x1 = x0 + Math.cos(angle) * len;
    const y1 = y0 + Math.sin(angle) * len;
    ctx.lineCap = 'round';
    if (firing) {
        ctx.strokeStyle = 'rgba(120,200,255,0.55)';
        ctx.lineWidth = c * 0.42;
        ctx.beginPath(); ctx.moveTo(x0, y0); ctx.lineTo(x1, y1); ctx.stroke();
        ctx.strokeStyle = '#e8f6ff';
        ctx.lineWidth = c * 0.16;
        ctx.beginPath(); ctx.moveTo(x0, y0); ctx.lineTo(x1, y1); ctx.stroke();
    } else {
        ctx.strokeStyle = ENEMY_SPEC.blue.color;
        ctx.globalAlpha = 0.35 + 0.5 * Math.abs(Math.sin(t * 14));
        ctx.lineWidth = Math.max(1, c * 0.06);
        ctx.setLineDash([c * 0.12, c * 0.1]);
        ctx.beginPath(); ctx.moveTo(x0, y0); ctx.lineTo(x1, y1); ctx.stroke();
        ctx.setLineDash([]);
        ctx.globalAlpha = 1;
    }
    ctx.lineCap = 'butt';
}

// PLAYER_HEALTH hearts above the player; partial hearts fill left to right (laser chip damage).
function drawHearts(ctx: CanvasRenderingContext2D, x: number, y: number, c: number, health: number) {
    const r = c * 0.13;
    const gap = r * 2.3;
    for (let i = 0; i < PLAYER_HEALTH; i++) {
        const hx = x + (i - (PLAYER_HEALTH - 1) / 2) * gap;
        const fill = Math.max(0, Math.min(1, health - i));
        ctx.fillStyle = 'rgba(0,0,0,0.45)';
        heart(ctx, hx, y, r);
        if (fill > 0) {
            ctx.save();
            ctx.beginPath();
            ctx.rect(hx - r * 1.2, y - r * 1.5, r * 2.4 * fill, r * 3);
            ctx.clip();
            ctx.fillStyle = '#ff4d6d';
            heart(ctx, hx, y, r);
            ctx.restore();
        }
    }
}

function heart(ctx: CanvasRenderingContext2D, x: number, y: number, r: number) {
    ctx.beginPath();
    ctx.moveTo(x, y + r);
    ctx.bezierCurveTo(x - r * 1.2, y, x - r * 1.1, y - r * 1.1, x, y - r * 0.4);
    ctx.bezierCurveTo(x + r * 1.1, y - r * 1.1, x + r * 1.2, y, x, y + r);
    ctx.fill();
}

function drawDino(ctx: CanvasRenderingContext2D, x: number, y: number, c: number, facing: number) {
    ctx.fillStyle = 'rgba(0,0,0,0.2)';
    ctx.beginPath();
    ctx.ellipse(x, y + c * 0.3, c * 0.3, c * 0.1, 0, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = COLORS.dino;
    circle(ctx, x, y, c * 0.34);
    ctx.fillStyle = '#3fa63e';
    for (const k of [-1, 0, 1]) circle(ctx, x + k * c * 0.14, y - c * 0.32, c * 0.08); // spikes
    // Eye + pupil look where the dino faces (aims melee / throws).
    const ex = DX[facing] * c * 0.12;
    const ey = DY[facing] * c * 0.1 - c * 0.06;
    ctx.fillStyle = 'white';
    circle(ctx, x + ex, y + ey, c * 0.11);
    ctx.fillStyle = '#222';
    circle(ctx, x + ex + DX[facing] * c * 0.04, y + ey + DY[facing] * c * 0.04, c * 0.05);
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
