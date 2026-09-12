import { useEffect, useRef, useState, useCallback } from 'react';
import { Canvas } from '../lib/Canvas';
import { generateWatabouCity, type WatabouCity } from './Map17Logic';

const PALETTE = {
    water: '#1e3a5f',
    waterDeep: '#0f233d',
    coastline: '#38bdf8',
    river: '#38bdf8',
    riverBank: '#0c4a6e',
    ground: '#131b26',
    farmland: '#1e2922',
    slumGround: '#1a1f2c',
    residentialGround: '#1f2937',
    craftsmenGround: '#282f3d',
    castleGround: '#374151',
    marketGround: '#334155',
    cityWall: '#e2e8f0',
    cityWallShadow: 'rgba(0,0,0,0.6)',
    tower: '#cbd5e1',
    gate: '#f59e0b',
    arterialRoad: '#f8fafc',
    arterialRoadCasing: '#090d14',
    alley: '#64748b'
};

export default function Map17({ width = 800, height = 800 }: { width?: number; height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const canvasInstanceRef = useRef<Canvas | null>(null);

    // Generation Parameters
    const [seed, setSeed] = useState(1337);
    const [cellCount, setCellCount] = useState(350);
    const [relaxationPasses, setRelaxationPasses] = useState(3);
    const [wallRadius, setWallRadius] = useState(200);
    const [hasCoast, setHasCoast] = useState(true);
    const [hasRiver, setHasRiver] = useState(true);

    // Visual Layers Toggles
    const [showWalls, setShowWalls] = useState(true);
    const [showBuildings, setShowBuildings] = useState(true);
    const [showRoads, setShowRoads] = useState(true);
    const [showWards, setShowWards] = useState(true);
    const [showWater, setShowWater] = useState(true);
    const [showMesh, setShowMesh] = useState(false);

    const [cityData, setCityData] = useState<WatabouCity | null>(null);
    const [renderTrigger, setRenderTrigger] = useState(0);

    // Generate City
    const generate = useCallback(() => {
        const city = generateWatabouCity({
            width,
            height,
            seed,
            cellCount,
            relaxationPasses,
            hasCoast,
            hasRiver,
            wallRadius
        });
        setCityData(city);
    }, [width, height, seed, cellCount, relaxationPasses, wallRadius, hasCoast, hasRiver]);

    useEffect(() => {
        generate();
    }, [generate]);

    // Canvas pan / zoom interactions
    useEffect(() => {
        if (!canvasRef.current) return;
        const canvas = new Canvas(canvasRef.current);
        canvas.initInteractions({ onInteraction: () => setRenderTrigger(t => t + 1) });
        canvasInstanceRef.current = canvas;

        return () => {
            canvas.destroyInteractions();
        };
    }, [width, height]);

    // Render Canvas
    useEffect(() => {
        const canvas = canvasInstanceRef.current;
        if (!canvas || !cityData) return;

        const ctx = canvas.ctx;
        canvas.clear();

        // 1. Background Ground
        canvas.rect(-width * 4, -height * 4, width * 8, height * 8, { fill: PALETTE.ground });

        canvas.save();

        // 2. Draw Ward Ground Polygons
        if (showWards) {
            cityData.cells.forEach(cell => {
                if (cell.vertices.length < 3) return;

                let fill = PALETTE.ground;
                if (cell.isWater) {
                    fill = showWater ? PALETTE.water : PALETTE.ground;
                } else if (cell.wardType === 'FARMLAND') {
                    fill = PALETTE.farmland;
                } else if (cell.wardType === 'SLUM') {
                    fill = PALETTE.slumGround;
                } else if (cell.wardType === 'RESIDENTIAL') {
                    fill = PALETTE.residentialGround;
                } else if (cell.wardType === 'CRAFTSMEN') {
                    fill = PALETTE.craftsmenGround;
                } else if (cell.wardType === 'CASTLE') {
                    fill = PALETTE.castleGround;
                } else if (cell.wardType === 'MARKET') {
                    fill = PALETTE.marketGround;
                }

                ctx.beginPath();
                cell.vertices.forEach((v, i) => {
                    if (i === 0) ctx.moveTo(v.x, v.y);
                    else ctx.lineTo(v.x, v.y);
                });
                ctx.closePath();
                ctx.fillStyle = fill;
                ctx.fill();

                if (showMesh) {
                    ctx.strokeStyle = 'rgba(255,255,255,0.06)';
                    ctx.lineWidth = 0.8;
                    ctx.stroke();
                }
            });
        }

        // 3. Draw Coastlines & Water Waves
        if (showWater && hasCoast) {
            cityData.coastlines.forEach(([p0, p1]) => {
                ctx.beginPath();
                ctx.moveTo(p0.x, p0.y);
                ctx.lineTo(p1.x, p1.y);
                ctx.strokeStyle = PALETTE.coastline;
                ctx.lineWidth = 2.5;
                ctx.stroke();
            });
        }

        // 4. Draw River (Curved Ribbon)
        if (showWater && hasRiver && cityData.riverPath.length >= 2) {
            // River Bed / Deep Outline
            ctx.beginPath();
            cityData.riverPath.forEach((p, i) => {
                if (i === 0) ctx.moveTo(p.x, p.y);
                else ctx.lineTo(p.x, p.y);
            });
            ctx.lineCap = 'round';
            ctx.lineJoin = 'round';
            ctx.strokeStyle = PALETTE.river;
            ctx.lineWidth = 14;
            ctx.stroke();

            // River Water Highlight
            ctx.beginPath();
            cityData.riverPath.forEach((p, i) => {
                if (i === 0) ctx.moveTo(p.x, p.y);
                else ctx.lineTo(p.x, p.y);
            });
            ctx.strokeStyle = '#7dd3fc';
            ctx.lineWidth = 6;
            ctx.stroke();
        }

        // 5. Draw Primary Arterial Roads & Alleys
        if (showRoads) {
            // Casing
            cityData.arterialRoads.forEach(path => {
                ctx.beginPath();
                path.forEach((p, i) => {
                    if (i === 0) ctx.moveTo(p.x, p.y);
                    else ctx.lineTo(p.x, p.y);
                });
                ctx.lineCap = 'round';
                ctx.lineJoin = 'round';
                ctx.strokeStyle = PALETTE.arterialRoadCasing;
                ctx.lineWidth = 7;
                ctx.stroke();
            });

            // Centerline
            cityData.arterialRoads.forEach(path => {
                ctx.beginPath();
                path.forEach((p, i) => {
                    if (i === 0) ctx.moveTo(p.x, p.y);
                    else ctx.lineTo(p.x, p.y);
                });
                ctx.strokeStyle = PALETTE.arterialRoad;
                ctx.lineWidth = 3.5;
                ctx.stroke();
            });
        }

        // 6. Draw Inset Building Parcels & Pitched Roofs
        if (showBuildings) {
            cityData.cells.forEach(cell => {
                cell.buildings.forEach(b => {
                    if (b.polygon.length < 3) return;

                    // Building Drop Shadow
                    ctx.beginPath();
                    b.polygon.forEach((v, i) => {
                        if (i === 0) ctx.moveTo(v.x + 1.5, v.y + 2.5);
                        else ctx.lineTo(v.x + 1.5, v.y + 2.5);
                    });
                    ctx.closePath();
                    ctx.fillStyle = 'rgba(0, 0, 0, 0.45)';
                    ctx.fill();

                    // Building Roof Fill
                    ctx.beginPath();
                    b.polygon.forEach((v, i) => {
                        if (i === 0) ctx.moveTo(v.x, v.y);
                        else ctx.lineTo(v.x, v.y);
                    });
                    ctx.closePath();
                    ctx.fillStyle = b.roofColor;
                    ctx.fill();

                    // Roof Eaves Border
                    ctx.strokeStyle = 'rgba(0, 0, 0, 0.5)';
                    ctx.lineWidth = 0.8;
                    ctx.stroke();

                    // Draw Central Ridge Line (Medieval Gable Roof effect)
                    if (b.polygon.length >= 4) {
                        const mid1x = (b.polygon[0].x + b.polygon[1].x) / 2;
                        const mid1y = (b.polygon[0].y + b.polygon[1].y) / 2;
                        const mid2x = (b.polygon[2].x + b.polygon[3].x) / 2;
                        const mid2y = (b.polygon[2].y + b.polygon[3].y) / 2;

                        ctx.beginPath();
                        ctx.moveTo(mid1x, mid1y);
                        ctx.lineTo(mid2x, mid2y);
                        ctx.strokeStyle = 'rgba(255, 255, 255, 0.35)';
                        ctx.lineWidth = 1.0;
                        ctx.stroke();
                    }
                });
            });
        }

        // 7. Draw City Walls, Towers, and Gates
        if (showWalls && cityData.cityWalls.length >= 3) {
            // Wall Shadow
            ctx.beginPath();
            cityData.cityWalls.forEach((v, i) => {
                if (i === 0) ctx.moveTo(v.x + 3, v.y + 4);
                else ctx.lineTo(v.x + 3, v.y + 4);
            });
            ctx.closePath();
            ctx.strokeStyle = PALETTE.cityWallShadow;
            ctx.lineWidth = 6;
            ctx.stroke();

            // Main Stone Curtain Wall
            ctx.beginPath();
            cityData.cityWalls.forEach((v, i) => {
                if (i === 0) ctx.moveTo(v.x, v.y);
                else ctx.lineTo(v.x, v.y);
            });
            ctx.closePath();
            ctx.strokeStyle = PALETTE.cityWall;
            ctx.lineWidth = 3.8;
            ctx.stroke();

            // Defensive Towers
            cityData.towers.forEach(t => {
                ctx.beginPath();
                ctx.arc(t.x + 1.5, t.y + 2, 4.5, 0, Math.PI * 2);
                ctx.fillStyle = 'rgba(0,0,0,0.5)';
                ctx.fill();

                ctx.beginPath();
                ctx.arc(t.x, t.y, 4.2, 0, Math.PI * 2);
                ctx.fillStyle = PALETTE.tower;
                ctx.fill();
                ctx.strokeStyle = '#475569';
                ctx.lineWidth = 1;
                ctx.stroke();
            });

            // City Gates
            cityData.gates.forEach(g => {
                ctx.beginPath();
                ctx.arc(g.point.x, g.point.y, 6.5, 0, Math.PI * 2);
                ctx.fillStyle = PALETTE.gate;
                ctx.fill();
                ctx.strokeStyle = '#ffffff';
                ctx.lineWidth = 1.5;
                ctx.stroke();
            });
        }

        canvas.restore();
    }, [cityData, showWalls, showBuildings, showRoads, showWards, showWater, showMesh, renderTrigger, width, height]);

    return (
        <div
            style={{
                width: '100%',
                height: '100%',
                position: 'relative',
                overflow: 'hidden',
                background: PALETTE.ground,
                fontFamily: 'Outfit, sans-serif'
            }}
        >
            <canvas
                ref={canvasRef}
                width={width}
                height={height}
                style={{ display: 'block', width: '100%', height: '100%', cursor: 'grab' }}
            />

            {/* UI Parameter Controls */}
            <div
                style={{
                    position: 'absolute',
                    top: 16,
                    left: 16,
                    display: 'flex',
                    flexDirection: 'column',
                    gap: 10,
                    zIndex: 10,
                    background: 'rgba(10, 15, 26, 0.92)',
                    backdropFilter: 'blur(12px)',
                    padding: '16px',
                    borderRadius: 12,
                    border: '1px solid rgba(59, 130, 246, 0.25)',
                    maxWidth: 320,
                    boxShadow: '0 8px 32px rgba(0,0,0,0.5)'
                }}
            >
                <div style={{ display: 'flex', gap: 8 }}>
                    <button
                        onClick={() => setSeed(Math.floor(Math.random() * 999999))}
                        style={{
                            flex: 1,
                            background: '#3b82f6',
                            color: 'white',
                            border: 'none',
                            padding: '8px 14px',
                            borderRadius: 6,
                            cursor: 'pointer',
                            fontWeight: 700,
                            fontSize: '0.8rem',
                            letterSpacing: '0.5px'
                        }}
                    >
                        🎲 RANDOMIZE CITY
                    </button>
                    <button
                        onClick={() => setShowMesh(!showMesh)}
                        style={{
                            background: showMesh ? 'rgba(59, 130, 246, 0.3)' : 'rgba(255,255,255,0.06)',
                            color: showMesh ? '#60a5fa' : '#94a3b8',
                            border: '1px solid rgba(255,255,255,0.1)',
                            padding: '8px 12px',
                            borderRadius: 6,
                            cursor: 'pointer',
                            fontSize: '0.75rem',
                            fontWeight: 600
                        }}
                    >
                        MESH
                    </button>
                </div>

                {/* Voronoi Cell Density */}
                <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.75rem', color: '#cbd5e1' }}>
                        <span style={{ fontWeight: 700, color: '#60a5fa' }}>VORONOI CELLS</span>
                        <span style={{ fontWeight: 600 }}>{cellCount}</span>
                    </div>
                    <input
                        type="range"
                        min="150"
                        max="700"
                        value={cellCount}
                        onChange={e => setCellCount(Number(e.target.value))}
                        style={{ cursor: 'pointer', accentColor: '#3b82f6', width: '100%' }}
                    />
                </div>

                {/* Lloyd's Relaxation Passes */}
                <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.75rem', color: '#cbd5e1' }}>
                        <span style={{ fontWeight: 700, color: '#60a5fa' }}>LLOYD'S RELAXATION</span>
                        <span style={{ fontWeight: 600 }}>{relaxationPasses} passes</span>
                    </div>
                    <input
                        type="range"
                        min="0"
                        max="6"
                        value={relaxationPasses}
                        onChange={e => setRelaxationPasses(Number(e.target.value))}
                        style={{ cursor: 'pointer', accentColor: '#3b82f6', width: '100%' }}
                    />
                </div>

                {/* Wall Radius */}
                <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.75rem', color: '#cbd5e1' }}>
                        <span style={{ fontWeight: 700, color: '#60a5fa' }}>CITY WALL RADIUS</span>
                        <span style={{ fontWeight: 600 }}>{wallRadius}px</span>
                    </div>
                    <input
                        type="range"
                        min="100"
                        max="350"
                        value={wallRadius}
                        onChange={e => setWallRadius(Number(e.target.value))}
                        style={{ cursor: 'pointer', accentColor: '#3b82f6', width: '100%' }}
                    />
                </div>

                {/* Geography Toggles */}
                <div style={{ display: 'flex', gap: 6 }}>
                    <button
                        onClick={() => setHasCoast(!hasCoast)}
                        style={{
                            flex: 1,
                            padding: '6px',
                            fontSize: '0.7rem',
                            borderRadius: 6,
                            border: 'none',
                            cursor: 'pointer',
                            fontWeight: hasCoast ? 700 : 500,
                            background: hasCoast ? '#0284c7' : 'rgba(255,255,255,0.06)',
                            color: hasCoast ? 'white' : '#94a3b8'
                        }}
                    >
                        🌊 COASTLINE
                    </button>
                    <button
                        onClick={() => setHasRiver(!hasRiver)}
                        style={{
                            flex: 1,
                            padding: '6px',
                            fontSize: '0.7rem',
                            borderRadius: 6,
                            border: 'none',
                            cursor: 'pointer',
                            fontWeight: hasRiver ? 700 : 500,
                            background: hasRiver ? '#0284c7' : 'rgba(255,255,255,0.06)',
                            color: hasRiver ? 'white' : '#94a3b8'
                        }}
                    >
                        🏞️ RIVER
                    </button>
                </div>

                {/* Visual Layers */}
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 4, marginTop: 4 }}>
                    {[
                        { label: 'WALLS', val: showWalls, set: setShowWalls },
                        { label: 'HOUSES', val: showBuildings, set: setShowBuildings },
                        { label: 'ROADS', val: showRoads, set: setShowRoads },
                        { label: 'WARDS', val: showWards, set: setShowWards },
                        { label: 'WATER', val: showWater, set: setShowWater }
                    ].map(item => (
                        <button
                            key={item.label}
                            onClick={() => item.set(!item.val)}
                            style={{
                                padding: '5px 2px',
                                fontSize: '0.68rem',
                                borderRadius: 4,
                                border: 'none',
                                cursor: 'pointer',
                                fontWeight: item.val ? 700 : 500,
                                background: item.val ? 'rgba(59, 130, 246, 0.4)' : 'rgba(255,255,255,0.04)',
                                color: item.val ? '#93c5fd' : '#64748b'
                            }}
                        >
                            {item.label}
                        </button>
                    ))}
                </div>
            </div>

            {/* Bottom Title */}
            <div style={{ position: 'absolute', bottom: 20, right: 20, textAlign: 'right', pointerEvents: 'none' }}>
                <h1 style={{ color: 'white', margin: 0, fontSize: '1.8rem', fontWeight: 800, letterSpacing: '1px' }}>
                    MEDIEVAL FANTASY CITY
                </h1>
                <p style={{ color: '#94a3b8', margin: 0, fontSize: '0.85rem' }}>
                    WATABOU-INSPIRED VORONOI PIPELINE
                </p>
            </div>
        </div>
    );
}
