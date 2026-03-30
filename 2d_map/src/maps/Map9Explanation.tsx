export default function Map9Explanation() {
    return (
        <div style={{ color: '#fff', fontSize: '15px', lineHeight: 1.6 }}>
            <h2 style={{ marginTop: 0, color: '#e0e0e0' }}>Mansion Blueprint</h2>
            <p>
                This map simulates a structured, room-based layout typical of classic survival horror games like **Resident Evil**.
            </p>

            <ul style={{ paddingLeft: '20px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                <li>
                    <strong>1. Grid-Aligned Architecture (40px):</strong>
                    Every room, corridor, and rotunda is now snapped to a universal 40px grid. This ensures perfect alignment with the background blueprint and simplifies the "Room Merging" logic, creating a clean, CAD-like aesthetic.
                </li>
                <li>
                    <strong>2. Wall Anchoring & Sprouting:</strong>
                    The algorithm picks a random room and side, then calculates the new room's snapped coordinates based on its parent's grid position. This eliminates pixel-level gaps and offsets.
                </li>
                <li>
                    <strong>2. Hybrid Collision Logic:</strong>
                    To support both Rectangles and Circles, the engine now uses a multi-modal collision system:
                    <ul style={{ paddingLeft: '20px', fontSize: '13px', marginTop: '8px', opacity: 0.9 }}>
                        <li><strong>Rect-to-Rect:</strong> Standard AABB overlap check.</li>
                        <li><strong>Circle-to-Circle:</strong> Distance between centers vs. sum of radii.</li>
                        <li><strong>Circle-to-Rect:</strong> Finding the closest point on the rectangle to the circle center and checking radial distance.</li>
                    </ul>
                </li>
                <li>
                    <strong>3. Half-Grid Filleting (20px):</strong>
                    The corner radius is now exactly half of the grid size (20px). This creates a more refined, industrial CAD look where rooms have smaller, subtle rounded edges rather than large circular caps.
                </li>
                <li>
                    <strong>4. Mixed Corner Profiles:</strong>
                    Each room now features a combination of <strong>Sharp (90&deg;)</strong> and <strong>Filleted</strong> vertices. The generator only applies rounding to "exposed" corners, ensuring that internal structural joints stay perfectly square while outer boundaries become smoothly curved.
                </li>
                <li>
                    <strong>5. Negative Space (Courtyards):</strong>
                    Before rooms are generated, the engine carves out 3-5 <strong>Air Rectangles</strong> (Voids). These act as absolute blockers, forcing the procedural growth to wrap around them, creating U-shaped wings, cloisters, and airy architectural silhouettes.
                </li>
                <li>
                    <strong>6. Radial Collision:</strong>
                    Interaction math is updated to handle the "missing" areas of rounded corners. Clicking the empty space where a corner was removed will not trigger the room, ensuring a high-fidelity user experience.
                </li>
            </ul>
        </div>
    );
}
