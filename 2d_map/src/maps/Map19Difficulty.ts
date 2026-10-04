// Puzzle Islands — difficulty score (roadmap step 4) and the budget that drives the generator.
//
//   score = Sokoban (pushes + log2 solver states)
//         + mechanics (locks, laser / plate gates, pulsing lasers, batteries, teleporters, keys)
//         + Σ enemy weight × pressure (full if its chase circle covers the solution route,
//           a little if it sits off-route — you can usually avoid it)
//
// The budget goes the other way: one DIFFICULTY number (1..10) is turned into generator
// settings (size, rooms, pushes, locks, mechanics, enemies), then the result is scored so
// you can see what you actually got.
import type { GenParams } from './Map19Generator';
import { type Level } from './Map19Rules';
import type { Push } from './Map19Solver';
import { dist, ENEMY_SPEC, parseEnemies, type EnemyKind } from './Map19Enemies';
import { applyToken, solutionSteps, startPlay } from './Map19Play';

export const ENEMY_WEIGHT: Record<EnemyKind, number> = { patrol: 1, red: 2, yellow: 4, blue: 5 };
const OFF_ROUTE = 0.3; // an enemy you never have to pass still adds a little pressure

export type Difficulty = {
    total: number;
    sokoban: number;
    mechanics: number;
    enemies: number;
    onRoute: number; // enemies whose chase circle covers the route
    enemyCount: number;
    tier: 'Easy' | 'Medium' | 'Hard' | 'Expert';
};

const tierOf = (t: number): Difficulty['tier'] => (t < 18 ? 'Easy' : t < 30 ? 'Medium' : t < 45 ? 'Hard' : 'Expert');

/** Cells the player walks through on the solver's solution (plus the final state). */
function routeOf(L: Level, legs: Push[][]) {
    let p = startPlay(L);
    const cells = new Set([p.s.player]);
    for (const d of solutionSteps(L, legs)) {
        p = applyToken(p, d);
        cells.add(p.s.player);
    }
    return { cells: [...cells] };
}

export function scoreLevel(L: Level, rows: string[], pushes: number, states: number, legs: Push[][]): Difficulty {
    const route = routeOf(L, legs);
    const sokoban = pushes + Math.log2(1 + states);
    const count = (ch: string) => L.base.filter((b) => b === ch).length;
    const mechanics = 2 * L.gates.size
        + 3 * count('H') + 2 * count('J') + L.emitters.filter((e) => e.pulse).length + L.batteries.length
        + 2 * L.padPairs.length + (L.golds.length - 1);
    let enemies = 0;
    let onRoute = 0;
    const list = parseEnemies(L, rows);
    for (const e of list) {
        const radius = Math.max(ENEMY_SPEC[e.kind].chase, ENEMY_SPEC[e.kind].reach, 2); // patroller: ~its beat
        const near = route.cells.some((c) => dist(L, c, e.cell) <= radius);
        if (near) onRoute++;
        enemies += ENEMY_WEIGHT[e.kind] * (near ? 1 : OFF_ROUTE);
    }
    const total = Math.round((sokoban + mechanics + enemies) * 10) / 10;
    return { total, sokoban, mechanics, enemies, onRoute, enemyCount: list.length, tier: tierOf(total) };
}

/** Generator settings for a difficulty budget 1..10 (seed kept from `base`). */
export function budgetParams(d: number, base: GenParams): GenParams {
    const k = Math.max(1, Math.min(10, Math.round(d)));
    const step = (from: number) => (k >= from ? 1 : 0);
    return {
        ...base,
        width: 15 + 2 * Math.ceil(k / 2),
        height: 11 + Math.ceil(k / 2),
        rooms: Math.min(7, 2 + Math.ceil(k / 2)),
        minPushes: 1 + Math.round(k * 0.6), // higher floors reject most layouts (slow)
        extraCrates: 1 + Math.floor(k / 3),
        locks: step(3) + step(7),
        bombs: 2 + step(7), // fewer bombs → the bomb → alien check rejects most layouts
        aliens: 2,
        patrollers: 1 + step(6),
        reds: Math.floor((k + 1) / 3),
        yellows: step(4) + step(8),
        blues: step(6) + step(9),
        laserGates: step(3),
        mirrorChains: step(5) + step(9),
        woodLasers: step(4) + step(8),
        goldKeys: 1 + step(2) + step(6),
        teleporters: step(3) + step(7),
        plateGates: step(2) + step(6),
        pulseLasers: Math.floor(k / 3),
        deadEmitterChance: k >= 4 ? 0.6 : 0,
        hearts: 1 + step(6),
        bombPickups: 2 + step(5),
        woodWalls: 1 + Math.floor(k / 3),
        woodDoors: step(2) + step(6),
    };
}
