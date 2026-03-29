export default function Map6Explanation() {
    return (
        <div style={{ color: '#fff', fontSize: '15px', lineHeight: 1.6 }}>
            <h2 style={{ marginTop: 0, color: '#e0e0e0' }}>Recursive Plant Growth</h2>
            <p>
                This map simulates the organic growth of a branching structure (a plant or coral) using a recursive algorithm.
            </p>

            <ul style={{ paddingLeft: '20px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                <li>
                    <strong>1. Recursive Sprouting:</strong>
                    Every frame, the plant attempts to grow a new "node" from a leaf or branch. Each node inherits its size and color from its parent, with slight random variations (entropy).
                </li>
                <li>
                    <strong>2. Constraint-Based Growth:</strong>
                    Growth is governed by strict rules:
                    <ul>
                        <li>Nodes cannot grow beyond the canvas borders.</li>
                        <li>Nodes must be at least 10px in diameter.</li>
                        <li>New nodes must not intersect with any existing part of the plant structure (Collision Detection).</li>
                    </ul>
                </li>
                <li>
                    <strong>3. Tree Pruning:</strong>
                    The entire structure is a <em>Recursive Tree</em> data structure. You can interactively click any node to "prune" it. This removes the selected branch and all its descendants from the memory, allowing the plant to refocus growth on other sections.
                </li>
                <li>
                    <strong>4. Animation Loop:</strong>
                    Unlike static maps, this uses an active animation loop (<code>requestAnimationFrame</code>) to continuously poll for growth opportunities, creating a "live" simulation.
                </li>
            </ul>
        </div>
    );
}
