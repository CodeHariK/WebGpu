import React, { useEffect, useRef } from 'react';
import { Vector2 } from '../lib/Vector2';
import {
    PATTERNS,
    TILE_REGISTRY,
    type TileConfig,
    getHexVertices,
    drawTileRoads
} from './Map14Logic';

const EXPLANATION_STYLES = {
    container: {
        background: 'rgba(10, 11, 18, 0.9)',
        color: '#94a3b8',
        padding: '24px',
        borderRadius: '12px',
        border: '1px solid rgba(59, 130, 246, 0.2)',
        maxWidth: '800px',
        fontSize: '0.9rem',
        lineHeight: '1.6',
        maxHeight: '80vh',
        overflowY: 'auto' as const
    },
    title: { color: '#f8fafc', marginBottom: '20px', fontWeight: 800, fontSize: '1.5rem' },
    section: { marginBottom: '20px' },
    header: { color: '#60a5fa', fontWeight: 700, display: 'block', marginBottom: '8px', textTransform: 'uppercase' as const, fontSize: '0.75rem' },
    grid: { display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px' },
    tileItem: {
        display: 'flex',
        flexDirection: 'column' as const,
        alignItems: 'center',
        background: 'rgba(255,255,255,0.03)',
        padding: '12px 8px',
        borderRadius: '8px',
        border: '1px solid rgba(255, 255, 255, 0.05)'
    }
};

const TilePreview: React.FC<{ tile: TileConfig, size: number }> = ({ tile, size }) => {
    const canvasRef = useRef<HTMLCanvasElement>(null);

    useEffect(() => {
        if (!canvasRef.current) return;
        const canvas = canvasRef.current;
        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        ctx.clearRect(0, 0, canvas.width, canvas.height);

        const center = new Vector2(canvas.width / 2, canvas.height / 2);
        const hexV = getHexVertices(center, size);

        // Draw hexagon background & boundary
        ctx.beginPath();
        hexV.forEach((pt, i) => {
            if (i === 0) ctx.moveTo(pt.x, pt.y);
            else ctx.lineTo(pt.x, pt.y);
        });
        ctx.closePath();
        ctx.fillStyle = 'rgba(15, 23, 42, 0.7)';
        ctx.fill();
        ctx.strokeStyle = 'rgba(59, 130, 246, 0.3)';
        ctx.lineWidth = 1;
        ctx.stroke();

        // Draw tile connections
        drawTileRoads(ctx, center, size, tile, '#60a5fa', 0.16);
    }, [tile, size]);

    return (
        <canvas
            ref={canvasRef}
            width={size * 3}
            height={size * 3}
            style={{ width: size * 2, height: size * 2 }}
        />
    );
};

export default function Map14Explanation() {
    // 6 base patterns (rotation #0 for each)
    const samples = PATTERNS.map((_, i) => TILE_REGISTRY[i * 6]);

    return (
        <div style={EXPLANATION_STYLES.container}>
            <h1 style={EXPLANATION_STYLES.title}>HEXAGONAL TRUCHET NETWORK</h1>

            <section style={EXPLANATION_STYLES.section}>
                <span style={EXPLANATION_STYLES.header}>Overview</span>
                <p>
                    Map 14 is a minimalist generative network utilizing <b>Hexagonal Truchet</b> tiles.
                    Unlike traditional square Truchet tiles, hexagons provide six possible entry/exit points,
                    leading to more organic and complex path structures.
                </p>
            </section>

            <section style={EXPLANATION_STYLES.section}>
                <span style={EXPLANATION_STYLES.header}>The 6 Fundamental Patterns</span>
                <div style={EXPLANATION_STYLES.grid}>
                    {samples.map((tile, i) => (
                        <div key={i} style={EXPLANATION_STYLES.tileItem}>
                            <TilePreview tile={tile} size={22} />
                            <span style={{ fontSize: '0.7rem', color: '#f8fafc', textAlign: 'center', marginTop: '6px', fontWeight: 600 }}>
                                {PATTERNS[i].name}
                            </span>
                        </div>
                    ))}
                </div>
            </section>

            <section style={EXPLANATION_STYLES.section}>
                <span style={EXPLANATION_STYLES.header}>Generative Logic</span>
                <p>
                    Every time you click <b>RANDOMIZE</b>, the entire grid is generated outward from the origin
                    using a Breadth-First Search (BFS) constraint-satisfaction solver. Each cell picks one of the
                    36 possible tiles in the registry (6 base patterns, each with 6 rotational variants) such that
                    all road ports connect seamlessly across adjacent hexagon edges with verified C1 continuity.
                </p>
            </section>
        </div>
    );
}
