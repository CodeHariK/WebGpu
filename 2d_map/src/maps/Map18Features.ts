import { Vector2 } from '../lib/Vector2';
import { isWaterBiome, type BiomeWorld } from './Map18Logic';
import type { RiverArc } from './Map18Rivers';
import type { RoadNetwork } from './Map18Roads';

/**
 * Map 18 feature layer — a small tile collection placed on the terrain.
 *
 * Tiles: 3 building types (house / cottage / hall), 2 tree types (round / pine),
 * a park and a pond. Placement: a VILLAGE clusters buildings + a park + a pond
 * around every town; AMBIENT nature scatters tree clusters on forest hexes and
 * ponds in low wet hollows. Everything is placed to avoid the road and river
 * corridors (via a spatial-hash of the drawn arcs), so the roads read as
 * weaving between the buildings and ponds rather than through them.
 */

export type FeatureKind = 'house' | 'cottage' | 'hall' | 'tree' | 'pine' | 'snowpine' | 'cactus' | 'rock' | 'park' | 'pond' | 'farm';
export interface Feature { kind: FeatureKind; p: Vector2; s: number; hue: number; variant?: 'adobe' | 'cabin'; }

const key = (q: number, r: number) => `${q},${r}`;

// ---- spatial hash of "blocked" points (road/river corridors + placed items) ----
class Blocker {
    private cell: number;
    private grid = new Map<string, { x: number; y: number; r: number }[]>();
    constructor(cell: number) { this.cell = cell; }
    add(x: number, y: number, r: number) {
        const gx = Math.floor(x / this.cell), gy = Math.floor(y / this.cell);
        const k = `${gx},${gy}`; const a = this.grid.get(k); if (a) a.push({ x, y, r }); else this.grid.set(k, [{ x, y, r }]);
    }
    blocked(x: number, y: number, rad: number): boolean {
        const gx = Math.floor(x / this.cell), gy = Math.floor(y / this.cell);
        for (let i = -1; i <= 1; i++) for (let j = -1; j <= 1; j++) {
            const arr = this.grid.get(`${gx + i},${gy + j}`); if (!arr) continue;
            for (const o of arr) { const dx = o.x - x, dy = o.y - y; if (dx * dx + dy * dy < (o.r + rad) * (o.r + rad)) return true; }
        }
        return false;
    }
}

// sample a few points along a truchet bezier arc (matches Map18Rivers/Roads control points)
function sampleArc(m1: Vector2, m2: Vector2, c: Vector2, hexSize: number, curve: number, into: (x: number, y: number) => void) {
    const d1 = c.clone().sub(m1).normalize(), d2 = c.clone().sub(m2).normalize();
    const s = hexSize * curve;
    const cp1 = m1.clone().add(d1.mult(s)), cp2 = m2.clone().add(d2.mult(s));
    for (const t of [0, 0.33, 0.66, 1]) {
        const u = 1 - t;
        const x = u * u * u * m1.x + 3 * u * u * t * cp1.x + 3 * u * t * t * cp2.x + t * t * t * m2.x;
        const y = u * u * u * m1.y + 3 * u * u * t * cp1.y + 3 * u * t * t * cp2.y + t * t * t * m2.y;
        into(x, y);
    }
}

export interface FeatureOptions { villages: boolean; nature: number; /* 0..100 */ seed: number; }

export function computeFeatures(world: BiomeWorld, rivers: RiverArc[], roads: RoadNetwork, opts: FeatureOptions): Feature[] {
    const { cells, hexSize } = world;
    let s = (opts.seed ^ 0x51ed270b) >>> 0;
    const rng = () => { s = (s + 0x6d2b79f5) | 0; let t = Math.imul(s ^ (s >>> 15), 1 | s); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };

    // corridor blocker (roads + rivers) so features never sit on a route
    const roadBlock = new Blocker(hexSize);
    for (const ar of roads.arcs) sampleArc(ar.m1, ar.m2, ar.c, hexSize, 0.5, (x, y) => roadBlock.add(x, y, hexSize * 0.16));
    for (const ar of rivers) sampleArc(ar.m1, ar.m2, ar.c, hexSize, 0.5, (x, y) => roadBlock.add(x, y, Math.max(hexSize * 0.16, ar.w * 0.5)));

    // item blocker (features don't overlap each other)
    const itemBlock = new Blocker(hexSize);
    const feats: Feature[] = [];
    const place = (f: Feature) => { feats.push(f); itemBlock.add(f.p.x, f.p.y, f.s * 0.7); };
    const free = (x: number, y: number, rad: number) => !roadBlock.blocked(x, y, rad) && !itemBlock.blocked(x, y, rad);
    // world point -> is it dry land? (invert the pointy-top hex transform, cube-round)
    const axialRound = (q: number, r: number) => {
        let x = q, z = r, y = -x - z;
        let rx = Math.round(x), ry = Math.round(y), rz = Math.round(z);
        const xd = Math.abs(rx - x), yd = Math.abs(ry - y), zd = Math.abs(rz - z);
        if (xd > yd && xd > zd) rx = -ry - rz; else if (yd > zd) ry = -rx - rz; else rz = -rx - ry;
        return { q: rx, r: rz };
    };
    const landAt = (x: number, y: number) => {
        const r = y / (1.5 * hexSize);
        const q = x / (Math.sqrt(3) * hexSize) - r / 2;
        const rr = axialRound(q, r);
        const cell = cells.get(key(rr.q, rr.r));
        return !!cell && cell.regionId >= 0 && !isWaterBiome(cell.biome);
    };
    const biomeAt = (x: number, y: number): string => {
        const r = y / (1.5 * hexSize);
        const q = x / (Math.sqrt(3) * hexSize) - r / 2;
        const rr = axialRound(q, r);
        return cells.get(key(rr.q, rr.r))?.biome ?? '';
    };
    const isDesert = (b: string) => b === 'SUBTROPICAL_DESERT' || b === 'TEMPERATE_DESERT';
    const isSnowy = (b: string) => b === 'SNOW' || b === 'TUNDRA';


    // ---- Villages around each town ----
    if (opts.villages) {
        const bTypes: FeatureKind[] = ['house', 'cottage', 'hall'];
        for (const t of roads.towns) {
            const tb = biomeAt(t.x, t.y);
            const bstyle: 'adobe' | 'cabin' | undefined = isDesert(tb) ? 'adobe' : isSnowy(tb) ? 'cabin' : undefined;
            const n = 6 + Math.floor(rng() * 7);
            let placed = 0, tries = 0;
            // buildings clustered near centre
            while (placed < n && tries < n * 12) {
                tries++;
                const ang = rng() * Math.PI * 2;
                const rad = hexSize * (0.5 + rng() * 2.0);
                const x = t.x + Math.cos(ang) * rad, y = t.y + Math.sin(ang) * rad;
                const bs = hexSize * (0.34 + rng() * 0.16);
                if (!free(x, y, bs)) continue;
                const kind = rng() < 0.15 ? 'hall' : bTypes[Math.floor(rng() * 2)];
                place({ kind, p: new Vector2(x, y), s: kind === 'hall' ? bs * 1.5 : bs, hue: rng(), variant: bstyle });
                placed++;
            }
            // a park at the village fringe
            for (let a = 0; a < 8; a++) {
                const ang = rng() * Math.PI * 2, rad = hexSize * (2.2 + rng() * 1.2);
                const x = t.x + Math.cos(ang) * rad, y = t.y + Math.sin(ang) * rad;
                const ps = hexSize * (0.7 + rng() * 0.4);
                if (free(x, y, ps)) { place({ kind: 'park', p: new Vector2(x, y), s: ps, hue: rng() }); break; }
            }
            // farm fields ringing the village
            const nf = 2 + Math.floor(rng() * 3);
            let fp = 0, ft = 0;
            while (fp < nf && ft < nf * 10) {
                ft++;
                const ang = rng() * Math.PI * 2, rad = hexSize * (2.6 + rng() * 1.8);
                const x = t.x + Math.cos(ang) * rad, y = t.y + Math.sin(ang) * rad;
                const fs = hexSize * (0.85 + rng() * 0.4);
                if (landAt(x, y) && free(x, y, fs)) { place({ kind: 'farm', p: new Vector2(x, y), s: fs, hue: rng() }); fp++; }
            }
        }
    }

    // ---- Ambient nature on the terrain ----
    const natureP = opts.nature / 100;
    for (const c of cells.values()) {
        if (c.regionId < 0 || isWaterBiome(c.biome)) continue;
        const b = c.biome;
        const forest = b === 'TEMPERATE_FOREST' || b === 'TEMPERATE_RAINFOREST' || b === 'TROPICAL_FOREST' || b === 'TROPICAL_RAINFOREST' || b === 'TAIGA';
        if (forest && rng() < natureP) {
            const cl = 2 + Math.floor(rng() * 3);
            for (let i = 0; i < cl; i++) {
                const x = c.center.x + (rng() - 0.5) * hexSize * 1.3, y = c.center.y + (rng() - 0.5) * hexSize * 1.3;
                const ts = hexSize * (0.26 + rng() * 0.14);
                if (free(x, y, ts * 0.8)) place({ kind: b === 'TAIGA' || rng() < 0.3 ? 'pine' : 'tree', p: new Vector2(x, y), s: ts, hue: rng() });
            }
        }
        const desert = b === 'SUBTROPICAL_DESERT' || b === 'TEMPERATE_DESERT';
        const snowy = b === 'SNOW' || b === 'TUNDRA';
        const barren = b === 'BARE' || b === 'SCORCHED';
        if (desert && rng() < natureP * 0.8) {
            const cl = 1 + Math.floor(rng() * 2);
            for (let i = 0; i < cl; i++) {
                const x = c.center.x + (rng() - 0.5) * hexSize * 1.2, y = c.center.y + (rng() - 0.5) * hexSize * 1.2;
                const ts = hexSize * (0.24 + rng() * 0.12);
                if (free(x, y, ts * 0.8)) place({ kind: rng() < 0.6 ? 'cactus' : 'rock', p: new Vector2(x, y), s: ts, hue: rng() });
            }
        }
        if (snowy && rng() < natureP * 0.7) {
            const cl = 1 + Math.floor(rng() * 2);
            for (let i = 0; i < cl; i++) {
                const x = c.center.x + (rng() - 0.5) * hexSize * 1.2, y = c.center.y + (rng() - 0.5) * hexSize * 1.2;
                const ts = hexSize * (0.26 + rng() * 0.12);
                if (free(x, y, ts * 0.8)) place({ kind: rng() < 0.7 ? 'snowpine' : 'rock', p: new Vector2(x, y), s: ts, hue: rng() });
            }
        }
        if (barren && rng() < natureP * 0.5) {
            const x = c.center.x + (rng() - 0.5) * hexSize, y = c.center.y + (rng() - 0.5) * hexSize;
            const ts = hexSize * (0.28 + rng() * 0.12);
            if (free(x, y, ts * 0.8)) place({ kind: 'rock', p: new Vector2(x, y), s: ts, hue: rng() });
        }
    }

    void key;
    return feats;
}

// ---------- tile drawing collection ----------
const ROOFS = ['#e07a5f', '#d9803f', '#c85a54', '#b7823f', '#8a9b6e', '#6c8ea4'];

function shadow(ctx: CanvasRenderingContext2D, x: number, y: number, s: number) {
    ctx.fillStyle = 'rgba(0,0,0,0.16)';
    ctx.beginPath(); ctx.ellipse(x + s * 0.12, y + s * 0.22, s * 0.62, s * 0.5, 0, 0, Math.PI * 2); ctx.fill();
}
function roundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number) {
    const rr = Math.min(r, Math.min(w, h) / 2);
    ctx.beginPath();
    ctx.moveTo(x + rr, y); ctx.arcTo(x + w, y, x + w, y + h, rr); ctx.arcTo(x + w, y + h, x, y + h, rr);
    ctx.arcTo(x, y + h, x, y, rr); ctx.arcTo(x, y, x + w, y, rr); ctx.closePath();
}

export function drawFeatures(ctx: CanvasRenderingContext2D, feats: Feature[]) {
    // parks & ponds first (ground), then trees, then buildings on top
    const order: FeatureKind[] = ['farm', 'park', 'pond', 'rock', 'cactus', 'tree', 'pine', 'snowpine', 'house', 'cottage', 'hall'];
    const byKind = new Map<FeatureKind, Feature[]>();
    for (const f of feats) { const a = byKind.get(f.kind); if (a) a.push(f); else byKind.set(f.kind, [f]); }
    for (const kind of order) {
        const list = byKind.get(kind); if (!list) continue;
        for (const f of list) drawOne(ctx, f);
    }
}

function drawOne(ctx: CanvasRenderingContext2D, f: Feature) {
    const { p: { x, y }, s } = f;
    switch (f.kind) {
        case 'farm': {
            const w = s * 1.9, h = s * 1.5;
            ctx.save();
            roundRect(ctx, x - w / 2, y - h / 2, w, h, s * 0.22); ctx.clip();
            ctx.translate(x, y); ctx.rotate((f.hue - 0.5) * 1.2);
            const stripe = s * 0.34;
            for (let i = -5; i <= 5; i++) { ctx.fillStyle = i % 2 === 0 ? '#a9bd63' : '#cbb884'; ctx.fillRect(i * stripe, -2 * s, stripe, 4 * s); }
            ctx.restore();
            roundRect(ctx, x - w / 2, y - h / 2, w, h, s * 0.22);
            ctx.strokeStyle = '#7d8a4f'; ctx.lineWidth = Math.max(1, s * 0.12); ctx.stroke();
            break;
        }
        case 'park': {
            ctx.fillStyle = '#8ec98a';
            ctx.beginPath(); ctx.ellipse(x, y, s, s * 0.85, 0, 0, Math.PI * 2); ctx.fill();
            for (let i = 0; i < 3; i++) { const tx = x + (f.hue * 2 - 1 + i * 0.4 - 0.4) * s * 0.5, ty = y + ((i % 2) - 0.5) * s * 0.4; treeTop(ctx, tx, ty, s * 0.3, '#3f7d4f', '#5fa05f'); }
            break;
        }
        case 'pond': {
            ctx.fillStyle = '#6db2d6'; ctx.beginPath(); ctx.ellipse(x, y, s, s * 0.72, f.hue * 0.6, 0, Math.PI * 2); ctx.fill();
            ctx.fillStyle = '#8fcbe6'; ctx.beginPath(); ctx.ellipse(x - s * 0.15, y - s * 0.12, s * 0.55, s * 0.38, f.hue * 0.6, 0, Math.PI * 2); ctx.fill();
            break;
        }
        case 'tree': {
            shadow(ctx, x, y, s * 0.8);
            treeTop(ctx, x, y, s, '#3f7d4f', '#63ab63');
            break;
        }
        case 'cactus': {
            shadow(ctx, x, y, s * 0.6);
            ctx.fillStyle = '#57a15f';
            roundRect(ctx, x - s * 0.16, y - s, s * 0.32, s * 1.7, s * 0.16); ctx.fill();
            roundRect(ctx, x - s * 0.5, y - s * 0.15, s * 0.34, s * 0.2, s * 0.09); ctx.fill();
            roundRect(ctx, x - s * 0.5, y - s * 0.55, s * 0.2, s * 0.5, s * 0.09); ctx.fill();
            roundRect(ctx, x + s * 0.16, y - s * 0.35, s * 0.34, s * 0.2, s * 0.09); ctx.fill();
            roundRect(ctx, x + s * 0.3, y - s * 0.85, s * 0.2, s * 0.55, s * 0.09); ctx.fill();
            break;
        }
        case 'rock': {
            shadow(ctx, x, y, s * 0.5);
            ctx.fillStyle = '#8b8b86'; ctx.beginPath(); ctx.ellipse(x, y, s * 0.72, s * 0.5, 0, 0, Math.PI * 2); ctx.fill();
            ctx.fillStyle = '#a3a39b'; ctx.beginPath(); ctx.ellipse(x - s * 0.2, y - s * 0.16, s * 0.4, s * 0.28, 0, 0, Math.PI * 2); ctx.fill();
            ctx.fillStyle = '#75756e'; ctx.beginPath(); ctx.ellipse(x + s * 0.34, y + s * 0.12, s * 0.32, s * 0.22, 0, 0, Math.PI * 2); ctx.fill();
            break;
        }
        case 'snowpine': {
            shadow(ctx, x, y, s * 0.7);
            ctx.fillStyle = '#4d6b57';
            ctx.beginPath(); ctx.moveTo(x, y - s); ctx.lineTo(x + s * 0.7, y + s * 0.6); ctx.lineTo(x - s * 0.7, y + s * 0.6); ctx.closePath(); ctx.fill();
            ctx.fillStyle = '#eaf3f6'; // snow load
            ctx.beginPath(); ctx.moveTo(x, y - s); ctx.lineTo(x + s * 0.36, y - s * 0.1); ctx.lineTo(x - s * 0.36, y - s * 0.1); ctx.closePath(); ctx.fill();
            ctx.beginPath(); ctx.ellipse(x, y - s, s * 0.14, s * 0.14, 0, 0, Math.PI * 2); ctx.fill();
            break;
        }
        case 'pine': {
            shadow(ctx, x, y, s * 0.7);
            ctx.fillStyle = '#356b45';
            ctx.beginPath(); ctx.moveTo(x, y - s); ctx.lineTo(x + s * 0.7, y + s * 0.6); ctx.lineTo(x - s * 0.7, y + s * 0.6); ctx.closePath(); ctx.fill();
            ctx.fillStyle = '#4c8a58';
            ctx.beginPath(); ctx.moveTo(x, y - s); ctx.lineTo(x + s * 0.4, y - s * 0.1); ctx.lineTo(x - s * 0.4, y - s * 0.1); ctx.closePath(); ctx.fill();
            break;
        }
        default: { // buildings
            const w = s * 1.1, h = s * 1.1;
            shadow(ctx, x, y, s);
            let wall = '#efe6d6';
            let roof = ROOFS[Math.floor(f.hue * ROOFS.length) % ROOFS.length];
            if (f.variant === 'adobe') { wall = '#e7d3ac'; roof = ['#c98a4c', '#b9825a', '#cf9a5f'][Math.floor(f.hue * 3) % 3]; }
            else if (f.variant === 'cabin') { wall = '#dfe7ee'; roof = '#6f5442'; }
            // wall
            ctx.fillStyle = wall; roundRect(ctx, x - w / 2, y - h / 2, w, h, s * 0.18); ctx.fill();
            // roof (top half)
            ctx.fillStyle = roof; roundRect(ctx, x - w / 2, y - h / 2, w, h * 0.62, s * 0.18); ctx.fill();
            if (f.variant === 'cabin') { // snow load on the roof
                ctx.fillStyle = '#f2f7fb'; roundRect(ctx, x - w / 2, y - h / 2, w, h * 0.26, s * 0.18); ctx.fill();
            }
            if (f.kind === 'hall') { ctx.fillStyle = roof; ctx.beginPath(); ctx.arc(x + w * 0.28, y - h * 0.28, s * 0.16, 0, Math.PI * 2); ctx.fill(); }
            // door
            ctx.fillStyle = 'rgba(0,0,0,0.25)'; roundRect(ctx, x - s * 0.12, y + h * 0.08, s * 0.24, h * 0.34, s * 0.06); ctx.fill();
        }
    }
}
function treeTop(ctx: CanvasRenderingContext2D, x: number, y: number, s: number, base: string, hi: string) {
    ctx.fillStyle = base; ctx.beginPath(); ctx.arc(x, y, s, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = hi; ctx.beginPath(); ctx.arc(x - s * 0.28, y - s * 0.28, s * 0.55, 0, Math.PI * 2); ctx.fill();
}
