import { useEffect, useRef, useState, useCallback } from 'react';
import { Canvas } from '../lib/Canvas';
import { 
    CirclePacker, 
    RectangularShape, 
    TriangularShape, 
    CircularShape,
    mulberry32, 
    type PackedCircle, 
    type PackedShape 
} from './Map16Logic';

const COLORS = {
    bg: '#050a15',
    grid: '#111b33',
    accent: '#0ea5e9',
    text: '#94a3b8',
    cyan: '#22d3ee',
    blue: '#3b82f6',
    glow: 'rgba(14, 165, 233, 0.15)'
};

export default function Map16({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const packerRef = useRef<CirclePacker | null>(null);
    const requestRef = useRef<number | null>(null);
    
    // State
    const [seed, setSeed] = useState(Math.round(Math.random() * 1000));
    const [gridDivs, setGridDivs] = useState(60);
    const [padding, setPadding] = useState(2);
    const [numTries, setNumTries] = useState(150000);
    const [minScale, setMinScale] = useState(10);
    const [maxScale, setMaxScale] = useState(150);
    
    const [items, setItems] = useState<PackedCircle[]>([]);
    const [shapes, setShapes] = useState<PackedShape[]>([]);
    const [isGenerating, setIsGenerating] = useState(false);
    const [progress, setProgress] = useState(0);

    const generate = useCallback(() => {
        setIsGenerating(true);
        setProgress(0);
        setItems([]);
        setShapes([]);

        const packer = new CirclePacker(width, height, gridDivs, padding);
        packerRef.current = packer;

        const rand = mulberry32(seed);
        const TAU = Math.PI * 2;
        
        let currentTry = 0;
        const chunkSize = 1500; // Tries per frame

        const loop = () => {
            if (currentTry >= numTries) {
                setIsGenerating(false);
                setItems([...packer.items]);
                setShapes([...packer.shapes]);
                return;
            }

            for (let i = 0; i < chunkSize && currentTry < numTries; i++) {
                currentTry++;
                
                const x = rand() * width;
                const y = rand() * height;
                const rotation = rand() * TAU;
                const typeRand = rand();
                const type = typeRand < 0.33 ? 'R' : (typeRand < 0.66 ? 'T' : 'C');
                const randW = 1 + rand() * 14;
                const randH = 1 + rand() * 14;

                let shape: RectangularShape | TriangularShape | CircularShape;
                let metadata: Omit<PackedShape, 'circles'>;

                if (type === 'R') {
                    shape = new RectangularShape(randW, randH);
                    metadata = { type: 'R', x, y, scale: minScale, rotation, sizeW: randW, sizeH: randH };
                } else if (type === 'T') {
                    shape = new TriangularShape(randW);
                    metadata = { type: 'T', x, y, scale: minScale, rotation, side: randW };
                } else {
                    shape = new CircularShape(randW);
                    metadata = { type: 'C', x, y, scale: minScale, rotation, radius: randW };
                }

                let currentScale = minScale;
                let lastValid: PackedShape | null = null;

                while (currentScale < maxScale) {
                    shape.scaleRotateTranslate(currentScale, rotation, x, y);
                    const added = packer.tryToAddShape(shape.circles, { ...metadata, scale: currentScale }, false);
                    
                    if (!added) break;
                    lastValid = added;
                    currentScale += 1.0; // Growth Step
                }

                if (lastValid) {
                    packer.tryToAddShape(lastValid.circles, lastValid, true);
                }
            }

            setProgress(Math.floor((currentTry / numTries) * 100));
            setItems([...packer.items]);
            setShapes([...packer.shapes]);
            requestRef.current = window.requestAnimationFrame(loop);
        };

        requestRef.current = window.requestAnimationFrame(loop);
    }, [seed, gridDivs, padding, numTries, minScale, maxScale, width, height]);

    useEffect(() => {
        generate();
        return () => {
            if (requestRef.current) cancelAnimationFrame(requestRef.current);
        };
    }, [generate]);

    useEffect(() => {
        const ctx = canvasRef.current?.getContext('2d');
        if (!ctx) return;

        const canvas = new Canvas(canvasRef.current!);
        canvas.clear();

        // Draw circles
        ctx.lineWidth = 1;
        items.forEach(c => {
            ctx.beginPath();
            ctx.arc(c.x, c.y, c.r, 0, Math.PI * 2);
            ctx.strokeStyle = COLORS.blue;
            ctx.stroke();
            
            ctx.fillStyle = COLORS.glow;
            ctx.fill();
        });

        // Draw shape perimeters
        shapes.forEach(s => {
            ctx.beginPath();
            ctx.lineWidth = 1.5;
            ctx.strokeStyle = COLORS.cyan;
            s.circles.forEach((c, i) => {
                if (i === 0) ctx.moveTo(c.x, c.y);
                else ctx.lineTo(c.x, c.y);
            });
            ctx.closePath();
            ctx.stroke();
        });

        return () => {
            canvas.destroyInteractions();
        };
    }, [items, shapes, width, height]);

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
                    <h2 style={{ margin: '0 0 15px 0', fontSize: '1.2rem', color: COLORS.accent, fontWeight: 700, letterSpacing: '1px' }}>SHAPE PACKER</h2>
                    
                    <div style={{ display: 'flex', flexDirection: 'row', alignItems: 'center', gap: 10, marginBottom: 15 }}>
                        <div style={{ height: 4, background: 'rgba(255,255,255,0.1)', flex: 1, borderRadius: 2, overflow: 'hidden' }}>
                            <div style={{ height: '100%', background: COLORS.accent, width: `${progress}%`, transition: 'width 0.1s' }} />
                        </div>
                        <span style={{ fontSize: '0.7rem', color: COLORS.text, width: 30 }}>{progress}%</span>
                    </div>

                    <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
                        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.8rem', opacity: 0.7 }}>
                                <span>GRID DIVS</span>
                                <span>{gridDivs}</span>
                            </div>
                            <input type="range" min="20" max="150" value={gridDivs} onChange={e => setGridDivs(parseInt(e.target.value))} />
                        </div>

                        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.8rem', opacity: 0.7 }}>
                                <span>SPACING (PAD)</span>
                                <span>{padding}px</span>
                            </div>
                            <input type="range" min="0" max="10" step="0.5" value={padding} onChange={e => setPadding(parseFloat(e.target.value))} />
                        </div>

                        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.8rem', opacity: 0.7 }}>
                                <span>TOTAL TRIES</span>
                                <span>{(numTries / 1000).toFixed(0)}k</span>
                            </div>
                            <input type="range" min="10000" max="500000" step="10000" value={numTries} onChange={e => setNumTries(parseInt(e.target.value))} />
                        </div>

                        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.8rem', opacity: 0.7 }}>
                                <span>MIN SCALE</span>
                                <span>{minScale}</span>
                            </div>
                            <input type="range" min="2" max="50" step="1" value={minScale} onChange={e => setMinScale(parseInt(e.target.value))} />
                        </div>

                        <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.8rem', opacity: 0.7 }}>
                                <span>MAX SCALE</span>
                                <span>{maxScale}</span>
                            </div>
                            <input type="range" min="20" max="400" step="5" value={maxScale} onChange={e => setMaxScale(parseInt(e.target.value))} />
                        </div>

                        <div style={{ display: 'flex', flexDirection: 'column', gap: 4, marginTop: 5 }}>
                            <div style={{ fontSize: '0.7rem', opacity: 0.6, letterSpacing: '1px' }}>VERSION_SEED</div>
                            <input 
                                type="text" 
                                value={seed} 
                                onChange={e => setSeed(parseInt(e.target.value) || 0)}
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
                            onClick={() => !isGenerating && setSeed(Math.round(Math.random() * 99999))}
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
                                opacity: isGenerating ? 0.7 : 1
                            }}
                        >
                            {isGenerating ? 'PACKING...' : 'RESEED & PACK'}
                        </button>
                    </div>
                </div>

                <div style={{ background: 'rgba(10, 15, 30, 0.6)', padding: '10px 15px', borderRadius: '12px', fontSize: '0.75rem', color: COLORS.text, border: '1px solid rgba(255,255,255,0.05)' }}>
                    {shapes.length} Shapes • {items.length} Circles<br/>
                    Spatial Hash Grid: {gridDivs}x{gridDivs}
                </div>
            </div>
            
            <div style={{ position: 'absolute', bottom: 30, right: 30, textAlign: 'right', pointerEvents: 'none' }}>
                <div style={{ fontSize: '3rem', fontWeight: 900, color: 'white', opacity: 0.1, lineHeight: 1 }}>GEOMETRY</div>
                <div style={{ fontSize: '1rem', color: COLORS.accent, opacity: 0.5, letterSpacing: '4px' }}>PACKER_CORE_16</div>
            </div>
        </div>
    );
}
