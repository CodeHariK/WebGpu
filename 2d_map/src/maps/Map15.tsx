import { useEffect, useRef, useState, useCallback } from 'react';
import { Vector2 } from '../lib/Vector2';
import { Canvas } from '../lib/Canvas';
import { generateNodes, generateEdges, removeIsolatedNodes, subdivideEdges, generateHierarchy, findFaces, type Node, type Edge, type GenConfig } from './Map15Logic';

const COLORS = {
    bg: '#050a15',
    grid: '#111b33',
    road: '#38bdf8',
    roadBlue: '#3b82f6',
    roadGlow: 'rgba(56, 189, 248, 0.2)',
    yellow: '#fbbf24',
    red: '#ef4444',
    orange: '#f97316',
    text: '#94a3b8',
    accent: '#0ea5e9'
};

export default function Map15({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const canvasInstanceRef = useRef<Canvas | null>(null);
    
    const [nodes, setNodes] = useState<Node[]>([]);
    const [edges, setEdges] = useState<Edge[]>([]);
    const [faces, setFaces] = useState<number[][]>([]);
    const [seed, setSeed] = useState(Math.random());
    
    // Generation Config
    const [splitIter, setSplitIter] = useState(3.5);
    const [minDist] = useState(30);
    const [maxDist] = useState(60);
    const [connRadius, setConnRadius] = useState(120);
    const [minRoadLen, setMinRoadLen] = useState(30);
    const [maxRoadLen, setMaxRoadLen] = useState(120);
    const [minAngle, setMinAngle] = useState(60);
    const [showNodes, setShowNodes] = useState(false);
    const [showFaces, setShowFaces] = useState(false);
    const [isGenerating, setIsGenerating] = useState(false);
    const [depth, setDepth] = useState(1);

    const generate = useCallback(() => {
        setIsGenerating(true);
        const config: GenConfig = {
            minDist,
            maxDist,
            connectionRadius: connRadius,
            minRoadLen,
            maxRoadLen,
            minAngle,
            maxDegree: 4,
            splitChance: 1.0
        };

        const seeds = [
            { pos: new Vector2(0, 0), s: splitIter },
            { pos: new Vector2(200, 0), s: splitIter * 0.5 },
            { pos: new Vector2(-200, 0), s: splitIter * 0.5 },
            { pos: new Vector2(0, 200), s: splitIter * 0.5 },
            { pos: new Vector2(0, -200), s: splitIter * 0.5 },
        ];

        let genNodes = generateNodes(seeds, config, seed);
        let genEdges = generateEdges(genNodes, config, [], 0, seed + 50);
        
        // --- Part 2 Architecture ---
        // 1. Subdivide
        genEdges = subdivideEdges(genNodes, genEdges, connRadius * 0.5);
        
        // 2. Conquer (Recursion)
        const hierarchy = generateHierarchy(genNodes, genEdges, config, depth, seed);
        genNodes = hierarchy.nodes;
        genEdges = hierarchy.edges;

        setNodes(removeIsolatedNodes(genNodes, genEdges));
        setEdges(genEdges);
        setFaces(findFaces(genNodes, genEdges));
        setIsGenerating(false);
    }, [splitIter, minDist, maxDist, connRadius, minRoadLen, maxRoadLen, minAngle, seed, depth]);

    useEffect(() => {
        generate();
    }, [generate]);

    useEffect(() => {
        if (!canvasRef.current) return;
        
        const canvas = new Canvas(canvasRef.current);
        canvasInstanceRef.current = canvas;
        canvas.initInteractions({ onInteraction: () => render() });
        
        // Initial center
        canvas.offset = new Vector2(width / 2, height / 2);
        canvas.zoom = 1.0;

        const render = () => {
            canvas.clear();
            canvas.grid(100, COLORS.grid);
            canvas.grid(20, 'rgba(17, 27, 51, 0.3)');

            // Draw Faces
            if (showFaces) {
                faces.forEach((face, i) => {
                    const poly = face.map(id => nodes.find(n => n.id === id)?.pos).filter(Boolean) as Vector2[];
                    if (poly.length > 2) {
                        const hue = (i * 137.5) % 360;
                        canvas.polygon(poly, { fill: `hsla(${hue}, 70%, 50%, 0.1)`, stroke: `hsla(${hue}, 70%, 50%, 0.3)`, lineWidth: 1 });
                    }
                });
            }

            // Draw Edges
            edges.forEach(edge => {
                const u = nodes.find(n => n.id === edge.u);
                const v = nodes.find(n => n.id === edge.v);
                if (u && v) {
                    if (edge.level === 0) {
                        // Level 0: Blue Roads
                        canvas.line(u.pos, v.pos, { stroke: COLORS.roadBlue, lineWidth: 2.5, alpha: 0.8 });
                    } else if (edge.level === 1) {
                        // Level 1: Red Roads
                        canvas.line(u.pos, v.pos, { stroke: COLORS.red, lineWidth: 1.5, alpha: 0.6 });
                    } else if (edge.level === 2) {
                        // Level 2: Orange Roads
                        canvas.line(u.pos, v.pos, { stroke: COLORS.orange, lineWidth: 1.0, alpha: 0.5 });
                    }
                }
            });

            // Draw Nodes
            nodes.forEach(node => {
                let color = COLORS.yellow;
                let size = 4;
                if (node.level === 1) { color = COLORS.red; size = 3.5; }
                else if (node.level === 2) { color = COLORS.orange; size = 2.5; }
                
                // Show all midpoint nodes by default to visualize the anchor points
                canvas.circle(node.pos, size, { fill: color });
            });
        };

        render();

        return () => {
            canvas.destroyInteractions();
        };
    }, [nodes, edges, showNodes, width, height]);

    return (
        <div style={{ width: '100%', height: '100%', position: 'relative', background: COLORS.bg, overflow: 'hidden', fontFamily: 'Inter, sans-serif' }}>
            <canvas 
                ref={canvasRef} 
                width={width} 
                height={height} 
                style={{ cursor: 'crosshair', display: 'block' }}
            />

            {/* UI Overlay */}
            <div style={{ position: 'absolute', top: 20, left: 20, zIndex: 10, display: 'flex', flexDirection: 'column', gap: 15, width: 280, color: 'white' }}>
                <div style={{ background: 'rgba(10, 15, 30, 0.85)', padding: '20px', borderRadius: '12px', border: '1px solid rgba(255,255,255,0.1)', backdropFilter: 'blur(8px)' }}>
                    <h2 style={{ margin: '0 0 15px 0', fontSize: '1.2rem', color: COLORS.accent, fontWeight: 700, letterSpacing: '1px' }}>STREET GROWTH</h2>
                    
                    <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
                        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.8rem', opacity: 0.7 }}>
                                <span>ITERATIONS</span>
                                <span>{splitIter.toFixed(1)}</span>
                            </div>
                            <input type="range" min="1" max="5" step="0.1" value={splitIter} onChange={e => setSplitIter(parseFloat(e.target.value))} />
                        </div>

                        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.8rem', opacity: 0.7 }}>
                                <span>CONN. RADIUS</span>
                                <span>{connRadius}px</span>
                            </div>
                            <input type="range" min="40" max="150" value={connRadius} onChange={e => setConnRadius(parseInt(e.target.value))} />
                        </div>

                        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.8rem', opacity: 0.7 }}>
                                <span>MIN ROAD LEN</span>
                                <span>{minRoadLen}px</span>
                            </div>
                            <input type="range" min="10" max="100" value={minRoadLen} onChange={e => setMinRoadLen(parseInt(e.target.value))} />
                        </div>

                        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.8rem', opacity: 0.7 }}>
                                <span>MAX ROAD LEN</span>
                                <span>{maxRoadLen}px</span>
                            </div>
                            <input type="range" min="40" max="250" value={maxRoadLen} onChange={e => setMaxRoadLen(parseInt(e.target.value))} />
                        </div>

                        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.8rem', opacity: 0.7 }}>
                                <span>MIN ANGLE</span>
                                <span>{minAngle}°</span>
                            </div>
                            <input type="range" min="30" max="90" value={minAngle} onChange={e => setMinAngle(parseInt(e.target.value))} />
                        </div>

                        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.8rem', opacity: 0.7 }}>
                                <span>RECURSION DEPTH</span>
                                <span>{depth}</span>
                            </div>
                            <input type="range" min="0" max="3" step="1" value={depth} onChange={e => setDepth(parseInt(e.target.value))} />
                        </div>

                        <label style={{ display: 'flex', alignItems: 'center', gap: 10, cursor: 'pointer', fontSize: '0.8rem', opacity: 0.8 }}>
                            <input type="checkbox" checked={showNodes} onChange={e => setShowNodes(e.target.checked)} />
                            SHOW NODES
                        </label>

                        <label style={{ display: 'flex', alignItems: 'center', gap: 10, cursor: 'pointer', fontSize: '0.8rem', opacity: 0.8 }}>
                            <input type="checkbox" checked={showFaces} onChange={e => setShowFaces(e.target.checked)} />
                            DEBUG_FACES
                        </label>

                        <div style={{ display: 'flex', flexDirection: 'column', gap: 4, marginTop: 5 }}>
                            <div style={{ fontSize: '0.7rem', opacity: 0.6, letterSpacing: '1px' }}>RNG SEED</div>
                            <input 
                                type="text" 
                                value={seed} 
                                onChange={e => setSeed(parseFloat(e.target.value) || 0)}
                                style={{ 
                                    background: 'rgba(255,255,255,0.05)', 
                                    border: '1px solid rgba(255,255,255,0.1)', 
                                    padding: '8px', 
                                    borderRadius: '6px', 
                                    color: COLORS.accent, 
                                    fontSize: '0.8rem',
                                    outline: 'none',
                                    fontFamily: 'monospace'
                                }}
                            />
                        </div>

                        <button 
                            onClick={() => !isGenerating && setSeed(Math.random())}
                            disabled={isGenerating}
                            style={{ 
                                marginTop: 10,
                                background: isGenerating ? '#1e293b' : COLORS.accent,
                                color: 'white',
                                border: 'none',
                                padding: '12px',
                                borderRadius: '8px',
                                fontWeight: 700,
                                cursor: isGenerating ? 'not-allowed' : 'pointer',
                                transition: 'all 0.2s',
                                fontSize: '0.9rem',
                                opacity: isGenerating ? 0.5 : 1
                            }}
                        >
                            {isGenerating ? 'GENERATING...' : 'BOOM: RECURSE!'}
                        </button>
                    </div>
                </div>

                <div style={{ background: 'rgba(10, 15, 30, 0.6)', padding: '10px 15px', borderRadius: '12px', fontSize: '0.75rem', color: COLORS.text, border: '1px solid rgba(255,255,255,0.05)' }}>
                    {nodes.length} Nodes • {edges.length} Edges<br/>
                    Middle click to Pan • Scroll to Zoom
                </div>
            </div>
            
            <div style={{ position: 'absolute', bottom: 30, right: 30, textAlign: 'right', pointerEvents: 'none' }}>
                <div style={{ fontSize: '3rem', fontWeight: 900, color: 'white', opacity: 0.1, lineHeight: 1 }}>BLUEPRINT</div>
                <div style={{ fontSize: '1rem', color: COLORS.accent, opacity: 0.5, letterSpacing: '4px' }}>MAP_SYSTEM_15</div>
            </div>
        </div>
    );
}

