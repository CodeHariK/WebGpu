import React from 'react';

const ACCENT = '#fbbf24';
const h3 = { color: ACCENT, marginTop: 22, fontSize: '1.05rem' } as const;

const Map22Explanation: React.FC = () => (
    <div style={{ padding: 20, color: '#a8a29e', lineHeight: 1.6, fontFamily: 'Inter, sans-serif' }}>
        <h2 style={{ color: 'white', marginBottom: 15, borderBottom: `2px solid ${ACCENT}`, paddingBottom: 10 }}>
            Map 22: Wick — you are the light
        </h2>
        <p>
            A little living candle walks a drowned valley at night. Its <strong>wax</strong> is everything at once: its
            health (a hit melts some off), its ammo (each flame it throws costs some) and its light (less wax, a smaller
            circle of light — and the dark comes closer). No arena: the level is winding paths over black water, with
            clearings, plank bridges and fog.
        </p>
        <p><strong>Controls:</strong> WASD walk · mouse aim · hold click: throw flames · right click / E: flare · Space: dash · stand at an unlit lantern to kindle it · R restart · P pause.</p>

        <h3 style={h3}>Light is the weapon</h3>
        <p>
            Thrown flames burn what they hit and leave an <strong>ember</strong> where they land: a small light that keeps
            burning for a few seconds. On the water they just hiss out. The <strong>flare</strong> is a big burst of
            light all round you — expensive, but it clears the space. <strong>Lanterns</strong> are lasting light: lit,
            they keep the dark off, melt wax back onto you, and are where you come back if your flame goes out.
        </p>

        <h3 style={h3}>The things in the dark</h3>
        <p><strong>Shade</strong> — rises from the water and lurks just outside your light. Light burns it. To reach you it has to come in: its eyes flare red, then it lunges straight through. Throw a flame into it during the tell and it pulls back.</p>
        <p><strong>Moth</strong> — flies to the brightest thing near it. Usually you: it clings and drinks your wax until you dash or flare. But a lit lantern or a fresh ember is brighter — throw one and it'll circle that instead.</p>
        <p><strong>Weeper</strong> — tall, pale, hands over its face. It only moves in the dark; in light it freezes, and only then can it be hurt. Keep it lit — embers behind you help.</p>

        <p><strong>Snuffer</strong> — a hunched thing with a snuffer cone for a head. It goes for the lights, not you: it floats to the lantern or ember nearest you and lowers its cone over it while a ring fills. Hit it to stop it; a snuffed lantern has to be lit again. With nothing lit near, it holds your flame and drinks wax — dash twice to break free.</p>
        <p><strong>Drowned hand</strong> — waits under the water by the path (faint ripples). Walk the edge near it and the water parts, then it grabs you and drags you toward the water, drinking wax. Dash twice, or burn it while it's up; it sinks and waits again.</p>
        <p><strong>Gloom</strong> — a floating ball of un-light that drifts in behind you. Everything near it goes dim: your light shrinks round it, shades come in, a weeper in it wakes. Light can't hurt it — only flames.</p>
        <p><strong>Wax leech</strong> — a hit knocks wax off you as blobs you can walk back over. Leeches race you for them and grow bigger and tougher with each one. Burst a leech and the wax it swallowed spills back out.</p>

        <h3 style={h3}>The way through</h3>
        <p>
            Each clearing wakes when you walk in: shapes rise out of the water in waves. Light all its lanterns and beat
            its waves, and the fog wall across the path ahead parts. Out on the paths, away from lantern light, the dark
            sends a shade now and then. A side path leads to the Hermit's Lamp: a lantern and spare candles. Light the
            chapel's lanterns at the end to finish.
        </p>

        <h3 style={h3}>Level 2: the Wax Works</h3>
        <p>
            The old candle-makers' village up a wooded hill: no water, black woods (roots grab from their edge), a path
            that forks after the Market Square — the sheltered west lane or the narrow, windy east ridge — and then
            switchbacks up to the Bell Tower, with a side path to the Old Kiln.
        </p>
        <p>
            <strong>Gusts</strong> blow over the open stretches: leaves skitter across first, then it blows for a couple
            of seconds, pushing you and your flames, burning embers out faster and bending your own flame low — your light
            shrinks right when the dark is near. <strong>Wax vats</strong> in the Chandlery glow (shades keep away) and
            refill you while you wade, slowly. Moths that circle a flame too long singe and drop; leeches near a strong
            flame slowly melt.
        </p>
    </div>
);

export default Map22Explanation;
