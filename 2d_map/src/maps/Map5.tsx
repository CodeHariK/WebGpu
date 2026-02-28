import { useEffect, useRef, useState } from 'react';

export default function Map5({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);

    // UI State Controls
    const [n, setN] = useState(2);
    const [tileSize, setTileSize] = useState(1000);
    const [zoom, setZoom] = useState(0.5);
    const [seed, setSeed] = useState(1234567);
    const [showDebug, setShowDebug] = useState(false);

    const handleRegenerate = () => {
        setSeed(Math.floor(Math.random() * 10000000));
    };

    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        const tileCanvas = document.createElement('canvas');
        tileCanvas.width = tileSize;
        tileCanvas.height = tileSize;
        const tileCtx = tileCanvas.getContext('2d');
        if (!tileCtx) return;

        generateRandomRoadTile(tileCtx);

        // --- DRAW TO MAIN CANVAS (Infinite Grid) ---
        ctx.fillStyle = '#b7e4c7'; // Grass background
        ctx.fillRect(0, 0, width, height);

        ctx.save();
        ctx.scale(zoom, zoom);

        let cols = Math.ceil((width / zoom) / tileSize) + 1;
        let rows = Math.ceil((height / zoom) / tileSize) + 1;

        for (let i = 0; i < cols; i++) {
            for (let j = 0; j < rows; j++) {
                ctx.drawImage(tileCanvas, i * tileSize, j * tileSize);

                // Draw tile boundaries for easier debugging
                if (showDebug) {
                    ctx.strokeStyle = 'rgba(255, 0, 0, 0.5)';
                    ctx.lineWidth = 4;
                    ctx.strokeRect(i * tileSize, j * tileSize, tileSize, tileSize);
                }
            }
        }
        ctx.restore();

        // --- PIPELINE FOR MAP 5 ---
        function generateRandomRoadTile(tCtx: CanvasRenderingContext2D) {
            // Background grass
            tCtx.fillStyle = '#b7e4c7';
            tCtx.fillRect(0, 0, tileSize, tileSize);

            // 1. Define Fixed Edge Points
            // We divide each side into `n` connections. 
            const segment = tileSize / n;
            const offset = segment / 2; // Centers of the segments

            const nodes: { x: number, y: number, side: string }[] = [];

            // Generate Top algorithms (y=0)
            for (let i = 0; i < n; i++) nodes.push({ x: (i * segment) + offset, y: 0, side: 'top' });
            // Generate Bottom (y=tileSize)
            for (let i = 0; i < n; i++) nodes.push({ x: (i * segment) + offset, y: tileSize, side: 'bottom' });
            // Generate Left (x=0)
            for (let i = 0; i < n; i++) nodes.push({ x: 0, y: (i * segment) + offset, side: 'left' });
            // Generate Right (x=tileSize)
            for (let i = 0; i < n; i++) nodes.push({ x: tileSize, y: (i * segment) + offset, side: 'right' });

            // Randomly pair them up!
            let currentSeed = seed;
            function random() {
                currentSeed = (currentSeed * 1664525 + 1013904223) % 4294967296;
                return currentSeed / 4294967296;
            }

            // Fisher-Yates shuffle
            for (let i = nodes.length - 1; i > 0; i--) {
                const j = Math.floor(random() * (i + 1));
                [nodes[i], nodes[j]] = [nodes[j], nodes[i]];
            }

            const getControlPoint = (node: { x: number, y: number, side: string }) => {
                const CP_DIST = tileSize * 0.4; // Push inward perfectly perpendicular
                if (node.side === 'top') return { cx: node.x, cy: node.y + CP_DIST };
                if (node.side === 'bottom') return { cx: node.x, cy: node.y - CP_DIST };
                if (node.side === 'left') return { cx: node.x + CP_DIST, cy: node.y };
                return { cx: node.x - CP_DIST, cy: node.y }; // Right
            };

            const drawRoad = (n1: { x: number, y: number, side: string }, n2: { x: number, y: number, side: string }) => {
                const cp1 = getControlPoint(n1);
                const cp2 = getControlPoint(n2);

                const drawCurve = (weight: number, color: string, dash: number[] = []) => {
                    tCtx.strokeStyle = color;
                    tCtx.lineWidth = weight;
                    tCtx.setLineDash(dash);
                    tCtx.lineCap = 'round';
                    tCtx.lineJoin = 'round';
                    tCtx.beginPath();
                    tCtx.moveTo(n1.x, n1.y);
                    tCtx.bezierCurveTo(cp1.cx, cp1.cy, cp2.cx, cp2.cy, n2.x, n2.y);
                    tCtx.stroke();
                    tCtx.setLineDash([]); // Reset
                };

                // Shadow / Edge
                drawCurve(46, '#757575');
                // Main asphalt
                drawCurve(40, '#9e9e9e');
                // Dashed center line
                drawCurve(2, '#ffffff', [15, 15]);

                // DEBUG HANDLES
                if (showDebug) {
                    // Draw Line from Anchor to Control Point
                    tCtx.strokeStyle = 'magenta';
                    tCtx.lineWidth = 4;
                    tCtx.beginPath();
                    tCtx.moveTo(n1.x, n1.y);
                    tCtx.lineTo(cp1.cx, cp1.cy);
                    tCtx.stroke();

                    tCtx.strokeStyle = 'cyan';
                    tCtx.lineWidth = 4;
                    tCtx.beginPath();
                    tCtx.moveTo(n2.x, n2.y);
                    tCtx.lineTo(cp2.cx, cp2.cy);
                    tCtx.stroke();

                    // Draw Control Points
                    tCtx.fillStyle = 'magenta';
                    tCtx.beginPath(); tCtx.arc(cp1.cx, cp1.cy, 12, 0, Math.PI * 2); tCtx.fill();
                    tCtx.fillStyle = 'cyan';
                    tCtx.beginPath(); tCtx.arc(cp2.cx, cp2.cy, 12, 0, Math.PI * 2); tCtx.fill();

                    // Draw Edge Anchor point
                    tCtx.fillStyle = '#ffff00';
                    tCtx.beginPath(); tCtx.arc(n1.x, n1.y, 8, 0, Math.PI * 2); tCtx.fill();
                    tCtx.beginPath(); tCtx.arc(n2.x, n2.y, 8, 0, Math.PI * 2); tCtx.fill();
                }
            };

            // Draw pairs!
            for (let i = 0; i < nodes.length; i += 2) {
                drawRoad(nodes[i], nodes[i + 1]);
            }
        }

    }, [width, height, n, tileSize, zoom, seed, showDebug]);

    return (
        <div style={{ position: 'relative', width, height }}>
            {/* Control Panel Overlay */}
            <div style={{
                position: 'absolute',
                top: 10,
                left: 10,
                background: 'rgba(0, 0, 0, 0.7)',
                padding: '12px',
                borderRadius: '8px',
                color: 'white',
                fontFamily: 'sans-serif',
                display: 'flex',
                flexDirection: 'column',
                gap: '12px',
                zIndex: 10
            }}>
                <button
                    onClick={handleRegenerate}
                    style={{ background: '#3b82f6', color: 'white', border: 'none', padding: '8px', borderRadius: '4px', cursor: 'pointer', fontWeight: 'bold' }}>
                    Regenerate Layout
                </button>

                <label style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer' }}>
                    <input type="checkbox" checked={showDebug} onChange={e => setShowDebug(e.target.checked)} />
                    Show Bezier Handles
                </label>

                <label style={{ display: 'flex', justifyContent: 'space-between', width: '200px' }}>
                    <span>Connections (n): {n}</span>
                    <input type="range" min="1" max="10" step="1" value={n} onChange={e => setN(Number(e.target.value))} />
                </label>
                <label style={{ display: 'flex', justifyContent: 'space-between', width: '200px' }}>
                    <span>Tile Size: {tileSize}px</span>
                    <input type="range" min="300" max="2000" step="100" value={tileSize} onChange={e => setTileSize(Number(e.target.value))} />
                </label>
                <label style={{ display: 'flex', justifyContent: 'space-between', width: '200px' }}>
                    <span>Zoom: {zoom.toFixed(2)}x</span>
                    <input type="range" min="0.1" max="1.5" step="0.05" value={zoom} onChange={e => setZoom(Number(e.target.value))} />
                </label>
            </div>

            <canvas ref={canvasRef} width={width} height={height} style={{ borderRadius: 8, display: 'block', margin: '0 auto', boxShadow: '0 4px 6px rgba(0,0,0,0.3)' }} />
        </div>
    );
}
