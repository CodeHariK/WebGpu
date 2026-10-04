const S = {
    container: { background: 'rgba(10, 11, 18, 0.9)', color: '#94a3b8', padding: '24px', borderRadius: '12px', border: '1px solid rgba(34, 176, 125, 0.25)', maxWidth: '800px', fontSize: '0.9rem', lineHeight: '1.6', maxHeight: '80vh', overflowY: 'auto' as const },
    title: { color: '#f8fafc', marginBottom: '20px', fontWeight: 800, fontSize: '1.5rem' },
    section: { marginBottom: '20px' },
    header: { color: '#34d399', fontWeight: 700, display: 'block', marginBottom: '8px', textTransform: 'uppercase' as const, fontSize: '0.75rem', letterSpacing: '0.5px' },
    code: { fontFamily: 'monospace', color: '#e2e8f0', background: 'rgba(255,255,255,0.06)', padding: '0 4px', borderRadius: 4 },
};

export default function Map19Explanation() {
    return (
        <div style={S.container}>
            <h1 style={S.title}>PUZZLE ISLANDS</h1>
            <section style={S.section}>
                <span style={S.header}>What this is</span>
                <p>
                    A fast 2D lab for the game's Dino-and-Aliens puzzle rooms (Godot <span style={S.code}>PuzzleGrid</span>).
                    Same rules as the game: try ideas here first, port the ones that work.
                </p>
            </section>
            <section style={S.section}>
                <span style={S.header}>Rules</span>
                <ul>
                    <li>Push crates one cell (no pulling: a crate in a corner is stuck).</li>
                    <li><b>Goal</b>: collect <b>every gold key</b> 🔑, then bump the <b>exit door</b>. Aliens (purple) are a bonus: blow them up with box bombs or your own bombs — the keys never depend on them.</li>
                    <li><b>Teleporter pads</b> (glowing swirls, pairs share a colour): step on one and come out of its partner (blocked if a crate sits on the partner). Some lead into a sealed room with a gold key; some replace a doorway.</li>
                    <li><b>Enemies</b> act in real time on their own clocks: they patrol until they notice you (a "!" appears), then chase / attack. Crates, walls and closed gates block movement, bullets and lasers — push a crate into a firing line as a shield.
                        <ul>
                            <li><b>Patroller</b> (orange): walks a line, turns around when blocked.</li>
                            <li><b>Red</b>: melee, dumb, short chase (2) — slow, plods straight at you, stands stuck if blocked, forgets you as soon as you leave its circle.</li>
                            <li><b>Yellow</b>: sentry shooter (5) — patrols its line and only fires straight ahead, the way it faces (dashed sight line). It never turns to aim and never chases: sneak past behind or beside it.</li>
                            <li><b>Blue</b>: medium-range laser (3), intelligent, chase (6) — hunts you with pathfinding; with a clear line of sight at any angle (360°) it charges (blinking line), then zaps continuously, swinging the beam after you, until you leave its range or break line of sight (duck behind a wall or crate). Circles show each enemy's chase range (filled) and attack range (dashed).</li>
                        </ul>
                    </li>
                    <li><b>Lasers</b>: emitters are free-standing posts that fire a beam. You can cross a beam, but it costs 1 ❤ the moment it's on you (stepping in, or it switches on / swings onto you — e.g. putting a battery in while standing in front) and burns 2 ❤/s while you stay. The solver never crosses beams. Crates block beams. <b>Mirrors</b> (/ \) are boxes: push them like crates (they survive blasts); face one and press <b>E</b> to flip it 90° (Shift+arrow turns in place). A live beam that hits a <b>box bomb</b> blows it up. Light every <b>receiver</b> ◎ to open the cyan laser gates. <b>Pulsing</b> emitters (orange) fire on a beat clock — flicker warning, then on, then off: time your crossing. Beams destroy enemies too (red is dumb enough to walk in).</li>
                    <li><b>Wooden doors</b> (planks with iron bands): stuck shut for everyone — enemies can never open them, so the room behind is safe. Plant a bomb next to one (or throw one at it) and the blast smashes it for good. They seal side rooms with a reward, or are blastable shortcuts between rooms.</li>
                    <li><b>Wooden walls</b>: some stretches of room wall are planks instead of hedge. They are real walls — no walking, crates, bullets or blasts, and bombs can't break them — but lasers shine straight through (emitter beams and the blue enemy's laser), so a beam or a blue can reach you from the next room.</li>
                    <li><b>Pressure plates</b>: a crate on every plate opens the orange plate gates.</li>
                    <li><b>Batteries</b> 🔋: some emitters have a battery slot. Face one and press <b>E</b> to put a battery in (on) or take it out (off) — so a battery can move from a hazard emitter to the one you need. Pick up loose batteries by walking over them. The solver plans all of this too.</li>
                    <li><b>Your moves</b>: Space = melee swipe at the cell you face (1 damage, knocks back and stuns; red and blue take 2 hits). B = time bomb (2.5 s), T = throw a bomb up to 3 cells (explodes on landing, lands on an enemy in the way), C = remote bomb, X = detonate all remotes. Bombs come from pickups (+2); hearts heal 1.</li>
                    <li><b>Blasts</b>: a cross of 2 cells, stopped by walls, mirrors, emitters and gates. They kill enemies and aliens, break the first crate in each arm, set off other bombs and box bombs (chain reactions) and hurt you (1 ❤). Lasers set bombs off. The solver never uses bombs — breaking the wrong crate can strand you (Z undo).</li>
                    <li><b>Coloured gates</b> open once you hold the matching coloured key (walk over it). Keys are never used up.</li>
                </ul>
            </section>
            <section style={S.section}>
                <span style={S.header}>Dungeon generator (default — World 1)</span>
                <p>Kid-friendly redesign, step 1. A level is a small tree of rooms and every room has a <b>role</b> (tinted + labelled): <b>start</b>, <b>puzzle</b> (crates block the way to the next doorway), <b>rest</b> (a heart, nothing to solve), <b>bonus</b> (side room: a gold key behind crates) and <b>final</b> (the last gold key + the exit, hardest puzzle). Only crates, gold keys and the exit — no enemies or other mechanics yet.</p>
                <p>Each room puzzle is a small "vault" around its goal — rocks with an opening or two and a few crates scrambled by reverse pulls — built and solved <i>on its own</i>, then the whole level is solved once. Puzzle sizes ramp up along the route to the final room; DIFFICULTY raises that peak (1–5 pushes per room) and packs puzzles closer together (fewer rest rooms). GENERATOR toggles back to the classic everything-mixed lab.</p>
            </section>
            <section style={S.section}>
                <span style={S.header}>Generator</span>
                <ol>
                    <li>Binary space partition into rooms; every split leaves a one-cell doorway, so rooms form a tree.</li>
                    <li>Start + entrance bridge, a gold key in the farthest room (more in other rooms, one maybe in a teleporter-only sealed room), exit on another room's edge, aliens elsewhere.</li>
                    <li>Laser gate puzzles on route doorways, each on its own colour channel (gate + receivers): a <b>mirror chain</b> (emitter → mirror → mirror → receiver; mirrors mis-turned, and one may sit off the line to push back in) or a <b>laser through a wooden wall</b> (emitter post near a plank section, receiver post just inside the next room; a crate or a dead emitter blocks it first).</li>
                    <li>Up to LOCKS route doorways become coloured gates; each key is placed in a room reachable before its gate — a side room when possible, so you detour to fetch it (Zelda / Resident Evil).</li>
                    <li>Other route doorways get a crate; extra crates and box bombs near aliens.</li>
                    <li>Enemies spawn outside the start room and at least 5 cells from the player.</li>
                    <li>A BFS solver over pushes must reach key → exit (≥ MIN PUSHES); a quick local check confirms a bomb can be pushed next to an alien from where it sits; otherwise retry. Same seed → same level.</li>
                </ol>
                <p>SHOW SOLUTION replays the solver's minimum-push answer.</p>
            </section>
            <section style={S.section}>
                <span style={S.header}>Difficulty</span>
                <p>
                    Score = Sokoban (pushes + log₂ solver states) + mechanics (2 per lock, 3 per laser gate, 2 per plate gate, 1 per pulsing laser; 1 per battery, 2 per teleporter pair, 1 per extra gold key; mirror flips count as pushes) + Σ
                    enemy weight (patrol 1, red 2, yellow 4, blue 5) — full weight when its chase circle covers the solution route, 30% when
                    it sits off-route. Easy &lt; 18 ≤ Medium &lt; 30 ≤ Hard &lt; 45 ≤ Expert.
                </p>
                <p>The DIFFICULTY slider (1–10) is a budget: it sets size, rooms, pushes, locks, mechanics and enemies together. Moving any other setting switches back to MANUAL from those values.</p>
            </section>
        </div>
    );
}
