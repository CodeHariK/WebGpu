// Puzzle Islands — player bombs (Bomberman style) and their blasts. Real-time play only:
// the solver never uses bombs, so blowing up a crate can make a level easier — or stuck.
//
//   time    dropped on your cell, explodes after FUSE seconds
//   throw   lobbed up to THROW_RANGE cells ahead (stops at walls / crates, lands ON an enemy
//           in the way), explodes on landing
//   remote  dropped on your cell, explodes when you press detonate (all remotes at once)
//
// A blast is a cross reaching BLAST_RANGE cells, stopped by walls, mirrors, emitters,
// receivers, gates and doors. It destroys enemies and the first wooden crate in each arm (metal
// crates and mirrors stop it unharmed), sets off other bombs (chain reactions), and hurts the player (1 heart).
// Mirror boxes are sturdy: they stop the flames but survive. A wooden door (D) next to a blast
// catches it and breaks — the only way through. Wooden walls (F) are walls: they stop it.
// Lasers set bombs off too.
import { BOX, boxCell, boxKind, DX, DY, hasBox, tileAt, type Level, type State } from './Map19Rules';
import type { Enemy } from './Map19Enemies';

export type BombKind = 'time' | 'throw' | 'remote';
export const BOMB_KINDS: BombKind[] = ['time', 'throw', 'remote'];
export type Bomb = { kind: BombKind; cell: number; from: number; fuse: number; flight: number };
export type Blast = { cells: number[]; t: number };
export type Ammo = Record<BombKind, number>;

export const FUSE = 2.5;
export const FLIGHT = 0.25; // seconds a thrown bomb is in the air
export const THROW_RANGE = 3;
export const BLAST_RANGE = 2;
export const BLAST_SHOW = 0.45; // seconds the flames stay on screen
export const PICKUP_AMMO = 2;

const neighbour = (L: Level, c: number, d: number) => {
    const x = (c % L.w) + DX[d];
    const y = Math.floor(c / L.w) + DY[d];
    return x < 0 || y < 0 || x >= L.w || y >= L.h ? -1 : y * L.w + x;
};

/** Flames stop at these (not included). Floor, plates and open gates let them through. */
function blocksBlast(L: Level, s: State, c: number): boolean {
    const b = tileAt(L, s, c);
    if (b === 'L') return (s.keys & (1 << L.gates.get(c)!)) === 0;
    return b === '#' || b === 'F' || b === 'M' || b === 'O' || b === 'Q' || b === 'H' || b === 'J' || b === ' ';
}

const breakable = (L: Level, s: State, c: number) => {
    const b = tileAt(L, s, c);
    return b === 'D';
};

/** Cross of cells a bomb at `center` burns: each arm ends at a blocker or after the first box. */
export function blastCells(L: Level, s: State, center: number): number[] {
    const out = [center];
    for (let d = 0; d < 4; d++) {
        let c = center;
        for (let r = 0; r < BLAST_RANGE; r++) {
            c = neighbour(L, c, d);
            if (c < 0) break;
            if (breakable(L, s, c)) { out.push(c); break; } // the wooden door takes the hit
            if (blocksBlast(L, s, c)) break;
            out.push(c);
            if (hasBox(s, c)) break;
        }
    }
    return out;
}

/** Where a thrown bomb lands, and whether it lands on an enemy (explodes right there). */
export function throwTarget(L: Level, s: State, enemies: Enemy[], bombs: Bomb[], from: number, dir: number): number {
    let at = from;
    for (let r = 0; r < THROW_RANGE; r++) {
        const n = neighbour(L, at, dir);
        if (n < 0 || blocksBlast(L, s, n) || breakable(L, s, n) || hasBox(s, n)) break;
        at = n;
        if (enemies.some((e) => e.cell === n) || bombs.some((b) => b.cell === n)) break;
    }
    return at;
}

export type Detonation = {
    s: State; // crates destroyed
    bombs: Bomb[]; // bombs left
    enemies: Enemy[];
    cells: number[]; // every burned cell (for drawing)
    playerHit: boolean;
    kills: number;
    crates: number;
    doors: number; // wooden doors broken
};

/** Explode the bombs at `centers` and everything they chain into. */
export function detonate(L: Level, s0: State, bombs0: Bomb[], enemies0: Enemy[], centers: number[]): Detonation {
    let s = s0;
    let bombs = bombs0;
    const queue = [...centers];
    const burned = new Set<number>();
    const done = new Set<number>();
    let crates = 0;
    let doors = 0;
    while (queue.length) {
        const center = queue.shift()!;
        if (done.has(center)) continue;
        done.add(center);
        bombs = bombs.filter((b) => b.cell !== center || b.flight > 0);
        for (const c of blastCells(L, s, center)) {
            burned.add(c);
            if (breakable(L, s, c)) { s = { ...s, broken: [...s.broken, c].sort((a, b) => a - b) }; doors++; continue; }
            if (c !== center && bombs.some((b) => b.cell === c && b.flight <= 0)) queue.push(c); // chain
            const bi = s.boxes.findIndex((b) => boxCell(b) === c);
            if (bi >= 0 && boxKind(s.boxes[bi]) === BOX.crate) {
                s = { ...s, boxes: s.boxes.filter((_, i) => i !== bi) };
                crates++;
            }
        }
    }
    const enemies = enemies0.filter((e) => !burned.has(e.cell));
    return {
        s, bombs, enemies, cells: [...burned], playerHit: burned.has(s.player),
        kills: enemies0.length - enemies.length, crates, doors,
    };
}
