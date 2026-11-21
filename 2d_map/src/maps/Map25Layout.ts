// Islands — where the 4 islands go, how big each is, and which pairs a bridge joins.
//
// Sizes: one big, two medium, one small. Positions: random, then pushed apart (and inside the
// map) a few dozen times, so they spread evenly but not on a grid. Bridges are chosen before any
// island has a shape: the shortest links that join them all (a minimum spanning tree), plus the
// shortest other link that crosses none of them, for one loop.

export type Island = { id: number; x: number; y: number; size: number };
export type Link = { a: number; b: number };
export type Layout = { islands: Island[]; links: Link[] };

const SIZES = [1, 0.78, 0.7, 0.52];

const gap = (p_a: Island, p_b: Island) => Math.hypot(p_a.x - p_b.x, p_a.y - p_b.y) - p_a.size - p_b.size;

function crosses(p_a: Island, p_b: Island, p_c: Island, p_d: Island): boolean {
    if (p_a === p_c || p_a === p_d || p_b === p_c || p_b === p_d) return false;
    const side = (p: Island, q: Island, r: Island) => Math.sign((q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x));
    return side(p_a, p_b, p_c) !== side(p_a, p_b, p_d) && side(p_c, p_d, p_a) !== side(p_c, p_d, p_b);
}

// Pushes overlapping islands apart and keeps each inside the map, p_rounds times.
function spread(p_islands: Island[], p_width: number, p_height: number, p_channel: number, p_rounds: number): void {
    for (let round = 0; round < p_rounds; round++) {
        for (const a of p_islands) {
            for (const b of p_islands) {
                if (a.id >= b.id) continue;
                const want = (a.size + b.size) * 1.15 + p_channel;
                const dx = b.x - a.x;
                const dy = b.y - a.y;
                const d = Math.hypot(dx, dy) || 1;
                if (d >= want) continue;
                const push = (want - d) * 0.5;
                a.x -= (dx / d) * push;
                a.y -= (dy / d) * push;
                b.x += (dx / d) * push;
                b.y += (dy / d) * push;
            }
        }
        for (const i of p_islands) {
            const margin = i.size * 1.05;
            i.x = Math.min(p_width - margin, Math.max(margin, i.x));
            i.y = Math.min(p_height - margin, Math.max(margin, i.y));
        }
    }
}

// A bridge between p_link's islands mustn't pass over a third island.
function clearOfOthers(p_islands: Island[], p_link: Link): boolean {
    const a = p_islands[p_link.a];
    const b = p_islands[p_link.b];
    return p_islands.every((c) => {
        if (c === a || c === b) return true;
        const dx = b.x - a.x;
        const dy = b.y - a.y;
        const t = Math.min(1, Math.max(0, ((c.x - a.x) * dx + (c.y - a.y) * dy) / (dx * dx + dy * dy)));
        return Math.hypot(c.x - (a.x + dx * t), c.y - (a.y + dy * t)) > c.size * 1.3;
    });
}

function chooseLinks(p_islands: Island[]): Link[] {
    const pairs: Link[] = [];
    for (const a of p_islands) for (const b of p_islands) if (a.id < b.id) pairs.push({ a: a.id, b: b.id });
    pairs.sort((p, q) => gap(p_islands[p.a], p_islands[p.b]) - gap(p_islands[q.a], p_islands[q.b]));
    const group = p_islands.map((i) => i.id);
    const find = (i: number): number => (group[i] === i ? i : (group[i] = find(group[i])));
    const links: Link[] = [];
    for (const p of pairs) {
        if (find(p.a) === find(p.b)) continue;
        group[find(p.a)] = find(p.b);
        links.push(p);
    }
    const extra = pairs.find((p) => !links.includes(p) && links.every((l) => !crosses(p_islands[p.a], p_islands[p.b], p_islands[l.a], p_islands[l.b])) && clearOfOthers(p_islands, p));
    if (extra) links.push(extra);
    return links;
}

export function makeLayout(p_size: number, p_channel: number, p_width: number, p_height: number, p_random: () => number): Layout {
    const order = [0, 1, 2, 3].sort(() => p_random() - 0.5);
    const islands: Island[] = order.map((k, id) => ({
        id,
        x: p_width * (0.3 + p_random() * 0.4),
        y: p_height * (0.3 + p_random() * 0.4),
        size: p_size * SIZES[k] * (0.92 + p_random() * 0.16),
    }));
    spread(islands, p_width, p_height, p_channel, 80);
    return { islands, links: chooseLinks(islands) };
}
