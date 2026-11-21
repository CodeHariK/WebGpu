// Street Map — drawn on top of the painted map: the boulevards and the freeway (double lines from
// step 6), trees, names along the boulevards and on the districts, a few pins.
import { makeRandom } from './Map24Random';
import type { Road } from './Map24Roads';
import { LAKE_PARK, locate, type Here, type World } from './Map24Streets';
import { heightAt, nearestLakeEdge } from './Map24Terrain';

const INK = '#5d5349';
const HAND = '"Chalkboard SE", "Comic Sans MS", "Marker Felt", cursive';
const DISTRICTS = ['Willow Heights', 'Pebble Park', 'Old Town', 'Lantern Hill', 'Mossy Vale', 'Sunny Side', 'Clover Flats', 'Harbor View', 'Teacup Row', 'Bramble End', 'Fernwood', 'Little Pier'];
const SPOTS = ['Moss Café', 'Bun House', 'Lilypad Tea', 'Pebble Coffee', 'Sprout Bakery', 'Cocoa Corner'];

function path(p_ctx: CanvasRenderingContext2D, p_road: Road): void {
    p_ctx.beginPath();
    p_road.points.forEach((p, i) => (i ? p_ctx.lineTo(p.x, p.y) : p_ctx.moveTo(p.x, p.y)));
}

function stroke(p_ctx: CanvasRenderingContext2D, p_road: Road, p_colour: string, p_width: number): void {
    path(p_ctx, p_road);
    p_ctx.strokeStyle = p_colour;
    p_ctx.lineWidth = p_width;
    p_ctx.lineCap = 'round';
    p_ctx.lineJoin = 'round';
    p_ctx.stroke();
}

// Steps 2 - 5: plain lines. Step 6: casing, then the paper-coloured middle: a double line.
export function drawRoads(p_ctx: CanvasRenderingContext2D, p_roads: Road[], p_styled: boolean): void {
    if (!p_styled) {
        for (const r of p_roads) stroke(p_ctx, r, INK, r.kind === 'freeway' ? 4 : 2.6);
        return;
    }
    const order = [...p_roads].sort((a, b) => Number(a.kind === 'freeway') - Number(b.kind === 'freeway')); // the freeway on top
    for (const r of order) stroke(p_ctx, r, INK, r.kind === 'freeway' ? 11 : 6.5);
    for (const r of order) stroke(p_ctx, r, r.kind === 'freeway' ? '#dcd5c8' : '#fdf9ef', r.kind === 'freeway' ? 8 : 4);
    for (const r of order) if (r.kind === 'freeway') stroke(p_ctx, r, '#b9b0a2', 1);
}

function tree(p_ctx: CanvasRenderingContext2D, p_x: number, p_y: number, p_r: number): void {
    p_ctx.beginPath();
    p_ctx.ellipse(p_x + 0.8, p_y + p_r * 0.8, p_r * 0.9, p_r * 0.35, 0, 0, Math.PI * 2);
    p_ctx.fillStyle = 'rgba(90, 110, 70, 0.18)';
    p_ctx.fill();
    p_ctx.beginPath();
    p_ctx.arc(p_x, p_y, p_r, 0, Math.PI * 2);
    p_ctx.fillStyle = '#9fc27c';
    p_ctx.fill();
    p_ctx.strokeStyle = '#5f8448';
    p_ctx.lineWidth = 0.9;
    p_ctx.stroke();
}

// Trees on the hills (in clumps) and in the parks round the lakes, back to front.
export function drawTrees(p_ctx: CanvasRenderingContext2D, p_w: World): void {
    const t = p_w.terrain;
    const random = makeRandom(p_w.settings.seed * 31 + 7);
    const trees: { x: number; y: number; r: number }[] = [];
    for (let n = 0; n < 9000 && trees.length < 900; n++) {
        const x = random() * t.width;
        const y = random() * t.height;
        const onHill = heightAt(t, x, y) > t.hillLevel + 0.008;
        const lake = t.lakes.length ? nearestLakeEdge(t, x, y) : Infinity;
        const inPark = lake > 3 && lake < LAKE_PARK - 3;
        const clump = t.noise(x / 28 + 40, y / 28) > 0.1;
        if ((onHill && clump && random() < 0.5) || (inPark && random() < 0.35)) trees.push({ x, y, r: 2.4 + random() * 1.6 });
    }
    trees.sort((a, b) => a.y - b.y);
    for (const p of trees) tree(p_ctx, p.x, p.y, p.r);
}

function text(p_ctx: CanvasRenderingContext2D, p_text: string, p_x: number, p_y: number, p_angle: number, p_font: string): void {
    p_ctx.save();
    p_ctx.translate(p_x, p_y);
    p_ctx.rotate(p_angle);
    p_ctx.font = p_font;
    p_ctx.textAlign = 'center';
    p_ctx.textBaseline = 'middle';
    p_ctx.lineJoin = 'round';
    p_ctx.lineWidth = 4;
    p_ctx.strokeStyle = 'rgba(244, 236, 216, 0.95)';
    p_ctx.strokeText(p_text, 0, 0);
    p_ctx.fillStyle = '#3f372f';
    p_ctx.fillText(p_text, 0, 0);
    p_ctx.restore();
}

// Each boulevard's name, beside it, a little way along, turned with it (never upside down).
export function drawRoadNames(p_ctx: CanvasRenderingContext2D, p_roads: Road[], p_w: World): void {
    for (const r of p_roads) {
        const pts = r.points.filter((p) => p.x > 40 && p.y > 40 && p.x < p_w.terrain.width - 40 && p.y < p_w.terrain.height - 40);
        if (pts.length < 40) continue;
        const i = Math.floor(pts.length * (0.3 + ((r.id * 0.17) % 0.4)));
        const a = pts[Math.max(0, i - 6)];
        const b = pts[Math.min(pts.length - 1, i + 6)];
        let angle = Math.atan2(b.y - a.y, b.x - a.x);
        if (angle > Math.PI / 2) angle -= Math.PI;
        if (angle < -Math.PI / 2) angle += Math.PI;
        const off = r.kind === 'freeway' ? 12 : 9;
        text(p_ctx, r.name, pts[i].x - Math.sin(angle) * off, pts[i].y + Math.cos(angle) * off, angle, `600 11px ${HAND}`);
    }
}

// Names on the biggest districts (not on hills or water), spread apart.
export function drawDistrictNames(p_ctx: CanvasRenderingContext2D, p_w: World): void {
    const t = p_w.terrain;
    const here: Here = { zone: -1, out: 0, along: 0, spread: 1, point: 0 };
    const sums = new Map<number, { x: number; y: number; n: number }>();
    for (let y = 20; y < t.height - 20; y += 8) {
        for (let x = 20; x < t.width - 20; x += 8) {
            if (heightAt(t, x, y) > t.hillLevel || (t.lakes.length && nearestLakeEdge(t, x, y) < 30)) continue;
            locate(p_w, x, y, here);
            if (here.zone < 0 || here.out < 30) continue;
            const s = sums.get(here.zone) ?? { x: 0, y: 0, n: 0 };
            s.x += x;
            s.y += y;
            s.n++;
            sums.set(here.zone, s);
        }
    }
    const placed: { x: number; y: number }[] = [];
    const big = [...sums.values()].sort((a, b) => b.n - a.n);
    const first = p_w.settings.seed % DISTRICTS.length; // each map its own names, none twice
    for (const s of big) {
        if (placed.length >= 6 || s.n < 60) break;
        const x = Math.min(t.width - 80, Math.max(80, s.x / s.n)); // keep the name on the map
        const y = Math.min(t.height - 20, Math.max(20, s.y / s.n));
        if (placed.some((p) => Math.hypot(p.x - x, p.y - y) < 150)) continue;
        text(p_ctx, DISTRICTS[(first + placed.length) % DISTRICTS.length].toUpperCase(), x, y, 0, `700 14px ${HAND}`);
        placed.push({ x, y });
    }
}

function pin(p_ctx: CanvasRenderingContext2D, p_x: number, p_y: number, p_label: string): void {
    p_ctx.save();
    p_ctx.translate(p_x, p_y);
    p_ctx.beginPath();
    p_ctx.moveTo(0, 0);
    p_ctx.bezierCurveTo(-10, -12, -10, -26, 0, -26);
    p_ctx.bezierCurveTo(10, -26, 10, -12, 0, 0);
    p_ctx.fillStyle = '#c8372d';
    p_ctx.fill();
    p_ctx.strokeStyle = '#6e1a14';
    p_ctx.lineWidth = 1.2;
    p_ctx.stroke();
    p_ctx.beginPath();
    p_ctx.arc(0, -17, 5, 0, Math.PI * 2);
    p_ctx.fillStyle = 'white';
    p_ctx.fill();
    p_ctx.font = `700 11px ${HAND}`;
    const w = p_ctx.measureText(p_label).width + 14;
    p_ctx.beginPath();
    p_ctx.roundRect(12, -27, w, 20, 6);
    p_ctx.fillStyle = 'white';
    p_ctx.fill();
    p_ctx.strokeStyle = '#3f372f';
    p_ctx.stroke();
    p_ctx.fillStyle = '#3f372f';
    p_ctx.textBaseline = 'middle';
    p_ctx.fillText(p_label.toUpperCase(), 19, -17);
    p_ctx.restore();
}

// A few pins near the boulevards, apart from each other.
export function drawPins(p_ctx: CanvasRenderingContext2D, p_roads: Road[], p_w: World): void {
    const random = makeRandom(p_w.settings.seed * 17 + 3);
    const t = p_w.terrain;
    const placed: { x: number; y: number }[] = [];
    for (let n = 0; n < 200 && placed.length < 4; n++) {
        const r = p_roads[Math.floor(random() * p_roads.length)];
        const p = r.points[Math.floor(random() * r.points.length)];
        const x = p.x + (random() - 0.5) * 30;
        const y = p.y + (random() - 0.5) * 30;
        if (x < 30 || y < 40 || x > t.width - 140 || y > t.height - 20) continue;
        if (heightAt(t, x, y) > t.hillLevel || (t.lakes.length && nearestLakeEdge(t, x, y) < 5)) continue;
        if (placed.some((q) => Math.hypot(q.x - x, q.y - y) < 160)) continue;
        placed.push({ x, y });
        pin(p_ctx, x, y, SPOTS[placed.length - 1]);
    }
}
