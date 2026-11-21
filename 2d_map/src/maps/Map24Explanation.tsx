import React from 'react';
import { STEPS } from './Map24Settings';

const ACCENT = '#c8a46a';
const h3 = { color: ACCENT, marginTop: 22, fontSize: '1.05rem' } as const;

const Map24Explanation: React.FC = () => (
    <div style={{ padding: 20, color: '#a8a29e', lineHeight: 1.6, fontFamily: 'Inter, sans-serif' }}>
        <h2 style={{ color: 'white', marginBottom: 15, borderBottom: `2px solid ${ACCENT}`, paddingBottom: 10 }}>
            Map 24: Street Map — a hand-drawn city map
        </h2>
        <p>
            A city map in the style of an illustrated neighbourhood map: a few long boulevards cross it, every district
            between them is filled with small streets lined up with its boulevard, the hills are green with winding roads,
            and the lakes sit in parks. Use Prev / Next to build it one step at a time.
        </p>

        <h3 style={h3}>The steps</h3>
        {STEPS.map((s, i) => (
            <p key={s.title}><strong>{i + 1}. {s.title}</strong> — {s.text}</p>
        ))}

        <h3 style={h3}>Why it looks like a real city</h3>
        <p>
            Real street grids are laid out along the main road they grew from, and grids that grew from different roads
            don't line up where they meet. Here each side of each boulevard is its own district with its own block size and
            offset, so you get the same T-junctions and odd-angled meetings. On the inside of a bend the streets out from
            the boulevard would crowd together, so only every other one is kept there, and none where they would nearly
            meet.
        </p>

        <h3 style={h3}>Files</h3>
        <p>
            Map24Terrain (heights, hills, lakes) · Map24Roads (the boulevard walkers) · Map24Samples (the roads as points) ·
            Map24Field (districts by jump flood) · Map24Streets (the small streets, per pixel) · Map24Paint (the painted
            layer) · Map24Draw (roads, trees, names, pins) · Map24Render (puts it together).
        </p>

        <h3 style={h3}>Not yet</h3>
        <p>The roads are drawn, not stored as a road graph, so they can't be exported to the game yet; that comes next.</p>
    </div>
);

export default Map24Explanation;
