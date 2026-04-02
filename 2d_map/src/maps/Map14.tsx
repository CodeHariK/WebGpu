import { useEffect, useRef, useState } from 'react';
import { Vector2 } from '../lib/Vector2';
import { Canvas } from '../lib/Canvas';
import { TILE_REGISTRY, type TileConfig, getNeighborPos } from './Map14Logic';

const COLORS = {
    bg: '#0a0b12',
    accent: '#3b82f6',
    grid: '#1e293b',
    text: '#94a3b8'
};

const Map14 = ({ width = 800, height = 800 }: { width?: number, height?: number }) => {
    const containerRef = useRef<HTMLDivElement>(null);
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const [seed, setSeed] = useState(0);
    const [gridSize, setGridSize] = useState(30);
    const [collapsed, setCollapsed] = useState<Map<string, TileConfig>>(new Map());

    const getHexCenter = (q: number, r: number, size: number) => {
        const x = size * (Math.sqrt(3) * q + Math.sqrt(3) / 2 * r);
        const y = size * (3 / 2 * r);
        return new Vector2(x, y);
    };

    const getHexVertices = (center: Vector2, size: number): Vector2[] => {
        const vertices: Vector2[] = [];
        for (let i = 0; i < 6; i++) {
            const angle = (i * 60 + 30) * (Math.PI / 180);
            vertices.push(new Vector2(
                center.x + size * Math.cos(angle),
                center.y + size * Math.sin(angle)
            ));
        }
        return vertices;
    };

    const drawTileRoads = (ctx: CanvasRenderingContext2D, center: Vector2, size: number, tile: TileConfig) => {
        const v = getHexVertices(center, size);
        ctx.lineCap = 'round';
        ctx.lineWidth = size * 0.15;
        ctx.strokeStyle = '#f8fafc';

        tile.connections.forEach(conn => {
            const e1 = conn[0];
            const e2 = conn[1];
            const m1 = new Vector2((v[e1].x + v[(e1 + 1) % 6].x) / 2, (v[e1].y + v[(e1 + 1) % 6].y) / 2);
            const m2 = new Vector2((v[e2].x + v[(e2 + 1) % 6].x) / 2, (v[e2].y + v[(e2 + 1) % 6].y) / 2);
            const d1 = center.clone().sub(m1).normalize();
            const d2 = center.clone().sub(m2).normalize();
            const dist = Math.abs(e1 - e2) > 3 ? 6 - Math.abs(e1 - e2) : Math.abs(e1 - e2);
            let sScale = dist === 1 ? size * 0.35 : (dist === 2 ? size * 0.55 : size * 0.7);
            const cp1 = m1.clone().add(d1.mult(sScale));
            const cp2 = m2.clone().add(d2.mult(sScale));
            ctx.beginPath();
            ctx.moveTo(m1.x, m1.y);
            ctx.bezierCurveTo(cp1.x, cp1.y, cp2.x, cp2.y, m2.x, m2.y);
            ctx.stroke();
        });
    };

    // Topological Filler: Outward propagation with constraint checks
    useEffect(() => {
        const map = new Map<string, TileConfig>();
        const queue: { q: number, r: number }[] = [{ q: 0, r: 0 }];
        const range = Math.min(60, Math.ceil(window.innerHeight / (gridSize * 1.5)) + 2);
        const MAX_CELLS = 5000;

        // 1. Initial Seed Tile
        const randomIndex = Math.floor(Math.random() * TILE_REGISTRY.length);
        map.set("0,0", TILE_REGISTRY[randomIndex]);

        const processed = new Set<string>();
        processed.add("0,0");

        // High-Performance BFS with index-based traversal
        let head = 0;
        while (head < queue.length && map.size < MAX_CELLS) {
            const curr = queue[head++];
            
            for (let side = 0; side < 6; side++) {
                const neighbor = getNeighborPos(curr.q, curr.r, side);
                const nKey = `${neighbor.q},${neighbor.r}`;
                
                if (processed.has(nKey)) continue;
                if (Math.abs(neighbor.q) > range || Math.abs(neighbor.r) > range) {
                    processed.add(nKey);
                    continue;
                }
                
                // Which tiles from the registry fit the neighbors of 'nKey'?
                const validOptions = TILE_REGISTRY.filter(tile => {
                    for (let s = 0; s < 6; s++) {
                        const checkPos = getNeighborPos(neighbor.q, neighbor.r, s);
                        const checkTile = map.get(`${checkPos.q},${checkPos.r}`);
                        
                        if (checkTile) {
                            // 1. Check existing neighbors (Topological Matching)
                            if (tile.sideHashes[s] !== checkTile.sideHashes[(s + 3) % 6]) {
                                return false;
                            }
                        } else if (Math.abs(checkPos.q) > range || Math.abs(checkPos.r) > range) {
                            // 2. Check Virtual Borders (Must not have road pointing to Air)
                            if (tile.sideHashes[s] === true) {
                                return false;
                            }
                        }
                    }
                    return true;
                });

                if (validOptions.length > 0) {
                    const picked = validOptions[Math.floor(Math.random() * validOptions.length)];
                    map.set(nKey, picked);
                    processed.add(nKey);
                    queue.push(neighbor);
                } else {
                    processed.add(nKey); // Terminate this branch
                }
            }
        }
        
        setCollapsed(map);
    }, [seed, gridSize]);

    useEffect(() => {
        if (!canvasRef.current) return;
        const canvas = new Canvas(canvasRef.current);
        const width = canvasRef.current.width;
        const height = canvasRef.current.height;

        const DrawLoop = () => {
            canvas.ctx.setTransform(1, 0, 0, 1, 0, 0);
            canvas.ctx.fillStyle = COLORS.bg;
            canvas.ctx.fillRect(0, 0, width, height);

            canvas.clear();
            canvas.ctx.save();
            canvas.ctx.translate(width / 2, height / 2);

            for (const [key, tile] of collapsed.entries()) {
                const [q, r] = key.split(',').map(Number);
                const center = getHexCenter(q, r, gridSize);
                if (Math.abs(center.x) > width / 2 + 100 || Math.abs(center.y) > height / 2 + 100) continue;

                // 1. Visible Grid
                const hexV = getHexVertices(center, gridSize);
                canvas.polygon(hexV, { stroke: 'rgba(255,255,255,0.15)', lineWidth: 1.2 });

                // 2. Immediate Roads
                drawTileRoads(canvas.ctx, center, gridSize, tile);
            }

            canvas.ctx.restore();
        };

        DrawLoop();
    }, [collapsed, gridSize]);

    return (
        <div ref={containerRef} style={{ width: '100%', height: '100%', position: 'relative', overflow: 'hidden', background: COLORS.bg, fontFamily: 'Outfit, sans-serif' }}>
            <canvas ref={canvasRef} width={width} height={height} />

            <div style={{ position: 'absolute', top: 20, left: 20, display: 'flex', gap: 10, zIndex: 10 }}>
                <button 
                    onClick={() => setSeed(Math.random())}
                    style={{ background: COLORS.accent, color: 'white', border: 'none', padding: '10px 20px', borderRadius: 8, cursor: 'pointer', fontWeight: 600 }}
                >
                    RANDOMIZE
                </button>
                <div style={{ padding: '10px', background: 'rgba(255,255,255,0.05)', borderRadius: 8, display: 'flex', alignItems: 'center', gap: 10, color: COLORS.text }}>
                    <span>SCALE</span>
                    <input 
                        type="range" min="15" max="100" value={gridSize} 
                        onChange={(e) => setGridSize(Number(e.target.value))} 
                        style={{ cursor: 'pointer' }}
                    />
                </div>
            </div>

            <div style={{ position: 'absolute', bottom: 20, right: 20, textAlign: 'right', pointerEvents: 'none' }}>
                <h1 style={{ color: 'white', margin: 0, fontSize: '2rem', fontWeight: 800 }}>TRUCHET MAP</h1>
                <p style={{ color: COLORS.text, margin: 0 }}>VALIDATED TOPOLOGICAL NETWORK</p>
            </div>
        </div>
    );
};

export default Map14;
