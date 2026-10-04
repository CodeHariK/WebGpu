// Puzzle Islands — level sources beyond "generate this seed":
//  * TARGET DIFFICULTY: generate N neighbouring seeds, keep the one whose score is closest.
//  * PASTED LAYOUT: a hand-tuned ASCII level (copied out of the lab and edited), solved so it
//    can be played, replayed and scored like a generated one.
import type { GenResult } from './Map19Generator';
import { parseLevel } from './Map19Rules';
import { solveKeyAndExit } from './Map19Solver';
import { scoreLevel } from './Map19Difficulty';

export const TARGET_TRIES = 8; // seeds generated per TARGET SCORE pick

export type Picked<R extends GenResult> = { result: R; seed: number; score: number; tried: number; min: number; max: number }; // min / max: candidate score range

/** Try seeds base .. base+n-1 and keep the solvable one scoring closest to `target`. */
export function pickByTarget<R extends GenResult>(generate: (seed: number) => R, base: number, target: number, n: number): Picked<R> {
    let best: Picked<R> | null = null;
    let first: R | null = null;
    let min = Infinity;
    let max = -Infinity;
    for (let k = 0; k < n; k++) {
        const r = generate(base + k);
        first ??= r;
        if (!r.ok) continue;
        const score = scoreLevel(parseLevel(r.rows), r.rows, r.pushes, r.states, r.legs).total;
        min = Math.min(min, score);
        max = Math.max(max, score);
        if (!best || Math.abs(score - target) < Math.abs(best.score - target)) best = { result: r, seed: base + k, score, tried: n, min, max };
    }
    return best ? { ...best, min, max } : { result: first!, seed: base, score: 0, tried: n, min: 0, max: 0 };
}

/** Clean pasted text into level rows (trailing blanks off, ragged rows padded with walls). */
export function layoutRows(text: string): string[] {
    const rows = text.replace(/\r/g, '').split('\n').map((r) => r.replace(/\s+$/, '')).filter((r) => r.length);
    const w = Math.max(0, ...rows.map((r) => r.length));
    return rows.map((r) => r.padEnd(w, '#'));
}

/** A pasted layout as a GenResult: solved for the key → exit route, or ok=false with a reason. */
export function layoutResult(rows: string[]): GenResult & { error?: string } {
    const empty = { ok: false, rows, pushes: 0, attempts: 1, states: 0, legs: [], rejected: { layout: 0, unsolvable: 0, easy: 0 } };
    if (rows.length < 3) return { ...empty, error: 'too few rows' };
    const L = parseLevel(rows);
    if (L.start.player < 0) return { ...empty, error: 'no player (P)' };
    if (!L.door.length) return { ...empty, error: 'no exit door (E)' };
    const sol = solveKeyAndExit(L, 40000);
    if (!sol.solved) return { ...empty, rejected: { ...empty.rejected, unsolvable: 1 }, error: `not solvable (searched ${sol.states} states)` };
    return { ...empty, ok: true, pushes: sol.pushes, states: sol.states, legs: sol.legs ?? [] };
}
