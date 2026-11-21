import React from 'react';

const ACCENT = '#7cc4d8';
const h3 = { color: ACCENT, marginTop: 22, fontSize: '1.05rem' } as const;

const Map25Explanation: React.FC = () => (
    <div style={{ padding: 20, color: '#a8a29e', lineHeight: 1.6, fontFamily: 'Inter, sans-serif' }}>
        <h2 style={{ color: 'white', marginBottom: 15, borderBottom: `2px solid ${ACCENT}`, paddingBottom: 10 }}>
            Map 25: Islands — noise with a falloff
        </h2>
        <p>
            Four islands joined by bridges, each a noise map with a falloff. Tick <strong>show how it's built</strong> to
            see each falloff's reach, the headlands and the bridge ends.
        </p>

        <h3 style={h3}>1. Where the islands go</h3>
        <p>
            One big, two medium, one small. Dropped near the middle, then pushed apart a few dozen times until no two
            are too close and all are on the map. Each island owns a territory: the points nearer to it than to any
            other (bigger islands reach further). Land never comes within half a channel of a territory's edge, so
            islands can't touch.
        </p>

        <h3 style={h3}>2. Bridges first</h3>
        <p>
            Before any island has a shape: the shortest links that join all four (a minimum spanning tree), plus the
            shortest other link that crosses none of them and passes over no third island, for one loop. Each island
            gets a headland (a fat line) out to each of its bridges, ending half a channel short of the territory
            border, so the two ends of a bridge face each other across a narrow strait.
        </p>

        <h3 style={h3}>3. The shape: noise and falloff</h3>
        <p>
            A height for every point: noise (4 sizes added together, each half the size and half as strong) plus a
            hill that is 1 at the island's middle and falls to 0 its size away (1 − (distance / size)²). Land is where
            the height is above sea level. Near the middle the hill keeps it land; near the edge only the noise's high
            spots poke up (headlands, islets) and its low spots sink (bays, lagoons). <strong>noise strength</strong>
            is how much the noise counts against the hill (0: plain circles), <strong>noise size</strong> how big its
            biggest features are. Each island reads the noise at its own offset. The headlands are melted in with a
            smooth minimum (<strong>headland blend</strong>).
        </p>

        <h3 style={h3}>4. The colours</h3>
        <p>
            The height only says where the coast is. The real distance to the coast is then measured on a 2-pixel
            grid: the grid points right by the coast start it, and two sweeps over the grid (down-right, then up-left)
            pass it on, each point taking its nearest neighbour's distance plus the step. The colours come from that
            distance alone: grass (darker inland), sand, the coast line, shallow to deep water, a foam line.
        </p>
    </div>
);

export default Map25Explanation;
