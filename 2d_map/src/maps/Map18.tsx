import { useEffect, useMemo, useRef, useState } from 'react';
import { Vector2 } from '../lib/Vector2';
import { Canvas } from '../lib/Canvas';
import { getHexVertices } from './Map14Logic';
import {
    generateBiomeWorld,
    marchHexCoastline,
    hexColorRuns,
    BIOME_COLORS,
    type BiomeWorld,
} from './Map18Logic';

const UI = {
    bg: '#0a0b12',
    accent: '#22b07d',
    grid: 'rgba(0, 0, 0, 0.10)',
    text: '#94a3b8',
};

const Map18 = ({ width = 800, height = 800 }: { width?: number; height?: number }) => {
    const containerRef = useRef<HTMLDivElement>(null);
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const canvasInstanceRef = useRef<Canvas | null>(null);

    const [seed, setSeed] = useState(1234);
    const [hexSize, setHexSize] = useState(14);
    const [seaLevelPct, setSeaLevelPct] = useState(38); // %
    const [showCoast, setShowCoast] = useState(true);
    const [smooth, setSmooth] = useState(true);
    const [showGrid, setShowGrid] = useState(true);
    const [renderTrigger, setRenderTrigger] = useState(0);

    const world: BiomeWorld = useMemo(
        () =>
            generateBiomeWorld({
                width,
                height,
                seed,
                hexSize,
                seaLevel: seaLevelPct / 100,
            }),
        [seed, hexSize, seaLevelPct, width, height]
    );

    // Init canvas + pan/zoom
    useEffect(() => {
        if (!canvasRef.current) return;
        const canvas = new Canvas(canvasRef.current);
        canvas.initInteractions({ onInteraction: () => setRenderTrigger((t) => t + 1) });
        canvasInstanceRef.current = canvas;
        return () => canvas.destroyInteractions();
    }, [width, height]);

    // Render loop
    useEffect(() => {
        const canvas = canvasInstanceRef.current;
        if (!canvas) return;

        canvas.clear();
        canvas.rect(-width * 4, -height * 4, width * 8, height * 8, { fill: '#25406b' }); // ocean backdrop

        canvas.save();
        canvas.translate(new Vector2(width / 2, height / 2));

        const ctxFill = canvas.ctx;

        if (smooth) {
            // SMOOTH FILL — corner colouring cut by Truchet arcs. Each hex is filled
            // with its base (most common corner) colour; every run of corners that
            // differs is cut off by one Truchet bezier arc joining the midpoints of
            // the two edges bounding the run, and overpainted in the run's colour.
            // Because corners are shared world points, arcs meet at shared edge
            // midpoints and chain into continuous contours across hexes. Overlaps
            // between runs of different colours are left as-is.
            const edgeMid = (verts: Vector2[], e: number): Vector2 =>
                new Vector2((verts[e].x + verts[(e + 1) % 6].x) / 2, (verts[e].y + verts[(e + 1) % 6].y) / 2);

            for (const c of world.cells.values()) {
                const verts = getHexVertices(c.center, hexSize);
                const cats = verts.map((v) => world.categoryAt(v.x, v.y));
                const { base, runs } = hexColorRuns(cats);

                // Base fill.
                ctxFill.beginPath();
                ctxFill.moveTo(verts[0].x, verts[0].y);
                for (let i = 1; i < 6; i++) ctxFill.lineTo(verts[i].x, verts[i].y);
                ctxFill.closePath();
                ctxFill.fillStyle = BIOME_COLORS[base];
                ctxFill.fill();

                // Each differing run, cut off by its Truchet arc.
                for (const run of runs) {
                    const inEdge = (run.a + 5) % 6;   // edge between corner a-1 and a
                    const outEdge = run.b;            // edge between corner b and b+1
                    const mIn = edgeMid(verts, inEdge);
                    const mOut = edgeMid(verts, outEdge);

                    // Arc control points pull toward the centre; strength grows with run width.
                    const sep = Math.min(Math.abs(outEdge - inEdge), 6 - Math.abs(outEdge - inEdge));
                    const sScale = (sep <= 1 ? 0.35 : sep === 2 ? 0.55 : 0.7) * hexSize;
                    const dOut = c.center.clone().sub(mOut).normalize();
                    const dIn = c.center.clone().sub(mIn).normalize();
                    const cp1 = mOut.clone().add(dOut.clone().mult(sScale));
                    const cp2 = mIn.clone().add(dIn.clone().mult(sScale));

                    // Filled region: mIn -> corners a..b -> mOut -> arc back to mIn.
                    ctxFill.beginPath();
                    ctxFill.moveTo(mIn.x, mIn.y);
                    let i = run.a;
                    while (true) {
                        ctxFill.lineTo(verts[i].x, verts[i].y);
                        if (i === run.b) break;
                        i = (i + 1) % 6;
                    }
                    ctxFill.lineTo(mOut.x, mOut.y);
                    ctxFill.bezierCurveTo(cp1.x, cp1.y, cp2.x, cp2.y, mIn.x, mIn.y);
                    ctxFill.closePath();
                    ctxFill.fillStyle = BIOME_COLORS[run.biome];
                    ctxFill.fill();

                    // Contour drawn in the run's vertex colour.
                    ctxFill.beginPath();
                    ctxFill.moveTo(mOut.x, mOut.y);
                    ctxFill.bezierCurveTo(cp1.x, cp1.y, cp2.x, cp2.y, mIn.x, mIn.y);
                    ctxFill.lineWidth = Math.max(1, hexSize * 0.08);
                    ctxFill.lineCap = 'round';
                    ctxFill.strokeStyle = BIOME_COLORS[run.biome];
                    ctxFill.stroke();
                }
            }
        } else {
            // MOSAIC — flat per-hex fill + faint lattice.
            for (const c of world.cells.values()) {
                const verts = getHexVertices(c.center, hexSize);
                canvas.polygon(verts, { fill: BIOME_COLORS[c.biome] });
            }
        }

        // Hex-edge overlay — draw the hex lattice on top of the fill.
        if (showGrid) {
            for (const c of world.cells.values()) {
                const verts = getHexVertices(c.center, hexSize);
                canvas.polygon(verts, { stroke: UI.grid, lineWidth: 0.6 });
            }
        }

        // Smooth coastline overlay via marching triangles on the shared elevation field.
        if (showCoast) {
            const ctx = canvas.ctx;
            ctx.lineWidth = Math.max(1.2, hexSize * 0.12);
            ctx.lineCap = 'round';
            ctx.strokeStyle = 'rgba(245, 250, 255, 0.85)';
            ctx.beginPath();
            for (const c of world.cells.values()) {
                // Only bother with hexes near the coast (mixed neighbourhood).
                const segs = marchHexCoastline(c.center, hexSize, world.seaLevel, world.sampleElevation);
                for (const [a, b] of segs) {
                    ctx.moveTo(a.x, a.y);
                    ctx.lineTo(b.x, b.y);
                }
            }
            ctx.stroke();
        }

        canvas.restore();
    }, [world, hexSize, showCoast, smooth, showGrid, renderTrigger, width, height]);

    return (
        <div
            ref={containerRef}
            style={{
                width: '100%',
                height: '100%',
                position: 'relative',
                overflow: 'hidden',
                background: UI.bg,
                fontFamily: 'Outfit, sans-serif',
            }}
        >
            <canvas
                ref={canvasRef}
                width={width}
                height={height}
                style={{ display: 'block', width: '100%', height: '100%' }}
            />

            <div style={{ position: 'absolute', top: 20, left: 20, display: 'flex', gap: 10, zIndex: 10, flexWrap: 'wrap', maxWidth: width - 40 }}>
                <button
                    onClick={() => setSeed(Math.floor(Math.random() * 99999))}
                    style={{
                        background: UI.accent,
                        color: 'white',
                        border: 'none',
                        padding: '10px 20px',
                        borderRadius: 8,
                        cursor: 'pointer',
                        fontWeight: 600,
                    }}
                >
                    RANDOMIZE
                </button>

                <Slider label="SCALE" min={9} max={30} value={hexSize} onChange={setHexSize} />
                <Slider label="SEA LEVEL" min={20} max={60} value={seaLevelPct} onChange={setSeaLevelPct} />

                <button
                    onClick={() => setShowGrid((v) => !v)}
                    style={{
                        background: showGrid ? 'rgba(255,255,255,0.12)' : 'rgba(255,255,255,0.05)',
                        color: UI.text,
                        border: '1px solid rgba(255,255,255,0.12)',
                        padding: '10px 16px',
                        borderRadius: 8,
                        cursor: 'pointer',
                        fontWeight: 600,
                        fontSize: '0.8rem',
                    }}
                >
                    GRID {showGrid ? 'ON' : 'OFF'}
                </button>

                <button
                    onClick={() => setSmooth((v) => !v)}
                    style={{
                        background: smooth ? 'rgba(255,255,255,0.12)' : 'rgba(255,255,255,0.05)',
                        color: UI.text,
                        border: '1px solid rgba(255,255,255,0.12)',
                        padding: '10px 16px',
                        borderRadius: 8,
                        cursor: 'pointer',
                        fontWeight: 600,
                        fontSize: '0.8rem',
                    }}
                >
                    FILL {smooth ? 'SMOOTH' : 'MOSAIC'}
                </button>

                <button
                    onClick={() => setShowCoast((s) => !s)}
                    style={{
                        background: showCoast ? 'rgba(255,255,255,0.12)' : 'rgba(255,255,255,0.05)',
                        color: UI.text,
                        border: '1px solid rgba(255,255,255,0.12)',
                        padding: '10px 16px',
                        borderRadius: 8,
                        cursor: 'pointer',
                        fontWeight: 600,
                        fontSize: '0.8rem',
                    }}
                >
                    COASTLINE {showCoast ? 'ON' : 'OFF'}
                </button>
            </div>

            <div style={{ position: 'absolute', bottom: 20, left: 20, background: 'rgba(10,11,18,0.6)', backdropFilter: 'blur(6px)', padding: '8px 14px', borderRadius: 8, color: UI.text, fontSize: '0.75rem', border: '1px solid rgba(255,255,255,0.08)', pointerEvents: 'none' }}>
                {world.landRegions} landmass{world.landRegions === 1 ? '' : 'es'} · {world.landCount} land / {world.waterCount} water hexes
            </div>

            <div style={{ position: 'absolute', bottom: 20, right: 20, textAlign: 'right', pointerEvents: 'none' }}>
                <h1 style={{ color: 'white', margin: 0, fontSize: '1.8rem', fontWeight: 800, letterSpacing: '1px' }}>
                    HEX BIOMES
                </h1>
                <p style={{ color: UI.text, margin: 0, fontSize: '0.85rem' }}>
                    NOISE · WHITTAKER · FLOOD-FILL
                </p>
            </div>
        </div>
    );
};

const Slider = ({ label, min, max, value, onChange }: { label: string; min: number; max: number; value: number; onChange: (v: number) => void }) => (
    <div
        style={{
            padding: '10px 16px',
            background: 'rgba(255,255,255,0.05)',
            backdropFilter: 'blur(8px)',
            borderRadius: 8,
            display: 'flex',
            alignItems: 'center',
            gap: 10,
            color: UI.text,
            border: '1px solid rgba(255,255,255,0.1)',
        }}
    >
        <span style={{ fontSize: '0.75rem', fontWeight: 600 }}>{label}</span>
        <input
            type="range"
            min={min}
            max={max}
            value={value}
            onChange={(e) => onChange(Number(e.target.value))}
            style={{ cursor: 'pointer', accentColor: UI.accent }}
        />
    </div>
);

export default Map18;
