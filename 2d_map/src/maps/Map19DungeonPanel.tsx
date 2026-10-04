// Puzzle Islands — the dungeon generator's control panel, in collapsible sections:
//   LEVEL       preset (World · Level), seed, size, rooms
//   DIFFICULTY  difficulty dial + TARGET SCORE (generate N seeds, keep the closest)
//   ENEMIES     on / off, threat × multiplier
//   PICKUPS     ammo generosity (bombs per enemy)
//   SOLUTION    replay, dotted route overlay
//   LAYOUT      copy the level as ASCII, hand-tune it, paste it back and play it
import { useState } from 'react';
import type { DungeonParams, RoomRole } from './Map19Dungeon';
import { PRESETS, presetParams } from './Map19Presets';
import { Section, Slider } from './Map19Ui';
import { btn, UI } from './Map19UiStyle';
import { TARGET_TRIES } from './Map19Pick';

type Props = {
    world: number; // the GENERATOR dropdown's world: sets enemies, filters the level presets
    dgen: DungeonParams;
    setDgen: (p: DungeonParams) => void;
    target: number; // 0 = off
    setTarget: (t: number) => void;
    picked: { seed: number; score: number; min: number; max: number } | null;
    showPath: boolean;
    setShowPath: (v: boolean) => void;
    onSolution: () => void;
    rows: string[];
    custom: boolean;
    onPaste: (text: string) => string; // returns an error ('' = ok)
    onClearLayout: () => void;
    roleColors: Record<RoomRole, string>;
};

export function DungeonPanel(p: Props) {
    const { dgen } = p;
    const set = <K extends keyof DungeonParams>(k: K) => (v: DungeonParams[K]) => p.setDgen({ ...dgen, [k]: v });
    const [preset, setPreset] = useState('');
    const [presetWorld, setPresetWorld] = useState(p.world);
    if (presetWorld !== p.world) { setPresetWorld(p.world); setPreset(''); } // another world: level back to custom
    return (
        <div style={{ padding: '6px 16px 10px' }}>
            <Section title="LEVEL">
                <select
                    value={preset}
                    onChange={(e) => { setPreset(e.target.value); if (e.target.value) p.setDgen(presetParams(e.target.value, dgen.seed)); }}
                    style={{ ...btn(false), padding: '9px 10px' }}
                >
                    <option value="">Level: custom</option>
                    {PRESETS.filter((x) => x.world === p.world).map((x) => <option key={x.id} value={x.id}>{x.label}</option>)}
                </select>
                <button onClick={() => set('seed')(Math.floor(Math.random() * 99999))} style={btn(true)}>🎲 SEED</button>
                <Slider label={`SEED ${dgen.seed}`} min={0} max={99999} value={dgen.seed} onChange={set('seed')} />
                <Slider label={`WIDTH ${dgen.width}`} min={13} max={35} value={dgen.width} onChange={set('width')} />
                <Slider label={`HEIGHT ${dgen.height}`} min={9} max={25} value={dgen.height} onChange={set('height')} />
                <Slider label={`ROOMS ${dgen.rooms}`} min={2} max={9} value={dgen.rooms} onChange={set('rooms')} />
            </Section>
            <Section title="DIFFICULTY">
                <Slider label={`DIFFICULTY ${dgen.difficulty}`} min={1} max={10} value={dgen.difficulty} onChange={set('difficulty')} />
                <Slider label={`TARGET SCORE ${p.target || 'OFF'}`} min={0} max={40} value={p.target} onChange={p.setTarget} />
                <span style={{ color: UI.text, fontSize: '0.72rem' }}>
                    {p.custom
                        ? 'playing a pasted layout — generator settings ignored'
                        : p.target
                        ? p.picked
                            ? `picked seed ${p.picked.seed} — score ${p.picked.score} for target ${p.target} (${TARGET_TRIES} seeds at this difficulty score ${p.picked.min}–${p.picked.max}${p.target > p.picked.max ? ': raise DIFFICULTY / size for more' : p.target < p.picked.min ? ': lower DIFFICULTY / size for less' : ''})`
                            : 'no solvable candidate'
                        : `target off: seed ${dgen.seed} as is · Easy < 18 ≤ Medium < 30 ≤ Hard < 45 ≤ Expert`}
                </span>
            </Section>
            <Section title="ENEMIES & PICKUPS">
                <span style={{ color: UI.text, fontSize: '0.72rem' }}>enemies {dgen.enemies ? 'on' : 'off'} · lasers {dgen.lasers ? 'on' : 'off'} · teleporters {dgen.teleports ? 'on' : 'off'} · locks + plates {dgen.locks ? 'on' : 'off'} · wood / batteries / pulses {dgen.wood ? 'on' : 'off'} (set by the world)</span>
                {dgen.enemies && <Slider label={`THREAT ×${dgen.threatScale.toFixed(2)}`} min={0.5} max={2} step={0.25} value={dgen.threatScale} onChange={set('threatScale')} />}
                {dgen.enemies && <Slider label={`BOMBS / ENEMY ${dgen.ammoPerEnemy.toFixed(2)}`} min={1} max={2} step={0.25} value={dgen.ammoPerEnemy} onChange={set('ammoPerEnemy')} />}
                <span style={{ color: UI.text, fontSize: '0.72rem' }}>
                    {(Object.keys(p.roleColors) as RoomRole[]).map((r) => <span key={r} style={{ color: p.roleColors[r], marginRight: 8 }}>■ {r}</span>)}
                </span>
            </Section>
            <Section title="SOLUTION">
                <button onClick={p.onSolution} style={btn(false)}>▶ REPLAY SOLUTION</button>
                <button onClick={() => p.setShowPath(!p.showPath)} style={btn(p.showPath)}>ROUTE OVERLAY: {p.showPath ? 'ON' : 'OFF'}</button>
                <span style={{ color: UI.text, fontSize: '0.72rem' }}>dotted line = the solver's walk · orange dots = pushes · green ring = finish</span>
            </Section>
            <LayoutBox key={p.rows.join('\n')} rows={p.rows} custom={p.custom} onPaste={p.onPaste} onClear={p.onClearLayout} />
        </div>
    );
}

/** Copy the current level as ASCII, edit it, paste it back and play it. */
function LayoutBox({ rows, custom, onPaste, onClear }: { rows: string[]; custom: boolean; onPaste: (t: string) => string; onClear: () => void }) {
    const [text, setText] = useState(() => rows.join('\n')); // remounted (key) when the level changes
    const [error, setError] = useState('');
    return (
        <Section title={`LAYOUT (ASCII)${custom ? ' — playing a pasted layout' : ''}`} open={custom}>
            <textarea
                value={text}
                onChange={(e) => setText(e.target.value)}
                spellCheck={false}
                style={{ width: '100%', minHeight: 200, boxSizing: 'border-box', background: '#11131c', color: '#e2e8f0', border: '1px solid rgba(255,255,255,0.12)', borderRadius: 8, padding: 10, fontFamily: 'ui-monospace, Menlo, monospace', fontSize: '0.75rem', lineHeight: 1.15 }}
            />
            <button onClick={() => setError(onPaste(text))} style={btn(true)}>PLAY THIS LAYOUT</button>
            <button onClick={() => navigator.clipboard?.writeText(text)} style={btn(false)}>COPY</button>
            {custom && <button onClick={onClear} style={btn(false)}>BACK TO GENERATOR</button>}
            {error && <span style={{ color: '#ff7a7a', fontSize: '0.75rem' }}>{error}</span>}
            <span style={{ color: UI.text, fontSize: '0.7rem' }}>chars: # wall · . floor · P player · E exit · K gold key · B crate · N metal crate · p / m enemies · t bombs · S supply · h heart (see Map19Rules)</span>
        </Section>
    );
}
