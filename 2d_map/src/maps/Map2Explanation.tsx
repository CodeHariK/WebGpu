export default function Map2Explanation() {
    return (
        <div style={{ color: '#fff', fontSize: '15px', lineHeight: 1.6 }}>
            <h2 style={{ marginTop: 0, color: '#e0e0e0' }}>Childish City Map</h2>
            <p>
                Building on the foundational collision and packing algorithms of the previous map, this iteration transforms abstract curves into an organic "city" layout complete with plots and dashed roads.
            </p>

            <ul style={{ paddingLeft: '20px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                <li>
                    <strong>1. Thematic Noise:</strong>
                    Instead of random blobs, shapes are styled using hard-coded color palettes resembling parks, lakes, and concrete blocks, utilizing custom noise and jagged lines for more variation.
                </li>
                <li>
                    <strong>2. Proximity Checking (Nearest Neighbors):</strong>
                    This map modifies the core connection algorithm. Instead of blindly choosing a secondary point to connect to, it filters the available sampled points for valid nodes within a &lt; maxRoadLength radius.
                </li>
                <li>
                    <strong>3. Variation vs Perfection:</strong>
                    If the algorithm always connected exactly the closest points, it would look overly structured and rigid. Instead, it filters for the Top 3 closest nodes, and picks randomly between them to ensure organic, playful imperfections.
                </li>
                <li>
                    <strong>4. Multi-pass Rendering:</strong>
                    Roads are rendered using standard Canvas strokes and <code>setLineDash([10, 10])</code>. Thick dark lines form the base road, and dashed yellow lines are stroked overtop for the visual effect.
                </li>
            </ul>
        </div>
    );
}
