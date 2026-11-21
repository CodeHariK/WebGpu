// Colour Blocks — the borders between blocks, as lines (the highways).
//
// A border is made of grid edges: the side between two cells of different colours. Their corner
// points (the grid's lattice) are joined by those edges into a network. A corner with 1, 3 or 4
// border edges is an end: the map's edge, or a junction where blocks meet; except a corner with 4
// edges but only 2 colours round it (a checkerboard pinch), which is two borders passing (the
// edges are paired off and walked straight through). Walking from each end along corners that
// have exactly 2 edges (or pass through) gives one line, ending at the next end. Whatever is left
// over is closed loops (a block wrapped inside another). Each line is then rounded by corner
// cutting (Chaikin), keeping its two ends where they are, so lines still meet at the junctions.
// Before rounding, each line is pulled straight (straighten): the grid's stairs are mostly gone.

export type Point = { x: number; y: number };
export type Highway = { points: Point[]; closed: boolean };
export type Borders = { highways: Highway[]; junctions: Point[] };

export function findBorders(p_n: number, p_owner: Int32Array, p_straighten: number, p_smoothing: number): Borders {
    const w = p_n + 1; // corners along each side
    const links: number[][] = Array.from({ length: w * w }, () => []);
    const join = (a: number, b: number) => {
        links[a].push(b);
        links[b].push(a);
    };
    for (let j = 0; j < p_n; j++) {
        for (let i = 0; i < p_n; i++) {
            const k = p_owner[j * p_n + i];
            if (i + 1 < p_n && p_owner[j * p_n + i + 1] !== k) join(j * w + i + 1, (j + 1) * w + i + 1); // the side to the right
            if (j + 1 < p_n && p_owner[(j + 1) * p_n + i] !== k) join((j + 1) * w + i, (j + 1) * w + i + 1); // the side below
        }
    }
    // a pinch: 4 edges, 2 colours round the corner (cells up-left, up-right, down-left, down-right)
    const pinch = new Uint8Array(w * w);
    for (let v = 0; v < w * w; v++) {
        if (links[v].length !== 4) continue;
        const x = v % w;
        const y = (v - x) / w;
        const ul = p_owner[(y - 1) * p_n + x - 1];
        const ur = p_owner[(y - 1) * p_n + x];
        const dl = p_owner[y * p_n + x - 1];
        const dr = p_owner[y * p_n + x];
        if (ul === dr && ur === dl) pinch[v] = 1;
    }
    // through a pinch the edges pair up: up with left, down with right
    const through = (p_v: number, p_from: number): number => {
        const up = p_v - w;
        const down = p_v + w;
        const left = p_v - 1;
        const right = p_v + 1;
        return p_from === up ? left : p_from === left ? up : p_from === down ? right : down;
    };
    const used = new Set<number>();
    const edgeKey = (a: number, b: number) => (a < b ? a * w * w + b : b * w * w + a);
    const corner = (v: number): Point => ({ x: v % w, y: Math.floor(v / w) });
    const passes = (v: number) => links[v].length === 2 || pinch[v] === 1;
    const walk = (p_start: number, p_next: number): number[] => {
        const path = [p_start];
        let prev = p_start;
        let cur = p_next;
        used.add(edgeKey(prev, cur));
        while (passes(cur) && cur !== p_start) {
            path.push(cur);
            const next = pinch[cur] ? through(cur, prev) : links[cur][0] === prev ? links[cur][1] : links[cur][0];
            prev = cur;
            cur = next;
            used.add(edgeKey(prev, cur));
        }
        path.push(cur);
        return path;
    };
    const highways: Highway[] = [];
    const junctions: Point[] = [];
    for (let v = 0; v < w * w; v++) {
        const degree = links[v].length;
        if (degree === 0 || passes(v)) continue;
        if (degree >= 3) junctions.push(corner(v));
        for (const u of links[v]) {
            if (used.has(edgeKey(v, u))) continue;
            highways.push({ points: walk(v, u).map(corner), closed: false });
        }
    }
    for (let v = 0; v < w * w; v++) { // loops: no ends to start from
        for (const u of links[v]) {
            if (used.has(edgeKey(v, u))) continue;
            highways.push({ points: walk(v, u).map(corner), closed: true });
        }
    }
    return { highways: highways.map((h) => smooth(straighten(h, p_straighten), p_smoothing)), junctions };
}

// Chaikin corner cutting, p_passes times: every corner replaced by two points a quarter of the way
// along its two sides. An open line keeps its first and last points.
function smooth(p_h: Highway, p_passes: number): Highway {
    let pts = p_h.points;
    for (let pass = 0; pass < p_passes && pts.length > 2; pass++) {
        const out: Point[] = p_h.closed ? [] : [pts[0]];
        const count = p_h.closed ? pts.length - 1 : pts.length - 1; // a loop repeats its first point at the end
        for (let i = 0; i < count; i++) {
            const a = pts[i];
            const b = pts[i + 1];
            out.push({ x: a.x * 0.75 + b.x * 0.25, y: a.y * 0.75 + b.y * 0.25 });
            out.push({ x: a.x * 0.25 + b.x * 0.75, y: a.y * 0.25 + b.y * 0.75 });
        }
        if (p_h.closed) out.push(out[0]);
        else out.push(pts[pts.length - 1]);
        pts = out;
    }
    return { points: pts, closed: p_h.closed };
}

// Ramer–Douglas–Peucker: keep the point farthest from the line between the ends if it is more than
// p_tolerance off it (and do the same for both halves), else drop everything between the ends.
function straighten(p_h: Highway, p_tolerance: number): Highway {
    const pts = p_h.points;
    if (pts.length < 3 || p_tolerance <= 0) return p_h;
    const keep = new Uint8Array(pts.length);
    keep[0] = 1;
    keep[pts.length - 1] = 1;
    if (p_h.closed) keep[Math.floor(pts.length / 2)] = 1; // a loop starts and ends on the same point: split it in two
    const ranges: [number, number][] = [];
    for (let a = 0, b = 1; b < pts.length; b++) {
        if (keep[b]) {
            ranges.push([a, b]);
            a = b;
        }
    }
    while (ranges.length) {
        const [a, b] = ranges.pop()!;
        const ax = pts[a].x;
        const ay = pts[a].y;
        const dx = pts[b].x - ax;
        const dy = pts[b].y - ay;
        const length = Math.hypot(dx, dy) || 1;
        let far = -1;
        let farD = p_tolerance;
        for (let i = a + 1; i < b; i++) {
            const d = Math.abs((pts[i].x - ax) * dy - (pts[i].y - ay) * dx) / length;
            if (d > farD) {
                farD = d;
                far = i;
            }
        }
        if (far < 0) continue;
        keep[far] = 1;
        ranges.push([a, far], [far, b]);
    }
    return { points: pts.filter((_, i) => keep[i]), closed: p_h.closed };
}
