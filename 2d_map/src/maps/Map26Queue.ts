// Colour Blocks — the queue at a moment of the spread (for step 2's view). At clock p_until, every
// cell reached by then has burnt and queued its neighbours; the cells still waiting in the queue
// are the ones not reached yet that have a burnt neighbour (the frontier). Each waits for the
// earliest of the catch times its burnt neighbours offer (their time + step x its cost ÷ their
// colour's speed), in that neighbour's colour: exactly what the search's queue holds then (its out-
// of-date entries aside). The one with the earliest catch time is the next to burn.
import { NEIGHBOURS, type Spread } from './Map26Spread';

export type Waiting = { cell: number; colour: number; at: number };
export type QueueView = { waiting: Waiting[]; next: Waiting | null };

export function queueAt(p_s: Spread, p_until: number): QueueView {
    const n = p_s.n;
    const waiting: Waiting[] = [];
    let next: Waiting | null = null;
    for (let j = 0; j < n; j++) {
        for (let i = 0; i < n; i++) {
            const c = j * n + i;
            if (p_s.time[c] <= p_until) continue; // burnt already
            let best: Waiting | null = null;
            for (const [di, dj, step] of NEIGHBOURS) {
                const ni = i + di;
                const nj = j + dj;
                if (ni < 0 || nj < 0 || ni >= n || nj >= n) continue;
                const nc = nj * n + ni;
                if (p_s.time[nc] > p_until) continue; // not burning yet: offers nothing
                const colour = p_s.reached[nc];
                const at = p_s.time[nc] + (step * p_s.cost[c]) / p_s.speed[colour];
                if (!best || at < best.at) best = { cell: c, colour, at };
            }
            if (!best) continue;
            waiting.push(best);
            if (!next || best.at < next.at) next = best;
        }
    }
    return { waiting, next };
}
