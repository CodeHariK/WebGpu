// Puzzle Islands — solution overlay: the solver's route as a dotted line, pushes as dots.
import type { Level } from './Map19Rules';
import type { Push } from './Map19Solver';
import { applyToken, solutionSteps, startPlay } from './Map19Play';

export type RoutePoint = { cell: number; push: boolean };

/** Every cell the player stands on along the solution (enemies left out, like the solver). */
export function solutionRoute(L: Level, legs: Push[][]): RoutePoint[] {
    let p = startPlay(L);
    const out: RoutePoint[] = [{ cell: p.s.player, push: false }];
    for (const tok of solutionSteps(L, legs)) {
        const next = applyToken(p, tok);
        if (next.s.player !== p.s.player) out.push({ cell: next.s.player, push: next.pushes > p.pushes });
        p = next;
    }
    return out;
}

export function drawRoute(ctx: CanvasRenderingContext2D, L: Level, route: RoutePoint[], cell: number) {
    if (route.length < 2) return;
    const at = (c: number) => [(c % L.w + 0.5) * cell, (Math.floor(c / L.w) + 0.5) * cell] as const;
    ctx.save();
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    ctx.setLineDash([cell * 0.12, cell * 0.22]);
    for (const [color, width] of [['rgba(0,0,0,0.45)', cell * 0.16], ['#fff6c2', cell * 0.09]] as const) {
        ctx.strokeStyle = color;
        ctx.lineWidth = width;
        ctx.beginPath();
        route.forEach((pt, i) => {
            const [x, y] = at(pt.cell);
            const prev = route[i - 1];
            const jump = !prev || Math.abs((prev.cell % L.w) - (pt.cell % L.w)) + Math.abs(Math.floor(prev.cell / L.w) - Math.floor(pt.cell / L.w)) > 1; // teleport
            if (jump) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        });
        ctx.stroke();
    }
    ctx.setLineDash([]);
    ctx.fillStyle = '#ffb02e';
    for (const pt of route) {
        if (!pt.push) continue;
        const [x, y] = at(pt.cell);
        ctx.beginPath();
        ctx.arc(x, y, cell * 0.11, 0, Math.PI * 2);
        ctx.fill();
    }
    const [ex, ey] = at(route[route.length - 1].cell);
    ctx.strokeStyle = '#7dff9a';
    ctx.lineWidth = cell * 0.08;
    ctx.beginPath();
    ctx.arc(ex, ey, cell * 0.3, 0, Math.PI * 2);
    ctx.stroke();
    ctx.restore();
}
