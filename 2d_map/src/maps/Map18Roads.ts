import { Vector2 } from '../lib/Vector2';
import { getHexVertices, getNeighborPos, DIRECTIONS } from './Map14Logic';
import { isWaterBiome, type BiomeWorld, type HexCell } from './Map18Logic';

/**
 * Map 18 road layer — a settlement network drawn as Hex Truchet arcs.
 *
 * Towns are placed on good lowland, spread apart. Within each landmass they are
 * wired into a network (minimum spanning tree + a few extra links for loops),
 * and every link is routed with Dijkstra over the hex grid whose step cost adds
 * a penalty for slope and rough biomes and forbids water — so roads hug flat
 * ground and bend around the terrain. Each hex a road crosses is rendered as a
 * Truchet bezier arc between the entry and exit ports; hexes shared by several
 * roads become junctions.
 */

export interface RoadArc { m1: Vector2; m2: Vector2; c: Vector2; }
export interface RoadNetwork { arcs: RoadArc[]; towns: Vector2[]; }

const key = (q: number, r: number) => `${q},${r}`;
const emid = (v: Vector2[], e: number) => new Vector2((v[e].x + v[(e + 1) % 6].x) / 2, (v[e].y + v[(e + 1) % 6].y) / 2);
const portToward = (dq: number, dr: number) => {
    for (let s = 0; s < 6; s++) if (DIRECTIONS[s].q === dq && DIRECTIONS[s].r === dr) return s;
    return -1;
};
const isWater = (c: HexCell) => c.regionId < 0 || isWaterBiome(c.biome);

class MinHeap {
    private a: { k: number; v: string }[] = [];
    get size() { return this.a.length; }
    push(k: number, v: string) { const a = this.a; a.push({ k, v }); let i = a.length - 1; while (i > 0) { const p = (i - 1) >> 1; if (a[p].k <= a[i].k) break;[a[p], a[i]] = [a[i], a[p]]; i = p; } }
    pop() { const a = this.a; const top = a[0]; const last = a.pop()!; if (a.length) { a[0] = last; let i = 0; const n = a.length; for (; ;) { let s = i; const l = 2 * i + 1, r = l + 1; if (l < n && a[l].k < a[s].k) s = l; if (r < n && a[r].k < a[s].k) s = r; if (s === i) break;[a[s], a[i]] = [a[i], a[s]]; i = s; } } return top; }
}

// biome roughness penalty (mountains/snow/desert are harder to build across)
function roughness(b: string): number {
    if (b === 'SNOW' || b === 'BARE' || b === 'SCORCHED') return 8;
    if (b === 'TUNDRA' || b === 'TAIGA' || b === 'SHRUBLAND') return 2.5;
    if (b === 'TEMPERATE_DESERT' || b === 'SUBTROPICAL_DESERT') return 1.5;
    return 0;
}

export function computeRoadNetwork(world: BiomeWorld, townCount: number, seed: number): RoadNetwork {
    const { cells, hexSize, seaLevel } = world;
    // deterministic RNG independent of the biome RNG
    let s = (seed ^ 0x9e3779b9) >>> 0;
    const rng = () => { s = (s + 0x6d2b79f5) | 0; let t = Math.imul(s ^ (s >>> 15), 1 | s); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };

    // Candidate town sites: lowland, not water.
    const cand: HexCell[] = [];
    for (const c of cells.values()) {
        if (isWater(c)) continue;
        const eAbove = (c.elevation - seaLevel) / (1 - seaLevel);
        if (eAbove < 0.5) cand.push(c);
    }
    // shuffle
    for (let i = cand.length - 1; i > 0; i--) { const j = Math.floor(rng() * (i + 1));[cand[i], cand[j]] = [cand[j], cand[i]]; }
    const minD = hexSize * 5;
    const towns: HexCell[] = [];
    for (const c of cand) {
        if (towns.every(t => Vector2.dist(t.center, c.center) > minD)) { towns.push(c); if (towns.length >= townCount) break; }
    }

    // group towns per landmass
    const byRegion = new Map<number, HexCell[]>();
    for (const t of towns) { const a = byRegion.get(t.regionId); if (a) a.push(t); else byRegion.set(t.regionId, [t]); }

    // Dijkstra over land of one region; returns hex-key path or null.
    const route = (a: HexCell, b: HexCell, region: number): string[] | null => {
        const dist = new Map<string, number>();
        const prev = new Map<string, string>();
        const ak = key(a.q, a.r), bk = key(b.q, b.r);
        dist.set(ak, 0);
        const heap = new MinHeap(); heap.push(0, ak);
        while (heap.size) {
            const { v: uk } = heap.pop();
            if (uk === bk) break;
            const du = dist.get(uk)!;
            const uc = cells.get(uk)!;
            const [uq, ur] = uk.split(',').map(Number);
            for (let s2 = 0; s2 < 6; s2++) {
                const n = getNeighborPos(uq, ur, s2);
                const nk = key(n.q, n.r);
                const nc = cells.get(nk);
                if (!nc || nc.regionId !== region || isWater(nc)) continue;
                const slope = Math.abs(nc.elevation - uc.elevation) * 40;
                const w = 1 + slope + roughness(nc.biome);
                const nd = du + w;
                if (nd < (dist.get(nk) ?? Infinity)) { dist.set(nk, nd); prev.set(nk, uk); heap.push(nd, nk); }
            }
        }
        if (!prev.has(bk) && ak !== bk) return null;
        const path = [bk]; let c = bk; while (c !== ak) { c = prev.get(c)!; if (!c) return null; path.push(c); } path.reverse();
        return path;
    };

    // Build edges: MST per region (by world distance) + a few extra links for loops.
    const edges: [HexCell, HexCell][] = [];
    for (const [region, T] of byRegion) {
        if (T.length < 2) continue;
        const connected = [T[0]]; const remaining = T.slice(1);
        while (remaining.length) {
            let bi = -1, ba: HexCell | null = null, bd = Infinity;
            for (const a of connected) for (let j = 0; j < remaining.length; j++) { const d = Vector2.dist(a.center, remaining[j].center); if (d < bd) { bd = d; bi = j; ba = a; } }
            if (bi < 0 || !ba) break;
            edges.push([ba, remaining[bi]]); connected.push(remaining[bi]); remaining.splice(bi, 1);
        }
        // extra loop links
        for (const a of T) {
            let nb: HexCell | null = null, nd = Infinity;
            for (const b of T) { if (b === a) continue; const d = Vector2.dist(a.center, b.center); if (d < nd) { nd = d; nb = b; } }
            if (nb && rng() < 0.35) edges.push([a, nb]);
        }
        void region;
    }

    // Route edges and collect Truchet arcs (deduped).
    const seen = new Set<string>();
    const arcs: RoadArc[] = [];
    const addArc = (cell: HexCell, p1: Vector2, p2: Vector2, id: string) => {
        if (seen.has(id)) return; seen.add(id);
        arcs.push({ m1: p1, m2: p2, c: cell.center });
    };
    for (const [a, b] of edges) {
        const path = route(a, b, a.regionId);
        if (!path || path.length < 2) continue;
        for (let i = 0; i < path.length; i++) {
            const hk = path[i];
            const cell = cells.get(hk)!;
            const [q, r] = hk.split(',').map(Number);
            const v = getHexVertices(cell.center, hexSize);
            const portTo = (ok: string) => { const [oq, or_] = ok.split(',').map(Number); return portToward(oq - q, or_ - r); };
            if (i === 0) { const ep = portTo(path[i + 1]); if (ep >= 0) addArc(cell, cell.center.clone(), emid(v, ep), `${hk}|c${ep}`); }
            else if (i === path.length - 1) { const ep = portTo(path[i - 1]); if (ep >= 0) addArc(cell, cell.center.clone(), emid(v, ep), `${hk}|c${ep}`); }
            else {
                const e1 = portTo(path[i - 1]), e2 = portTo(path[i + 1]);
                if (e1 >= 0 && e2 >= 0) { const lo = Math.min(e1, e2), hi = Math.max(e1, e2); addArc(cell, emid(v, e1), emid(v, e2), `${hk}|${lo}-${hi}`); }
            }
        }
    }

    return { arcs, towns: towns.map(t => t.center.clone()) };
}

export function drawRoads(ctx: CanvasRenderingContext2D, net: RoadNetwork, hexSize: number, curve = 0.5) {
    const w = Math.max(2, hexSize * 0.16);
    const pass = (extra: number, color: string) => {
        ctx.strokeStyle = color; ctx.lineCap = 'round'; ctx.lineJoin = 'round'; ctx.lineWidth = w + extra;
        for (const ar of net.arcs) {
            const d1 = ar.c.clone().sub(ar.m1).normalize();
            const d2 = ar.c.clone().sub(ar.m2).normalize();
            const s = hexSize * curve;
            const cp1 = ar.m1.clone().add(d1.mult(s));
            const cp2 = ar.m2.clone().add(d2.mult(s));
            ctx.beginPath(); ctx.moveTo(ar.m1.x, ar.m1.y);
            ctx.bezierCurveTo(cp1.x, cp1.y, cp2.x, cp2.y, ar.m2.x, ar.m2.y);
            ctx.stroke();
        }
    };
    pass(Math.max(2, hexSize * 0.10), '#6f5b3e'); // casing
    pass(0, '#e3d2a6');                            // road surface
    // towns
    for (const t of net.towns) {
        const r = Math.max(3, hexSize * 0.42);
        ctx.fillStyle = '#ffffff'; ctx.beginPath(); ctx.arc(t.x, t.y, r, 0, Math.PI * 2); ctx.fill();
        ctx.fillStyle = '#c0392b'; ctx.beginPath(); ctx.arc(t.x, t.y, r * 0.68, 0, Math.PI * 2); ctx.fill();
    }
}
