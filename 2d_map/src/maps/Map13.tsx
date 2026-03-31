import { useEffect, useRef, useState } from 'react';
import type { CSSProperties } from 'react';
import { Canvas } from '../lib/Canvas';
import { Vector2 } from '../lib/Vector2';
import { createNoise2D } from 'simplex-noise';

// Simple seeded random helper
const seededRandom = (s: number) => {
    let seed = s;
    return () => {
        seed = (seed * 9301 + 49297) % 233280;
        return seed / 233280;
    };
};

// --- HEX MATH ---
interface Hex {
    q: number;
    r: number;
}

const HEX_DIRECTIONS: Hex[] = [
    { q: 1, r: 0 },   // Right
    { q: 0, r: 1 },   // Down-Right
    { q: -1, r: 1 },  // Down-Left
    { q: -1, r: 0 },  // Left
    { q: 0, r: -1 },  // Up-Left
    { q: 1, r: -1 }   // Up-Right
];

function hexToPixel(q: number, r: number, size: number, center: Vector2): Vector2 {
    const x = size * (Math.sqrt(3) * q + (Math.sqrt(3) / 2) * r);
    const y = size * (1.5 * r);
    return new Vector2(x + center.x, y + center.y);
}

function getHexKey(q: number, r: number): string {
    return `${q},${r}`;
}

function parseHexKey(key: string): Hex {
    const [q, r] = key.split(',').map(Number);
    return { q, r };
}

export default function Map13({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const canvasInstanceRef = useRef<Canvas | null>(null);

    // UI State
    const [seed, setSeed] = useState(42);
    const [gridSize, setGridSize] = useState(15);
    const [spawnCount, setSpawnCount] = useState(5);
    const [smoothness, setSmoothness] = useState(2);
    const [noiseAmp, setNoiseAmp] = useState(6);
    const [noiseScale, setNoiseScale] = useState(0.04);
    const [filledHexes, setFilledHexes] = useState<Set<string>>(new Set());
    const [renderTrigger, setRenderTrigger] = useState(0);

    const chaikin = (points: Vector2[], iterations: number): Vector2[] => {
        if (iterations <= 0 || points.length < 3) return points;
        let newPoints: Vector2[] = [];
        for (let i = 0; i < points.length; i++) {
            const p1 = points[i];
            const p2 = points[(i + 1) % points.length];
            const q = new Vector2(p1.x * 0.75 + p2.x * 0.25, p1.y * 0.75 + p2.y * 0.25);
            const r = new Vector2(p1.x * 0.25 + p2.x * 0.75, p1.y * 0.25 + p2.y * 0.75);
            newPoints.push(q, r);
        }
        return chaikin(newPoints, iterations - 1);
    };

    // Initial Spawn
    const spawn = () => {
        const r = () => {
            let t = Math.random() * 99999;
            return () => {
                t = Math.imul(t ^ t >>> 15, t | 1);
                t ^= t + Math.imul(t ^ t >>> 7, t | 61);
                return ((t ^ t >>> 14) >>> 0) / 4294967296;
            };
        };
        const rand = r();
        const initial = new Set<string>();
        const radius = Math.floor(Math.min(width, height) / (gridSize * 4));

        for (let i = 0; i < spawnCount; i++) {
            const q = Math.floor(rand() * radius * 2 - radius);
            const rCoord = Math.floor(rand() * radius * 2 - radius);
            initial.add(getHexKey(q, rCoord));
        }
        setFilledHexes(initial);
    };

    const grow = (steps = 1) => {
        setFilledHexes(prev => {
            const next = new Set(prev);
            for (let s = 0; s < steps; s++) {
                const currentArr = Array.from(next);
                if (currentArr.length === 0) break;

                // Pick a random hex from the current set
                const randomKey = currentArr[Math.floor(Math.random() * currentArr.length)];
                const hex = parseHexKey(randomKey);

                // Find empty neighbors
                const emptyNeighbors = HEX_DIRECTIONS
                    .map(d => getHexKey(hex.q + d.q, hex.r + d.r))
                    .filter(nKey => !next.has(nKey));

                if (emptyNeighbors.length > 0) {
                    const chosen = emptyNeighbors[Math.floor(Math.random() * emptyNeighbors.length)];
                    next.add(chosen);
                }
            }
            return next;
        });
    };

    // Connect Islands
    const connectIslands = () => {
        setFilledHexes(prev => {
            const next = new Set(prev);
            if (prev.size === 0) return prev;

            // 1. Identify Clusters (using a simple flood-fill style grouping)
            const clusters: Hex[][] = [];
            const visited = new Set<string>();

            for (const hKey of prev) {
                if (visited.has(hKey)) continue;

                const cluster: Hex[] = [];
                const queue = [hKey];
                visited.add(hKey);

                while (queue.length > 0) {
                    const currentKey = queue.shift()!;
                    const currentHex = parseHexKey(currentKey);
                    cluster.push(currentHex);

                    HEX_DIRECTIONS.forEach(d => {
                        const nKey = getHexKey(currentHex.q + d.q, currentHex.r + d.r);
                        if (prev.has(nKey) && !visited.has(nKey)) {
                            visited.add(nKey);
                            queue.push(nKey);
                        }
                    });
                }
                clusters.push(cluster);
            }

            if (clusters.length <= 1) return prev;

            // 2. Connectivity Math Helpers
            const getDistance = (h1: Hex, h2: Hex) =>
                (Math.abs(h1.q - h2.q) + Math.abs(h1.q + h1.r - h2.q - h2.r) + Math.abs(h1.r - h2.r)) / 2;

            const hexRound = (q: number, r: number) => {
                let s = -q - r;
                let qi = Math.round(q);
                let ri = Math.round(r);
                let si = Math.round(s);
                const q_diff = Math.abs(qi - q);
                const r_diff = Math.abs(ri - r);
                const s_diff = Math.abs(si - s);
                if (q_diff > r_diff && q_diff > s_diff) qi = -ri - si;
                else if (r_diff > s_diff) ri = -qi - si;
                return { q: qi, r: ri };
            };

            const drawLine = (h1: Hex, h2: Hex) => {
                const N = getDistance(h1, h2);
                for (let i = 0; i <= N; i++) {
                    const t = N === 0 ? 0 : i / N;
                    const r_val = h1.r + (h2.r - h1.r) * t;
                    const q_val = h1.q + (h2.q - h1.q) * t;
                    const rounded = hexRound(q_val, r_val);
                    next.add(getHexKey(rounded.q, rounded.r));
                }
            };

            // 3. Connect Clusters (MST style: keep merging nearest clusters)
            const parent = Array.from({ length: clusters.length }, (_, i) => i);
            const find = (i: number): number => parent[i] === i ? i : (parent[i] = find(parent[i]));
            const union = (i: number, j: number) => {
                const rootI = find(i);
                const rootJ = find(j);
                if (rootI !== rootJ) {
                    parent[rootI] = rootJ;
                    return true;
                }
                return false;
            };

            let merges = 0;
            while (merges < clusters.length - 1) {
                let bestDist = Infinity;
                let bestH1: Hex | null = null;
                let bestH2: Hex | null = null;
                let bestI = -1;
                let bestJ = -1;

                for (let i = 0; i < clusters.length; i++) {
                    for (let j = i + 1; j < clusters.length; j++) {
                        if (find(i) === find(j)) continue;

                        for (const h1 of clusters[i]) {
                            for (const h2 of clusters[j]) {
                                const d = getDistance(h1, h2);
                                if (d < bestDist) {
                                    bestDist = d;
                                    bestH1 = h1;
                                    bestH2 = h2;
                                    bestI = i;
                                    bestJ = j;
                                }
                            }
                        }
                    }
                }

                if (bestH1 && bestH2) {
                    drawLine(bestH1, bestH2);
                    union(bestI, bestJ);
                    merges++;
                } else break;
            }

            return next;
        });
    };

    // Reset and Initial Spawn
    useEffect(() => {
        spawn();
    }, [seed, spawnCount]);

    // Init Canvas
    useEffect(() => {
        if (!canvasRef.current) return;
        const instance = new Canvas(canvasRef.current);
        instance.initInteractions({ onInteraction: () => setRenderTrigger(t => t + 1) });
        canvasInstanceRef.current = instance;
    }, [width, height]);

    const getHexVertices = (q: number, r: number, size: number, ctr: Vector2): Vector2[] => {
        const verts: Vector2[] = [];
        for (let i = 0; i < 6; i++) {
            const angleDeg = 60 * i - 30;
            const angleRad = (Math.PI / 180) * angleDeg;
            const p = hexToPixel(q, r, size, ctr);
            verts.push(new Vector2(p.x + size * Math.cos(angleRad), p.y + size * Math.sin(angleRad)));
        }
        return verts;
    };

    // Draw Loop
    useEffect(() => {
        const canvas = canvasInstanceRef.current;
        if (!canvas) return;

        // 1. Identify Clusters (for context/tinting)
        const clusterMap: Map<string, number> = new Map();
        let islandCount = 0;
        const visitedClusters = new Set<string>();

        filledHexes.forEach(hKey => {
            if (!visitedClusters.has(hKey)) {
                const queue = [hKey];
                visitedClusters.add(hKey);
                while (queue.length > 0) {
                    const currentKey = queue.shift()!;
                    clusterMap.set(currentKey, islandCount);
                    const hex = parseHexKey(currentKey);
                    HEX_DIRECTIONS.forEach(d => {
                        const nKey = getHexKey(hex.q + d.q, hex.r + d.r);
                        if (filledHexes.has(nKey) && !visitedClusters.has(nKey)) {
                            visitedClusters.add(nKey);
                            queue.push(nKey);
                        }
                    });
                }
                islandCount++;
            }
        });

        // 2. Clear and Setup
        canvas.clear();
        canvas.rect(-1000, -1000, width + 2000, height + 2000, { fill: '#0a0b12' });
        const center = new Vector2(width / 2, height / 2);

        // 3. Draw Background Grid
        const gridRadius = 25;
        for (let q = -gridRadius; q <= gridRadius; q++) {
            for (let rCoord = -gridRadius; rCoord <= gridRadius; rCoord++) {
                const verts = getHexVertices(q, rCoord, gridSize, center);
                canvas.polygon(verts, { stroke: 'rgba(59, 130, 246, 0.05)', lineWidth: 1 });
            }
        }

        // 4. Draw Hex Tints
        filledHexes.forEach(hKey => {
            const h = parseHexKey(hKey);
            const verts = getHexVertices(h.q, h.r, gridSize, center);
            const clusterIdx = clusterMap.get(hKey) ?? 0;
            const islandColor = `hsl(${(clusterIdx * 137.5) % 360}, 80%, 65%)`;
            canvas.polygon(verts, { fill: islandColor, alpha: 0.12 });
        });

        // 5. Extract Air-facing Edges
        const allEdges: { p1: Vector2; p2: Vector2 }[] = [];
        filledHexes.forEach(hKey => {
            const h = parseHexKey(hKey);
            const verts = getHexVertices(h.q, h.r, gridSize, center);
            for (let i = 0; i < 6; i++) {
                const nKey = getHexKey(h.q + HEX_DIRECTIONS[i].q, h.r + HEX_DIRECTIONS[i].r);
                if (!filledHexes.has(nKey)) {
                    allEdges.push({ p1: verts[i], p2: verts[(i + 1) % 6] });
                }
            }
        });

        // 6. Chain Edges into Loops
        const loops: Vector2[][] = [];
        const usedEdges = new Set<number>();
        for (let i = 0; i < allEdges.length; i++) {
            if (usedEdges.has(i)) continue;
            const loop = [allEdges[i].p1, allEdges[i].p2];
            usedEdges.add(i);
            let found = true;
            while (found) {
                found = false;
                const last = loop[loop.length - 1];
                for (let j = 0; j < allEdges.length; j++) {
                    if (usedEdges.has(j)) continue;
                    if (Vector2.dist(allEdges[j].p1, last) < 0.1) {
                        loop.push(allEdges[j].p2);
                        usedEdges.add(j);
                        found = true;
                        break;
                    } else if (Vector2.dist(allEdges[j].p2, last) < 0.1) {
                        loop.push(allEdges[j].p1);
                        usedEdges.add(j);
                        found = true;
                        break;
                    }
                }
            }
            if (loop.length > 2 && Vector2.dist(loop[0], loop[loop.length - 1]) < 0.1) {
                loop.pop();
            }
            loops.push(loop);
        }

        // 7. Render Unique Colors for Each Smoothed Periphery Loop
        const noise2D = createNoise2D(seededRandom(seed));
        loops.forEach((loop, loopIdx) => {
            // Apply Disturbance Before Smoothing
            const disturbed = loop.map(p => {
                const nx = noise2D(p.x * noiseScale, p.y * noiseScale);
                const ny = noise2D(p.x * noiseScale + 13.37, p.y * noiseScale + 13.37);
                return new Vector2(p.x + nx * noiseAmp, p.y + ny * noiseAmp);
            });

            const smoothed = chaikin(disturbed, smoothness);
            const loopColor = `hsl(${(loopIdx * 137.5) % 360}, 85%, 65%)`; // Distinct color per loop
            
            canvas.polygon(smoothed, { stroke: loopColor, lineWidth: 2, alpha: 0.95 });
            // Subtle glow
            canvas.polygon(smoothed, { stroke: loopColor, lineWidth: 6, alpha: 0.15 });
        });

    }, [filledHexes, gridSize, renderTrigger, width, height, smoothness, noiseAmp, noiseScale, seed]);

    return (
        <div style={UI_STYLES.container}>
            <div style={UI_STYLES.panel}>
                <div style={UI_STYLES.group}>
                    <label style={UI_STYLES.label}>WORLD SEED</label>
                    <div style={UI_STYLES.inputRow}>
                        <input type="number" value={seed} onChange={(e) => setSeed(parseInt(e.target.value) || 0)} style={UI_STYLES.input} />
                        <button onClick={() => setSeed(Math.floor(Math.random() * 99999))} style={UI_STYLES.buttonPrimary}>🎲</button>
                    </div>
                </div>

                <div style={UI_STYLES.group}>
                    <label style={UI_STYLES.label}>SPAWN COUNT: {spawnCount}</label>
                    <input type="range" min="1" max="50" value={spawnCount} onChange={(e) => setSpawnCount(parseInt(e.target.value))} style={UI_STYLES.slider} />
                </div>

                <div style={UI_STYLES.group}>
                    <label style={UI_STYLES.label}>GRID SIZE: {gridSize}px</label>
                    <input type="range" min="10" max="40" value={gridSize} onChange={(e) => setGridSize(parseInt(e.target.value))} style={UI_STYLES.slider} />
                </div>

                <div style={UI_STYLES.group}>
                    <label style={UI_STYLES.label}>SMOOTHNESS: {smoothness}</label>
                    <input type="range" min="0" max="5" value={smoothness} onChange={(e) => setSmoothness(parseInt(e.target.value))} style={UI_STYLES.slider} />
                </div>

                <div style={UI_STYLES.group}>
                    <label style={UI_STYLES.label}>DISTURBANCE (AMP): {noiseAmp}</label>
                    <input type="range" min="0" max="30" value={noiseAmp} onChange={(e) => setNoiseAmp(parseInt(e.target.value))} style={UI_STYLES.slider} />
                </div>

                <div style={UI_STYLES.group}>
                    <label style={UI_STYLES.label}>NOISE SCALE: {noiseScale.toFixed(3)}</label>
                    <input type="range" min="0.005" max="0.1" step="0.005" value={noiseScale} onChange={(e) => setNoiseScale(parseFloat(e.target.value))} style={UI_STYLES.slider} />
                </div>

                <div style={UI_STYLES.group}>
                    <button onClick={spawn} style={UI_STYLES.buttonAlt}>RESPAWN</button>
                    <button onClick={() => grow(1)} style={UI_STYLES.buttonPrimary}>GROW x1</button>
                    <button onClick={() => grow(10)} style={UI_STYLES.buttonPrimary}>GROW x10</button>
                    <button onClick={connectIslands} style={UI_STYLES.buttonAccent}>CONNECT ISLANDS</button>
                </div>

                <div style={UI_STYLES.group}>
                    <label style={UI_STYLES.label}>STATISTICS</label>
                    <p style={{ color: '#94a3b8', fontSize: '12px', margin: 0 }}>Cells: {filledHexes.size}</p>
                </div>
            </div>
            <canvas ref={canvasRef} width={width} height={height} style={UI_STYLES.canvas} />
        </div>
    );
}

const UI_STYLES = {
    container: { position: 'relative', width: '100%', height: '100%', background: '#0a0b12', overflow: 'hidden' } as CSSProperties,
    panel: { position: 'absolute', top: 20, left: 20, background: 'rgba(6, 11, 19, 0.95)', backdropFilter: 'blur(16px)', padding: '24px', borderRadius: '16px', color: 'white', fontFamily: '"Outfit", sans-serif', display: 'flex', flexDirection: 'column', gap: '20px', zIndex: 10, border: '1px solid rgba(59, 130, 246, 0.2)', width: '280px', boxShadow: '0 12px 40px rgba(0, 0, 0, 0.5)' } as CSSProperties,
    group: { display: 'flex', flexDirection: 'column', gap: '6px' } as CSSProperties,
    label: { fontSize: '11px', color: '#60a5fa', fontWeight: '700', letterSpacing: '1.5px', textTransform: 'uppercase' } as CSSProperties,
    inputRow: { display: 'flex', gap: '10px' } as CSSProperties,
    input: { background: '#0f172a', border: '1px solid #1e293b', color: 'white', padding: '8px 12px', borderRadius: '8px', width: '100%', fontSize: '14px', outline: 'none' } as CSSProperties,
    buttonPrimary: { background: '#3b82f6', border: 'none', color: 'white', padding: '10px 16px', borderRadius: '8px', cursor: 'pointer', fontWeight: 'bold', fontSize: '12px', flex: 1 } as CSSProperties,
    buttonAccent: { background: '#8b5cf6', border: 'none', color: 'white', padding: '10px 16px', borderRadius: '8px', cursor: 'pointer', fontWeight: 'bold', fontSize: '12px', width: '100%' } as CSSProperties,
    buttonAlt: { background: '#1e293b', border: '1px solid #334155', color: '#94a3b8', padding: '10px 16px', borderRadius: '8px', cursor: 'pointer', fontWeight: 'bold', fontSize: '12px', width: '100%' } as CSSProperties,
    slider: { width: '100%', accentColor: '#3b82f6', cursor: 'pointer' } as CSSProperties,
    canvas: { display: 'block', width: '100%', height: '100%', cursor: 'crosshair' } as CSSProperties
};
