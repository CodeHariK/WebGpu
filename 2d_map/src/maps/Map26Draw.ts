// Colour Blocks — drawing: the cells (coloured once the spread has reached them; the ground's cost
// shaded in under the empty ones), the queue, the grid, the seeds (with their speeds), and the
// highways along the borders.
import type { Borders, Highway } from './Map26Borders';
import type { QueueView } from './Map26Queue';
import type { Spread } from './Map26Spread';

const COLOURS = ['#f4a6a6', '#f6c99a', '#f3e39a', '#b9e3a1', '#9fd8cf', '#a6c8f2', '#c3b2ee', '#eab0d9', '#d6c4a8', '#a8d8a0', '#f0b8a0', '#b0cfe8'];
const EMPTY = '#f5f1e8';
const MUD: [number, number, number] = [150, 128, 100]; // the most expensive ground
const EMPTY_RGB: [number, number, number] = [245, 241, 232];

// p_until: the spread's clock (cells reached later stay empty), Infinity for all of them. While
// spreading, each cell shows the colour that reached it (the blocks are tidied after).
// p_ground: shade the empty cells by their cost (light: quick, brown: slow).
export function drawCells(p_ctx: CanvasRenderingContext2D, p_s: Spread, p_size: number, p_until: number, p_ground: boolean): void {
    const colours = p_until === Infinity ? p_s.owner : p_s.reached;
    const cell = p_size / p_s.n;
    p_ctx.fillStyle = EMPTY;
    p_ctx.fillRect(0, 0, p_size, p_size);
    const most = Math.max(...p_s.cost);
    for (let j = 0; j < p_s.n; j++) {
        for (let i = 0; i < p_s.n; i++) {
            const c = j * p_s.n + i;
            if (p_s.time[c] > p_until) {
                if (!p_ground || most <= 1) continue;
                const f = (p_s.cost[c] - 1) / (most - 1); // 0: cost 1 .. 1: the most expensive
                const rgb = EMPTY_RGB.map((e, m) => Math.round(e + (MUD[m] - e) * f));
                p_ctx.fillStyle = `rgb(${rgb[0]}, ${rgb[1]}, ${rgb[2]})`;
                p_ctx.fillRect(i * cell, j * cell, cell + 0.5, cell + 0.5);
                continue;
            }
            p_ctx.fillStyle = COLOURS[colours[c] % COLOURS.length];
            p_ctx.fillRect(i * cell, j * cell, cell + 0.5, cell + 0.5); // + 0.5: no hairline gaps
        }
    }
}

export function drawGrid(p_ctx: CanvasRenderingContext2D, p_n: number, p_size: number): void {
    const cell = p_size / p_n;
    p_ctx.strokeStyle = 'rgba(80, 70, 60, 0.12)';
    p_ctx.lineWidth = 1;
    p_ctx.beginPath();
    for (let k = 0; k <= p_n; k++) {
        p_ctx.moveTo(k * cell, 0);
        p_ctx.lineTo(k * cell, p_size);
        p_ctx.moveTo(0, k * cell);
        p_ctx.lineTo(p_size, k * cell);
    }
    p_ctx.stroke();
}

// p_speeds: write each colour's speed beside its seed.
export function drawSeeds(p_ctx: CanvasRenderingContext2D, p_s: Spread, p_size: number, p_speeds: boolean): void {
    const cell = p_size / p_s.n;
    const labelled = new Set<number>();
    p_s.seeds.forEach((s) => {
        const k = s.colour;
        const x = (s.i + 0.5) * cell;
        const y = (s.j + 0.5) * cell;
        p_ctx.beginPath();
        p_ctx.arc(x, y, Math.max(4, cell * 0.8), 0, Math.PI * 2);
        p_ctx.fillStyle = COLOURS[k % COLOURS.length];
        p_ctx.fill();
        p_ctx.strokeStyle = '#3f372f';
        p_ctx.lineWidth = 2;
        p_ctx.stroke();
        if (!p_speeds || labelled.has(k)) return; // the speed once a colour
        labelled.add(k);
        const label = `speed ${p_s.speed[k].toFixed(2)}`;
        p_ctx.font = '600 12px Inter, sans-serif';
        p_ctx.textAlign = 'center';
        p_ctx.textBaseline = 'bottom';
        p_ctx.lineJoin = 'round';
        p_ctx.lineWidth = 4;
        p_ctx.strokeStyle = 'rgba(255, 255, 255, 0.9)';
        p_ctx.strokeText(label, x, y - 9); // above the seed, clear of the queue round it
        p_ctx.fillStyle = '#3f372f';
        p_ctx.fillText(label, x, y - 9);
        p_ctx.textAlign = 'start';
    });
}

// The waiting cells: their colour-to-be, faint, with a dark outline; the next to burn ringed.
export function drawQueue(p_ctx: CanvasRenderingContext2D, p_q: QueueView, p_n: number, p_size: number): void {
    const cell = p_size / p_n;
    p_ctx.save();
    p_ctx.globalAlpha = 0.55;
    for (const w of p_q.waiting) {
        p_ctx.fillStyle = COLOURS[w.colour % COLOURS.length];
        p_ctx.fillRect((w.cell % p_n) * cell, Math.floor(w.cell / p_n) * cell, cell, cell);
    }
    p_ctx.globalAlpha = 1;
    p_ctx.strokeStyle = '#3f372f';
    p_ctx.lineWidth = 1;
    for (const w of p_q.waiting) p_ctx.strokeRect((w.cell % p_n) * cell + 0.5, Math.floor(w.cell / p_n) * cell + 0.5, cell - 1, cell - 1);
    if (p_q.next) {
        p_ctx.beginPath();
        p_ctx.arc(((p_q.next.cell % p_n) + 0.5) * cell, (Math.floor(p_q.next.cell / p_n) + 0.5) * cell, cell * 1.6, 0, Math.PI * 2);
        p_ctx.strokeStyle = '#c8372d';
        p_ctx.lineWidth = 2.5;
        p_ctx.stroke();
    }
    p_ctx.restore();
}

function path(p_ctx: CanvasRenderingContext2D, p_h: Highway, p_cell: number): void {
    p_ctx.beginPath();
    p_h.points.forEach((p, i) => (i ? p_ctx.lineTo(p.x * p_cell, p.y * p_cell) : p_ctx.moveTo(p.x * p_cell, p.y * p_cell)));
}

// Casing first for all, then the yellow middle for all, so crossings join cleanly; junction dots.
export function drawHighways(p_ctx: CanvasRenderingContext2D, p_b: Borders, p_n: number, p_size: number): void {
    const cell = p_size / p_n;
    p_ctx.lineCap = 'round';
    p_ctx.lineJoin = 'round';
    for (const [colour, width] of [['#5d5349', 7], ['#ffe39a', 4]] as const) {
        p_ctx.strokeStyle = colour;
        p_ctx.lineWidth = width;
        for (const h of p_b.highways) {
            path(p_ctx, h, cell);
            p_ctx.stroke();
        }
    }
    p_ctx.fillStyle = '#5d5349';
    for (const j of p_b.junctions) {
        p_ctx.beginPath();
        p_ctx.arc(j.x * cell, j.y * cell, 3, 0, Math.PI * 2);
        p_ctx.fill();
    }
}
