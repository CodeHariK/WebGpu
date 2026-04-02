import React from 'react';

const COLORS = {
    bg: '#050a15',
    accent: '#0ea5e9',
    text: '#94a3b8',
    shape: '#38bdf8',
    shapeGlow: 'rgba(56, 189, 248, 0.2)',
    circle: '#22d3ee',
    circleGlow: 'rgba(34, 211, 238, 0.1)'
};

const Map16Explanation: React.FC = () => {
    const accent = COLORS.accent;

    return (
        <div style={{ padding: '20px', color: COLORS.text, lineHeight: '1.6', fontFamily: 'Inter, sans-serif' }}>
            <h2 style={{ color: 'white', marginBottom: '15px', borderBottom: `2px solid ${accent}`, paddingBottom: '10px' }}>
                Map 16: Sequential Shape Packing
            </h2>
            
            <p>
                This algorithm demonstrates a high-performance <strong>Circle Packing</strong> strategy 
                to place complex composite shapes (Rectangles and Triangles) within a constrained space.
            </p>

            <h3 style={{ color: accent, marginTop: '25px', fontSize: '1.1rem' }}>1. The Growth Phase</h3>
            <p>
                Every shape begins as a tiny seed. It grows iteratively, increasing its radius or scale until 
                it detects a collision with a neighbor or the canvas boundary. This results in charactersitic 
                tightly-packed "organic" layouts.
            </p>

            <h3 style={{ color: accent, marginTop: '25px', fontSize: '1.1rem' }}>2. Spatial Hashing</h3>
            <p>
                To handle up to <strong>200,000</strong> placement attempts without freezing the browser, the map uses 
                a <strong>Grid-based Spatial Hash</strong>. Instead of checking every circle against every other circle 
                (O(N²)), it only checks the immediate grid cells the new circle touches (O(1)).
            </p>

            <h3 style={{ color: accent, marginTop: '25px', fontSize: '1.1rem' }}>3. Composite Tracing</h3>
            <p>
                Complex shapes like rectangles and triangles are treated as a "cloud" of small circles 
                tracing their perimeter. The algorithm validates the entire cloud before committing the shape, 
                ensuring perfect topological non-interference.
            </p>

            <div style={{ marginTop: '30px', padding: '15px', background: 'rgba(255,255,255,0.03)', borderRadius: '8px', borderLeft: `4px solid ${accent}` }}>
                <strong>Tip:</strong> Increase the <strong>GRID DIVS</strong> to improve the precision of the 
                packing in tight spaces, though this adds memory overhead to the spatial hash.
            </div>
        </div>
    );
};

export default Map16Explanation;
