import React from 'react';
import { STEPS } from './Map26Settings';

const ACCENT = '#f3c86b';
const h3 = { color: ACCENT, marginTop: 22, fontSize: '1.05rem' } as const;

const Map26Explanation: React.FC = () => (
    <div style={{ padding: 20, color: '#a8a29e', lineHeight: 1.6, fontFamily: 'Inter, sans-serif' }}>
        <h2 style={{ color: 'white', marginBottom: 15, borderBottom: `2px solid ${ACCENT}`, paddingBottom: 10 }}>
            Map 26: Colour Blocks — spread colours, borders as highways
        </h2>
        <p>
            A few colours spread over a square grid from their seeds until they fill it. Each colour becomes an area
            block, and where two blocks meet runs a highway. Use Prev / Next (or ← →) to go through it. Step 2 plays
            the spread: pause it, step it a tick at a time (◀ ▶, or ← →), or drag its clock.
        </p>

        <h3 style={h3}>The steps</h3>
        {STEPS.map((s, i) => (
            <p key={s.title}><strong>{i + 1}. {s.title}</strong> — {s.text}</p>
        ))}

        <h3 style={h3}>Is it a Voronoi diagram?</h3>
        <p>
            Nearly: each cell goes to the seed it can be reached from soonest, which is Voronoi's rule (nearest seed)
            with distance measured as travel time. A classic Voronoi has straight borders because its distance is
            the straight line and every seed is the same. Here the borders bend and go jagged because: steps are
            grid steps (to 8 neighbours, so even a plain spread makes octagons, and every border is a staircase of
            cells); each cell costs a different time to step into (roughness: noise), so fronts slow down in places;
            and colours spread at different speeds (speed variety), which curves the borders. Roughness 0 and speed
            variety 0 give the plain grid Voronoi.
        </p>

        <h3 style={h3}>Files</h3>
        <p>
            Map26Spread (the grid, seeds, the spread and the tidy-up) · Map26Borders (tracing, straightening and
            rounding the highways) · Map26Draw (cells, grid, seeds, highways) · Map26Render (puts it together).
        </p>

        <h3 style={h3}>Not yet</h3>
        <p>
            The block colours are still the grid's cells, so next to a straightened highway their edge shows little
            steps; the blocks could be redrawn from the highways instead.
        </p>
    </div>
);

export default Map26Explanation;
