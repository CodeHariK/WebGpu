const EXPLANATION_STYLES = {
    container: {
        background: 'rgba(10, 11, 18, 0.9)',
        color: '#94a3b8',
        padding: '24px',
        borderRadius: '12px',
        border: '1px solid rgba(59, 130, 246, 0.2)',
        maxWidth: '800px',
        fontSize: '0.9rem',
        lineHeight: '1.6',
        maxHeight: '80vh',
        overflowY: 'auto' as const
    },
    title: { color: '#f8fafc', marginBottom: '20px', fontWeight: 800, fontSize: '1.5rem' },
    section: { marginBottom: '20px' },
    header: { color: '#60a5fa', fontWeight: 700, display: 'block', marginBottom: '8px', textTransform: 'uppercase' as const, fontSize: '0.75rem' }
};

export default function Map17Explanation() {
    return (
        <div style={EXPLANATION_STYLES.container}>
            <h1 style={EXPLANATION_STYLES.title}>MEDIEVAL FANTASY CITY (WATABOU PIPELINE)</h1>

            <section style={EXPLANATION_STYLES.section}>
                <span style={EXPLANATION_STYLES.header}>Overview</span>
                <p>
                    Map 17 implements the algorithmic pipeline inspired by <b>Oleg Dolya (watabou)'s Medieval Fantasy City Generator</b>.
                    Instead of a grid or hexagonal layout, the generator constructs a fully organic medieval town using a multi-phase computational geometry pipeline.
                </p>
            </section>

            <section style={EXPLANATION_STYLES.section}>
                <span style={EXPLANATION_STYLES.header}>The 5-Stage Watabou Pipeline</span>
                <p>
                    <b>1. Relaxed Voronoi Dual Mesh:</b><br/>
                    Random seed points are triangulated with Delaunay Triangulation. <i>Lloyd’s Relaxation</i> iteratively moves each seed point to its polygon centroid, creating natural ~120° polygon angles and even spacing.<br/><br/>

                    <b>2. Elevation, Coastline & River Descent:</b><br/>
                    Simplex noise elevation combined with radial/coastal gradients assigns water and land cells. A mountain spring vertex is selected inland, and a river cascades downhill along the steepest descent until merging into the ocean.<br/><br/>

                    <b>3. Citadel, City Walls & Gates:</b><br/>
                    A high-elevation defensible cell becomes the Citadel/Castle. A perimeter of curtain walls is generated along the outer boundary of urban Voronoi cells, complete with defensive corner towers and gated exits.<br/><br/>

                    <b>4. Arterial Roads & Avenues:</b><br/>
                    Primary highways connect the Market Square and Castle directly to the city gates, cutting the city into distinct <b>Wards</b> (Craftsmen Quarter, Residential District, Shantytowns, and Outlying Farmland).<br/><br/>

                    <b>5. Urban Fabric & Parcel Subdivision:</b><br/>
                    Each ward polygon is inset to leave room for perimeter streets. The block is then recursively subdivided into narrow street-facing lots. Finally, building footprints are inset and decorated with gable roof ridge lines.
                </p>
            </section>

            <section style={EXPLANATION_STYLES.section}>
                <span style={EXPLANATION_STYLES.header}>Interactive Controls</span>
                <p>
                    • <b>VORONOI CELLS:</b> Controls the total number of seed cells in the simulation.<br/>
                    • <b>LLOYD'S RELAXATION:</b> Adjusts organic vs. uniform cell shapes.<br/>
                    • <b>CITY WALL RADIUS:</b> Scales the defensive curtain wall perimeter.<br/>
                    • <b>COASTLINE & RIVER:</b> Toggles dynamic geography and hydrological pathfinding.<br/>
                    • <b>VISUAL LAYERS:</b> Toggle walls, houses, arterial roads, wards, and underlying mesh.
                </p>
            </section>
        </div>
    );
}
