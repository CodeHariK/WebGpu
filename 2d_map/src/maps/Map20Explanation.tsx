import React from 'react';

const ACCENT = '#f97316';
const h3 = { color: ACCENT, marginTop: 22, fontSize: '1.05rem' } as const;

const Map20Explanation: React.FC = () => (
    <div style={{ padding: 20, color: '#94a3b8', lineHeight: 1.6, fontFamily: 'Inter, sans-serif' }}>
        <h2 style={{ color: 'white', marginBottom: 15, borderBottom: `2px solid ${ACCENT}`, paddingBottom: 10 }}>
            Map 20: Enemy Arena — simple AI that fights well
        </h2>
        <p>
            Far Cry, Batman Arkham and BioShock don't use clever planners. Enemies patrol a few points near home,
            notice you slowly, call their friends, stand in a ring round you and <strong>take turns</strong> to hit,
            with a <strong>warning</strong> before each hit. This lab rebuilds that with a handful of cheap tricks,
            each one switchable so you can see what it adds.
        </p>
        <p><strong>Controls:</strong> WASD move · mouse aim · click swing · Shift sneak (quiet, slower to be spotted) · Space shout.</p>

        <h3 style={h3}>1. Patrol + leash</h3>
        <p>Each enemy has a home and a patrol loop (hand-placed). In a fight, if it gets too far from home it gives up and walks back.</p>

        <h3 style={h3}>2. Noticing: cone + one ray + meter</h3>
        <p>
            Inside the view cone and range, one ray to you. While it sees you the bar fills (faster up close, slower
            when you sneak); full = spotted. Footsteps and shouts are noises with a radius: it comes over to look.
        </p>

        <h3 style={h3}>3. Shout + share</h3>
        <p>The one that spots you shouts: everyone in range joins. While anyone in the fight sees you, all of them know where you are.</p>

        <h3 style={h3}>4. Slot ring (surround)</h3>
        <p>
            Fixed slots on a circle round you; a slot counts if it isn't in a wall and has a clear line to you (one ray).
            Each melee enemy takes the nearest free slot (greedy, with a bonus for keeping its own). Cyan dots in the arena.
        </p>

        <h3 style={h3}>5. Context steering</h3>
        <p>
            No path finder: each frame an enemy scores 16 directions — toward its goal is good, a wall close that way
            (one short ray), a squadmate that way or the player (when waiting) is bad — and walks the best one.
        </p>

        <h3 style={h3}>6. Attack tokens + wind-up</h3>
        <p>
            Only N enemies may attack at once (gold ring). Whoever is behind you goes first. It winds up (growing red
            ring), then lunges where you <em>were</em>: step aside to dodge. The token rests a moment before the next one.
        </p>

        <h3 style={h3}>7. Ranged + cover markers</h3>
        <p>
            Squares are shooters. They hide at the nearest hand-placed cover spot you can't see (green diamonds), peek
            out, aim (dashed laser tracks you, solid = locked: move!) and fire.
        </p>

        <h3 style={h3}>8. Lose you → search → home; hurt → stagger / retreat</h3>
        <p>Unseen for a few seconds: go to the last seen spot (magenta x), look around, walk home. Hit: knocked back; on 1 hp it backs off for a while.</p>

        <div style={{ marginTop: 26, padding: 14, background: 'rgba(255,255,255,0.03)', borderRadius: 8, borderLeft: `4px solid ${ACCENT}` }}>
            <strong>Cost:</strong> per enemy per frame one sight ray and 16 short steering rays; the ring is 8 rays.
            No grid, no navmesh. See <code>Todo_EnemyAI.md</code> for the plan to port it to Godot.
        </div>
    </div>
);

export default Map20Explanation;
