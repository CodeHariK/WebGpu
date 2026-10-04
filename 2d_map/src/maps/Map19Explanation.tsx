// Puzzle Islands — the full-width catalogue under the controls: every mechanic and enemy built
// so far, where the idea comes from (Dino & Aliens, Link's Awakening, Bomberman) and which world
// of the dungeon generator brings it in.
type Src = 'D&A' | 'LA' | 'BM' | 'Ours';
type Row = { name: string; src: Src[]; world: string; char: string; what: string };

const SRC: Record<Src, { label: string; color: string }> = {
    'D&A': { label: 'Dino & Aliens', color: '#7dff9a' },
    LA: { label: "Link's Awakening", color: '#7ec8ff' },
    BM: { label: 'Bomberman', color: '#ff9a3c' },
    Ours: { label: 'ours', color: '#c4b5fd' },
};

const MECHANICS: Row[] = [
    { name: 'Crates', src: ['D&A'], world: '1', char: 'B', what: 'Push one cell, never pull — a crate in a corner is stuck. Wooden crates break in a blast.' },
    { name: 'Metal crates', src: ['Ours'], world: '1', char: 'N', what: 'Push like crates, bomb-proof. Every dungeon puzzle uses them, so a stray bomb never wrecks a puzzle.' },
    { name: 'Gold keys + exit', src: ['D&A', 'LA'], world: '1', char: 'K E', what: 'Collect every gold key, then bump the exit door. One in the final room, one behind the bonus side room\'s puzzle.' },
    { name: 'Room reset', src: ['LA'], world: '1', char: '—', what: 'Walk out of an unsolved room and back in: its crates jump back. A room is solved once its keys are taken, a blocked doorway opens, or you leave by pad.' },
    { name: 'Hearts', src: ['LA'], world: '1', char: 'h', what: '3 hearts. Rest rooms hold a heart; at ≤ 1½ hearts beaten enemies drop one.' },
    { name: 'Melee swipe', src: ['LA'], world: '2', char: 'Space', what: '1 damage to the cell you face, knocks back and stuns. Reds and blues take 2 hits.' },
    { name: 'Time bomb', src: ['BM'], world: '2', char: 'B key · t', what: '2.5 s fuse. Pickups give +2.' },
    { name: 'Throw / remote bomb', src: ['BM'], world: 'classic', char: 'T C X', what: 'Throw up to 3 cells (explodes on landing); remote bombs go off on X.' },
    { name: 'Blast', src: ['BM'], world: '2', char: '—', what: 'A + of 2 cells. Kills enemies, breaks the first wooden crate per arm, chains other bombs, hurts you (1 ❤). Stopped by walls, metal crates, mirrors, posts, gates.' },
    { name: 'Bomb supply crate', src: ['BM', 'Ours'], world: '2', char: 'S', what: 'By every combat room\'s door: stand on it to top time bombs back up to 3 — never used up.' },
    { name: 'Ammo by threat', src: ['Ours'], world: '2', char: 't', what: '≈1.5 bombs per enemy (BOMBS / ENEMY slider) placed inside the entrance of every room with enemies.' },
    { name: 'Laser emitter', src: ['D&A'], world: '3', char: '> < v u', what: 'A free-standing post firing a beam. Crossable but costs 1 ❤ on entry, then 2 ❤/s. Kills enemies, sets bombs off.' },
    { name: 'Mirror crate', src: ['D&A'], world: '3', char: '/ \\', what: 'Pushable; E flips it 90°. Bombs can\'t break it.' },
    { name: 'Receiver + laser gate', src: ['D&A'], world: '3', char: 'O H', what: 'Light every receiver of a channel and its gate opens (3 channels: O/H, o/U, 0/V).' },
    { name: 'Teleporter pads', src: ['D&A', 'LA'], world: '4', char: '@ & $', what: 'Pairs. Step on one, come out of its partner — blocked if a crate sits on the partner.' },
    { name: 'Coloured keys + gates', src: ['LA'], world: '5', char: 'r g y / R G Y', what: 'Walk over a key to hold it (never used up); its gate opens. The key waits in a side room — go fetch it.' },
    { name: 'Pressure plate', src: ['LA'], world: '5', char: '_ J', what: 'A crate on every plate opens the plate gates.' },
    { name: 'Batteries + slot emitters', src: ['D&A'], world: '6', char: 'Z 1–8', what: 'A dead emitter has an empty slot: pick up the battery, face the emitter, E to put it in (or take it out).' },
    { name: 'Wooden wall', src: ['Ours'], world: '6', char: 'F', what: 'A plank stretch of room wall: blocks walking, crates, bullets and blasts — but lasers shine through.' },
    { name: 'Wooden door', src: ['BM', 'LA'], world: '6', char: 'D', what: 'Shut for everyone (enemies too). A blast next to it smashes it: treasure side rooms, blastable shortcuts.' },
    { name: 'Pulsing laser', src: ['D&A'], world: '6', char: ') ( w n', what: 'Fires on a beat clock: flicker warning, on, off — cross in the gap.' },
];

const ENEMIES: Row[] = [
    { name: 'Patroller', src: ['BM', 'LA'], world: '2', char: 'p', what: 'Walks a line, turns when blocked. 1 hp, weight 1. Bumping you costs 1 ❤.' },
    { name: 'Red', src: ['LA'], world: '2', char: 'm', what: 'Melee, dumb and slow: chases 2 cells, plods straight at you, stuck when blocked, forgets you outside its circle. 2 hp, weight 2.' },
    { name: 'Yellow sentry', src: ['Ours'], world: 'classic', char: 's', what: 'Patrols and only shoots straight ahead the way it faces (range 5, slow bullets). Never turns to aim — sneak past behind it.' },
    { name: 'Blue laser', src: ['Ours'], world: 'classic', char: 'l', what: 'Hunts with pathfinding (chase 6); with line of sight at any angle it charges, then zaps (range 3, 0.5 ❤/s) until you break line of sight. 2 hp.' },
];

const WORLDS: [string, string][] = [
    ['World 1', 'crates + gold keys + exit — rooms: start, puzzle, rest, bonus (key behind a puzzle), final'],
    ['World 2', '+ enemies and your bombs — combat (biggest rooms, supply crate), mixed (small puzzle + an enemy)'],
    ['World 3', '+ lasers and mirror crates — laser room: aim the beam at the receiver to open the gate'],
    ['World 4', '+ teleporter pads — teleport room: the pad onward sits behind a crate puzzle'],
    ['World 5', '+ coloured keys and plates — lock room (key in a side room), plate room'],
    ['World 6', '+ batteries, wooden walls / doors, pulsing lasers — dead emitter, beam through planks, treasure room'],
    ['Classic', 'every mechanic and all four enemies mixed, one slider each (the original lab generator)'],
];

const S = {
    wrap: { width: '100%', boxSizing: 'border-box' as const, background: '#0d0f16', color: '#94a3b8', padding: '24px 28px', borderRadius: 12, border: '1px solid rgba(34,176,125,0.25)', fontSize: '0.88rem', lineHeight: 1.55 },
    h1: { color: '#f8fafc', fontWeight: 800, fontSize: '1.4rem', margin: '0 0 6px' },
    h2: { color: '#34d399', fontWeight: 700, fontSize: '0.78rem', letterSpacing: 0.6, textTransform: 'uppercase' as const, margin: '22px 0 8px' },
    table: { width: '100%', borderCollapse: 'collapse' as const },
    th: { textAlign: 'left' as const, color: '#cbd5e1', fontWeight: 700, fontSize: '0.75rem', padding: '6px 8px', borderBottom: '1px solid rgba(255,255,255,0.12)', whiteSpace: 'nowrap' as const },
    td: { padding: '6px 8px', borderBottom: '1px solid rgba(255,255,255,0.06)', verticalAlign: 'top' as const },
    code: { fontFamily: 'ui-monospace, Menlo, monospace', color: '#e2e8f0', background: 'rgba(255,255,255,0.06)', padding: '0 4px', borderRadius: 4, whiteSpace: 'nowrap' as const },
    tag: (c: string) => ({ display: 'inline-block', color: c, border: `1px solid ${c}55`, borderRadius: 999, padding: '0 7px', fontSize: '0.7rem', margin: '1px 4px 1px 0', whiteSpace: 'nowrap' as const }),
};

function Table({ rows }: { rows: Row[] }) {
    return (
        <div style={{ overflowX: 'auto' }}>
            <table style={S.table}>
                <thead>
                    <tr><th style={S.th}>Name</th><th style={S.th}>Inspired by</th><th style={S.th}>World</th><th style={S.th}>Layout</th><th style={S.th}>What it does</th></tr>
                </thead>
                <tbody>
                    {rows.map((r) => (
                        <tr key={r.name}>
                            <td style={{ ...S.td, color: '#f1f5f9', fontWeight: 600, whiteSpace: 'nowrap' }}>{r.name}</td>
                            <td style={S.td}>{r.src.map((x) => <span key={x} style={S.tag(SRC[x].color)}>{SRC[x].label}</span>)}</td>
                            <td style={{ ...S.td, whiteSpace: 'nowrap' }}>{r.world}</td>
                            <td style={S.td}><span style={S.code}>{r.char}</span></td>
                            <td style={S.td}>{r.what}</td>
                        </tr>
                    ))}
                </tbody>
            </table>
        </div>
    );
}

export default function Map19Explanation() {
    return (
        <div style={S.wrap}>
            <h1 style={S.h1}>PUZZLE ISLANDS — what's built</h1>
            <p style={{ margin: 0 }}>
                A 2D lab for the game's puzzle rooms (Godot <span style={S.code}>PuzzleGrid</span>), built from three games:{' '}
                <span style={S.tag(SRC['D&A'].color)}>Dino &amp; Aliens</span> crates, lasers, mirrors and teleporters ·{' '}
                <span style={S.tag(SRC.LA.color)}>Link's Awakening</span> a dungeon of rooms with one idea each, keys, plates, room reset, hearts, sword ·{' '}
                <span style={S.tag(SRC.BM.color)}>Bomberman</span> bombs, cross blasts, chain reactions, blastable doors. Try ideas here, port the ones that work.
            </p>

            <h2 style={S.h2}>Worlds (GENERATOR dropdown) — each adds one idea, keeps the earlier ones</h2>
            <table style={S.table}>
                <tbody>
                    {WORLDS.map(([w, what]) => (
                        <tr key={w}><td style={{ ...S.td, color: '#f1f5f9', fontWeight: 600, whiteSpace: 'nowrap', width: 90 }}>{w}</td><td style={S.td}>{what}</td></tr>
                    ))}
                </tbody>
            </table>

            <h2 style={S.h2}>Mechanics ({MECHANICS.length})</h2>
            <Table rows={MECHANICS} />

            <h2 style={S.h2}>Enemies ({ENEMIES.length}) — real time, each with one readable rule</h2>
            <Table rows={ENEMIES} />
            <p>They patrol until they notice you ("!"), then act. Walls, crates and closed gates block them, their bullets and lasers. Circles show chase range (filled) and attack range (dashed). Threat = Σ weights; rooms get a threat budget by role (THREAT × slider).</p>

            <h2 style={S.h2}>How a world level is generated</h2>
            <ol style={{ margin: 0, paddingLeft: 20 }}>
                <li>Split the map into rooms (BSP, one-cell doorways → a room tree); start and exit rooms far apart; the route between them.</li>
                <li>Give every room a <b>role</b> along the route (puzzle → rest every few rooms, fights in the biggest rooms; the newest world's room type first, then a fight, then older ones), pushes ramping up to the final room.</li>
                <li>Build each room's puzzle <b>alone</b> in a sealed copy and solve it there (crate vaults scrambled by reverse pulls; laser / plate / teleport rooms from their builders) — a few ms each.</li>
                <li>Enemies by threat budget (spread out, ≥ 3 cells from doors), ammo by threat, a supply crate per combat room.</li>
                <li>Check the whole level room by room (<b>legs</b>: each room's goal, a lock's key first, the bonus key, then the exit); full search only as a fallback. Same seed → same level.</li>
            </ol>

            <h2 style={S.h2}>Controls</h2>
            <p style={{ margin: 0 }}>
                Arrows / WASD move · Shift+arrow turn · E flip mirror / battery in-out · Space melee · B time bomb · T throw · C remote, X detonate · Z undo · R restart · N next seed.
                Score = Sokoban (pushes + log₂ solver states) + mechanics + enemy weights near the route — Easy &lt; 18 ≤ Medium &lt; 30 ≤ Hard &lt; 45 ≤ Expert.
            </p>
        </div>
    );
}
