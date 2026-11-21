// Colour Blocks — step 9: where on the island the city may grow, and its edge as a highway.
//
// A cell is city if its middle is on land and at least cityMargin pixels from the coast (the coast
// distance, Map26Island). Only the biggest piece of those cells is kept (side-to-side flood fills):
// headlands pinched off by the margin, and islets, stay countryside. Every other cell is outside:
// a wall the spread never enters, so no block, district, street or house reaches the beach.
//
// The city's edge gets a highway of its own (the ring road): for finding the borders only, the
// outside counts as one more colour, so the side between a block and the outside is a border like
// any other, and the highways between blocks meet it at junctions.
import { coastDistance, type Island } from './Map26Island';
import { NEIGHBOURS } from './Map26Grow';

const SIDES = NEIGHBOURS.slice(0, 4);
const OUTSIDE = 1 << 20; // the outside's colour, while finding the borders: no block has it

// Per cell of an n x n grid covering p_size pixels: 1 outside the city, 0 inside.
export function cityOutside(p_n: number, p_size: number, p_island: Island, p_margin: number): Uint8Array {
    const cell = p_size / p_n;
    const inside = new Uint8Array(p_n * p_n);
    for (let j = 0; j < p_n; j++) for (let i = 0; i < p_n; i++) inside[j * p_n + i] = coastDistance(p_island, (i + 0.5) * cell, (j + 0.5) * cell) <= -p_margin ? 1 : 0;
    const outside = new Uint8Array(p_n * p_n).fill(1);
    for (const c of biggestPiece(inside, p_n)) outside[c] = 0;
    return outside;
}

// The owners with the outside (−1) as one more colour: the borders found from them include the
// city's edge.
export function withEdge(p_owner: Int32Array): Int32Array {
    return p_owner.map((k) => (k < 0 ? OUTSIDE : k));
}

// The cells of the biggest side-to-side joined piece of p_inside.
function biggestPiece(p_inside: Uint8Array, p_n: number): number[] {
    const seen = new Uint8Array(p_n * p_n);
    let biggest: number[] = [];
    for (let start = 0; start < p_n * p_n; start++) {
        if (seen[start] || !p_inside[start]) continue;
        const cells = [start];
        seen[start] = 1;
        for (let q = 0; q < cells.length; q++) {
            const i = cells[q] % p_n;
            const j = (cells[q] - i) / p_n;
            for (const [di, dj] of SIDES) {
                const ni = i + di;
                const nj = j + dj;
                if (ni < 0 || nj < 0 || ni >= p_n || nj >= p_n) continue;
                const nc = nj * p_n + ni;
                if (seen[nc] || !p_inside[nc]) continue;
                seen[nc] = 1;
                cells.push(nc);
            }
        }
        if (cells.length > biggest.length) biggest = cells;
    }
    return biggest;
}
