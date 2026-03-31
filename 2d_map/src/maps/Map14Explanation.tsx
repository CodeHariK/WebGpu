import React from 'react';
import { PATTERNS, TILE_REGISTRY } from './Map14Logic';

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
    grid: { display: 'flex', flexWrap: 'wrap' as const, gap: '15px' },
    tileItem: { 
        display: 'flex', 
        flexDirection: 'column' as const, 
        alignItems: 'center', 
        background: 'rgba(255,255,255,0.03)',
        padding: '10px',
        borderRadius: '8px',
        width: '100px'
    }
};

const TilePreview: React.FC<{ tile: any, size: number }> = ({ tile, size }) => {
    const canvasRef = React.useRef<HTMLCanvasElement>(null);
    React.useEffect(() => {
        if (!canvasRef.current || !tile.canvas) return;
        const ctx = canvasRef.current.getContext('2d')!;
        ctx.clearRect(0, 0, size * 3, size * 3);
        ctx.drawImage(tile.canvas, 0, 0, size * 3, size * 3);
    }, [tile, size]);

    return <canvas ref={canvasRef} width={size * 3} height={size * 3} style={{ width: size * 1.5, height: size * 1.5 }} />;
};

export default function Map14Explanation() {
    // Only show the first rotation of each base pattern
    const samples = TILE_REGISTRY.filter((_, i) => i % 6 === 0);

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
                            <TilePreview tile={tile} size={20} />
                            <span style={{ fontSize: '0.65rem', color: '#f8fafc', textAlign: 'center', marginTop: '5px' }}>
                                {PATTERNS[i].name}
                            </span>
                        </div>
                    ))}
                </div>
            </section>

            <section style={EXPLANATION_STYLES.section}>
                <span style={EXPLANATION_STYLES.header}>Generative Logic</span>
                <p>
                    Every time you click <b>RANDOMIZE</b>, the entire grid is recalculated using a deterministic 
                    Pseudo-Random Number Generator (PRNG). Each cell picks one of the 36 possible tiles in the 
                    registry (6 base patterns, each with 6 rotational variants).
                </p>
            </section>
        </div>
    );
}
