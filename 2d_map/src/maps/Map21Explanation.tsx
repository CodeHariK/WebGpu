import React from 'react';

const ACCENT = '#f472b6';
const h3 = { color: ACCENT, marginTop: 22, fontSize: '1.05rem' } as const;

const Map21Explanation: React.FC = () => (
    <div style={{ padding: 20, color: '#94a3b8', lineHeight: 1.6, fontFamily: 'Inter, sans-serif' }}>
        <h2 style={{ color: 'white', marginBottom: 15, borderBottom: `2px solid ${ACCENT}`, paddingBottom: 10 }}>
            Map 21: Combat Sandbox — find the fun in 2D first
        </h2>
        <p>
            The fights on the clown planet, flattened to top-down so ideas can be tried in minutes. Toys with clowns
            locked inside drop in from the top; you break them with the Bubble Wand. Every "feel" effect can be switched
            off, every number tuned while you play.
        </p>
        <p>
            <strong>Themes</strong> are different places in the human operation, each with its own look and its own
            levels: the <strong>Toxic Factory</strong> (the Drill Works: belts, vents, sludge pipes, lifts, the drill
            core) and the <strong>Toy Assembly Plant</strong> (where clowns are sealed into toys: assembly lines,
            parts crates, grabber arms). Pick one with the THEME buttons.
        </p>
        <p><strong>Controls:</strong> WASD move · mouse aim · hold click to blow bubbles · Q fizz bubble · Space / Shift dash · right click / E pop · R restart · P pause.</p>

        <h3 style={h3}>Break the machines, find the keys, get out</h3>
        <p>
            Each level has <strong>machines</strong> to break (amber target rings; amber arrows point to them). Break
            them all — and on the last level the drill core — and the <strong>exit</strong> gate turns green. Walk
            into it to clear the level. Toys keep coming while there's work left, so don't dawdle.
        </p>
        <p>
            Some machines switch the place off when they break: the <strong>terminal</strong> stops the belts and
            crushers, <strong>valves</strong> shut the vents near them, <strong>generators</strong> in the Lift Shaft
            cut a lift, and the <strong>shield pumps</strong> keep the drill core's shield up. A <strong>locker</strong>
            drops a key; walk up to the matching coloured door with it and the door slides open.
        </p>
        <p>
            Shoot <strong>supply crates</strong> open for a joy jar (+15 joy), a heart, or a <strong>fizz</strong> can:
            two big bubbles (Q) that burst in a blast — great against machines and crowds, never hurts you. Toys
            sometimes drop joy jars too.
        </p>

        <h3 style={h3}>Push forward or starve</h3>
        <p>
            Bubbles cost <strong>joy</strong>. A broken toy frees its clown as joy spirits that fly to you — six if you
            broke it up close, four from mid range, two from far. Gas creeps up from the bottom of the arena; clearing a
            wave pushes it back. Hanging back means running dry and choking.
        </p>

        <h3 style={h3}>Dash and perfect dodges</h3>
        <p>
            The dash makes you invincible for its first moment. A bolt passing through you then is a
            <strong> PERFECT</strong>: slow motion and a joy bonus. The pop (right click) shoves and hurts everything
            close — your answer to being rushed, and the way to stun a riot toy.
        </p>

        <h3 style={h3}>The toys</h3>
        <p><strong>Rifleman</strong> — hides behind cover, shows a red sight line before a 3-bolt burst. Hit it during the line and the burst is cancelled. After two bursts it stops to wind its key (glowing yellow): double damage — rush it.</p>
        <p><strong>Spotter drone</strong> — circles at a distance. While its light is on you, riflemen aim in half the time and much tighter. Knock it and the light goes out for a moment. Kill it first.</p>
        <p><strong>Riot toy</strong> — bubbles bounce off its shield. It walks at you, winds up (a red lane shows where), then charges. Sidestep and it slams into a block or the wall: stunned, no shield, double damage. It also hurts its own side if they're in the way.</p>

        <h3 style={h3}>Cover</h3>
        <p>Grey blocks are tall: nothing gets through. Orange crates are low: they only stop a shot when someone is crouched right behind one — a rifleman ducking between bursts, or you hugging a crate. So riflemen are open while they aim and fire (shoot the red line!) and safe while they duck.</p>

        <h3 style={h3}>Theme: Toy Assembly Plant — grabber arms</h3>
        <p>
            Claws ride rails over the assembly lines. Stand under one and it stops, a red reticle shrinks on the floor,
            and it drops: whoever is still there is carried to the press at the end of the line and dropped in. A toy
            gets crushed; you get a moment to dash out before it slams. Grabbed, dash three times to wriggle free.
            Six bubbles on a lowered claw jam it; breaking the terminal switches them all off. The trick is to lure
            toys onto the belts and let the arms do the work.
        </p>

        <h3 style={h3}>Hazards</h3>
        <p>Toxic pools hurt you — and melt any toy you knock into them.</p>
    </div>
);

export default Map21Explanation;
