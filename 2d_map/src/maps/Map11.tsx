import { useEffect, useRef, useState } from 'react';
import type { CSSProperties } from 'react';
import { Canvas } from '../lib/Canvas';
import { Vector2 } from '../lib/Vector2';

// --- STATIC SHIFFMAN METABALL ENGINE ---

interface Bubble {
    center: Vector2;
    radius: number;
}

// Traditional Linear Interpolation logic (Coding Train)
function getLerpPoint(v1: number, v2: number, p1: Vector2, p2: Vector2, threshold: number): Vector2 {
    const amt = (threshold - v1) / (v2 - v1);
    return new Vector2(
        p1.x + amt * (p2.x - p1.x),
        p1.y + amt * (p2.y - p1.y)
    );
}

// Marching Squares Case Mapping
const CASES: { [key: number]: number[][] } = {
    0: [], 1: [[2, 3]], 2: [[1, 2]], 3: [[1, 3]], 4: [[0, 1]], 5: [[0, 3], [1, 2]], 6: [[0, 2]], 7: [[0, 3]],
    8: [[0, 3]], 9: [[0, 2]], 10: [[0, 1], [2, 3]], 11: [[0, 1]], 12: [[1, 3]], 13: [[1, 2]], 14: [[2, 3]], 15: []
};

export default function Map11({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const canvasInstanceRef = useRef<Canvas | null>(null);
    
    // Config
    const [seed, setSeed] = useState(42);
    const [gridCellSize, setGridCellSize] = useState(16); // rez
    const [numBubbles, setNumBubbles] = useState(6);
    const [maxRadius, setMaxRadius] = useState(60);
    const [layers, setLayers] = useState(3);
    const [showSources, setShowSources] = useState(true);
    
    const [bubbles, setBubbles] = useState<Bubble[]>([]);
    const [renderTrigger, setRenderTrigger] = useState(0);

    // Static Hub Generator (Seed-based)
    useEffect(() => {
        const rand = () => {
            let t = seed;
            return () => {
                t = Math.imul(t ^ t >>> 15, t | 1);
                t ^= t + Math.imul(t ^ t >>> 7, t | 61);
                return ((t ^ t >>> 14) >>> 0) / 4294967296;
            };
        };
        const r = rand();
        const initial: Bubble[] = [];
        for (let i = 0; i < numBubbles; i++) {
            initial.push({
                center: new Vector2(r() * width, r() * height),
                radius: 30 + r() * maxRadius
            });
        }
        setBubbles(initial);
    }, [seed, numBubbles, maxRadius, width, height]);

    // Draw Loop
    useEffect(() => {
        const canvasEl = canvasRef.current;
        if (!canvasEl || !canvasInstanceRef.current) return;
        const canvas = canvasInstanceRef.current;
        canvas.clear();
        canvas.rect(-1000, -1000, width + 2000, height + 2000, { fill: '#0a0b12' });
        canvas.grid(gridCellSize, 'rgba(59, 130, 246, 0.04)');

        // 1. Grid Field Determination
        const cols = Math.floor(width / gridCellSize) + 1;
        const rows = Math.floor(height / gridCellSize) + 1;
        
        const field: number[][] = Array.from({ length: rows }, (_, r) => 
            Array.from({ length: cols }, (_, c) => {
                const px = c * gridCellSize;
                const py = r * gridCellSize;
                let sum = 0;
                bubbles.forEach(b => {
                    const dx = px - b.center.x;
                    const dy = py - b.center.y;
                    sum += (b.radius * b.radius) / (dx * dx + dy * dy + 0.1);
                });
                return sum;
            })
        );

        // 2. High-Contrast Render
        if (showSources) {
            bubbles.forEach(b => {
                canvas.circle(b.center, b.radius, { fill: 'transparent', stroke: 'rgba(59, 130, 246, 0.1)', lineWidth: 1 });
                canvas.circle(b.center, 3, { fill: '#3b82f6', alpha: 0.3 });
            });
        }

        // 3. Marching Squares Extraction (Static)
        for (let l = 0; l < layers; l++) {
            const threshold = 1.0 + (l - Math.floor(layers / 2)) * 0.2;
            const isPrimary = Math.abs(threshold - 1.0) < 0.01;
            const alpha = isPrimary ? 0.9 : 0.2 - (Math.abs(l - Math.floor(layers / 2)) * 0.05);
            const color = isPrimary ? 'cyan' : `hsl(180, 70%, 40%)`;

            for (let r = 0; r < rows - 1; r++) {
                for (let c = 0; c < cols - 1; c++) {
                    const v0 = field[r][c], v1 = field[r][c + 1], v2 = field[r + 1][c + 1], v3 = field[r + 1][c];
                    const x = c * gridCellSize;
                    const y = r * gridCellSize;
                    const p0 = new Vector2(x, y), p1 = new Vector2(x + gridCellSize, y);
                    const p2 = new Vector2(x + gridCellSize, y + gridCellSize), p3 = new Vector2(x, y + gridCellSize);

                    const index = (v0 > threshold ? 8 : 0) | (v1 > threshold ? 4 : 0) | (v2 > threshold ? 2 : 0) | (v3 > threshold ? 1 : 0);
                    if (index === 0 || index === 15) continue;

                    const edges: Vector2[] = [
                        getLerpPoint(v0, v1, p0, p1, threshold), getLerpPoint(v1, v2, p1, p2, threshold),
                        getLerpPoint(v2, v3, p2, p3, threshold), getLerpPoint(v3, v0, p3, p0, threshold)
                    ];

                    CASES[index].forEach(conn => {
                        canvas.line(edges[conn[0]], edges[conn[1]], { stroke: color, alpha: alpha, lineWidth: isPrimary ? 3 : 1.5 });
                    });
                }
            }
        }
    }, [bubbles, gridCellSize, layers, showSources, width, height, renderTrigger]);

    // Init Canvas
    useEffect(() => {
        if (!canvasRef.current) return;
        const instance = new Canvas(canvasRef.current);
        instance.initInteractions({ onInteraction: () => setRenderTrigger(t => t + 1) });
        canvasInstanceRef.current = instance;
    }, [width, height]);

    return (
        <div style={UI_STYLES.container}>
            <div style={UI_STYLES.panel}>
                <div style={UI_STYLES.group}><label style={UI_STYLES.label}>WORLD SEED</label><div style={UI_STYLES.inputRow}><input type="number" value={seed} onChange={(e) => setSeed(parseInt(e.target.value) || 0)} style={UI_STYLES.input} /><button onClick={() => setSeed(Math.floor(Math.random() * 99999))} style={UI_STYLES.buttonPrimary}>🎲</button></div></div>
                <div style={UI_STYLES.group}><label style={UI_STYLES.label}>GRID CELL SIZE (rez): {gridCellSize}px</label><input type="range" min="8" max="50" value={gridCellSize} onChange={(e) => setGridCellSize(parseInt(e.target.value))} style={UI_STYLES.slider} /></div>
                <div style={UI_STYLES.group}><label style={UI_STYLES.label}>HUB COUNT: {numBubbles}</label><input type="range" min="1" max="25" value={numBubbles} onChange={(e) => setNumBubbles(parseInt(e.target.value))} style={UI_STYLES.slider} /></div>
                <div style={UI_STYLES.group}><label style={UI_STYLES.label}>RADIUS: {maxRadius}</label><input type="range" min="20" max="150" value={maxRadius} onChange={(e) => setMaxRadius(parseInt(e.target.value))} style={UI_STYLES.slider} /></div>
                <div style={UI_STYLES.group}><label style={UI_STYLES.label}>ISO LAYERS: {layers}</label><input type="range" min="1" max="15" value={layers} onChange={(e) => setLayers(parseInt(e.target.value))} style={UI_STYLES.slider} /></div>

                <div style={UI_STYLES.group}>
                    <label style={UI_STYLES.label}>VISUALS</label>
                    <button onClick={() => setShowSources(!showSources)} style={showSources ? UI_STYLES.buttonPrimary : UI_STYLES.buttonAlt}>SOURCES</button>
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
    buttonAlt: { background: '#1e293b', border: '1px solid #334155', color: '#94a3b8', padding: '10px 16px', borderRadius: '8px', cursor: 'pointer', fontWeight: 'bold', fontSize: '12px', flex: 1 } as CSSProperties,
    slider: { width: '100%', accentColor: '#3b82f6', cursor: 'pointer' } as CSSProperties,
    canvas: { display: 'block', width: '100%', height: '100%', cursor: 'crosshair' } as CSSProperties
};
