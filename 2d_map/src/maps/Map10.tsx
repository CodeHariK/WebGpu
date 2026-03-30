import { useEffect, useRef, useState, useCallback } from 'react';
import type { CSSProperties } from 'react';
import { Canvas } from '../lib/Canvas';
import { Vector2 } from '../lib/Vector2';
import { Spline } from '../lib/Spline';
import Delaunator from 'delaunator';

// --- VORONOI HELPERS ---

interface VoronoiCell {
    index: number;
    point: Vector2;
    vertices: Vector2[];
    neighborIndices: number[];
    color: string | null;
    minDepth: number;
    epicenterIndex: number;
}

function getCircumcenter(a: Vector2, b: Vector2, c: Vector2): Vector2 {
    const ad = a.x * a.x + a.y * a.y;
    const bd = b.x * b.x + b.y * b.y;
    const cd = c.x * c.x + c.y * c.y;
    const D = 2 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
    return new Vector2(
        1 / D * (ad * (b.y - c.y) + bd * (c.y - a.y) + cd * (a.y - b.y)),
        1 / D * (ad * (c.x - b.x) + bd * (a.x - c.x) + cd * (b.x - a.x))
    );
}

function mulberry32(a: number) {
    return function () {
        let t = a += 0x6D2B79F5;
        t = Math.imul(t ^ t >>> 15, t | 1);
        t ^= t + Math.imul(t ^ t >>> 7, t | 61);
        return ((t ^ t >>> 14) >>> 0) / 4294967296;
    }
}

const TERRITORY_COLORS = [
    '#3b82f6', '#8b5cf6', '#ec4899', '#10b981', '#f59e0b', '#ef4444', '#06b6d4', '#f97316'
];

export default function Map10({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const canvasInstanceRef = useRef<Canvas | null>(null);

    const [seed, setSeed] = useState(42);
    const [pointCount, setPointCount] = useState(400);
    const [numEpicenters, setNumEpicenters] = useState(5);
    const [falloffLimit, setFalloffLimit] = useState(8);
    const [minSpacing, setMinSpacing] = useState(180);
    const [showGeometry, setShowGeometry] = useState(false);
    const [showRings, setShowRings] = useState(true);
    const [showHighways, setShowHighways] = useState(true);

    const [cells, setCells] = useState<VoronoiCell[]>([]);
    const [ringRoads, setRingRoads] = useState<Vector2[][]>([]);
    const [highwayRoads, setHighwayRoads] = useState<Vector2[][]>([]);
    const [renderTrigger, setRenderTrigger] = useState(0);

    const generateVoronoi = useCallback(() => {
        const rand = mulberry32(seed);

        // 1. Generate Points using Jittered Grid
        const points: Vector2[] = [];
        const coords: number[] = [];
        const gridRes = Math.ceil(Math.sqrt(pointCount));
        const spacingX = width / gridRes;
        const spacingY = height / gridRes;

        for (let gy = 0; gy < gridRes; gy++) {
            for (let gx = 0; gx < gridRes; gx++) {
                if (points.length >= pointCount) break;
                const x = (gx + 0.5) * spacingX + (rand() - 0.5) * spacingX * 0.8;
                const y = (gy + 0.5) * spacingY + (rand() - 0.5) * spacingY * 0.8;
                points.push(new Vector2(x, y));
                coords.push(x, y);
            }
        }

        // 2. Triangulation
        const delaunay = new Delaunator(coords);
        const { triangles, halfedges } = delaunay;

        // 3. Cells
        const cellMap = new Map<number, VoronoiCell>();
        points.forEach((p, i) => cellMap.set(i, { index: i, point: p, vertices: [], neighborIndices: [], color: null, minDepth: Infinity, epicenterIndex: -1 }));

        const circumcenters: Vector2[] = [];
        for (let i = 0; i < triangles.length; i += 3) {
            circumcenters.push(getCircumcenter(points[triangles[i]], points[triangles[i + 1]], points[triangles[i + 2]]));
        }

        for (let i = 0; i < triangles.length; i++) {
            const pIdx = triangles[i];
            const cell = cellMap.get(pIdx)!;
            cell.vertices.push(circumcenters[Math.floor(i / 3)]);
            const oppHalf = halfedges[i];
            if (oppHalf !== -1) cell.neighborIndices.push(triangles[oppHalf]);
        }

        cellMap.forEach(cell => {
            const p = cell.point;
            cell.vertices.sort((a, b) => Math.atan2(a.y - p.y, a.x - p.x) - Math.atan2(b.y - p.y, b.x - p.x));
            cell.vertices = cell.vertices.filter((v, idx, self) => idx === self.findIndex(ov => Math.abs(ov.x - v.x) < 0.1 && Math.abs(ov.y - v.y) < 0.1));
            cell.neighborIndices = cell.neighborIndices.filter(nIdx => nIdx !== -1 && nIdx < points.length);
        });

        // 5. Distributed Epicenters
        const cellArray = Array.from(cellMap.values());
        const epicenters: number[] = [];
        const queue: [number, number, number][] = [];
        let curMinDist = minSpacing;
        let attempts = 0;

        while (epicenters.length < Math.min(numEpicenters, cellArray.length)) {
            const idx = Math.floor(rand() * cellArray.length);
            const candidate = cellArray[idx];
            if (!epicenters.some(eIdx => Vector2.dist(candidate.point, cellArray[eIdx].point) < curMinDist)) {
                epicenters.push(idx);
                cellArray[idx].color = TERRITORY_COLORS[Math.floor(rand() * TERRITORY_COLORS.length)];
                cellArray[idx].minDepth = 0;
                cellArray[idx].epicenterIndex = idx;
                queue.push([idx, 0, idx]);
                attempts = 0;
            } else if (++attempts > 50) { curMinDist *= 0.9; attempts = 0; }
        }

        // 6. BFS Territory Expansion
        const bfsQueue = [...queue];
        while (bfsQueue.length > 0) {
            const [currIdx, depth, eIdx] = bfsQueue.shift()!;
            const currCell = cellMap.get(currIdx)!;
            if (depth >= falloffLimit) continue;
            currCell.neighborIndices.forEach(nIdx => {
                const neighbor = cellMap.get(nIdx);
                if (neighbor && neighbor.minDepth === Infinity) {
                    neighbor.minDepth = depth + 1;
                    neighbor.color = currCell.color;
                    neighbor.epicenterIndex = eIdx;
                    bfsQueue.push([nIdx, depth + 1, eIdx]);
                }
            });
        }

        // 7. Ring Roads
        const newRingRoads: Vector2[][] = [];
        epicenters.forEach(seedIdx => {
            const ringDepths = [2, 4, 6].filter(d => d < falloffLimit);
            ringDepths.forEach(d => {
                const candidates = cellArray.filter(c => c.epicenterIndex === seedIdx && c.minDepth === d);
                if (candidates.length < 5) return;
                const candidateIndices = new Set(candidates.map(c => c.index));
                let startIdx = candidates[0].index;
                let currentIdx = startIdx;
                const chain: number[] = [startIdx];
                const visited = new Set<number>([startIdx]);
                for (let i = 0; i < candidates.length + 1; i++) {
                    const neighbors = cellMap.get(currentIdx)!.neighborIndices;
                    const next = neighbors.find(nIdx => candidateIndices.has(nIdx) && (nIdx === startIdx ? (chain.length >= 4) : !visited.has(nIdx)));
                    if (next === startIdx && chain.length >= 4) {
                        const loopPoints = chain.map(idx => cellMap.get(idx)!.point);
                        loopPoints.push(cellMap.get(startIdx)!.point);
                        newRingRoads.push(new Spline(loopPoints).getPath(12));
                        break;
                    }
                    if (next && !visited.has(next)) { currentIdx = next; chain.push(currentIdx); visited.add(currentIdx); } else { break; }
                }
            });
        });

        // 8. Non-Overlapping Inter-Cluster Highways
        const newHighwayRoads: Vector2[][] = [];
        const globalHighwayOccupied = new Set<number>();
        const connectedPairs = new Set<string>();

        epicenters.forEach(startIdx => {
            const neighbors = epicenters
                .filter(eIdx => eIdx !== startIdx)
                .sort((a, b) => Vector2.dist(cellArray[startIdx].point, cellArray[a].point) - Vector2.dist(cellArray[startIdx].point, cellArray[b].point))
                .slice(0, 3); // Slightly connect more neighbors

            neighbors.forEach(targetIdx => {
                const pairKey = [startIdx, targetIdx].sort().join('-');
                if (connectedPairs.has(pairKey)) return;
                connectedPairs.add(pairKey);

                // Global BFS with Exclusion Logic
                let finalPath: number[] | null = null;

                const runBFS = (useExclusion: boolean) => {
                    const q: [number, number[]][] = [[startIdx, [startIdx]]];
                    const localVisited = new Set<number>([startIdx]);

                    while (q.length > 0) {
                        const [curr, p] = q.shift()!;
                        if (curr === targetIdx) return p;

                        const neighbors = cellMap.get(curr)!.neighborIndices;
                        for (const nIdx of neighbors) {
                            const isAvailable = nIdx === targetIdx || (!localVisited.has(nIdx) && (!useExclusion || !globalHighwayOccupied.has(nIdx)));
                            if (isAvailable) {
                                localVisited.add(nIdx);
                                q.push([nIdx, [...p, nIdx]]);
                            }
                        }
                    }
                    return null;
                };

                // Attempt 1: Strict Exclusion
                finalPath = runBFS(true);

                // Attempt 2: Fallback (Ignore exclusion if no path exists)
                if (!finalPath) {
                    finalPath = runBFS(false);
                }

                if (finalPath && finalPath.length >= 2) {
                    newHighwayRoads.push(new Spline(finalPath.map(idx => cellMap.get(idx)!.point)).getPath(10));
                    // Mark all nodes in this path (except the start/end hubs) as occupied
                    finalPath.forEach((idx, i) => {
                        if (i !== 0 && i !== finalPath!.length - 1) {
                            globalHighwayOccupied.add(idx);
                        }
                    });
                }
            });
        });

        setCells(cellArray);
        setRingRoads(newRingRoads);
        setHighwayRoads(newHighwayRoads);
    }, [seed, pointCount, numEpicenters, falloffLimit, minSpacing, width, height]);

    useEffect(() => { generateVoronoi(); }, [generateVoronoi]);

    // Draw Loop
    useEffect(() => {
        const canvasEl = canvasRef.current;
        if (!canvasEl) return;
        if (!canvasInstanceRef.current) {
            const instance = new Canvas(canvasEl);
            instance.initInteractions({ onInteraction: () => setRenderTrigger(t => t + 1) });
            canvasInstanceRef.current = instance;
        }
        const canvas = canvasInstanceRef.current;
        canvas.clear();
        canvas.rect(-1000, -1000, width + 2000, height + 2000, { fill: '#0a0b12' });
        canvas.grid(40, 'rgba(59, 130, 246, 0.05)');

        cells.forEach(cell => {
            if (cell.vertices.length < 3) return;
            const alpha = Math.max(0, 1 - (cell.minDepth / falloffLimit));
            const fill = cell.color && alpha > 0 ? cell.color + Math.round(alpha * 255).toString(16).padStart(2, '0') : 'transparent';
            canvas.polygon(cell.vertices, { fill: fill, stroke: '#000000', lineWidth: 1 });
        });

        if (showRings) {
            ringRoads.forEach(path => {
                canvas.linePath(path, { stroke: '#fff', lineWidth: 1, alpha: 0.7 });
                canvas.linePath(path, { stroke: '#fff', lineWidth: 3, alpha: 0.1 });
            });
        }

        if (showHighways) {
            highwayRoads.forEach(path => {
                canvas.linePath(path, { stroke: 'cyan', lineWidth: 5, alpha: 0.4 });
                canvas.linePath(path, { stroke: '#fff', lineWidth: 2, alpha: 0.8 });
            });
        }

        if (showGeometry) {
            cells.forEach(cell => { if (cell.minDepth === 0) canvas.circle(cell.point, 5, { fill: '#fff', stroke: 'cyan', lineWidth: 2 }); });
        }
    }, [cells, ringRoads, highwayRoads, width, height, showGeometry, showRings, showHighways, falloffLimit, renderTrigger]);

    return (
        <div style={UI_STYLES.container}>
            <div style={UI_STYLES.panel}>
                <div style={UI_STYLES.group}><label style={UI_STYLES.label}>SEED</label><div style={UI_STYLES.inputRow}><input type="number" value={seed} onChange={(e) => setSeed(parseInt(e.target.value) || 0)} style={UI_STYLES.input} /><button onClick={() => setSeed(Math.floor(Math.random() * 99999))} style={UI_STYLES.buttonPrimary}>🎲</button></div></div>
                <div style={UI_STYLES.group}><label style={UI_STYLES.label}>DETAIL (POINTS): {pointCount}</label><input type="range" min="100" max="1000" value={pointCount} onChange={(e) => setPointCount(parseInt(e.target.value))} style={UI_STYLES.slider} /></div>
                <div style={UI_STYLES.group}><label style={UI_STYLES.label}>EPICENTERS: {numEpicenters}</label><input type="range" min="1" max="15" value={numEpicenters} onChange={(e) => setNumEpicenters(parseInt(e.target.value))} style={UI_STYLES.slider} /></div>
                <div style={UI_STYLES.group}><label style={UI_STYLES.label}>FALLOFF: {falloffLimit}</label><input type="range" min="2" max="25" value={falloffLimit} onChange={(e) => setFalloffLimit(parseInt(e.target.value))} style={UI_STYLES.slider} /></div>
                <div style={UI_STYLES.group}><label style={UI_STYLES.label}>MIN SPACING: {minSpacing}</label><input type="range" min="50" max="400" value={minSpacing} onChange={(e) => setMinSpacing(parseInt(e.target.value))} style={UI_STYLES.slider} /></div>

                <div style={UI_STYLES.group}>
                    <label style={UI_STYLES.label}>VISUALS</label>
                    <div style={UI_STYLES.inputRow}>
                        <button onClick={() => setShowRings(!showRings)} style={showRings ? UI_STYLES.buttonPrimary : UI_STYLES.buttonAlt}>RINGS</button>
                        <button onClick={() => setShowHighways(!showHighways)} style={showHighways ? UI_STYLES.buttonPrimary : UI_STYLES.buttonAlt}>HIGHWAY</button>
                        <button onClick={() => setShowGeometry(!showGeometry)} style={showGeometry ? UI_STYLES.buttonPrimary : UI_STYLES.buttonAlt}>SEEDS</button>
                    </div>
                </div>
            </div>
            <canvas ref={canvasRef} width={width} height={height} style={UI_STYLES.canvas} />
        </div>
    );
}

const UI_STYLES = {
    container: { position: 'relative', width: '100%', height: '100%', background: '#0e1111', overflow: 'hidden' } as CSSProperties,
    panel: { position: 'absolute', top: 20, left: 20, background: 'rgba(6, 11, 19, 0.95)', backdropFilter: 'blur(16px)', padding: '24px', borderRadius: '16px', color: 'white', fontFamily: '"Outfit", sans-serif', display: 'flex', flexDirection: 'column', gap: '20px', zIndex: 10, border: '1px solid rgba(59, 130, 246, 0.2)', width: '260px', boxShadow: '0 12px 40px rgba(0, 0, 0, 0.5)' } as CSSProperties,
    group: { display: 'flex', flexDirection: 'column', gap: '8px' } as CSSProperties,
    label: { fontSize: '11px', color: '#60a5fa', fontWeight: '700', letterSpacing: '1.5px', textTransform: 'uppercase' } as CSSProperties,
    inputRow: { display: 'flex', gap: '10px' } as CSSProperties,
    input: { background: '#0f172a', border: '1px solid #1e293b', color: 'white', padding: '8px 12px', borderRadius: '8px', width: '100%', fontSize: '14px', outline: 'none' } as CSSProperties,
    buttonPrimary: { background: '#3b82f6', border: 'none', color: 'white', padding: '10px 16px', borderRadius: '8px', cursor: 'pointer', fontWeight: 'bold', fontSize: '12px', flex: 1 } as CSSProperties,
    buttonAlt: { background: '#1e293b', border: '1px solid #334155', color: '#94a3b8', padding: '10px 16px', borderRadius: '8px', cursor: 'pointer', fontWeight: 'bold', fontSize: '12px', flex: 1 } as CSSProperties,
    slider: { width: '100%', accentColor: '#3b82f6', cursor: 'pointer' } as CSSProperties,
    canvas: { display: 'block', width: '100%', height: '100%', cursor: 'crosshair' } as CSSProperties
};
