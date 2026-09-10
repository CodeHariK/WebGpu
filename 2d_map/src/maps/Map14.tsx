import { useEffect, useRef, useState } from 'react';
import { Vector2 } from '../lib/Vector2';
import { Canvas } from '../lib/Canvas';
import {
    TILE_REGISTRY,
    type TileConfig,
    getNeighborPos,
    getHexCenter,
    getHexVertices,
    drawTileRoads
} from './Map14Logic';

const COLORS = {
    bg: '#0a0b12',
    accent: '#3b82f6',
    grid: 'rgba(255, 255, 255, 0.08)',
    text: '#94a3b8'
};

const Map14 = ({ width = 800, height = 800 }: { width?: number, height?: number }) => {
    const containerRef = useRef<HTMLDivElement>(null);
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const canvasInstanceRef = useRef<Canvas | null>(null);
    const [seed, setSeed] = useState(0);
    const [gridSize, setGridSize] = useState(30);
    const [collapsed, setCollapsed] = useState<Map<string, TileConfig>>(new Map());
    const [renderTrigger, setRenderTrigger] = useState(0);

    // Topological Generator: BFS propagation with exact edge constraint matching
    useEffect(() => {
        const map = new Map<string, TileConfig>();
        const queue: { q: number, r: number }[] = [{ q: 0, r: 0 }];
        const range = Math.ceil(Math.max(width, height) / (gridSize * Math.sqrt(3))) + 4;
        const MAX_CELLS = 5000;

        // Seed with a random active tile at origin
        const activeTiles = TILE_REGISTRY.filter(t => t.connections.length > 0);
        const initialTile = activeTiles[Math.floor(Math.random() * activeTiles.length)] || TILE_REGISTRY[0];
        map.set('0,0', initialTile);

        const processed = new Set<string>();
        processed.add('0,0');

        let head = 0;
        while (head < queue.length && map.size < MAX_CELLS) {
            const curr = queue[head++];

            for (let side = 0; side < 6; side++) {
                const neighbor = getNeighborPos(curr.q, curr.r, side);
                const nKey = `${neighbor.q},${neighbor.r}`;

                if (processed.has(nKey)) continue;

                // Check distance from origin in axial hex coordinates
                const hexDist = (Math.abs(neighbor.q) + Math.abs(neighbor.q + neighbor.r) + Math.abs(neighbor.r)) / 2;
                if (hexDist > range) {
                    processed.add(nKey);
                    continue;
                }

                // Check constraints from all adjacent cells
                const incomingSides: number[] = [];
                const requiredInactive: number[] = [];
                const openSides: number[] = [];

                for (let s = 0; s < 6; s++) {
                    const checkPos = getNeighborPos(neighbor.q, neighbor.r, s);
                    const checkTile = map.get(`${checkPos.q},${checkPos.r}`);

                    if (checkTile) {
                        const oppositeSide = (s + 3) % 6;
                        if (checkTile.sideHashes[oppositeSide]) {
                            incomingSides.push(s);
                        } else {
                            requiredInactive.push(s);
                        }
                    } else {
                        const checkDist = (Math.abs(checkPos.q) + Math.abs(checkPos.q + checkPos.r) + Math.abs(checkPos.r)) / 2;
                        if (checkDist > range) {
                            requiredInactive.push(s);
                        } else {
                            openSides.push(s);
                        }
                    }
                }

                // Find matching tiles in the registry
                const validOptions = TILE_REGISTRY.filter(tile => {
                    for (const reqActive of incomingSides) {
                        if (!tile.sideHashes[reqActive]) return false;
                    }
                    for (const reqInactive of requiredInactive) {
                        if (tile.sideHashes[reqInactive]) return false;
                    }
                    return true;
                });

                let picked: TileConfig;

                if (validOptions.length > 0) {
                    if (incomingSides.length > 0) {
                        // Has incoming road(s) - pick from valid matching options
                        picked = validOptions[Math.floor(Math.random() * validOptions.length)];
                    } else {
                        // No incoming roads - balance empty vs spawning new organic path loops
                        const activeOptions = validOptions.filter(t => t.connections.length > 0);
                        if (activeOptions.length > 0 && Math.random() < 0.3) {
                            picked = activeOptions[Math.floor(Math.random() * activeOptions.length)];
                        } else {
                            picked = validOptions.find(t => t.connections.length === 0) || validOptions[0];
                        }
                    }
                } else {
                    // Contradiction fallback: synthesize a perfect matching tile so no road is left dangling
                    const conns: number[][] = [];
                    const activeList = [...incomingSides];

                    // If odd number of incoming roads, route the remaining one to an open side
                    if (activeList.length % 2 !== 0 && openSides.length > 0) {
                        activeList.push(openSides[0]);
                    }

                    for (let i = 0; i + 1 < activeList.length; i += 2) {
                        conns.push([activeList[i], activeList[i + 1]]);
                    }

                    const sideHashes = [false, false, false, false, false, false];
                    conns.forEach(c => {
                        sideHashes[c[0]] = true;
                        sideHashes[c[1]] = true;
                    });

                    picked = {
                        name: 'MATCHING_AUTO',
                        connections: conns,
                        sideHashes
                    };
                }

                map.set(nKey, picked);
                processed.add(nKey);
                queue.push(neighbor);
            }
        }

        setCollapsed(map);
    }, [seed, gridSize, width, height]);

    // Initialize Canvas & Pan/Zoom interactions
    useEffect(() => {
        if (!canvasRef.current) return;
        const canvas = new Canvas(canvasRef.current);
        canvas.initInteractions({ onInteraction: () => setRenderTrigger(t => t + 1) });
        canvasInstanceRef.current = canvas;

        return () => {
            canvas.destroyInteractions();
        };
    }, [width, height]);

    // Render Loop
    useEffect(() => {
        const canvas = canvasInstanceRef.current;
        if (!canvas) return;

        canvas.clear();

        // Background
        canvas.rect(-width * 4, -height * 4, width * 8, height * 8, { fill: COLORS.bg });

        canvas.save();
        canvas.translate(new Vector2(width / 2, height / 2));

        for (const [key, tile] of collapsed.entries()) {
            const [q, r] = key.split(',').map(Number);
            const center = getHexCenter(q, r, gridSize);

            // 1. Visible Grid
            const hexV = getHexVertices(center, gridSize);
            canvas.polygon(hexV, { stroke: COLORS.grid, lineWidth: 1 });

            // 2. Connected Roads
            drawTileRoads(canvas.ctx, center, gridSize, tile, '#f8fafc', 0.15);
        }

        canvas.restore();
    }, [collapsed, gridSize, renderTrigger, width, height]);

    return (
        <div
            ref={containerRef}
            style={{
                width: '100%',
                height: '100%',
                position: 'relative',
                overflow: 'hidden',
                background: COLORS.bg,
                fontFamily: 'Outfit, sans-serif'
            }}
        >
            <canvas ref={canvasRef} width={width} height={height} style={{ display: 'block', width: '100%', height: '100%' }} />

            <div style={{ position: 'absolute', top: 20, left: 20, display: 'flex', gap: 10, zIndex: 10 }}>
                <button
                    onClick={() => setSeed(Math.random())}
                    style={{
                        background: COLORS.accent,
                        color: 'white',
                        border: 'none',
                        padding: '10px 20px',
                        borderRadius: 8,
                        cursor: 'pointer',
                        fontWeight: 600
                    }}
                >
                    RANDOMIZE
                </button>
                <div
                    style={{
                        padding: '10px 16px',
                        background: 'rgba(255,255,255,0.05)',
                        backdropFilter: 'blur(8px)',
                        borderRadius: 8,
                        display: 'flex',
                        alignItems: 'center',
                        gap: 10,
                        color: COLORS.text,
                        border: '1px solid rgba(255,255,255,0.1)'
                    }}
                >
                    <span style={{ fontSize: '0.85rem', fontWeight: 600 }}>SCALE</span>
                    <input
                        type="range"
                        min="15"
                        max="80"
                        value={gridSize}
                        onChange={(e) => setGridSize(Number(e.target.value))}
                        style={{ cursor: 'pointer', accentColor: COLORS.accent }}
                    />
                </div>
            </div>

            <div style={{ position: 'absolute', bottom: 20, right: 20, textAlign: 'right', pointerEvents: 'none' }}>
                <h1 style={{ color: 'white', margin: 0, fontSize: '1.8rem', fontWeight: 800, letterSpacing: '1px' }}>
                    TRUCHET MAP
                </h1>
                <p style={{ color: COLORS.text, margin: 0, fontSize: '0.85rem' }}>
                    VALIDATED TOPOLOGICAL NETWORK
                </p>
            </div>
        </div>
    );
};

export default Map14;
