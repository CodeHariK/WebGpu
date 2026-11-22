// Colour Blocks — drawing the placeholder buildings (Map26Buildings): each with a shadow cast to the
// bottom right as long as it is tall (its floors), so height shows on the flat map; the parks'
// ponds and trees; the malls' car parks with their bays.
import type { Buildings, Building } from './Map26Buildings';
import type { Circle, Rect } from './Map26Fit';

const ROOF: Record<Building['kind'], string> = { downtown: '#b9b3d6', tower: '#e8d9a8', industry: '#a9adb3', mall: '#e9a39b' };
const EDGE = 'rgba(45, 35, 40, 0.8)';
const SHADOW_PER_FLOOR = 0.45; // pixels of shadow a floor

function rectPath(p_ctx: CanvasRenderingContext2D, p_r: Rect, p_cell: number, p_dx: number, p_dy: number): void {
    const ux = Math.cos(p_r.angle);
    const uy = Math.sin(p_r.angle);
    const w = p_r.width / 2;
    const d = p_r.depth / 2;
    p_ctx.beginPath();
    for (const [a, b] of [[-w, -d], [w, -d], [w, d], [-w, d]]) {
        p_ctx.lineTo((p_r.x + ux * a - uy * b) * p_cell + p_dx, (p_r.y + uy * a + ux * b) * p_cell + p_dy);
    }
    p_ctx.closePath();
}

function circlePath(p_ctx: CanvasRenderingContext2D, p_c: Circle, p_cell: number, p_dx: number, p_dy: number): void {
    p_ctx.beginPath();
    p_ctx.arc(p_c.x * p_cell + p_dx, p_c.y * p_cell + p_dy, p_c.radius * p_cell, 0, Math.PI * 2);
}

// The car parks: grey, with lines across for the bays.
function drawCarParks(p_ctx: CanvasRenderingContext2D, p_parks: Rect[], p_cell: number): void {
    for (const r of p_parks) {
        p_ctx.fillStyle = '#9d9a94';
        rectPath(p_ctx, r, p_cell, 0, 0);
        p_ctx.fill();
        const ux = Math.cos(r.angle);
        const uy = Math.sin(r.angle);
        p_ctx.strokeStyle = 'rgba(255, 255, 255, 0.7)';
        p_ctx.lineWidth = 0.7;
        p_ctx.beginPath();
        for (let a = -r.width / 2 + 0.6; a < r.width / 2; a += 0.6) {
            for (const b of [-r.depth / 2 + 0.15, r.depth / 2 - 0.15]) {
                const x = r.x + ux * a;
                const y = r.y + uy * a;
                const inner = b > 0 ? b - r.depth * 0.3 : b + r.depth * 0.3;
                p_ctx.moveTo((x - uy * b) * p_cell, (y + ux * b) * p_cell);
                p_ctx.lineTo((x - uy * inner) * p_cell, (y + ux * inner) * p_cell);
            }
        }
        p_ctx.stroke();
    }
}

// The parks: ponds (blue, a pale rim) and trees (dark green, a lighter top, a soft shadow).
function drawPark(p_ctx: CanvasRenderingContext2D, p_b: Buildings, p_cell: number): void {
    for (const p of p_b.ponds) {
        p_ctx.fillStyle = '#d8e6c8';
        circlePath(p_ctx, { ...p, radius: p.radius + 0.35 }, p_cell, 0, 0);
        p_ctx.fill();
        p_ctx.fillStyle = '#5aa6d6';
        circlePath(p_ctx, p, p_cell, 0, 0);
        p_ctx.fill();
    }
    for (const t of p_b.trees) {
        p_ctx.fillStyle = 'rgba(30, 50, 20, 0.3)';
        circlePath(p_ctx, t, p_cell, 1.2, 1.2);
        p_ctx.fill();
        p_ctx.fillStyle = '#3f7d33';
        circlePath(p_ctx, t, p_cell, 0, 0);
        p_ctx.fill();
        p_ctx.fillStyle = '#6aa84f';
        circlePath(p_ctx, { x: t.x - t.radius * 0.25, y: t.y - t.radius * 0.25, radius: t.radius * 0.55 }, p_cell, 0, 0);
        p_ctx.fill();
    }
}

// p_cell: pixels a fine cell. p_flat: only what lies flat on the ground (car parks, ponds: for the
// 3D view's ground, which builds the rest itself).
export function drawBuildings(p_ctx: CanvasRenderingContext2D, p_b: Buildings, p_cell: number, p_flat = false): void {
    p_ctx.save();
    drawCarParks(p_ctx, p_b.carParks, p_cell);
    if (p_flat) {
        drawPark(p_ctx, { ...p_b, trees: [] }, p_cell);
        p_ctx.restore();
        return;
    }
    drawPark(p_ctx, p_b, p_cell);
    const shape = (b: Building, dx: number, dy: number) => (b.rect ? rectPath(p_ctx, b.rect, p_cell, dx, dy) : circlePath(p_ctx, b.circle!, p_cell, dx, dy));
    p_ctx.fillStyle = 'rgba(40, 30, 35, 0.28)'; // shadows first, all of them under every roof
    for (const b of p_b.buildings) {
        const s = Math.min(14, b.floors * SHADOW_PER_FLOOR + 0.8);
        for (let k = 1; k <= 3; k++) {
            shape(b, (s * k) / 3, (s * k) / 3);
            p_ctx.fill();
        }
    }
    p_ctx.strokeStyle = EDGE;
    p_ctx.lineWidth = 0.9;
    for (const b of p_b.buildings) {
        p_ctx.fillStyle = ROOF[b.kind];
        shape(b, 0, 0);
        p_ctx.fill();
        p_ctx.stroke();
        if (b.kind === 'industry' && b.rect) { // ridges along the shed
            const r = b.rect;
            const ux = Math.cos(r.angle);
            const uy = Math.sin(r.angle);
            p_ctx.beginPath();
            for (let q = -r.depth / 2 + 0.7; q < r.depth / 2; q += 0.7) {
                p_ctx.moveTo((r.x - ux * r.width / 2 - uy * q) * p_cell, (r.y - uy * r.width / 2 + ux * q) * p_cell);
                p_ctx.lineTo((r.x + ux * r.width / 2 - uy * q) * p_cell, (r.y + uy * r.width / 2 + ux * q) * p_cell);
            }
            p_ctx.stroke();
        }
        if (b.kind === 'tower' && b.circle) { // a ring on the round tower's roof
            circlePath(p_ctx, { ...b.circle, radius: b.circle.radius * 0.6 }, p_cell, 0, 0);
            p_ctx.stroke();
        }
    }
    p_ctx.restore();
}
