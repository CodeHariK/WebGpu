// Colour Blocks — step 12: streets traced along the street field (Map26Field), instead of one
// straight grid per district.
//
// Two families of streets: along the field (parallel to the nearest road) and across it (at right
// angles to it). Each street is traced from a seed point both ways, a half fine cell a step (the
// direction read at the step's middle, kept facing the way it was going), until it leaves its
// district (reaches a road), comes closer than half its spacing to another street of its family,
// turns too sharply (where the field has no clear direction), or gets too long. Streets of one
// family are kept about their spacing apart (evenly spaced streamlines, Jobard and Lefer): seeds
// are tried at that distance either side of every street already traced, nearest first, and only
// where no street of the family is that close. The first seeds are next to the roads: the streets
// along start one spacing (streetBlock: a block's depth) in from each road, the streets across
// start at the roads themselves (T-junctions), blockLength apart. Then every dead end is carried on
// straight (up to a street block) to the road or street ahead of it, if there is one that near, so
// the streets close into blocks (zoning works on those). Each street is pulled straight a little and
// cut into pieces for the houses.
import { straighten, type Highway, type Point } from './Map26Borders';
import { DETAIL, type Districts } from './Map26Districts';
import { angleAt, type Field } from './Map26Field';
import type { Settings } from './Map26Settings';
import type { Street, Streets } from './Map26Streets';

const STEP = 0.5; // fine cells a tracing step
const MAX_STEPS = 600;
const MAX_TURN = Math.cos(Math.PI / 4); // a step turning more than 45 degrees ends the street
const MIN_LENGTH = 6; // fine cells: shorter streets (stubs) are dropped
const TOUCH = 0.7; // fine cells: a dead end carried on this near a street has reached it

// Points (fine cells) bucketed by area: which are near a point?
class Near {
    private buckets = new Map<number, number[]>();
    private size: number;
    constructor(p_size: number) {
        this.size = p_size;
    }
    private key(i: number, j: number) {
        return j * 100003 + i;
    }
    add(p: Point): void {
        const k = this.key(Math.floor(p.x / this.size), Math.floor(p.y / this.size));
        if (!this.buckets.has(k)) this.buckets.set(k, []);
        this.buckets.get(k)!.push(p.x, p.y);
    }
    // Any point closer than p_r (at most the bucket size)?
    any(p: Point, p_r: number): boolean {
        const bi = Math.floor(p.x / this.size);
        const bj = Math.floor(p.y / this.size);
        for (let j = bj - 1; j <= bj + 1; j++) {
            for (let i = bi - 1; i <= bi + 1; i++) {
                const b = this.buckets.get(this.key(i, j));
                if (!b) continue;
                for (let m = 0; m < b.length; m += 2) if ((b[m] - p.x) ** 2 + (b[m + 1] - p.y) ** 2 < p_r * p_r) return true;
            }
        }
        return false;
    }
}

type Family = { across: boolean; spacing: number; near: Near };

// The family's direction at p (a unit vector, either way round).
function direction(p_field: Field, p_f: Family, p: Point): Point {
    const a = angleAt(p_field, p.x, p.y) + (p_f.across ? Math.PI / 2 : 0);
    return { x: Math.cos(a), y: Math.sin(a) };
}

function districtAt(p_d: Districts, p: Point): number {
    const i = Math.floor(p.x);
    const j = Math.floor(p.y);
    return i >= 0 && j >= 0 && i < p_d.n && j < p_d.n ? p_d.owner[j * p_d.n + i] : -1;
}

// One street through p_seed, both ways, inside district p_k.
function trace(p_d: Districts, p_field: Field, p_f: Family, p_seed: Point, p_k: number): Point[] {
    const halves: Point[][] = [];
    for (const sign of [1, -1]) {
        const half: Point[] = [];
        let p = p_seed;
        let prev = direction(p_field, p_f, p);
        prev = { x: prev.x * sign, y: prev.y * sign };
        for (let n = 0; n < MAX_STEPS; n++) {
            const facing = (d: Point) => (d.x * prev.x + d.y * prev.y < 0 ? { x: -d.x, y: -d.y } : d);
            const d1 = facing(direction(p_field, p_f, p));
            const d2 = facing(direction(p_field, p_f, { x: p.x + (d1.x * STEP) / 2, y: p.y + (d1.y * STEP) / 2 }));
            if (d2.x * prev.x + d2.y * prev.y < MAX_TURN) break;
            const q = { x: p.x + d2.x * STEP, y: p.y + d2.y * STEP };
            if (districtAt(p_d, q) !== p_k) break; // a road (or another district)
            if (p_f.near.any(q, p_f.spacing / 2)) break;
            half.push(q);
            prev = d2;
            p = q;
        }
        halves.push(half);
    }
    return [...halves[1].reverse(), p_seed, ...halves[0]];
}

// The family's streets, seeded from p_roads (fine cells) and then from each other.
function traceFamily(p_d: Districts, p_field: Field, p_f: Family, p_roads: Point[][], p_lines: Point[][]): void {
    const queue: Point[] = [];
    const offset = p_f.across ? 1.5 : p_f.spacing; // across: right at the road; along: a block in
    for (const pts of p_roads) {
        let walked = 0;
        for (let k = 0; k < pts.length - 1; k++) {
            const a = pts[k];
            const b = pts[k + 1];
            const length = Math.hypot(b.x - a.x, b.y - a.y);
            for (let t = 0; t < length; t += 1) {
                walked += 1;
                const p = { x: a.x + ((b.x - a.x) * t) / length, y: a.y + ((b.y - a.y) * t) / length };
                if (!p_f.across) p_f.near.add(p); // the roads count as streets along: none closer than half a block
                if (walked % Math.max(1, Math.round(p_f.across ? p_f.spacing / 3 : 2)) !== 0) continue;
                const nx = -(b.y - a.y) / length;
                const ny = (b.x - a.x) / length;
                queue.push({ x: p.x + nx * offset, y: p.y + ny * offset }, { x: p.x - nx * offset, y: p.y - ny * offset });
            }
        }
    }
    for (let q = 0; q < queue.length; q++) {
        const seed = queue[q];
        const k = districtAt(p_d, seed);
        if (k < 0 || p_f.near.any(seed, p_f.spacing * 0.9)) continue;
        const line = trace(p_d, p_field, p_f, seed, k);
        if ((line.length - 1) * STEP < MIN_LENGTH) continue;
        for (const p of line) p_f.near.add(p);
        p_lines.push(line);
        for (let m = 0; m < line.length; m += 4) { // new seeds a spacing either side
            const d = direction(p_field, p_f, line[m]);
            queue.push({ x: line[m].x - d.y * p_f.spacing, y: line[m].y + d.x * p_f.spacing }, { x: line[m].x + d.y * p_f.spacing, y: line[m].y - d.x * p_f.spacing });
        }
    }
}

// Carries each end of each street on straight, up to p_reach, to the first road (its district ends)
// or street (TOUCH near) ahead; an end with nothing that near stays a dead end.
function closeEnds(p_d: Districts, r_lines: Point[][], p_reach: number): void {
    const all = new Near(2);
    for (const line of r_lines) for (const p of line) all.add(p);
    for (const line of r_lines) {
        const k = districtAt(p_d, line[Math.floor(line.length / 2)]);
        for (const atStart of [true, false]) {
            const end = atStart ? line[0] : line[line.length - 1];
            const back = atStart ? line[Math.min(2, line.length - 1)] : line[Math.max(0, line.length - 3)];
            const length = Math.hypot(end.x - back.x, end.y - back.y) || 1;
            const dx = (end.x - back.x) / length;
            const dy = (end.y - back.y) / length;
            let reached: Point | null = null;
            for (let t = STEP; t <= p_reach; t += STEP) {
                const q = { x: end.x + dx * t, y: end.y + dy * t };
                if (districtAt(p_d, q) !== k || (t > 1 && all.any(q, TOUCH))) {
                    reached = q;
                    break;
                }
            }
            if (!reached) continue;
            if (atStart) line.unshift(reached);
            else line.push(reached);
        }
    }
}

export function makeFieldStreets(p_d: Districts, p_highways: Highway[], p_field: Field, p_settings: Settings): Streets {
    const roads = [...p_d.roads.highways.map((h) => h.points), ...p_highways.map((h) => h.points.map((p) => ({ x: p.x * DETAIL, y: p.y * DETAIL })))];
    const lines: Point[][] = [];
    traceFamily(p_d, p_field, { across: false, spacing: p_settings.streetBlock, near: new Near(p_settings.streetBlock) }, roads, lines);
    traceFamily(p_d, p_field, { across: true, spacing: p_settings.blockLength, near: new Near(p_settings.blockLength) }, roads, lines);
    closeEnds(p_d, lines, p_settings.streetBlock * 1.2);
    const streets: Street[] = [];
    const kept: Point[][] = [];
    for (const line of lines) {
        const pts = straighten({ points: line, closed: false }, 0.3).points;
        kept.push(pts);
        for (let k = 0; k < pts.length - 1; k++) streets.push({ ax: pts[k].x, ay: pts[k].y, bx: pts[k + 1].x, by: pts[k + 1].y });
    }
    return { streets, lines: kept };
}
