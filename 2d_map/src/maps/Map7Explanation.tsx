export default function Map7Explanation() {
    return (
        <div style={{ color: '#fff', fontSize: '15px', lineHeight: 1.6 }}>
            <h2 style={{ marginTop: 0, color: '#e0e0e0' }}>Procedural Growth City</h2>
            <p>
                This map demonstrates how the **Recursive Growth** algorithm can be adapted for urban planning simulation.
            </p>

            <ul style={{ paddingLeft: '20px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                <li>
                    <strong>1. Hierarchy (Arteries vs. Streets):</strong>
                    The city grows via two road types. **Arteries** form the main skeleton, branching at shallow angles to cover large distances. **Streets** sprout perpendicularly from Arteries to fill in blocks.
                </li>
                <li>
                    <strong>2. Automated Zoning:</strong>
                    Whenever a new road segment (branch) is finalized, the algorithm "zones" the surrounding land. It calculates perpendicular offsets to place **Building Footprints** without overlapping the road itself.
                </li>
                <li>
                    <strong>3. Space Colonization:</strong>
                    The city avoids chaos through distance-based constraints. If a new junction or building would be too close to existing infrastructure, the growth branch is "killed," simulating the lack of available real estate.
                </li>
                <li>
                    <strong>4. Interactive Demolition:</strong>
                    Just like the plant model, the city is a recursive tree. Clicking a junction (the grey dots) allows you to "Demolish" that node. This recursively removes all roads and buildings that branched from that point, allowing the city to re-grow in a different pattern.
                </li>
            </ul>
        </div>
    );
}
