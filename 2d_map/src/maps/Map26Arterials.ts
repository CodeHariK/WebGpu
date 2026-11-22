// Colour Blocks — step 10: arterials, highways that run on through the junctions.
//
// Every highway runs from junction to junction, so each one is short and the map reads as cells
// (stained glass). A real city has a few long roads that cross it. At every junction, the highway
// ends that meet there are paired up: the straightest pair first (their directions leaving the
// junction most nearly opposite), then the next straightest of those left, as long as a pair bends
// less than arterialBend degrees. Paired highways are one road: following the pairs from piece to
// piece gives chains. A chain of two or more pieces at least arterialLength cells long is an
// arterial (drawn wider). The ring road is left out (it stays a highway): arterials cross the city
// and end at it.
import type { Borders, Point } from './Map26Borders';
import type { Settings } from './Map26Settings';
import type { Spread } from './Map26Spread';

const LOOK = 3; // cells along a highway from its end: where its direction is measured

export type Arterials = { lines: Point[][]; pieces: Uint8Array }; // pieces: per highway, 1 if in an arterial

type End = { piece: number; end: number; dx: number; dy: number }; // end 0: its first point, 1: its last

// The direction a highway leaves its end p_end by: towards its point LOOK cells along.
function leaving(p_points: Point[], p_end: number): [number, number] {
    const pts = p_end === 0 ? p_points : [...p_points].reverse();
    let walked = 0;
    let k = 1;
    for (; k < pts.length - 1 && walked < LOOK; k++) walked += Math.hypot(pts[k].x - pts[k - 1].x, pts[k].y - pts[k - 1].y);
    const dx = pts[k].x - pts[0].x;
    const dy = pts[k].y - pts[0].y;
    const length = Math.hypot(dx, dy) || 1;
    return [dx / length, dy / length];
}

function lengthOf(p_points: Point[]): number {
    let sum = 0;
    for (let k = 1; k < p_points.length; k++) sum += Math.hypot(p_points[k].x - p_points[k - 1].x, p_points[k].y - p_points[k - 1].y);
    return sum;
}

// The point p_t (0..1) of the way along a line.
function pointAlong(p_points: Point[], p_t: number): Point {
    let left = lengthOf(p_points) * p_t;
    for (let k = 1; k < p_points.length; k++) {
        const a = p_points[k - 1];
        const b = p_points[k];
        const step = Math.hypot(b.x - a.x, b.y - a.y);
        if (left <= step && step > 0) return { x: a.x + ((b.x - a.x) * left) / step, y: a.y + ((b.y - a.y) * left) / step };
        left -= step;
    }
    return p_points[p_points.length - 1];
}

// Is the highway a piece of the ring road? Most of 9 points along its inside (not its ends: they
// can be junctions on the ring) have a cell outside the city (owner −1) among the 4 cells round
// them (the ring runs along the cells' edges, rounded a little).
export function onRing(p_points: Point[], p_spread: Spread): boolean {
    const n = p_spread.n;
    const outsideAt = (i: number, j: number) => i < 0 || j < 0 || i >= n || j >= n || p_spread.owner[j * n + i] < 0;
    let near = 0;
    for (let s = 1; s <= 9; s++) {
        const p = pointAlong(p_points, s / 10);
        const i = Math.floor(p.x - 0.5);
        const j = Math.floor(p.y - 0.5);
        if (outsideAt(i, j) || outsideAt(i + 1, j) || outsideAt(i, j + 1) || outsideAt(i + 1, j + 1)) near++;
    }
    return near >= 5;
}

// Per highway end (piece x 2 + end): the end it is paired with, or −1. p_skip: pieces left out.
function pairEnds(p_b: Borders, p_maxBend: number, p_skip: Uint8Array): Int32Array {
    const link = new Int32Array(p_b.highways.length * 2).fill(-1);
    const at = new Map<string, End[]>(); // the ends meeting at each corner
    p_b.highways.forEach((h, piece) => {
        if (h.closed || h.points.length < 2 || p_skip[piece]) return;
        for (const end of [0, 1]) {
            const p = end === 0 ? h.points[0] : h.points[h.points.length - 1];
            const [dx, dy] = leaving(h.points, end);
            const key = `${p.x},${p.y}`;
            if (!at.has(key)) at.set(key, []);
            at.get(key)!.push({ piece, end, dx, dy });
        }
    });
    const straight = -Math.cos((p_maxBend * Math.PI) / 180); // two directions this opposite or more
    for (const ends of at.values()) {
        const pairs: [number, End, End][] = [];
        for (let a = 0; a < ends.length; a++) {
            for (let b = a + 1; b < ends.length; b++) {
                if (ends[a].piece === ends[b].piece) continue; // a piece's own two ends (a loop)
                const dot = ends[a].dx * ends[b].dx + ends[a].dy * ends[b].dy; // −1: straight on
                if (dot <= straight) pairs.push([dot, ends[a], ends[b]]);
            }
        }
        pairs.sort((x, y) => x[0] - y[0]);
        for (const [, a, b] of pairs) {
            const ia = a.piece * 2 + a.end;
            const ib = b.piece * 2 + b.end;
            if (link[ia] >= 0 || link[ib] >= 0) continue;
            link[ia] = ib;
            link[ib] = ia;
        }
    }
    return link;
}

export function findArterials(p_b: Borders, p_spread: Spread, p_settings: Settings): Arterials {
    const ring = Uint8Array.from(p_b.highways, (h) => (onRing(h.points, p_spread) ? 1 : 0));
    const link = pairEnds(p_b, p_settings.arterialBend, ring);
    const seen = Uint8Array.from(ring); // the ring's pieces are never in a chain
    const pieces = new Uint8Array(p_b.highways.length);
    const lines: Point[][] = [];
    for (let first = 0; first < p_b.highways.length; first++) {
        if (seen[first] || p_b.highways[first].closed) continue;
        // back to the chain's start: out of each piece's other end until no pair (or round a loop)
        let piece = first;
        let end = 0; // the end the chain enters piece by
        for (let guard = 0; link[piece * 2 + end] >= 0 && guard < p_b.highways.length; guard++) {
            const other = link[piece * 2 + end];
            if (other >> 1 === first) break;
            piece = other >> 1;
            end = 1 - (other & 1);
        }
        // then forward, joining the pieces' points (the shared junction point once)
        const points: Point[] = [];
        const chain: number[] = [];
        for (;;) {
            seen[piece] = 1;
            chain.push(piece);
            const pts = p_b.highways[piece].points;
            const along = end === 0 ? pts : [...pts].reverse();
            points.push(...(points.length ? along.slice(1) : along));
            const next = link[piece * 2 + 1 - end];
            if (next < 0 || seen[next >> 1]) break;
            piece = next >> 1;
            end = next & 1;
        }
        if (chain.length < 2 || lengthOf(points) < p_settings.arterialLength) continue;
        lines.push(points);
        for (const c of chain) pieces[c] = 1;
    }
    return { lines, pieces };
}
