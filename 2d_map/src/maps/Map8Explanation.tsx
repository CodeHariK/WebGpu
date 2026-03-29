export default function Map8Explanation() {
    return (
        <div style={{ color: '#fff', fontSize: '15px', lineHeight: 1.6 }}>
            <h2 style={{ marginTop: 0, color: '#e0e0e0' }}>Multi-Source Biome Growth</h2>
            <p>
                This map explores competitive growth where each tree source represents a unique "Biome" with its own distinct color palette.
            </p>

            <ul style={{ paddingLeft: '20px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                <li>
                    <strong>1. Multiple Starters (N=3):</strong>
                    Unlike previous maps that start from a single point, this one initializes three unique trees at random, non-overlapping positions. They grow simultaneously, effectively "competing" for the available black space.
                </li>
                <li>
                    <strong>2. Space-Aware Collision (Node-to-Edge):</strong>
                    A sophisticated collision check ensures that new nodes don't just avoid other nodes, but also keep a safe distance (20px) from *any existing road segment* or branch. This prevents the "clumping" effect seen in simpler models.
                </li>
                <li>
                    <strong>3. Deterministic Intersection:</strong>
                    The algorithm uses a determinant-based math approach to identify and block any potential branch crossings. This ensures a clean, planar graph where branches never overlap.
                </li>
                <li>
                    <strong>4. High-Contrast Aesthetic:</strong>
                    Using a pure black background and stark white strokes, this map highlights the mathematical beauty of recursive branching and space colonization.
                </li>
            </ul>
        </div>
    );
}
