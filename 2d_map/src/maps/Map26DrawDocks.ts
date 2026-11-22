// Colour Blocks — drawing the docks (Map26Docks): the yards (concrete), the piers (planks), the
// ships (a hull with a pointed bow, a deck house) and the warehouses (rust roofs, ridges, a shadow).
import type { Docks } from './Map26Docks';
import type { Rect } from './Map26Fit';

function rectPath(p_ctx: CanvasRenderingContext2D, p_r: Rect, p_cell: number, p_dx = 0, p_dy = 0): void {
    const ux = Math.cos(p_r.angle);
    const uy = Math.sin(p_r.angle);
    const w = p_r.width / 2;
    const d = p_r.depth / 2;
    p_ctx.beginPath();
    for (const [a, b] of [[-w, -d], [w, -d], [w, d], [-w, d]]) p_ctx.lineTo((p_r.x + ux * a - uy * b) * p_cell + p_dx, (p_r.y + uy * a + ux * b) * p_cell + p_dy);
    p_ctx.closePath();
}

// A ship: its hull pointed at the far end (away from the shore), a small deck house near the stern.
function shipPath(p_ctx: CanvasRenderingContext2D, p_r: Rect, p_cell: number): void {
    const ux = Math.cos(p_r.angle);
    const uy = Math.sin(p_r.angle);
    const w = p_r.width / 2;
    const d = p_r.depth / 2;
    p_ctx.beginPath();
    for (const [a, b] of [[-w, -d], [w - d * 1.4, -d], [w, 0], [w - d * 1.4, d], [-w, d]]) p_ctx.lineTo((p_r.x + ux * a - uy * b) * p_cell, (p_r.y + uy * a + ux * b) * p_cell);
    p_ctx.closePath();
}

// p_cell: pixels a fine cell.
export function drawDocks(p_ctx: CanvasRenderingContext2D, p_d: Docks, p_cell: number): void {
    p_ctx.save();
    p_ctx.lineJoin = 'round';
    for (const yard of p_d.yards) {
        p_ctx.fillStyle = '#c3bfb6';
        p_ctx.strokeStyle = 'rgba(80, 75, 70, 0.6)';
        p_ctx.lineWidth = 1;
        p_ctx.beginPath();
        for (const p of yard) p_ctx.lineTo(p.x * p_cell, p.y * p_cell);
        p_ctx.closePath();
        p_ctx.fill();
        p_ctx.stroke();
    }
    for (const pier of p_d.piers) {
        p_ctx.fillStyle = 'rgba(20, 40, 60, 0.25)';
        rectPath(p_ctx, pier, p_cell, 2, 2);
        p_ctx.fill();
        p_ctx.fillStyle = '#9b7b5a';
        p_ctx.strokeStyle = '#5b4632';
        rectPath(p_ctx, pier, p_cell);
        p_ctx.fill();
        p_ctx.stroke();
    }
    for (const ship of p_d.ships) {
        p_ctx.fillStyle = '#f4f1ea';
        p_ctx.strokeStyle = '#3d3a40';
        p_ctx.lineWidth = 1;
        shipPath(p_ctx, ship, p_cell);
        p_ctx.fill();
        p_ctx.stroke();
        p_ctx.fillStyle = '#d9534f';
        rectPath(p_ctx, { ...ship, x: ship.x - Math.cos(ship.angle) * ship.width * 0.28, y: ship.y - Math.sin(ship.angle) * ship.width * 0.28, width: ship.width * 0.22, depth: ship.depth * 0.6 }, p_cell);
        p_ctx.fill();
    }
    for (const w of p_d.warehouses) {
        p_ctx.fillStyle = 'rgba(40, 30, 35, 0.28)';
        rectPath(p_ctx, w, p_cell, 1.6, 1.6);
        p_ctx.fill();
        p_ctx.fillStyle = '#c98d6a';
        p_ctx.strokeStyle = 'rgba(60, 35, 25, 0.85)';
        p_ctx.lineWidth = 0.9;
        rectPath(p_ctx, w, p_cell);
        p_ctx.fill();
        p_ctx.stroke();
        const ux = Math.cos(w.angle);
        const uy = Math.sin(w.angle);
        p_ctx.beginPath(); // ridges across the shed
        for (let a = -w.width / 2 + 0.8; a < w.width / 2; a += 0.8) {
            p_ctx.moveTo((w.x + ux * a + uy * w.depth / 2) * p_cell, (w.y + uy * a - ux * w.depth / 2) * p_cell);
            p_ctx.lineTo((w.x + ux * a - uy * w.depth / 2) * p_cell, (w.y + uy * a + ux * w.depth / 2) * p_cell);
        }
        p_ctx.stroke();
    }
    p_ctx.restore();
}
