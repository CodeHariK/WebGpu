import type { CSSProperties } from 'react';

export default function Map13Explanation() {
    return (
        <div style={EXPLANATION_STYLES.container}>
            <h2 style={EXPLANATION_STYLES.title}>Hex Growth Prototype</h2>
            <p style={EXPLANATION_STYLES.paragraph}>
                This experiment explores a basic **Stochastic Growth** model on a hexagonal grid using
                axial coordinates (q, r).
            </p>

            <h3 style={EXPLANATION_STYLES.subtitle}>1. Axial Coordinates</h3>
            <p style={EXPLANATION_STYLES.paragraph}>
                In a "Pointy-Top" hex grid, <code>hexToPixel</code> converts the axial <code>(q, r)</code>
                values to 2D Cartesian coordinates <code>(x, y)</code> using the standard formulas involving
                <code>sqrt(3)</code>. This ensures perfect alignment and consistent distance between neighbors.
            </p>

            <h3 style={EXPLANATION_STYLES.subtitle}>2. Nested Boundary Loops</h3>
            <p style={EXPLANATION_STYLES.paragraph}>
                Instead of drawing individual hex faces, the system chains all "air-facing" edges into 
                continuous topological loops. By applying the **Shoelace Formula**, we calculate the 
                signed area to distinguish between:
            </p>
            <ul style={{ ...EXPLANATION_STYLES.paragraph, paddingLeft: '20px' }}>
                <li><span style={{ color: '#22d3ee', fontWeight: 'bold' }}>Cyan Loops</span>: Outer coastlines (Positive Area).</li>
                <li><span style={{ color: '#f472b6', fontWeight: 'bold' }}>Pink Loops</span>: Internal lakes/holes (Negative Area).</li>
            </ul>

            <h3 style={EXPLANATION_STYLES.subtitle}>3. Island Connectivity (MST)</h3>
            <p style={EXPLANATION_STYLES.paragraph}>
                If growth produces isolated clusters, the "Connect Islands" function uses a
                **Flood Fill** to identify all distinct islands. It then uses a
                **Minimum Spanning Tree (MST)** approach to find the absolute shortest
                distances between these clusters and bridges them using **Axial Linear Interpolation**.
            </p>

            <div style={EXPLANATION_STYLES.tip}>
                <strong>Tip:</strong> Try spawning many seeds with small growth to see the
                automated bridging in action.
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
