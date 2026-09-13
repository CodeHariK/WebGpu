import { BIOME_COLORS, BIOME_LABELS, type Biome } from './Map18Logic';

const S = {
    container: {
        background: 'rgba(10, 11, 18, 0.9)',
        color: '#94a3b8',
        padding: '24px',
        borderRadius: '12px',
        border: '1px solid rgba(34, 176, 125, 0.25)',
        maxWidth: '800px',
        fontSize: '0.9rem',
        lineHeight: '1.6',
        maxHeight: '80vh',
        overflowY: 'auto' as const,
    },
    title: { color: '#f8fafc', marginBottom: '20px', fontWeight: 800, fontSize: '1.5rem' },
    section: { marginBottom: '20px' },
    header: { color: '#34d399', fontWeight: 700, display: 'block', marginBottom: '8px', textTransform: 'uppercase' as const, fontSize: '0.75rem', letterSpacing: '0.5px' },
    swatchGrid: { display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(150px, 1fr))', gap: '6px', marginTop: '10px' },
    swatchRow: { display: 'flex', alignItems: 'center', gap: '8px', fontSize: '0.78rem' },
    swatch: { width: 16, height: 16, borderRadius: 4, flexShrink: 0, border: '1px solid rgba(255,255,255,0.15)' },
    step: { marginBottom: '10px' },
    stepNum: { color: '#34d399', fontWeight: 700, marginRight: '6px' },
};

const LAND_BIOMES: Biome[] = [
    'BEACH', 'SUBTROPICAL_DESERT', 'GRASSLAND', 'TROPICAL_FOREST', 'TROPICAL_RAINFOREST',
    'TEMPERATE_DESERT', 'TEMPERATE_FOREST', 'TEMPERATE_RAINFOREST', 'SHRUBLAND', 'TAIGA',
    'TUNDRA', 'BARE', 'SCORCHED', 'SNOW',
];
const WATER_BIOMES: Biome[] = ['SHALLOW', 'OCEAN', 'DEEP_OCEAN', 'LAKE'];

export default function Map18Explanation() {
    return (
        <div style={S.container}>
            <h1 style={S.title}>HEX BIOME GENERATOR</h1>

            <section style={S.section}>
                <span style={S.header}>Overview</span>
                <p>
                    Where the Hex Truchet map colours the <b>edges</b> of each hexagon to grow 1-D road
                    networks, this map colours the <b>interiors and corners</b> to grow 2-D terrain. Each
                    hex carries two scalar values — <b>elevation</b> and <b>moisture</b> — and those two
                    numbers decide whether it is sea or land, and which biome the land becomes. It is the
                    Amit-Patel polygonal-map recipe, adapted from Voronoi polygons onto a clean hex lattice.
                </p>
            </section>

            <section style={S.section}>
                <span style={S.header}>The Pipeline</span>
                <div style={S.step}><span style={S.stepNum}>1.</span>
                    <b>Scalar fields.</b> Elevation and moisture come from layered (fBm) simplex noise. A
                    radial falloff pulls elevation down toward the edges, so a continent sits in the middle
                    surrounded by open ocean instead of noise wrapping forever.
                </div>
                <div style={S.step}><span style={S.stepNum}>2.</span>
                    <b>Sea level threshold.</b> Any hex below the sea-level cutoff is water; the rest is land.
                    Drag the <b>SEA LEVEL</b> slider to flood or drain the world in real time.
                </div>
                <div style={S.step}><span style={S.stepNum}>3.</span>
                    <b>Whittaker biomes.</b> Each land hex is classified by its (elevation, moisture) pair —
                    high &amp; dry becomes desert or bare rock, high &amp; wet becomes snow and taiga, low &amp;
                    wet becomes rainforest, and so on.
                </div>
                <div style={S.step}><span style={S.stepNum}>4.</span>
                    <b>Flood fill.</b> A breadth-first search over the hex neighbour graph starts from the map
                    border: every water hex it reaches is <i>ocean</i>; water it can't reach is an inland
                    <i> lake</i>. A second flood fill labels each contiguous landmass and culls islands smaller
                    than a few hexes back into shallow water.
                </div>
                <div style={S.step}><span style={S.stepNum}>5.</span>
                    <b>Smooth coastline.</b> Filling whole hexes gives a chunky, mosaic shore. To smooth it,
                    the same elevation field is sampled at every hex <b>corner</b>, each hex is split into its
                    6 triangles, and the sea-level iso-line is traced through each triangle (marching squares,
                    but per-triangle so there are no ambiguous saddle cases). Because corners are shared world
                    points, the segments join seamlessly across neighbours into one continuous coast. Toggle
                    <b> COASTLINE</b> to see the raw hex edges underneath.
                </div>
            </section>

            <section style={S.section}>
                <span style={S.header}>Water</span>
                <div style={S.swatchGrid}>
                    {WATER_BIOMES.map((b) => (
                        <div key={b} style={S.swatchRow}>
                            <span style={{ ...S.swatch, background: BIOME_COLORS[b] }} />
                            {BIOME_LABELS[b]}
                        </div>
                    ))}
                </div>
            </section>

            <section style={S.section}>
                <span style={S.header}>Land Biomes</span>
                <div style={S.swatchGrid}>
                    {LAND_BIOMES.map((b) => (
                        <div key={b} style={S.swatchRow}>
                            <span style={{ ...S.swatch, background: BIOME_COLORS[b] }} />
                            {BIOME_LABELS[b]}
                        </div>
                    ))}
                </div>
            </section>

            <section style={S.section}>
                <span style={S.header}>Extending It</span>
                <p>
                    The hex edges are still free — the Truchet road/river tiles from Map 14 can be layered on
                    top to route rivers downhill from peaks to the coast, or roads between settlements, giving
                    both the region colouring and the network in one map.
                </p>
            </section>
        </div>
    );
}
