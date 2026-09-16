import { useEffect, useMemo, useRef, useState } from 'react';
import { Vector2 } from '../lib/Vector2';
import { Canvas } from '../lib/Canvas';
import { getHexVertices } from './Map14Logic';
import {
    generateBiomeWorld,
    marchHexCoastline,
    hexColorRuns,
    BIOME_COLORS,
    isWaterBiome,
    type BiomeWorld,
    type Biome,
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

// Flat macro-zone view (the BIOME MAP toggle)
type Macro = 'water' | 'snow' | 'tundra' | 'desert' | 'grass' | 'forest' | 'jungle' | 'lava';
const MACRO_COLORS: Record<Macro | 'city', string> = {
    water: '#3a6ea5', snow: '#e9eff6', tundra: '#c3cbb0', desert: '#e3ca92',
    grass: '#93c85e', forest: '#409a4f', jungle: '#2b7d5a', lava: '#d24a2a', city: '#9aa0a6',
};
const LEGEND_BIOME: { label: string; color: string }[] = [
    { label: 'Snow', color: MACRO_COLORS.snow },
    { label: 'Tundra', color: MACRO_COLORS.tundra },
    { label: 'Desert', color: MACRO_COLORS.desert },
    { label: 'Grassland', color: MACRO_COLORS.grass },
    { label: 'Forest', color: MACRO_COLORS.forest },
    { label: 'Jungle', color: MACRO_COLORS.jungle },
    { label: 'Lava', color: MACRO_COLORS.lava },
];
// Not biomes: sea is a sea-level terrain state, city is a settlement overlay.
const LEGEND_TERRAIN: { label: string; color: string }[] = [
    { label: 'Sea', color: MACRO_COLORS.water },
    { label: 'City', color: MACRO_COLORS.city },
];
function macroKey(b: Biome): Macro {
    if (isWaterBiome(b)) return 'water';
    if (b === 'LAVA' || b === 'SCORCHED') return 'lava';
    if (b === 'SNOW') return 'snow';
    if (b === 'TUNDRA' || b === 'BARE') return 'tundra';
    if (b === 'TEMPERATE_DESERT' || b === 'SUBTROPICAL_DESERT') return 'desert';
    if (b === 'GRASSLAND' || b === 'SHRUBLAND') return 'grass';
    if (b === 'TROPICAL_FOREST' || b === 'TROPICAL_RAINFOREST') return 'jungle';
    return 'forest'; // temperate forest/rainforest, taiga, themed
}

type ViewMode = 'normal' | 'biome' | 'elevation' | 'moisture';
const hx = (c: string) => [parseInt(c.slice(1, 3), 16), parseInt(c.slice(3, 5), 16), parseInt(c.slice(5, 7), 16)];
function ramp(stops: [number, string][], t: number): string {
    const u = Math.max(0, Math.min(1, t));
    let a = stops[0], b = stops[stops.length - 1];
    for (let i = 0; i < stops.length - 1; i++) { if (u >= stops[i][0] && u <= stops[i + 1][0]) { a = stops[i]; b = stops[i + 1]; break; } }
    const f = b[0] === a[0] ? 0 : (u - a[0]) / (b[0] - a[0]);
    const ca = hx(a[1]), cb = hx(b[1]);
    const r = Math.round(ca[0] + (cb[0] - ca[0]) * f), g = Math.round(ca[1] + (cb[1] - ca[1]) * f), bl = Math.round(ca[2] + (cb[2] - ca[2]) * f);
    return `rgb(${r},${g},${bl})`;
}
const ELEV_STOPS: [number, string][] = [[0, '#3f7d4f'], [0.4, '#c9b36a'], [0.72, '#8a5a3a'], [1, '#ffffff']];
const SEA_STOPS: [number, string][] = [[0, '#0a2540'], [1, '#4a90c0']];
const MOIST_STOPS: [number, string][] = [[0, '#e2c98a'], [0.5, '#7dbf72'], [1, '#2b7fae']];
function elevColor(e: number, sea: number): string {
    return e < sea ? ramp(SEA_STOPS, e / sea) : ramp(ELEV_STOPS, (e - sea) / (1 - sea));
}
const moistColor = (m: number) => ramp(MOIST_STOPS, m);

// Biomes produced only by a theme (candy/spooky/chaos) — shown with their own
// colour in the biome-map overview instead of the realistic macro categories.
const THEMED_BIOMES = new Set<Biome>(['CANDY', 'CHOCOLATE', 'MINT', 'LICORICE', 'PUMPKIN', 'HAUNTED', 'BONE']);

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
    const [biomesOn, setBiomesOn] = useState(true);
    const [theme, setTheme] = useState('realistic');
    const [coherence, setCoherence] = useState(60); // %
    const [view, setView] = useState<ViewMode>('normal');
    const [renderTrigger, setRenderTrigger] = useState(0);

    const world: BiomeWorld = useMemo(
        () => generateBiomeWorld({ width, height, seed, hexSize, seaLevel: seaLevelPct / 100, biomes: biomesOn, theme, coherence: coherence / 100 }),
        [seed, hexSize, seaLevelPct, biomesOn, theme, coherence, width, height]
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

        // Data-map views: flat per-hex fills for biome zones / elevation / moisture.
        if (view !== 'normal') {
            const net = view === 'biome' ? computeRoadNetwork(world, townCount, seed) : null;
            const cityR = hexSize * 2.2;
            for (const c of world.cells.values()) {
                const verts = getHexVertices(c.center, hexSize);
                let col: string;
                if (view === 'elevation') col = elevColor(c.elevation, world.seaLevel);
                else if (view === 'moisture') col = moistColor(c.moisture);
                else {
                    const isCity = c.regionId >= 0 && net!.towns.some((t) => Vector2.dist(t, c.center) < cityR);
                    col = isCity ? MACRO_COLORS.city : (THEMED_BIOMES.has(c.biome) ? BIOME_COLORS[c.biome] : MACRO_COLORS[macroKey(c.biome)]);
                }
                canvas.polygon(verts, { fill: col, stroke: 'rgba(0,0,0,0.05)', lineWidth: 0.5 });
            }
            canvas.restore();
            return;
        }

        const ctxFill = canvas.ctx;
        // Colour every hex by its actual biome. Theme reskinning now happens in the
        // generator (forest regions become themed biomes), so no render-side skin.
        const fillOf = (b: Biome) => BIOME_COLORS[b];

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
                ctxFill.fillStyle = fillOf(base);
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
                    ctxFill.fillStyle = fillOf(run.biome);
                    ctxFill.fill();

                    ctxFill.beginPath();
                    ctxFill.moveTo(mOut.x, mOut.y);
                    ctxFill.bezierCurveTo(cp1.x, cp1.y, cp2.x, cp2.y, mIn.x, mIn.y);
                    ctxFill.lineWidth = Math.max(1, hexSize * 0.08);
                    ctxFill.lineCap = 'round';
                    ctxFill.strokeStyle = fillOf(run.biome);
                    ctxFill.stroke();
                }
            }
        } else {
            for (const c of world.cells.values()) {
                const verts = getHexVertices(c.center, hexSize);
                canvas.polygon(verts, { fill: fillOf(c.biome) });
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
    }, [world, hexSize, showCoast, smooth, showGrid, showRivers, riverDensity, showRoads, townCount, showFeatures, natureLevel, seed, theme, view, renderTrigger, width, height]);

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
                {view !== 'normal' && (
                    <div style={{ position: 'absolute', top: 16, left: 16, background: 'rgba(10,11,18,0.72)', backdropFilter: 'blur(6px)', padding: '10px 12px', borderRadius: 8, border: '1px solid rgba(255,255,255,0.12)', display: 'flex', flexDirection: 'column', gap: 6 }}>
                        {view === 'biome' && (
                            <>
                                <div style={{ color: '#94a3b8', fontSize: '0.62rem', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>Biomes</div>
                                {LEGEND_BIOME.map((l) => (
                                    <div key={l.label} style={{ display: 'flex', alignItems: 'center', gap: 8, color: '#e2e8f0', fontSize: '0.72rem', fontWeight: 600 }}>
                                        <span style={{ width: 14, height: 14, borderRadius: 3, background: l.color, border: '1px solid rgba(0,0,0,0.2)' }} />
                                        {l.label}
                                    </div>
                                ))}
                                <div style={{ color: '#94a3b8', fontSize: '0.62rem', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase', marginTop: 4 }}>Terrain</div>
                                {LEGEND_TERRAIN.map((l) => (
                                    <div key={l.label} style={{ display: 'flex', alignItems: 'center', gap: 8, color: '#e2e8f0', fontSize: '0.72rem', fontWeight: 600 }}>
                                        <span style={{ width: 14, height: 14, borderRadius: 3, background: l.color, border: '1px solid rgba(0,0,0,0.2)' }} />
                                        {l.label}
                                    </div>
                                ))}
                            </>
                        )}
                        {view !== 'biome' && (
                            <>
                                <div style={{ color: '#e2e8f0', fontSize: '0.75rem', fontWeight: 700 }}>{view === 'elevation' ? 'Elevation' : 'Moisture'}</div>
                                <div style={{ width: 120, height: 12, borderRadius: 3, border: '1px solid rgba(0,0,0,0.2)', background: view === 'elevation' ? 'linear-gradient(90deg,#0a2540,#4a90c0,#3f7d4f,#c9b36a,#8a5a3a,#ffffff)' : 'linear-gradient(90deg,#e2c98a,#7dbf72,#2b7fae)' }} />
                                <div style={{ display: 'flex', justifyContent: 'space-between', color: '#94a3b8', fontSize: '0.68rem' }}>
                                    <span>{view === 'elevation' ? 'Deep' : 'Dry'}</span><span>{view === 'elevation' ? 'Peak' : 'Wet'}</span>
                                </div>
                            </>
                        )}
                    </div>
                )}
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

                <Toggle label={`BIOMES ${biomesOn ? 'ON' : 'OFF'}`} on={biomesOn} onClick={() => setBiomesOn((v) => !v)} />
                <button
                    onClick={() => setTheme((t) => { const order = ['realistic', 'candy', 'spooky', 'chaos']; return order[(order.indexOf(t) + 1) % order.length]; })}
                    style={{ background: 'rgba(255,255,255,0.08)', color: '#e2e8f0', border: '1px solid rgba(255,255,255,0.14)', padding: '9px 14px', borderRadius: 8, cursor: 'pointer', fontWeight: 600, fontSize: '0.78rem' }}
                >
                    THEME: {theme.toUpperCase()}
                </button>
                <Slider label="COHERENCE" min={0} max={100} value={coherence} onChange={setCoherence} />
                <Toggle label="BIOME MAP" on={view === 'biome'} onClick={() => setView((v) => (v === 'biome' ? 'normal' : 'biome'))} />
                <Toggle label="ELEVATION" on={view === 'elevation'} onClick={() => setView((v) => (v === 'elevation' ? 'normal' : 'elevation'))} />
                <Toggle label="MOISTURE" on={view === 'moisture'} onClick={() => setView((v) => (v === 'moisture' ? 'normal' : 'moisture'))} />
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
