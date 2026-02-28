export default function Map5Explanation() {
    return (
        <div style={{ color: '#fff', fontSize: '15px', lineHeight: 1.6 }}>
            <h2 style={{ marginTop: 0, color: '#e0e0e0' }}>Grid Tiling</h2>
            <p>
                This map focuses purely on ensuring infinite, visually seamless tiling across a grid using evenly spaced node connections and Bezier curves.
            </p>

            <ul style={{ paddingLeft: '20px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                <li>
                    <strong>1. Flexible Boundary Nodes:</strong>
                    Every 1000x1000 tile divides its 4 edges (North, South, East, West) into <code>n</code> equal segments. The center of each segment serves as a fixed road connection node (resulting in exactly <code>4n</code> total nodes per tile). This guarantees perfect alignment with adjacent tiles matching the same <code>n</code>.
                </li>
                <li>
                    <strong>2. Perpendicular Inward Entry:</strong>
                    To prevent roads from zig-zagging harshly across the tile seams, we mathematically push a Bezier Control Point deep into the tile (using <code>TILE_SIZE * 0.4</code>) exactly perpendicular to the edge the node lies on.
                </li>
                <li>
                    <strong>3. Random Pairing:</strong>
                    We use a deterministic Pseudo-Random Number Generator (PRNG) heavily seeding a Fisher-Yates array shuffle. This pairs up all the edge nodes randomly, routing traffic organically through the tile in wild, chaotic overlapping highways.
                </li>
                <li>
                    <strong>4. Infinite Scalability:</strong>
                    When you scale the viewport out to 50%, you can see the 1000x1000 macro-tiles repeating across the infinite Canvas plane, but the generated web of Bezier pathways bridges the gaps flawlessly!
                </li>
            </ul>
        </div>
    );
}
