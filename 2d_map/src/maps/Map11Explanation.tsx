import type { CSSProperties } from 'react';

export default function Map11Explanation() {
    return (
        <div style={EXPLANATION_STYLES.container}>
            <h2 style={EXPLANATION_STYLES.title}>Static Metaball Map</h2>
            
            <h3 style={EXPLANATION_STYLES.subtitle}>1. Seed-Based Hubs</h3>
            <p style={EXPLANATION_STYLES.paragraph}>
                In contrast to a dynamic simulation, this map is <strong>Static</strong>. Hub 
                positions and radii are deterministically generated from a <strong>World Seed</strong>, 
                allowing for repeatable, stable infrastructure prototyping.
            </p>

            <h3 style={EXPLANATION_STYLES.subtitle}>2. Grid Cell Resolution (rez)</h3>
            <p style={EXPLANATION_STYLES.paragraph}>
                The resolution is controlled by the <strong>Grid Cell Size</strong> (in pixels). 
                A smaller cell size (e.g., 8px) provides a high-fidelity, smooth curve, while 
                a larger cell size (e.g., 32px) creates a sharp, blocky, "pixel-art" aesthetic 
                characteristic of early digital mapping.
            </p>

            <h3 style={EXPLANATION_STYLES.subtitle}>3. Full Canvas Potential Field</h3>
            <p style={EXPLANATION_STYLES.paragraph}>
                The algorithm samples the <strong>Inverse-Square Potential</strong> across the 
                entire canvas dimensions. <code>sum += r² / d²</code>. Marching Squares then 
                extracts the 1.0 threshold boundary using precise linear interpolation 
                along the cell edges.
            </p>

            <h3 style={EXPLANATION_STYLES.subtitle}>4. Technical Blueprint Layers</h3>
            <p style={EXPLANATION_STYLES.paragraph}>
                Additional <strong>ISO LAYERS</strong> act as analytical "shells," revealing 
                the depth and intensity of the field. This multi-layered approach helps 
                visualize how hubs influence their surroundings even before they fuse.
            </p>

            <div style={EXPLANATION_STYLES.tip}>
                <strong>Tech Tip:</strong> Set <strong>GRID CELL SIZE</strong> to 8px for 
                maximum smoothness, or 40px for a highly stylized, schematic look.
            </div>
        </div>
    );
}

const EXPLANATION_STYLES = {
    container: { fontFamily: '"Outfit", sans-serif', color: '#94a3b8', lineHeight: '1.6' } as CSSProperties,
    title: { color: '#f8fafc', fontSize: '24px', marginBottom: '16px', fontWeight: '700' } as CSSProperties,
    subtitle: { color: '#60a5fa', fontSize: '14px', marginTop: '24px', marginBottom: '8px', fontWeight: '700', letterSpacing: '1px', textTransform: 'uppercase' } as CSSProperties,
    paragraph: { fontSize: '15px', marginBottom: '12px' } as CSSProperties,
    tip: { marginTop: '32px', padding: '16px', background: 'rgba(59, 130, 246, 0.1)', borderRadius: '12px', border: '1px solid rgba(59, 130, 246, 0.2)', fontSize: '14px', color: '#bfdbfe' } as CSSProperties
};
