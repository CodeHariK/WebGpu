import { Vector2 } from '../lib/Vector2';
import { createNoise2D } from 'simplex-noise';
import Delaunator from 'delaunator';
import { getHexCenter, getHexVertices, getNeighborPos } from './Map14Logic';

/**
 * Map 18 — Hex Biome Generator
 *
 * Pipeline (Amit-Patel polygonal-map style, adapted to a hex lattice):
 *   1. Scalar fields: elevation + moisture sampled from fBm simplex noise,
 *      shaped by a radial island falloff so a continent sits at the origin.
 *   2. Classification: each hex is land or water by a sea-level threshold;
 *      land hexes get a Whittaker biome from (elevation, moisture).
 *   3. Flood fill (BFS over the hex neighbour graph):
 *        - ocean  = water reachable from the map border,
 *        - lake   = water not reachable from the border,
 *        - islands smaller than a minimum area are culled back to water.
 *   4. Smooth coastline: the same elevation field is sampled at hex *corners*
 *      and the sea-level iso-line is traced per-triangle (marching squares on
 *      the 6 triangles of each hex) — see marchHexCoastline().
 */

// ---------------------------------------------------------------------------
// Seeded RNG (mulberry32) — deterministic per seed
// ---------------------------------------------------------------------------
export function mulberry32(a: number) {
    return function () {
        a |= 0;
        a = (a + 0x6d2b79f5) | 0;
        let t = Math.imul(a ^ (a >>> 15), 1 | a);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
}

// ---------------------------------------------------------------------------
// Biomes
// ---------------------------------------------------------------------------
export type Biome =
    | 'DEEP_OCEAN' | 'OCEAN' | 'SHALLOW' | 'LAKE' | 'BEACH'
    | 'SCORCHED' | 'BARE' | 'TUNDRA' | 'SNOW'
    | 'TEMPERATE_DESERT' | 'SHRUBLAND' | 'TAIGA'
    | 'GRASSLAND' | 'TEMPERATE_FOREST' | 'TEMPERATE_RAINFOREST'
    | 'SUBTROPICAL_DESERT' | 'TROPICAL_FOREST' | 'TROPICAL_RAINFOREST'
    | 'LAVA'
    | 'CANDY' | 'CHOCOLATE' | 'MINT' | 'LICORICE'
    | 'PUMPKIN' | 'HAUNTED' | 'BONE';

export const BIOME_COLORS: Record<Biome, string> = {
    DEEP_OCEAN: '#2b3a67',
    OCEAN: '#33518a',
    SHALLOW: '#4d7bb5',
    LAKE: '#4a80c0',
    BEACH: '#d9cfa3',

    SCORCHED: '#5a5a54',
    BARE: '#8a877e',
    TUNDRA: '#bfc0a8',
    SNOW: '#f2f4f7',

    TEMPERATE_DESERT: '#cbc38a',
    SHRUBLAND: '#8aa06a',
    TAIGA: '#7fa27a',

    GRASSLAND: '#95bf6a',
    TEMPERATE_FOREST: '#5f9a54',
    TEMPERATE_RAINFOREST: '#3f7f52',

    LAVA: '#7c3b2e',
    CANDY: '#f4a6c6', CHOCOLATE: '#d46aa6', MINT: '#c9b0ec', LICORICE: '#7b52b0',
    PUMPKIN: '#e07b39', HAUNTED: '#4b3f63', BONE: '#d8d0bb',
    SUBTROPICAL_DESERT: '#dcc98f',
    TROPICAL_FOREST: '#67a95a',
    TROPICAL_RAINFOREST: '#2f8f5b',
};

export const BIOME_LABELS: Record<Biome, string> = {
    DEEP_OCEAN: 'Deep Ocean', OCEAN: 'Ocean', SHALLOW: 'Coastal Water', LAKE: 'Lake', BEACH: 'Beach',
    SCORCHED: 'Scorched', BARE: 'Bare Rock', TUNDRA: 'Tundra', SNOW: 'Snow',
    TEMPERATE_DESERT: 'Temperate Desert', SHRUBLAND: 'Shrubland', TAIGA: 'Taiga',
    GRASSLAND: 'Grassland', TEMPERATE_FOREST: 'Temperate Forest', TEMPERATE_RAINFOREST: 'Temperate Rainforest',
    LAVA: 'Lava', SUBTROPICAL_DESERT: 'Subtropical Desert', TROPICAL_FOREST: 'Tropical Forest', TROPICAL_RAINFOREST: 'Tropical Rainforest',
    CANDY: 'Candy', CHOCOLATE: 'Chocolate', MINT: 'Mint', LICORICE: 'Licorice', PUMPKIN: 'Pumpkin Patch', HAUNTED: 'Haunted', BONE: 'Boneyard',
};

/** Whittaker classification for a *land* hex from normalised elevation & moisture. */
export function classifyLand(e: number, m: number): Biome {
    if (e > 0.9 && m < 0.25) return 'LAVA'; // volcanic peaks (very high, dry)
    if (e > 0.82) {
        if (m < 0.1) return 'SCORCHED';
        if (m < 0.2) return 'BARE';
        if (m < 0.5) return 'TUNDRA';
        return 'SNOW';
    }
    if (e > 0.62) {
        if (m < 0.33) return 'TEMPERATE_DESERT';
        if (m < 0.66) return 'SHRUBLAND';
        return 'TAIGA';
    }
    if (e > 0.38) {
        if (m < 0.16) return 'TEMPERATE_DESERT';
        if (m < 0.5) return 'GRASSLAND';
        if (m < 0.83) return 'TEMPERATE_FOREST';
        return 'TEMPERATE_RAINFOREST';
    }
    if (m < 0.16) return 'SUBTROPICAL_DESERT';
    if (m < 0.33) return 'GRASSLAND';
    if (m < 0.66) return 'TROPICAL_FOREST';
    return 'TROPICAL_RAINFOREST';
}

/**
 * Region biome for the stylized Voronoi split: a coarse Whittaker on a
 * latitude-driven TEMPERATURE (north cold -> snow, south hot -> desert/tropics)
 * and a region MOISTURE. Used per Voronoi seed so each biome is a bold zone.
 */
export function classifyRegion(temp: number, moist: number, rng: () => number): Biome {
    if (temp < 0.18) return moist < 0.5 ? 'TUNDRA' : 'SNOW';
    if (temp < 0.40) return moist < 0.35 ? 'SHRUBLAND' : 'TAIGA';
    if (temp < 0.72) {
        if (moist < 0.26) return 'TEMPERATE_DESERT';
        if (moist < 0.52) return 'GRASSLAND';
        if (moist < 0.74) return 'TEMPERATE_FOREST';
        return 'TEMPERATE_RAINFOREST';
    }
    if (moist < 0.22) return rng() < 0.18 ? 'LAVA' : 'SUBTROPICAL_DESERT';
    if (moist < 0.50) return 'GRASSLAND';
    if (moist < 0.78) return 'TROPICAL_FOREST';
    return 'TROPICAL_RAINFOREST';
}

/** Themed biome pools for the whimsical themes, ordered so relaxation blends nicely. */
export const THEME_POOLS: Record<string, Biome[]> = {
    candy: ['MINT', 'CANDY', 'CHOCOLATE', 'LICORICE'],
    spooky: ['BONE', 'PUMPKIN', 'HAUNTED'],
};

export function isWaterBiome(b: Biome): boolean {
    return b === 'DEEP_OCEAN' || b === 'OCEAN' || b === 'SHALLOW' || b === 'LAKE';
}

// ---------------------------------------------------------------------------
// Cell + world model
// ---------------------------------------------------------------------------
export interface HexCell {
    q: number;
    r: number;
    center: Vector2;
    elevation: number; // [0,1], island-shaped
    moisture: number;  // [0,1]
    biome: Biome;
    regionId: number;  // land region id (-1 for water)
}

export interface BiomeWorld {
    cells: Map<string, HexCell>;
    landRegions: number;
    landCount: number;
    waterCount: number;
    sampleElevation: (x: number, y: number) => number; // shared field for corner sampling
    sampleMoisture: (x: number, y: number) => number;
    categoryAt: (x: number, y: number) => Biome; // field -> biome, used for smooth per-corner fill
    seaLevel: number;
    hexSize: number;
}

export interface BiomeOptions {
    width: number;
    height: number;
    seed: number;
    hexSize: number;
    seaLevel: number;   // [0,1] threshold on shaped elevation
    noiseScale?: number;
    minIslandArea?: number;
    biomes?: boolean; // false = ordinary green temperate map (no snow/desert/lava regions)
    theme?: string;   // 'realistic' | 'candy' | 'spooky' | 'chaos'
    coherence?: number; // 0..1 adjacency smoothing (0 = random neighbours, 1 = smooth bands)
}

const key = (q: number, r: number) => `${q},${r}`;

/** fBm accumulation, returns value in roughly [0,1]. */
function makeFbm(noise: (x: number, y: number) => number, octaves = 5, lacunarity = 2, gain = 0.5) {
    return (x: number, y: number): number => {
        let amp = 1;
        let freq = 1;
        let sum = 0;
        let norm = 0;
        for (let o = 0; o < octaves; o++) {
            sum += amp * noise(x * freq, y * freq);
            norm += amp;
            amp *= gain;
            freq *= lacunarity;
        }
        return (sum / norm) * 0.5 + 0.5; // [-1,1] -> [0,1]
    };
}

/**
 * Build the biome world. Elevation is an fBm field pulled down toward the
 * edges by a radial falloff, producing a continent centred on the origin.
 */
export function generateBiomeWorld(opts: BiomeOptions): BiomeWorld {
    const { width, height, seed, hexSize, seaLevel } = opts;
    const noiseScale = opts.noiseScale ?? 1;
    const biomesOn = opts.biomes ?? true;
    const theme = opts.theme ?? 'realistic';
    const coherence = opts.coherence ?? 0.6;
    const minIslandArea = opts.minIslandArea ?? 4;

    const rand = mulberry32(seed);
    const elevNoise = createNoise2D(rand);
    const moistNoise = createNoise2D(rand);

    const fbmE = makeFbm(elevNoise, 5, 2.05, 0.5);
    const fbmM = makeFbm(moistNoise, 4, 2.1, 0.55);

    // World-space frequency: a few big features across the viewport.
    const fe = (0.0016 / noiseScale);
    const fm = (0.0022 / noiseScale);

    // Radial island falloff. mapRadius is tuned so the continent fills the view.
    const mapRadius = Math.max(width, height) * 0.90;

    // Shared elevation sampler (used both for cell centres and hex corners).
    const sampleElevation = (x: number, y: number): number => {
        const base = fbmE(x * fe, y * fe);
        const d = Math.sqrt(x * x + y * y) / mapRadius;
        // Subtract a smooth radial bowl so edges fall below sea level.
        const shaped = base - Math.pow(Math.max(0, d), 2.2) * 0.55;
        // Inland mountain ranges: a low-frequency ridge that only rises well away
        // from the coast, so interiors can reach bare rock / snow elevations.
        const ridge = Math.max(0, fbmE(x * fe * 0.5 + 500, y * fe * 0.5 + 500) - 0.62) * 1.8;
        const peak = ridge * (1 - Math.min(1, d));
        return Math.max(0, Math.min(1, shaped + peak));
    };
    const sampleMoisture = (x: number, y: number): number => {
        // Contrast-stretched so dry regions reach desert and wet regions rainforest.
        const m = fbmM(x * fm + 1000, y * fm - 1000);
        return Math.max(0, Math.min(1, (m - 0.5) * 1.55 + 0.5));
    };

    // ---- Stylized biome REGIONS: domain-warped Voronoi -------------------------
    // Scatter biome seeds across the map; each land hex takes the biome of the
    // nearest seed, with the lookup point warped by low-frequency noise so region
    // borders wiggle organically instead of being straight Voronoi edges.
    const maxDim = Math.max(width, height);
    const seedSpacing = maxDim / 5.2;
    const warpAmp = seedSpacing * 0.6;
    const fw = 0.004 / noiseScale;
    // Scatter seed positions with a per-seed climate (temperature by latitude, moisture by noise).
    const rawSeeds: { x: number; y: number; temp: number; moist: number }[] = [];
    {
        const half = maxDim * 0.62;
        let attempts = 0;
        while (rawSeeds.length < 24 && attempts < 3000) {
            attempts++;
            const x = (rand() * 2 - 1) * half;
            const y = (rand() * 2 - 1) * half;
            if (rawSeeds.every((s) => Math.hypot(s.x - x, s.y - y) > seedSpacing * 0.82)) {
                const temp = Math.max(0, Math.min(1, (y / half) * 0.65 + 0.5 + (fbmM(x * 0.0018 - 5, y * 0.0018 + 5) - 0.5) * 0.28));
                const mraw = fbmM(x * 0.003 + 50, y * 0.003 - 50);
                const moist = Math.max(0, Math.min(1, (mraw - 0.5) * 1.5 + 0.5));
                rawSeeds.push({ x, y, temp, moist });
            }
        }
    }
    // Seed adjacency (Delaunay) so we can smooth biome choices between touching regions.
    const neighbors: Set<number>[] = rawSeeds.map(() => new Set<number>());
    if (rawSeeds.length >= 3) {
        const coords: number[] = [];
        for (const s of rawSeeds) coords.push(s.x, s.y);
        const del = new Delaunator(Float64Array.from(coords));
        const tri = del.triangles;
        for (let i = 0; i < tri.length; i += 3) {
            const a = tri[i], b = tri[i + 1], c = tri[i + 2];
            neighbors[a].add(b); neighbors[a].add(c);
            neighbors[b].add(a); neighbors[b].add(c);
            neighbors[c].add(a); neighbors[c].add(b);
        }
    }
    // Assign biomes by strategy, with a coherence-controlled adjacency relaxation.
    // --- Always compute the realistic climate biomes first (extremes come from here). ---
    let biomeArr: Biome[];
    {
        let tp = rawSeeds.map((s) => s.temp);
        let mp = rawSeeds.map((s) => s.moist);
        const f = coherence * 0.5;
        for (let pass = 0; pass < 3; pass++) {
            const nt = tp.slice(), nm = mp.slice();
            for (let i = 0; i < rawSeeds.length; i++) {
                const nb = [...neighbors[i]]; if (!nb.length) continue;
                let st = 0, sm = 0; for (const j of nb) { st += tp[j]; sm += mp[j]; }
                nt[i] = tp[i] + (st / nb.length - tp[i]) * f;
                nm[i] = mp[i] + (sm / nb.length - mp[i]) * f;
            }
            tp = nt; mp = nm;
        }
        if (rawSeeds.length) {
            const meanT = tp.reduce((a, b) => a + b, 0) / tp.length;
            const meanM = mp.reduce((a, b) => a + b, 0) / mp.length;
            const k = 1 + coherence * 0.7;
            for (let i = 0; i < tp.length; i++) {
                tp[i] = Math.max(0, Math.min(1, meanT + (tp[i] - meanT) * k));
                mp[i] = Math.max(0, Math.min(1, meanM + (mp[i] - meanM) * k));
            }
        }
        biomeArr = rawSeeds.map((_, i) => classifyRegion(tp[i], mp[i], rand));
        if (rawSeeds.length) {
            let hi = 0;
            for (let i = 1; i < tp.length; i++) if (tp[i] > tp[hi]) hi = i;
            if (tp[hi] > 0.76 && rand() < 0.7) biomeArr[hi] = 'LAVA';
            let di = -1, dm = 2;
            for (let i = 0; i < tp.length; i++) if (tp[i] > 0.45 && mp[i] < dm && biomeArr[i] !== 'LAVA') { dm = mp[i]; di = i; }
            if (di >= 0 && mp[di] < 0.42) biomeArr[di] = tp[di] > 0.72 ? 'SUBTROPICAL_DESERT' : 'TEMPERATE_DESERT';
        }
    }

    // --- Non-realistic themes reskin ONLY the forest/vegetation regions. ---
    // Snow, tundra, bare, desert, lava, beach and water always keep their realistic biome.
    if (theme !== 'realistic') {
        const FOREST = new Set<Biome>(['GRASSLAND', 'SHRUBLAND', 'TAIGA', 'TEMPERATE_FOREST', 'TEMPERATE_RAINFOREST', 'TROPICAL_FOREST', 'TROPICAL_RAINFOREST']);
        const isForest = biomeArr.map((b) => FOREST.has(b));
        if (theme === 'chaos') {
            // Wild vegetation: each forest region a random whimsical biome (no snow/lava/desert).
            const cpool: Biome[] = ['CANDY', 'CHOCOLATE', 'MINT', 'LICORICE', 'PUMPKIN', 'HAUNTED', 'BONE', 'TROPICAL_RAINFOREST', 'GRASSLAND'];
            for (let i = 0; i < biomeArr.length; i++) if (isForest[i]) biomeArr[i] = cpool[Math.floor(rand() * cpool.length)];
        } else {
            const pool = THEME_POOLS[theme] ?? THEME_POOLS.candy;
            let idx = rawSeeds.map((_, i) => (isForest[i] ? Math.floor(rand() * pool.length) : -1));
            const ff = coherence * 0.55;
            for (let pass = 0; pass < 3; pass++) {
                const ni = idx.slice();
                for (let i = 0; i < rawSeeds.length; i++) {
                    if (!isForest[i]) continue;
                    const nb = [...neighbors[i]].filter((j) => isForest[j]);
                    if (!nb.length) continue;
                    let s2 = 0; for (const j of nb) s2 += idx[j];
                    ni[i] = Math.max(0, Math.min(pool.length - 1, Math.round(idx[i] + (s2 / nb.length - idx[i]) * ff)));
                }
                idx = ni;
            }
            for (let i = 0; i < biomeArr.length; i++) if (isForest[i]) biomeArr[i] = pool[idx[i]];
        }
    }

    const seeds = rawSeeds.map((s, i) => ({ x: s.x, y: s.y, biome: biomeArr[i] }));
    // Ordinary (biomes-off) land: just greens, no desert/snow/lava.
    const ordinaryLand = (eAbove: number, m: number): Biome => {
        if (eAbove > 0.72) return 'TEMPERATE_FOREST';
        if (m > 0.55) return 'TEMPERATE_FOREST';
        return 'GRASSLAND';
    };
    const regionBiomeAt = (x: number, y: number): Biome => {
        const wx = x + (fbmE(x * fw, y * fw) - 0.5) * warpAmp;
        const wy = y + (fbmE(x * fw + 100, y * fw + 100) - 0.5) * warpAmp;
        let best = seeds[0], bd = Infinity;
        for (const s of seeds) { const dx = s.x - wx, dy = s.y - wy; const d = dx * dx + dy * dy; if (d < bd) { bd = d; best = s; } }
        return best ? best.biome : 'GRASSLAND';
    };

    // Axial range covering the viewport (+ margin) centred on origin.
    const range = Math.ceil(Math.max(width, height) / (hexSize * Math.sqrt(3))) + 3;

    const cells = new Map<string, HexCell>();
    for (let r = -range; r <= range; r++) {
        for (let q = -range; q <= range; q++) {
            const hexDist = (Math.abs(q) + Math.abs(q + r) + Math.abs(r)) / 2;
            if (hexDist > range) continue;
            const center = getHexCenter(q, r, hexSize);
            const e = sampleElevation(center.x, center.y);
            const m = sampleMoisture(center.x, center.y);
            cells.set(key(q, r), {
                q, r, center, elevation: e, moisture: m,
                biome: 'OCEAN', regionId: -1,
            });
        }
    }

    // -- Initial land/water split + land biome ------------------------------
    for (const c of cells.values()) {
        if (c.elevation < seaLevel) {
            c.biome = 'OCEAN'; // refined below
        } else {
            const eAbove = (c.elevation - seaLevel) / (1 - seaLevel);
            c.biome = biomesOn ? regionBiomeAt(c.center.x, c.center.y) : ordinaryLand(eAbove, c.moisture);
        }
    }

    // -- Flood fill: ocean detection from the border ------------------------
    // Border water hexes (near the axial edge) are seeds for the open ocean.
    const isWaterCell = (c: HexCell) => c.elevation < seaLevel;
    const oceanSet = new Set<string>();
    const queue: HexCell[] = [];
    for (const c of cells.values()) {
        const hexDist = (Math.abs(c.q) + Math.abs(c.q + c.r) + Math.abs(c.r)) / 2;
        if (hexDist >= range - 1 && isWaterCell(c)) {
            oceanSet.add(key(c.q, c.r));
            queue.push(c);
        }
    }
    let head = 0;
    while (head < queue.length) {
        const c = queue[head++];
        for (let s = 0; s < 6; s++) {
            const n = getNeighborPos(c.q, c.r, s);
            const nk = key(n.q, n.r);
            const nc = cells.get(nk);
            if (!nc || oceanSet.has(nk) || !isWaterCell(nc)) continue;
            oceanSet.add(nk);
            queue.push(nc);
        }
    }

    // -- Flood fill: label land regions, cull tiny islands ------------------
    let regionId = 0;
    const visited = new Set<string>();
    for (const c of cells.values()) {
        if (isWaterCell(c) || visited.has(key(c.q, c.r))) continue;
        // BFS this contiguous landmass.
        const region: HexCell[] = [];
        const stack: HexCell[] = [c];
        visited.add(key(c.q, c.r));
        while (stack.length) {
            const cur = stack.pop()!;
            region.push(cur);
            for (let s = 0; s < 6; s++) {
                const n = getNeighborPos(cur.q, cur.r, s);
                const nk = key(n.q, n.r);
                const nc = cells.get(nk);
                if (!nc || visited.has(nk) || isWaterCell(nc)) continue;
                visited.add(nk);
                stack.push(nc);
            }
        }
        if (region.length < minIslandArea) {
            // Cull: sink the island back to water.
            for (const rc of region) {
                rc.regionId = -1;
                oceanSet.add(key(rc.q, rc.r)); // treat as ocean-connected shallow
                rc.biome = 'SHALLOW';
            }
        } else {
            for (const rc of region) rc.regionId = regionId;
            regionId++;
        }
    }

    // -- Refine water biomes: ocean depth, coastal shallows, lakes ----------
    let landCount = 0;
    let waterCount = 0;
    for (const c of cells.values()) {
        if (c.regionId >= 0) { landCount++; continue; }
        waterCount++;
        const k = key(c.q, c.r);
        // Is this water hex adjacent to land? -> coastal shallow.
        let touchesLand = false;
        for (let s = 0; s < 6; s++) {
            const n = getNeighborPos(c.q, c.r, s);
            const nc = cells.get(key(n.q, n.r));
            if (nc && nc.regionId >= 0) { touchesLand = true; break; }
        }
        if (!oceanSet.has(k)) {
            c.biome = 'LAKE';
        } else if (touchesLand) {
            c.biome = 'SHALLOW';
        } else if (c.elevation < seaLevel * 0.55) {
            c.biome = 'DEEP_OCEAN';
        } else {
            c.biome = 'OCEAN';
        }
    }

    // -- Beach ring: land hexes adjacent to coastal water at low elevation --
    for (const c of cells.values()) {
        if (c.regionId < 0) continue;
        const eAbove = (c.elevation - seaLevel) / (1 - seaLevel);
        if (eAbove > 0.08) continue;
        let coastal = false;
        for (let s = 0; s < 6; s++) {
            const n = getNeighborPos(c.q, c.r, s);
            const nc = cells.get(key(n.q, n.r));
            if (nc && nc.regionId < 0) { coastal = true; break; }
        }
        if (coastal) c.biome = 'BEACH';
    }

    // Pure field -> biome classification (no flood-fill state): used to colour hex
    // corners and centres for the smooth, contour-respecting fill. Water subtype is
    // depth-based here (deep / ocean / shallow) since a single point has no
    // ocean-vs-lake connectivity; a thin BEACH band sits just above sea level.
    const categoryAt = (x: number, y: number): Biome => {
        const e = sampleElevation(x, y);
        if (e < seaLevel) {
            if (e < seaLevel * 0.5) return 'DEEP_OCEAN';
            if (e > seaLevel * 0.85) return 'SHALLOW';
            return 'OCEAN';
        }
        const eAbove = (e - seaLevel) / (1 - seaLevel);
        if (eAbove < 0.06) return 'BEACH';
        return biomesOn ? regionBiomeAt(x, y) : ordinaryLand(eAbove, sampleMoisture(x, y));
    };

    return {
        cells,
        landRegions: regionId,
        landCount,
        waterCount,
        sampleElevation,
        sampleMoisture,
        categoryAt,
        seaLevel,
        hexSize,
    };
}

// ---------------------------------------------------------------------------
// Marching-triangle coastline
// ---------------------------------------------------------------------------
/**
 * Trace the sea-level iso-contour across one hex by splitting it into its 6
 * triangles (centre + two adjacent corners) and, for each triangle, sampling
 * the shared elevation field at the three points, then emitting the marching
 * segment where the field crosses `iso`. Because corners are shared world
 * points, segments meet seamlessly across neighbouring hexes -> smooth coast.
 *
 * Returns pairs of Vector2 [a, b] segments in world space.
 */
export function marchHexCoastline(
    center: Vector2,
    hexSize: number,
    iso: number,
    sample: (x: number, y: number) => number
): [Vector2, Vector2][] {
    const verts = getHexVertices(center, hexSize);
    const segs: [Vector2, Vector2][] = [];

    const cE = sample(center.x, center.y);
    const vE = verts.map(v => sample(v.x, v.y));

    const lerpPt = (p1: Vector2, e1: number, p2: Vector2, e2: number): Vector2 => {
        const denom = (e2 - e1);
        const t = Math.abs(denom) < 1e-6 ? 0.5 : (iso - e1) / denom;
        const tc = Math.max(0, Math.min(1, t));
        return new Vector2(p1.x + (p2.x - p1.x) * tc, p1.y + (p2.y - p1.y) * tc);
    };

    for (let i = 0; i < 6; i++) {
        const a = center, ea = cE;
        const b = verts[i], eb = vE[i];
        const c = verts[(i + 1) % 6], ec = vE[(i + 1) % 6];

        // Marching triangle: build a 3-bit code of which corners are above iso.
        const above = (ea >= iso ? 1 : 0) | (eb >= iso ? 2 : 0) | (ec >= iso ? 4 : 0);
        if (above === 0 || above === 7) continue; // fully below / above -> no crossing

        // The iso-line crosses the two edges that separate above from below.
        const pts: Vector2[] = [];
        // edge a-b
        if ((ea >= iso) !== (eb >= iso)) pts.push(lerpPt(a, ea, b, eb));
        // edge b-c
        if ((eb >= iso) !== (ec >= iso)) pts.push(lerpPt(b, eb, c, ec));
        // edge c-a
        if ((ec >= iso) !== (ea >= iso)) pts.push(lerpPt(c, ec, a, ea));

        if (pts.length === 2) segs.push([pts[0], pts[1]]);
    }
    return segs;
}


// ---------------------------------------------------------------------------
// Multi-material triangle partition (marching triangles)
// ---------------------------------------------------------------------------
export interface FillRegion {
    pts: Vector2[];
    biome: Biome;
}

const mid = (p: Vector2, q: Vector2): Vector2 =>
    new Vector2((p.x + q.x) / 2, (p.y + q.y) / 2);

/**
 * Partition a triangle whose three vertices carry (possibly different) discrete
 * biome categories into flat-coloured sub-regions, with boundaries running along
 * edge midpoints so they stay continuous with neighbouring triangles.
 *
 *   - all three equal   -> one region (the whole triangle)
 *   - two equal, one odd -> a small triangle at the odd vertex + a quad for the pair
 *   - all three distinct -> three quads meeting at the centroid
 *
 * Fed the 6 triangles of a hex (centre + two adjacent corners), this turns the
 * corner colours into arcs and central regions exactly as in the hand example.
 */
export function partitionTriangle(
    A: Vector2, ca: Biome,
    B: Vector2, cb: Biome,
    C: Vector2, cc: Biome
): FillRegion[] {
    if (ca === cb && cb === cc) {
        return [{ pts: [A, B, C], biome: ca }];
    }
    // Two equal, one odd -> odd vertex gets a corner triangle, pair keeps the quad.
    if (ca === cb) { // C is odd
        const mAC = mid(A, C), mBC = mid(B, C);
        return [
            { pts: [A, B, mBC, mAC], biome: ca },
            { pts: [mAC, mBC, C], biome: cc },
        ];
    }
    if (cb === cc) { // A is odd
        const mAB = mid(A, B), mAC = mid(A, C);
        return [
            { pts: [B, C, mAC, mAB], biome: cb },
            { pts: [A, mAB, mAC], biome: ca },
        ];
    }
    if (ca === cc) { // B is odd
        const mAB = mid(A, B), mBC = mid(B, C);
        return [
            { pts: [A, mAB, mBC, C], biome: ca },
            { pts: [mAB, B, mBC], biome: cb },
        ];
    }
    // All three distinct -> three quads meeting at the centroid.
    const mAB = mid(A, B), mBC = mid(B, C), mCA = mid(C, A);
    const G = new Vector2((A.x + B.x + C.x) / 3, (A.y + B.y + C.y) / 3);
    return [
        { pts: [A, mAB, G, mCA], biome: ca },
        { pts: [B, mBC, G, mAB], biome: cb },
        { pts: [C, mCA, G, mBC], biome: cc },
    ];
}


// ---------------------------------------------------------------------------
// Truchet-arc corner colouring
// ---------------------------------------------------------------------------
/**
 * Given the six corner categories of a hex, return the base colour (the most
 * common corner category, filled across the whole hex) plus the runs of
 * consecutive corners that differ from it. Each run is cut off from the base by
 * a single Truchet arc joining the midpoints of the two edges that bound the
 * run — a 1-corner run uses the ADJACENT pattern, 2 corners MEDIUM, 3 corners
 * DIAMETRICAL — so the arc is exactly the contour between the two colours.
 *
 *   corners 0,2,4 = red,green,blue and 1,3,5 = yellow
 *     -> base yellow, three 1-corner runs (red/green/blue caps).
 */
export function hexColorRuns(corner: Biome[]): {
    base: Biome;
    runs: { a: number; b: number; biome: Biome }[];
} {
    // Most common corner category becomes the base fill.
    const counts = new Map<Biome, number>();
    for (const c of corner) counts.set(c, (counts.get(c) ?? 0) + 1);
    let base = corner[0];
    let best = -1;
    for (const [k, v] of counts) {
        if (v > best) { best = v; base = k; }
    }
    if (best === 6) return { base, runs: [] }; // uniform hex

    // Start scanning at a colour boundary so runs don't get split across the wrap.
    let start = 0;
    for (let i = 0; i < 6; i++) {
        if (corner[i] !== corner[(i + 5) % 6]) { start = i; break; }
    }

    const runs: { a: number; b: number; biome: Biome }[] = [];
    let i = 0;
    while (i < 6) {
        const a = (start + i) % 6;
        const col = corner[a];
        let len = 1;
        while (len < 6 && corner[(start + i + len) % 6] === col) len++;
        const b = (start + i + len - 1) % 6;
        if (col !== base) runs.push({ a, b, biome: col });
        i += len;
    }
    return { base, runs };
}
