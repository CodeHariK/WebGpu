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
import { computeRiverArcs, drawRivers } from './Map18Rivers';
import { computeRoadNetwork, drawRoads } from './Map18Roads';
import { computeFeatures, drawFeatures } from './Map18Features';

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

    const [seed, setSeed] = useState(42);
    const [hexSize, setHexSize] = useState(18);
    const [seaLevelPct, setSeaLevelPct] = useState(34); // %
    const [showCoast, setShowCoast] = useState(true);
    const [smooth, setSmooth] = useState(true);
    const [showGrid, setShowGrid] = useState(true);
    const [showRivers, setShowRivers] = useState(true);
    const [riverDensity, setRiverDensity] = useState(11);
    const [showRoads, setShowRoads] = useState(true);
    const [townCount, setTownCount] = useState(11);
    const [showFeatures, setShowFeatures] = useState(true);
    const [natureLevel, setNatureLevel] = useState(45);
    const [renderTrigger, setRenderTrigger] = useState(0);

    const world: BiomeWorld = useMemo(
        () => generateBiomeWorld({ width, height, seed, hexSize, seaLevel: seaLevelPct / 100 }),
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
        canvas.rect(-width * 4, -height * 4, width * 8, height * 8, { fill: '#25406b' });

        canvas.save();
        canvas.translate(new Vector2(width / 2, height / 2));

        const ctxFill = canvas.ctx;

        if (smooth) {
            const edgeMid = (verts: Vector2[], e: number): Vector2 =>
                new Vector2((verts[e].x + verts[(e + 1) % 6].x) / 2, (verts[e].y + verts[(e + 1) % 6].y) / 2);

            for (const c of world.cells.values()) {
                const verts = getHexVertices(c.center, hexSize);
                const cats = verts.map((v) => world.categoryAt(v.x, v.y));
                const { base, runs } = hexColorRuns(cats);

                ctxFill.beginPath();
                ctxFill.moveTo(verts[0].x, verts[0].y);
                for (let i = 1; i < 6; i++) ctxFill.lineTo(verts[i].x, verts[i].y);
                ctxFill.closePath();
                ctxFill.fillStyle = BIOME_COLORS[base];
                ctxFill.fill();

                for (const run of runs) {
                    const inEdge = (run.a + 5) % 6;
                    const outEdge = run.b;
                    const mIn = edgeMid(verts, inEdge);
                    const mOut = edgeMid(verts, outEdge);

                    const sep = Math.min(Math.abs(outEdge - inEdge), 6 - Math.abs(outEdge - inEdge));
                    const sScale = (sep <= 1 ? 0.35 : sep === 2 ? 0.55 : 0.7) * hexSize;
                    const dOut = c.center.clone().sub(mOut).normalize();
                    const dIn = c.center.clone().sub(mIn).normalize();
                    const cp1 = mOut.clone().add(dOut.clone().mult(sScale));
                    const cp2 = mIn.clone().add(dIn.clone().mult(sScale));

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
            for (const c of world.cells.values()) {
                const verts = getHexVertices(c.center, hexSize);
                canvas.polygon(verts, { fill: BIOME_COLORS[c.biome] });
            }
        }

        if (showGrid) {
            for (const c of world.cells.values()) {
                const verts = getHexVertices(c.center, hexSize);
                canvas.polygon(verts, { stroke: UI.grid, lineWidth: 0.6 });
            }
        }

        if (showCoast) {
            const ctx = canvas.ctx;
            ctx.lineWidth = Math.max(1.2, hexSize * 0.12);
            ctx.lineCap = 'round';
            ctx.strokeStyle = 'rgba(245, 250, 255, 0.85)';
            ctx.beginPath();
            for (const c of world.cells.values()) {
                const segs = marchHexCoastline(c.center, hexSize, world.seaLevel, world.sampleElevation);
                for (const [a, b] of segs) {
                    ctx.moveTo(a.x, a.y);
                    ctx.lineTo(b.x, b.y);
                }
            }
            ctx.stroke();
        }

        const riverArcs = showRivers ? computeRiverArcs(world, riverDensity) : [];
        if (showRivers) drawRivers(canvas.ctx, riverArcs, hexSize, 0.5);

        const roadNet = (showRoads || showFeatures) ? computeRoadNetwork(world, townCount, seed) : { arcs: [], towns: [] };
        if (showRoads) drawRoads(canvas.ctx, roadNet, hexSize, 0.5);

        if (showFeatures) {
            const feats = computeFeatures(world, riverArcs, roadNet, { villages: showRoads, nature: natureLevel, seed });
            drawFeatures(canvas.ctx, feats);
        }

        canvas.restore();
    }, [world, hexSize, showCoast, smooth, showGrid, showRivers, riverDensity, showRoads, townCount, showFeatures, natureLevel, seed, renderTrigger, width, height]);

    return (
        <div style={{ display: 'flex', flexDirection: 'column', background: UI.bg, fontFamily: 'Outfit, sans-serif', borderRadius: 8, overflow: 'hidden' }}>
            <div ref={containerRef} style={{ position: 'relative', width: '100%', lineHeight: 0 }}>
                <canvas ref={canvasRef} width={width} height={height} style={{ display: 'block', width: '100%', height: 'auto' }} />
                <div style={{ position: 'absolute', bottom: 16, left: 16, background: 'rgba(10,11,18,0.6)', backdropFilter: 'blur(6px)', padding: '8px 14px', borderRadius: 8, color: UI.text, fontSize: '0.75rem', border: '1px solid rgba(255,255,255,0.08)', pointerEvents: 'none', lineHeight: 1.3 }}>
                    {world.landRegions} landmass{world.landRegions === 1 ? '' : 'es'} &middot; {world.landCount} land / {world.waterCount} water hexes
                </div>
                <div style={{ position: 'absolute', bottom: 16, right: 16, textAlign: 'right', pointerEvents: 'none', lineHeight: 1.1 }}>
                    <h1 style={{ color: 'white', margin: 0, fontSize: '1.6rem', fontWeight: 800, letterSpacing: '1px' }}>HEX BIOMES</h1>
                    <p style={{ color: UI.text, margin: 0, fontSize: '0.8rem' }}>NOISE &middot; WHITTAKER &middot; FLOOD-FILL</p>
                </div>
            </div>

            <div style={{ display: 'flex', flexWrap: 'wrap', gap: 10, alignItems: 'center', padding: '14px 16px', borderTop: '1px solid rgba(255,255,255,0.08)' }}>
                <button onClick={() => setSeed(Math.floor(Math.random() * 9999))}
                    style={{ background: UI.accent, color: 'white', border: 'none', padding: '10px 18px', borderRadius: 8, cursor: 'pointer', fontWeight: 700 }}>
                    RANDOMIZE
                </button>
                <Slider label="SEED" min={0} max={9999} value={seed} onChange={setSeed} />
                <Slider label="SCALE" min={9} max={30} value={hexSize} onChange={setHexSize} />
                <Slider label="SEA LEVEL" min={20} max={60} value={seaLevelPct} onChange={setSeaLevelPct} />
                <Slider label="DRAINAGE" min={1} max={13} value={riverDensity} onChange={setRiverDensity} />
                <Slider label="TOWNS" min={2} max={20} value={townCount} onChange={setTownCount} />
                <Slider label="NATURE" min={0} max={80} value={natureLevel} onChange={setNatureLevel} />

                <Toggle label={`FILL ${smooth ? 'SMOOTH' : 'MOSAIC'}`} on={smooth} onClick={() => setSmooth((v) => !v)} />
                <Toggle label={`GRID ${showGrid ? 'ON' : 'OFF'}`} on={showGrid} onClick={() => setShowGrid((v) => !v)} />
                <Toggle label={`COASTLINE ${showCoast ? 'ON' : 'OFF'}`} on={showCoast} onClick={() => setShowCoast((v) => !v)} />
                <Toggle label={`RIVERS ${showRivers ? 'ON' : 'OFF'}`} on={showRivers} onClick={() => setShowRivers((v) => !v)} />
                <Toggle label={`ROADS ${showRoads ? 'ON' : 'OFF'}`} on={showRoads} onClick={() => setShowRoads((v) => !v)} />
                <Toggle label={`VILLAGES ${showFeatures ? 'ON' : 'OFF'}`} on={showFeatures} onClick={() => setShowFeatures((v) => !v)} />
            </div>
        </div>
    );
};

const Toggle = ({ label, on, onClick }: { label: string; on: boolean; onClick: () => void }) => (
    <button
        onClick={onClick}
        style={{
            background: on ? 'rgba(34,176,125,0.22)' : 'rgba(255,255,255,0.05)',
            color: on ? '#d7f5e8' : '#94a3b8',
            border: `1px solid ${on ? 'rgba(34,176,125,0.5)' : 'rgba(255,255,255,0.12)'}`,
            padding: '9px 14px',
            borderRadius: 8,
            cursor: 'pointer',
            fontWeight: 600,
            fontSize: '0.78rem',
        }}
    >
        {label}
    </button>
);

const Slider = ({ label, min, max, value, onChange }: { label: string; min: number; max: number; value: number; onChange: (v: number) => void }) => (
    <div style={{ padding: '8px 14px', background: 'rgba(255,255,255,0.05)', borderRadius: 8, display: 'flex', alignItems: 'center', gap: 10, color: UI.text, border: '1px solid rgba(255,255,255,0.1)' }}>
        <span style={{ fontSize: '0.72rem', fontWeight: 600, whiteSpace: 'nowrap' }}>{label}</span>
        <input type="range" min={min} max={max} value={value} onChange={(e) => onChange(Number(e.target.value))} style={{ cursor: 'pointer', accentColor: UI.accent }} />
    </div>
);

export default Map18;
