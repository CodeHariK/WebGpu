// Street Map — the boulevards (and the freeway): long, smooth roads made by a walker that heads for
// a point across the map, wobbles gently and steers round hills and lakes it sees ahead.
import type { Settings } from './Map24Settings';
import { heightAt, nearestLakeEdge, type Terrain } from './Map24Terrain';

export type Point = { x: number; y: number };

export type Road = {
    id: number;
    kind: 'boulevard' | 'freeway';
    name: string;
    points: Point[];
};

type Walk = {
    wobble: number; // radians the heading drifts either way
    turn: number; // most it turns a step (radians)
};

const STEP = 5; // pixels a walker step
const LOOK = 38; // how far ahead it looks for hills and lakes
const OUTSIDE = 90; // roads start and end this far outside the map
const BOULEVARD: Walk = { wobble: 0.45, turn: 0.05 };
const FREEWAY: Walk = { wobble: 0.15, turn: 0.025 };
const NAMES = ['Sunrise', 'Maple', 'Willow', 'Lantern', 'Pebble', 'Juniper', 'Harbor', 'Cedar', 'Marigold', 'Poppy', 'Clover', 'Bramble', 'Hazel', 'Fern'];
const KINDS = [' Boulevard', ' Avenue', ' Drive'];

const angleDiff = (p_a: number, p_b: number) => Math.atan2(Math.sin(p_a - p_b), Math.cos(p_a - p_b));
const clamp = (p_v: number, p_lo: number, p_hi: number) => Math.min(p_hi, Math.max(p_lo, p_v));

// How much the walker wants to avoid p: hills (more the higher) and lakes (with a margin).
function cost(p_t: Terrain, p_x: number, p_y: number): number {
    let c = clamp((heightAt(p_t, p_x, p_y) - p_t.hillLevel) * 12, 0, 1);
    if (nearestLakeEdge(p_t, p_x, p_y) < 22) c += 1.5;
    return c;
}

function inside(p_t: Terrain, p_x: number, p_y: number, p_margin: number): boolean {
    return p_x > -p_margin && p_y > -p_margin && p_x < p_t.width + p_margin && p_y < p_t.height + p_margin;
}

// Walks from p_start toward p_target until it leaves the map (or gives up).
function walk(p_t: Terrain, p_start: Point, p_target: Point, p_walk: Walk, p_salt: number): Point[] {
    const points: Point[] = [];
    let x = p_start.x;
    let y = p_start.y;
    let heading = Math.atan2(p_target.y - y, p_target.x - x);
    for (let n = 0; n < 900; n++) {
        points.push({ x, y });
        if (n > 8 && !inside(p_t, x, y, OUTSIDE)) break; // well past the edge: no road ends on the map
        const toward = Math.atan2(p_target.y - y, p_target.x - x);
        const wobble = p_t.noise(n * 0.012, p_salt) * p_walk.wobble;
        let turn = clamp(angleDiff(toward + wobble, heading) * 0.06, -p_walk.turn, p_walk.turn);
        const ahead = cost(p_t, x + Math.cos(heading) * LOOK, y + Math.sin(heading) * LOOK);
        if (ahead > 0) {
            const left = cost(p_t, x + Math.cos(heading - 0.5) * LOOK, y + Math.sin(heading - 0.5) * LOOK);
            const right = cost(p_t, x + Math.cos(heading + 0.5) * LOOK, y + Math.sin(heading + 0.5) * LOOK);
            turn += (left < right ? -1 : 1) * 0.09 * Math.min(ahead, 1.5);
        }
        heading += turn;
        x += Math.cos(heading) * STEP;
        y += Math.sin(heading) * STEP;
    }
    return points;
}

// Corner cutting (Chaikin), twice: rounds what is left of any kink.
function smooth(p_points: Point[]): Point[] {
    let pts = p_points;
    for (let pass = 0; pass < 2; pass++) {
        const out: Point[] = [pts[0]];
        for (let i = 0; i < pts.length - 1; i++) {
            const a = pts[i];
            const b = pts[i + 1];
            out.push({ x: a.x * 0.75 + b.x * 0.25, y: a.y * 0.75 + b.y * 0.25 });
            out.push({ x: a.x * 0.25 + b.x * 0.75, y: a.y * 0.25 + b.y * 0.75 });
        }
        out.push(pts[pts.length - 1]);
        pts = out;
    }
    return pts;
}

// A point on side p_side (0 top, 1 right, 2 bottom, 3 left) of the map, p_along (0..1) of the way.
function onEdge(p_t: Terrain, p_side: number, p_along: number, p_out: number): Point {
    const w = p_t.width;
    const h = p_t.height;
    if (p_side === 0) return { x: w * p_along, y: -p_out };
    if (p_side === 1) return { x: w + p_out, y: h * p_along };
    if (p_side === 2) return { x: w * p_along, y: h + p_out };
    return { x: -p_out, y: h * p_along };
}

// A road across the map: from one side to (roughly) the opposite one.
function crossing(p_t: Terrain, p_random: () => number, p_index: number): [Point, Point] {
    const side = (p_index + Math.floor(p_random() * 2) * 2) % 4; // alternate the directions
    const start = onEdge(p_t, side, 0.12 + p_random() * 0.76, OUTSIDE);
    const end = onEdge(p_t, (side + 2) % 4, 0.05 + p_random() * 0.9, 200);
    return [start, end];
}

// A road branching off p_from at a random point, heading out to the side.
function branch(p_t: Terrain, p_from: Road, p_random: () => number): [Point, Point] | null {
    const pts = p_from.points.filter((p) => inside(p_t, p.x, p.y, -60));
    if (pts.length < 20) return null;
    const i = 5 + Math.floor(p_random() * (pts.length - 10));
    const a = pts[i - 2];
    const b = pts[i + 2];
    const along = Math.atan2(b.y - a.y, b.x - a.x);
    const out = along + (p_random() < 0.5 ? 1 : -1) * (Math.PI / 2 + (p_random() - 0.5) * 0.6);
    return [pts[i], { x: pts[i].x + Math.cos(out) * 1500, y: pts[i].y + Math.sin(out) * 1500 }];
}

export function makeRoads(p_settings: Settings, p_t: Terrain, p_random: () => number): Road[] {
    const roads: Road[] = [];
    const first = Math.floor(p_random() * NAMES.length); // each boulevard its own name
    const name = () => NAMES[(first + roads.length) % NAMES.length] + KINDS[Math.floor(p_random() * KINDS.length)];
    if (p_settings.freeway) {
        const corner = Math.floor(p_random() * 4);
        const start = onEdge(p_t, corner, 0.15 + p_random() * 0.2, OUTSIDE);
        const end = onEdge(p_t, (corner + 2) % 4, 0.65 + p_random() * 0.2, 200);
        roads.push({ id: 0, kind: 'freeway', name: `Route ${1 + Math.floor(p_random() * 99)}`, points: smooth(walk(p_t, start, end, FREEWAY, 7.7)) });
    }
    for (let n = 0; n < p_settings.boulevards; n++) {
        const boulevards = roads.filter((r) => r.kind === 'boulevard');
        let ends: [Point, Point] | null = null;
        if (boulevards.length >= 2 && p_random() < 0.45) ends = branch(p_t, boulevards[Math.floor(p_random() * boulevards.length)], p_random);
        if (!ends) ends = crossing(p_t, p_random, n);
        const points = smooth(walk(p_t, ends[0], ends[1], BOULEVARD, 13.1 + n * 9.3));
        roads.push({ id: roads.length, kind: 'boulevard', name: name(), points });
    }
    return roads;
}
