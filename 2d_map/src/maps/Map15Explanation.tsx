import React from 'react';

const Map15Explanation: React.FC = () => {
    const accent = '#0ea5e9';
    return (
        <div style={{ color: '#94a3b8', lineHeight: '1.6', fontSize: '0.95rem' }}>
            <h2 style={{ color: 'white', borderBottom: '1px solid #1e293b', paddingBottom: '10px', marginBottom: '20px' }}>STREET GROWTH ALGORITHM</h2>
            
            <p>
                Map 15 implements a <strong>recursive graph growth</strong> algorithm using <strong>Midpoint Subdivision</strong>. 
                This method ensures a structured, hierarchical urban fabric that remains stable and predictable.
            </p>

            <h3 style={{ color: accent, marginTop: '25px', fontSize: '1.1rem' }}>Stage 1: Node Generation</h3>
            <p>
                The process begins with <strong>Seed Nodes</strong>. Using a seed-based PRNG, the network expands 
                organically. The initial nodes are always colored <strong>YELLOW</strong>.
            </p>

            <h3 style={{ color: accent, marginTop: '25px', fontSize: '1.1rem' }}>Stage 2: Edge Creation</h3>
            <p>
                Nodes are connected using geometric rules: <strong>No Crossings</strong>, <strong>Angular Separation</strong>, 
                and <strong>Scoring</strong>. These primary connections are colored <strong>BLUE</strong>.
            </p>

            <h3 style={{ color: accent, marginTop: '25px', fontSize: '1.1rem' }}>Stage 3: Face Finding</h3>
            <p>
                The algorithm traverses the graph to find every "face" (enclosed block). This allows for 
                recursive subdivision specifically within the boundaries of each city district.
            </p>

            <h3 style={{ color: accent, marginTop: '25px', fontSize: '1.1rem' }}>Stage 4: Blue Roads (Level 0)</h3>
            <p>
                The initial Blue roads form the main traffic arteries. They remain fixed even as 
                higher levels of detail are recursively added.
            </p>

            <h3 style={{ color: accent, marginTop: '25px', fontSize: '1.1rem' }}>Stage 5: Red Neighborhoods (Level 1)</h3>
            <p>
                The algorithm identifies enclosed city blocks and performs <strong>Face-Based Infilling</strong>. 
                It spawns a set of "neighborhood seeds" (RED) specifically within these boundaries. 
                Combined with traditional midpoints, these seeds branch out to form a dense inner street network.
            </p>

            <h3 style={{ color: accent, marginTop: '25px', fontSize: '1.1rem' }}>Stage 6: Orange Details (Level 2)</h3>
            <p>
                Finally, a third layer of subdivision occurs between the <strong>YELLOW</strong> and <strong>RED</strong> nodes. 
                New <strong>ORANGE</strong> streets branch out to refine the urban topology into its final, high-detail state.
            </p>
        </div>
    );
};

export default Map15Explanation;
