// Puzzle Islands — level presets: World · Level → dungeon generator settings.
// Each world adds ONE idea (see minecraft/TODO.md "Progression worlds"); levels inside a world
// ramp difficulty and size. Pick a preset, then override any slider.
//
// Worlds 1–6 are built: crates + gold keys, enemies + bombs, lasers + mirrors, teleporters,
// coloured keys + plates, batteries + wooden walls / doors + pulsing lasers.
import { DEFAULT_DUNGEON, type DungeonParams } from './Map19Dungeon';

export type Preset = { id: string; world: number; label: string; params: Partial<DungeonParams> };

/**
 * The GENERATOR dropdown: Classic (everything mixed) or a World. Each world adds ONE idea on top
 * of the previous ones; `ready` = the dungeon generator can build it yet.
 */
export type World = { id: number; name: string; adds: string; ready: boolean; enemies?: boolean; lasers?: boolean; teleports?: boolean; locks?: boolean; wood?: boolean };
export const WORLDS: World[] = [
    { id: 1, name: 'World 1', adds: 'crates + gold keys + exit', ready: true, enemies: false },
    { id: 2, name: 'World 2', adds: 'enemies + your bombs', ready: true, enemies: true },
    { id: 3, name: 'World 3', adds: 'lasers + mirror crates', ready: true, enemies: true, lasers: true },
    { id: 4, name: 'World 4', adds: 'teleporter pads', ready: true, enemies: true, lasers: true, teleports: true },
    { id: 5, name: 'World 5', adds: 'coloured keys + gates, plates', ready: true, enemies: true, lasers: true, teleports: true, locks: true },
    { id: 6, name: 'World 6', adds: 'batteries, wooden walls / doors, pulsing lasers', ready: true, enemies: true, lasers: true, teleports: true, locks: true, wood: true },
];

/** Dungeon settings for a world (its flags on top of the current settings). */
export function worldParams(world: number, cur: DungeonParams): DungeonParams {
    const w = WORLDS.find((x) => x.id === world);
    return { ...cur, enemies: !!w?.enemies, lasers: !!w?.lasers, teleports: !!w?.teleports, locks: !!w?.locks, wood: !!w?.wood };
}

const INTRO = ['', ' (crates + keys)', ' (meet the enemies)', ' (lasers + mirrors)', ' (teleporters)', ' (coloured keys + plates)', ' (batteries + wood)'];
const level = (world: number, n: number, difficulty: number, width: number, height: number, rooms: number): Preset => ({
    id: `w${world}-${n}`,
    world,
    label: `World ${world} · Level ${n}${n === 1 ? INTRO[world] : ''}`,
    params: { difficulty, width, height, rooms, enemies: world >= 2, lasers: world >= 3, teleports: world >= 4, locks: world >= 5, wood: world >= 6 },
});

export const PRESETS: Preset[] = [
    level(1, 1, 1, 15, 11, 3),
    level(1, 2, 2, 19, 13, 4),
    level(1, 3, 3, 21, 15, 5),
    level(1, 4, 4, 25, 17, 6),
    level(1, 5, 5, 27, 19, 7),
    level(2, 1, 2, 21, 15, 5),
    level(2, 2, 3, 25, 17, 6),
    level(2, 3, 5, 27, 19, 7),
    level(2, 4, 7, 31, 21, 8),
    level(2, 5, 9, 33, 23, 9),
    level(3, 1, 2, 21, 15, 5),
    level(3, 2, 3, 25, 17, 6),
    level(3, 3, 5, 27, 19, 7),
    level(3, 4, 7, 31, 21, 8),
    level(3, 5, 9, 33, 23, 9),
    level(4, 1, 2, 21, 15, 5),
    level(4, 2, 3, 25, 17, 6),
    level(4, 3, 5, 27, 19, 7),
    level(4, 4, 7, 31, 21, 8),
    level(4, 5, 9, 33, 23, 9),
    level(5, 1, 2, 23, 15, 6),
    level(5, 2, 3, 25, 17, 7),
    level(5, 3, 5, 27, 19, 7),
    level(5, 4, 7, 31, 21, 8),
    level(5, 5, 9, 33, 23, 9),
    level(6, 1, 2, 23, 15, 6),
    level(6, 2, 3, 25, 17, 7),
    level(6, 3, 5, 27, 19, 7),
    level(6, 4, 7, 31, 21, 8),
    level(6, 5, 9, 33, 23, 9),
];

/** Settings for a preset (generosity / threat sliders back to their defaults, seed kept). */
export function presetParams(id: string, seed: number): DungeonParams {
    const p = PRESETS.find((x) => x.id === id);
    return { ...DEFAULT_DUNGEON, ...(p?.params ?? {}), seed };
}
