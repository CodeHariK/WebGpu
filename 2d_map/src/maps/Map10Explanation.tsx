

export default function Map10Explanation() {
    return (
        <div style={EXPLANATION_STYLES.container}>
            <h2 style={EXPLANATION_STYLES.title}>Voronoi Territory Generation</h2>
            <h3 style={EXPLANATION_STYLES.subtitle}>1. Stable Jittered Grid</h3>
            <p style={EXPLANATION_STYLES.paragraph}>
                In contrast to pure random distribution, we use a <strong>Jittered Grid</strong>. Points are sampled
                on a regular grid and then slightly offset. This ensures a uniform distribution and rock-solid
                Voronoi geometry, preventing the "spiky" artifacts seen in unstable triangulations.
            </p>

            <h3 style={EXPLANATION_STYLES.subtitle}>2. Distributed Radial "Halos"</h3>
            <p style={EXPLANATION_STYLES.paragraph}>
                Instead of simple merging, we pick $K$ <strong>Distributed Epicenters</strong>. Using
                <strong>Poisson Rejection Sampling</strong>, we ensure that hubs are spread across the map
                rather than clumping. A <strong>Breadth-First Search (BFS)</strong> then steps opacity
                based on distance, creating vibrant cores that fade out radially.
            </p>

            <h3 style={EXPLANATION_STYLES.subtitle}>3. Validated Ring Roads</h3>
            <p style={EXPLANATION_STYLES.paragraph}>
                We generate Concentric Ring Roads by identifying sites at the same graph depth and color.
                A graph traversal algorithm verifies that these sites form a <strong>closed topological
                    circuit</strong> before rendering them as smooth splines.
            </p>

            <h3 style={EXPLANATION_STYLES.subtitle}>4. Inter-Cluster Highways</h3>
            <p style={EXPLANATION_STYLES.paragraph}>
                Grand infrastructure is created using a <strong>Global BFS</strong> across the entire Voronoi
                mesh to find the shortest topological route between epicenters. This forms a high-level
                <strong>Regional Network</strong> that bridges different territory hubs.
            </p>

            <h3 style={EXPLANATION_STYLES.subtitle}>4. Black Boundary Rendering</h3>
            <p style={EXPLANATION_STYLES.paragraph}>
                Every geometric edge of the Voronoi partition is rendered in solid <strong>black</strong>.
                This highlights the underlying mathematical structure and provides high contrast for the
                radial color steps.
            </p>

            <div style={EXPLANATION_STYLES.tip}>
                <strong>Tech Tip:</strong> Enable the "Sites" toggle to see the original point distribution
                that drives the geometry.
            </div>
        </div>
    );
}

const EXPLANATION_STYLES = {
    container: {
        color: '#e2e8f0',
        fontFamily: '"Outfit", sans-serif',
        lineHeight: '1.6',
    },
    title: {
        color: '#3b82f6',
        fontSize: '24px',
        marginBottom: '16px',
        fontWeight: '800',
        letterSpacing: '-0.5px'
    },
    subtitle: {
        color: '#60a5fa',
        fontSize: '14px',
        textTransform: 'uppercase',
        letterSpacing: '1px',
        margin: '24px 0 8px 0',
        fontWeight: '700'
    },
    paragraph: {
        fontSize: '15px',
        marginBottom: '12px',
        color: '#94a3b8'
    },
    tip: {
        marginTop: '32px',
        padding: '16px',
        background: 'rgba(59, 130, 246, 0.1)',
        borderLeft: '4px solid #3b82f6',
        borderRadius: '8px',
        fontSize: '14px',
        color: '#cbd5e1'
    }
} as const;
