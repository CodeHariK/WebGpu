import React from 'react';

const ACCENT = '#86efac';
const h3 = { color: ACCENT, marginTop: 22, fontSize: '1.05rem' } as const;

const Map23Explanation: React.FC = () => (
    <div style={{ padding: 20, color: '#a8a29e', lineHeight: 1.6, fontFamily: 'Inter, sans-serif' }}>
        <h2 style={{ color: 'white', marginBottom: 15, borderBottom: `2px solid ${ACCENT}`, paddingBottom: 10 }}>
            Map 23: Lockpick — a dungeon sneak
        </h2>
        <p>
            A little thief wakes in a cell under the Old Keep. No weapon: get the iron key from the barracks, the gate key
            from the treasury, and out through the gate — past guards who patrol, sleep, watch and sniff.
        </p>
        <p><strong>Controls:</strong> WASD move · hold Shift: sneak · hold Space: run · click: throw a pebble · E: use (doors, torches, barrels, keys) · R restart · P pause.</p>

        <h3 style={h3}>Being seen</h3>
        <p>
            Guards see in a cone. How far depends on the light where <em>you</em> are: in torchlight they spot you across
            the room, in the dark only up close. Sneaking makes you harder to see; running easier. While a guard sees
            you a ring fills over its head (?): a little and it stops and stares; full and it knows (!) and chases.
            Out of its sight it goes to where it last saw you and looks round. Get caught and you're back at the last
            door you came through (you keep your keys).
        </p>

        <h3 style={h3}>Being heard — and distracting</h3>
        <p>
            Everything makes noise: footsteps (loud on creaky boards, glass and water — sneaking keeps most of that
            quiet, except glass), doors, a doused torch, a splash. Guards go to look at what they hear. So throw a
            <strong> pebble</strong>: it lands with a clatter and pulls guards to it. Sleepers wake at noise near them.
            <strong> Alarm plates</strong> ring a bell that brings everyone.
        </p>

        <h3 style={h3}>The dungeon helps (and hurts)</h3>
        <p><strong>Torches</strong> — douse one (E) and the room goes dark; a guard who passes will come and relight it.</p>
        <p><strong>Doors</strong> — shut doors block sight and light. Guards open them on their way and leave them open.</p>
        <p><strong>Barrels</strong> — climb in (E) and you can't be seen. But if a guard saw you climb in, it checks.</p>
        <p><strong>Water</strong> — slow and splashy, but it washes away your scent.</p>

        <h3 style={h3}>Who's down there</h3>
        <p><strong>Guard</strong> — patrols a route, investigates, chases, relights torches.</p>
        <p><strong>Sleeper</strong> — asleep in the barracks. Wake one and it goes to look, then back to bed. The iron key is right by them, and there's glass on the floor.</p>
        <p><strong>Watcher</strong> — stands in the watch room sweeping a long, narrow lantern beam. Spot you and it rings the alarm.</p>
        <p><strong>Hound</strong> — hardly sees, but follows your scent trail (shown faintly when it's near). Finds you — even in a barrel — and barks for the guards. Cross water to lose it.</p>
    </div>
);

export default Map23Explanation;
