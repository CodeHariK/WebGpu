import { Vector2 } from '../lib/Vector2';
import { getHexVertices, getNeighborPos, DIRECTIONS } from './Map14Logic';
import { isWaterBiome, type BiomeWorld, type HexCell } from './Map18Logic';

/**
 * Map 18 river layer — real drainage on the biome terrain.
 *
 * Rivers here are not random: they follow Map 18's own elevation field. A
 * Priority-Flood from the ocean/lake cells fills pits and gives every LAND hex a
 * downstream pointer toward water, guaranteeing flow reaches the sea. Flow is
 * accumulated up the drainage tree; a channel is drawn only where flow passes a
 * threshold, and rendered as Hex Truchet bezier arcs (edge-midpoint to
 * edge-midpoint through the hex centre) with width ~ sqrt(flow) — thin
 * tributaries merging at confluences into a trunk that widens toward the coast.
 */

export interface RiverArc { m1: Vector2; m2: Vector2; c: Vector2; w: number; }

const key = (q: number, r: number) => `${q},${r}`;
const emid = (v: Vector2[], e: number) => new Vector2((v[e].x + v[(e + 1) % 6].x) / 2, (v[e].y + v[(e + 1) % 6].y) / 2);
const portToward = (dq: number, dr: number) => {
    for (let s = 0; s < 6; s++) if (DIRECTIONS[s].q === dq && DIRECTIONS[s].r === dr) return s;
    return -1;
};

// Tiny binary min-heap keyed by number, carrying a string id.
class MinHeap {
    private a: { k: number; v: string }[] = [];
    get size() { return this.a.length; }
    push(k: number, v: string) {
        const a = this.a; a.push({ k, v });
        let i = a.length - 1;
        while (i > 0) { const p = (i - 1) >> 1; if (a[p].k <= a[i].k) break;[a[p], a[i]] = [a[i], a[p]]; i = p; }
    }
    pop(): { k: number; v: string } {
        const a = this.a; const top = a[0]; const last = a.pop()!;
        if (a.length) { a[0] = last; let i = 0; const n = a.length; for (; ;) { let s = i; const l = 2 * i + 1, r = l + 1; if (l < n && a[l].k < a[s].k) s = l; if (r < n && a[r].k < a[s].k) s = r; if (s === i) break;[a[s], a[i]] = [a[i], a[s]]; i = s; } }
        return top;
    }
}

const isWater = (c: HexCell) => c.regionId < 0 || isWaterBiome(c.biome);

/**
 * Build the Truchet river arcs for the world's land, draining to its sea/lakes.
 * `density` (higher = denser) sets the flow threshold below which no channel is drawn.
 */
export function computeRiverArcs(world: BiomeWorld, density: number): RiverArc[] {
    const { cells, hexSize } = world;

    // Priority-Flood: outlets = water cells; flood inward, filling pits.
    const filled = new Map<string, number>();
    const downstream = new Map<string, string>();
    const visited = new Set<string>();
    const heap = new MinHeap();
    for (const c of cells.values()) {
        if (isWater(c)) {
            const k = key(c.q, c.r);
            filled.set(k, c.elevation);
            visited.add(k);
            heap.push(c.elevation, k);
        }
    }
    const popOrder: string[] = [];
    while (heap.size) {
        const { v: uk } = heap.pop();
        popOrder.push(uk);
        const [uq, ur] = uk.split(',').map(Number);
        const uFill = filled.get(uk)!;
        for (let s = 0; s < 6; s++) {
            const n = getNeighborPos(uq, ur, s);
            const nk = key(n.q, n.r);
            const nc = cells.get(nk);
            if (!nc || visited.has(nk)) continue;
            visited.add(nk);
            const nf = Math.max(nc.elevation, uFill); // fill pits so water can escape
            filled.set(nk, nf);
            downstream.set(nk, uk);                   // drains toward where we came from (toward the sea)
            heap.push(nf, nk);
        }
    }

    // Flow accumulation up the drainage tree (process high -> low).
    const acc = new Map<string, number>();
    for (const c of cells.values()) if (!isWater(c)) acc.set(key(c.q, c.r), 1);
    for (let i = popOrder.length - 1; i >= 0; i--) {
        const k = popOrder[i];
        if (isWater(cells.get(k)!)) continue;
        const d = downstream.get(k);
        if (d && !isWater(cells.get(d)!)) acc.set(d, (acc.get(d) || 0) + (acc.get(k) || 0));
    }

    let maxAcc = 1;
    for (const a of acc.values()) if (a > maxAcc) maxAcc = a;
    const K = Math.max(2, 16 - density);
    const wMin = 1.6, wScale = (hexSize * 0.55) / Math.sqrt(maxAcc);
    const wOf = (a: number) => wMin + wScale * Math.sqrt(a);

    // children map (who drains INTO each cell)
    const childrenOf = new Map<string, string[]>();
    for (const [k, d] of downstream) { const a = childrenOf.get(d); if (a) a.push(k); else childrenOf.set(d, [k]); }

    const arcs: RiverArc[] = [];
    for (const c of cells.values()) {
        if (isWater(c)) continue;
        const k = key(c.q, c.r);
        const a = acc.get(k) || 0;
        if (a < K) continue;
        const d = downstream.get(k);
        if (!d) continue;
        const [dq, dr] = d.split(',').map(Number);
        const downPort = portToward(dq - c.q, dr - c.r);
        if (downPort < 0) continue;
        const v = getHexVertices(c.center, hexSize);
        const mDown = emid(v, downPort);
        const drawnCh = (childrenOf.get(k) || []).filter(ck => (acc.get(ck) || 0) >= K);
        if (drawnCh.length === 0) {
            arcs.push({ m1: c.center.clone(), m2: mDown, c: c.center, w: wOf(a) }); // source
        } else {
            for (const ck of drawnCh) {
                const [cq, cr] = ck.split(',').map(Number);
                const up = portToward(cq - c.q, cr - c.r);
                if (up < 0) continue;
                arcs.push({ m1: emid(v, up), m2: mDown, c: c.center, w: wOf(acc.get(ck)!) });
            }
        }
    }
    return arcs;
}

/** Stroke the river arcs as three passes (casing, water, highlight). */
export function drawRivers(ctx: CanvasRenderingContext2D, arcs: RiverArc[], hexSize: number, curve = 0.5) {
    const pass = (extra: number, color: string, alpha: number, widthMul: number) => {
        ctx.strokeStyle = color; ctx.lineCap = 'round'; ctx.lineJoin = 'round'; ctx.globalAlpha = alpha;
        for (const ar of arcs) {
            const d1 = ar.c.clone().sub(ar.m1).normalize();
            const d2 = ar.c.clone().sub(ar.m2).normalize();
            const s = hexSize * curve;
            const cp1 = ar.m1.clone().add(d1.mult(s));
            const cp2 = ar.m2.clone().add(d2.mult(s));
            ctx.lineWidth = Math.max(1, ar.w * widthMul + extra);
            ctx.beginPath();
            ctx.moveTo(ar.m1.x, ar.m1.y);
            ctx.bezierCurveTo(cp1.x, cp1.y, cp2.x, cp2.y, ar.m2.x, ar.m2.y);
            ctx.stroke();
        }
        ctx.globalAlpha = 1;
    };
    pass(Math.max(2, hexSize * 0.12), '#28577f', 1, 1); // casing
    pass(0, '#4f9fd0', 1, 1);                            // water
    pass(0, '#a9dcf0', 0.5, 0.3);                        // highlight
}
