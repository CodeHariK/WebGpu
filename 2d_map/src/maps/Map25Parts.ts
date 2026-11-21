// Islands — the parts each island is made of: its hill (noise plus a falloff round its middle), a
// headland out to each of its bridges, and where those bridges land.
import type { Island, Layout } from './Map25Layout';
import type { Settings } from './Map25Settings';

export type Hill = { x: number; y: number; size: number; salt: number }; // salt: where it reads the noise
export type Headland = { ax: number; ay: number; ar: number; bx: number; by: number; br: number }; // a fat line, radius ar to br
export type Tip = { link: number; x: number; y: number; r: number }; // where a bridge lands
export type Parts = { island: number; hill: Hill; headlands: Headland[]; tips: Tip[] };

// How far from island a's middle, toward b, their territories meet (bigger islands get more room).
export function borderAlong(p_a: Island, p_b: Island): number {
    const d = Math.hypot(p_b.x - p_a.x, p_b.y - p_a.y);
    return (d * d + p_a.size * p_a.size - p_b.size * p_b.size) / (2 * d);
}

// A headland from the middle to each bridge's tip, which stops half a channel short of the
// territory border (so the two tips of a bridge face each other across the channel).
export function makeParts(p_island: Island, p_layout: Layout, p_settings: Settings, p_random: () => number): Parts {
    const s = p_island.size;
    const headlands: Headland[] = [];
    const tips: Tip[] = [];
    p_layout.links.forEach((link, index) => {
        if (link.a !== p_island.id && link.b !== p_island.id) return;
        const other = p_layout.islands[link.a === p_island.id ? link.b : link.a];
        const heading = Math.atan2(other.y - p_island.y, other.x - p_island.x);
        const tipR = Math.max(10, s * 0.14);
        const reach = borderAlong(p_island, other) - p_settings.channel / 2 - tipR;
        const tip = { x: p_island.x + Math.cos(heading) * reach, y: p_island.y + Math.sin(heading) * reach };
        headlands.push({ ax: p_island.x, ay: p_island.y, ar: s * 0.25, bx: tip.x, by: tip.y, br: tipR });
        tips.push({ link: index, x: tip.x, y: tip.y, r: tipR });
    });
    const hill: Hill = { x: p_island.x, y: p_island.y, size: s * 1.05, salt: p_random() * 1000 };
    return { island: p_island.id, hill, headlands, tips };
}
