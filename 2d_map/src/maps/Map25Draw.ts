// Islands — drawn on top: the bridges (planked decks on posts), and, when asked for, the parts the
// islands are made of: each falloff's reach (dashed circle), the headlands and the bridge ends.
import type { Layout } from './Map25Layout';
import type { Parts, Tip } from './Map25Parts';

const BRIDGE_WIDTH = 9;

function tipOf(p_parts: Parts[], p_island: number, p_link: number): Tip | undefined {
    return p_parts[p_island].tips.find((t) => t.link === p_link);
}

// A bridge from tip to tip, starting a little inland at each end.
function bridge(p_ctx: CanvasRenderingContext2D, p_a: Tip, p_b: Tip): void {
    const dx = p_b.x - p_a.x;
    const dy = p_b.y - p_a.y;
    const length = Math.hypot(dx, dy);
    const ux = dx / length;
    const uy = dy / length;
    const ax = p_a.x + ux * p_a.r * 0.3;
    const ay = p_a.y + uy * p_a.r * 0.3;
    const bx = p_b.x - ux * p_b.r * 0.3;
    const by = p_b.y - uy * p_b.r * 0.3;
    const span = Math.hypot(bx - ax, by - ay);
    p_ctx.save();
    p_ctx.translate(ax, ay);
    p_ctx.rotate(Math.atan2(uy, ux));
    p_ctx.fillStyle = 'rgba(20, 60, 90, 0.25)'; // shadow on the water
    p_ctx.fillRect(2, -BRIDGE_WIDTH / 2 + 4, span, BRIDGE_WIDTH);
    for (let x = 14; x < span - 10; x += 22) { // posts
        p_ctx.fillStyle = '#5b4632';
        p_ctx.fillRect(x - 2, -BRIDGE_WIDTH / 2 - 2, 4, BRIDGE_WIDTH + 4);
    }
    p_ctx.fillStyle = '#6a5440';
    p_ctx.fillRect(0, -BRIDGE_WIDTH / 2 - 1.5, span, BRIDGE_WIDTH + 3);
    p_ctx.fillStyle = '#e8cf9c';
    p_ctx.fillRect(0, -BRIDGE_WIDTH / 2, span, BRIDGE_WIDTH);
    p_ctx.strokeStyle = 'rgba(106, 84, 64, 0.45)'; // planks
    p_ctx.lineWidth = 0.8;
    for (let x = 3; x < span; x += 3.5) {
        p_ctx.beginPath();
        p_ctx.moveTo(x, -BRIDGE_WIDTH / 2);
        p_ctx.lineTo(x, BRIDGE_WIDTH / 2);
        p_ctx.stroke();
    }
    p_ctx.restore();
}

export function drawBridges(p_ctx: CanvasRenderingContext2D, p_layout: Layout, p_parts: Parts[]): void {
    p_layout.links.forEach((link, index) => {
        const a = tipOf(p_parts, link.a, index);
        const b = tipOf(p_parts, link.b, index);
        if (a && b) bridge(p_ctx, a, b);
    });
}

export function drawParts(p_ctx: CanvasRenderingContext2D, p_parts: Parts[]): void {
    p_ctx.save();
    for (const p of p_parts) {
        p_ctx.strokeStyle = '#2f5fa8'; // the falloff's reach: dashed
        p_ctx.setLineDash([5, 4]);
        p_ctx.lineWidth = 1.5;
        p_ctx.beginPath();
        p_ctx.arc(p.hill.x, p.hill.y, p.hill.size, 0, Math.PI * 2);
        p_ctx.stroke();
        p_ctx.setLineDash([]);
        p_ctx.strokeStyle = '#c8372d'; // headlands: their middle line and end circles
        p_ctx.lineWidth = 1.5;
        for (const h of p.headlands) {
            p_ctx.beginPath();
            p_ctx.moveTo(h.ax, h.ay);
            p_ctx.lineTo(h.bx, h.by);
            p_ctx.stroke();
            p_ctx.beginPath();
            p_ctx.arc(h.bx, h.by, h.br, 0, Math.PI * 2);
            p_ctx.stroke();
        }
        p_ctx.fillStyle = '#c8372d';
        for (const t of p.tips) {
            p_ctx.beginPath();
            p_ctx.arc(t.x, t.y, 4, 0, Math.PI * 2);
            p_ctx.fill();
        }
    }
    p_ctx.restore();
}
