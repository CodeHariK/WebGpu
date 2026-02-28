export default function Map1Explanation() {
    return (
        <div style={{ color: '#fff', fontSize: '15px', lineHeight: 1.6 }}>
            <h2 style={{ marginTop: 0, color: '#e0e0e0' }}>Algorithmic Curve Generator</h2>
            <p>
                This generative system creates non-overlapping, natural-looking connections between procedurally generated curvy shapes.
                To achieve this without heavy polynomial math locking up the browser, we use a technique called <strong>discretization</strong>.
            </p>

            <ul style={{ paddingLeft: '20px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                <li>
                    <strong>1. Shape Placement (Circle Packing):</strong>
                    Using bounding circles and collision buffers, we drop blobs or stars onto the canvas ensuring no two shapes intersect.
                </li>
                <li>
                    <strong>2. Perimeter Sampling:</strong>
                    We calculate the exact lengths of the perimeter segments of each shape and place connection nodes evenly across the perimeter. This provides uniform anchor points regardless of shape size.
                </li>
                <li>
                    <strong>3. Natural Curvature via Normals:</strong>
                    To ensure cubic bezier connections swoop <em>away</em> from the shape naturally, we calculate the outward normal vector at each anchor point. The control points of the bezier curve are pushed along this normal.
                </li>
                <li>
                    <strong>4. Fast Collision Validation:</strong>
                    Instead of math-heavy bezier intersection checks, the cubic bezier curves are chopped into 20 tiny straight line segments. We use standard ray-casting and segment intersection logic to instantly discard any connection that overlaps another curve or penetrates a shape!
                </li>
            </ul>
        </div>
    );
}
