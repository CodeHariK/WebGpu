// Colour Blocks — tidying a spread: spikes shaved, every block one piece, thin bits trimmed, tiny
// blocks dropped. Wall cells (owner −1) never change and are never handed to anyone.
import { DI, DJ, type Seed } from './Map26Grow';

// Shaves single-cell spikes (twice: each cell takes the colour 5 or more of its 8 neighbours share;
// seeds keep theirs), makes every block one piece (onePiece), and trims strands, tips and notches
// thinner than 5 cells (trimThin).
export function tidy(p_n: number, r_owner: Int32Array, p_seeds: Seed[], p_minBlock: number, p_trim: number): void {
    const isSeed = new Uint8Array(p_n * p_n);
    for (const s of p_seeds) isSeed[s.j * p_n + s.i] = 1;
    for (let pass = 0; pass < 2; pass++) {
        const before = Int32Array.from(r_owner);
        for (let j = 1; j < p_n - 1; j++) {
            for (let i = 1; i < p_n - 1; i++) {
                if (isSeed[j * p_n + i] || before[j * p_n + i] < 0) continue; // seeds and walls stay
                // the colour most neighbours have (a running vote: a colour with 5 of 8 always wins it)
                let leader = -1;
                let lead = 0;
                for (let m = 0; m < 8; m++) {
                    const k = before[(j + DJ[m]) * p_n + i + DI[m]];
                    if (lead === 0) {
                        leader = k;
                        lead = 1;
                    } else lead += k === leader ? 1 : -1;
                }
                if (leader < 0) continue;
                let count = 0;
                for (let m = 0; m < 8; m++) if (before[(j + DJ[m]) * p_n + i + DI[m]] === leader) count++;
                if (count >= 5) r_owner[j * p_n + i] = leader;
            }
        }
    }
    onePiece(p_n, r_owner, p_seeds, p_minBlock);
    trimThin(p_n, r_owner, isSeed, p_trim);
    onePiece(p_n, r_owner, p_seeds, p_minBlock); // trimming a strand can cut a block in two
}

// Thin bits (strands, tips and notches under 2 x TRIM + 1 cells wide) go to the colour beside them.
// An opening: a cell survives shrinking if every cell within TRIM of it (a square) shares its colour
// (the map's edge counts as the same); a cell is solid if a survivor of its colour is within TRIM.
// What isn't solid is thin.
function trimThin(p_n: number, r_owner: Int32Array, p_isSeed: Uint8Array, TRIM: number): void {
    const at = (i: number, j: number, p_k: number) => {
        if (i < 0 || j < 0 || i >= p_n || j >= p_n) return p_k; // the map's edge counts as the same colour
        const k = r_owner[j * p_n + i];
        return k < 0 ? p_k : k; // so does a wall
    };
    const core = new Uint8Array(p_n * p_n);
    for (let j = 0; j < p_n; j++) {
        for (let i = 0; i < p_n; i++) {
            const k = r_owner[j * p_n + i];
            let all = true;
            for (let dj = -TRIM; dj <= TRIM && all; dj++) for (let di = -TRIM; di <= TRIM && all; di++) all = at(i + di, j + dj, k) === k;
            core[j * p_n + i] = all ? 1 : 0;
        }
    }
    const before = Int32Array.from(r_owner);
    for (let j = 0; j < p_n; j++) {
        for (let i = 0; i < p_n; i++) {
            const c = j * p_n + i;
            const k = before[c];
            if (core[c] || p_isSeed[c] || k < 0) continue;
            let solid = false;
            for (let dj = -TRIM; dj <= TRIM && !solid; dj++) {
                for (let di = -TRIM; di <= TRIM && !solid; di++) {
                    const ni = i + di;
                    const nj = j + dj;
                    solid = ni >= 0 && nj >= 0 && ni < p_n && nj < p_n && core[nj * p_n + ni] === 1 && before[nj * p_n + ni] === k;
                }
            }
            if (solid) continue;
            const counts = new Map<number, number>();
            for (let m = 0; m < 8; m++) {
                const other = at(i + DI[m], j + DJ[m], k);
                if (other !== k && other >= 0) counts.set(other, (counts.get(other) ?? 0) + 1);
            }
            if (counts.size) r_owner[c] = [...counts.entries()].sort((a, b) => b[1] - a[1])[0][0];
        }
    }
}

// Every colour keeps only the pieces joined (side to side, not just at a corner) to its own seeds,
// and only those with MIN_BLOCK cells or more. Any other piece of it, whatever its size,
// goes to the colour it touches most. Repeated until
// nothing moves (a piece can be wrapped in another piece that is itself moving).
function onePiece(p_n: number, r_owner: Int32Array, p_seeds: Seed[], MIN_BLOCK: number): void {
    for (let round = 0; round < 50; round++) {
        const kept = new Uint8Array(p_n * p_n);
        for (let c = 0; c < p_n * p_n; c++) if (r_owner[c] < 0) kept[c] = 1; // walls stay walls
        p_seeds.forEach((s) => { // flood each colour from each of its seeds
            const k = s.colour;
            const start = s.j * p_n + s.i;
            if (r_owner[start] !== k || kept[start]) return;
            const queue = [start];
            kept[start] = 1;
            for (let q = 0; q < queue.length; q++) {
                const c = queue[q];
                const i = c % p_n;
                const j = (c - i) / p_n;
                for (let m = 0; m < 4; m++) { // the 4 sides
                    const ni = i + DI[m];
                    const nj = j + DJ[m];
                    if (ni < 0 || nj < 0 || ni >= p_n || nj >= p_n) continue;
                    const nc = nj * p_n + ni;
                    if (kept[nc] || r_owner[nc] !== k) continue;
                    kept[nc] = 1;
                    queue.push(nc);
                }
            }
            if (queue.length < MIN_BLOCK) for (const c of queue) kept[c] = 0; // too small to be a block: it goes too
        });
        let moved = false;
        for (let c = 0; c < p_n * p_n; c++) {
            if (kept[c]) continue;
            const i = c % p_n;
            const j = (c - i) / p_n;
            const counts = new Map<number, number>();
            for (let m = 0; m < 4; m++) { // the 4 sides; only kept neighbours count: they are where it belongs
                const ni = i + DI[m];
                const nj = j + DJ[m];
                if (ni < 0 || nj < 0 || ni >= p_n || nj >= p_n) continue;
                const nc = nj * p_n + ni;
                if (kept[nc] && r_owner[nc] >= 0) counts.set(r_owner[nc], (counts.get(r_owner[nc]) ?? 0) + 1);
            }
            if (counts.size === 0) continue; // inside the stray piece: its edge cells go first
            r_owner[c] = [...counts.entries()].sort((a, b) => b[1] - a[1])[0][0];
            kept[c] = 1;
            moved = true;
        }
        if (!moved) break;
    }
}
