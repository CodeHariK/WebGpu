// Combat Sandbox — a top-down 2D prototype of the fights on the clown planet, to find the fun fast.
// Themes (Map21Themes) are different places — the Toxic Factory, the Toy Assembly Plant — each with
// its own look and levels, each level adding a place mechanic and a toy; machines to break, keys
// and locked doors, an exit; bubble wand, fizz bubbles, dash with perfect dodges, a close-range pop,
// joy from freed clowns. Every "feel" effect can be switched off and every number tuned live. The rules are plain TypeScript (Map21Sim and friends) so they port to
// Godot once they're fun; this file only runs the loop, reads input and draws.
import { useEffect, useRef, useState } from 'react';
import { DEFAULT_SETTINGS, type Feel, type Settings } from './Map21Settings';
import { createSim, readyForNext, screenToWorld, skipWaves, step } from './Map21Sim';
import { THEMES } from './Map21Themes';
import { drawSim } from './Map21Render';
import type { Input, Sim } from './Map21State';

import { Section, Slider } from './Map19Ui';
import { btn } from './Map19UiStyle';

const DT = 1 / 60;
const FEEL: [keyof Feel, string][] = [
    ['shake', 'screen shake'], ['hitStop', 'hit-stop (freeze on hits)'], ['knockback', 'knockback'], ['flash', 'hit flash'],
    ['particles', 'particles'], ['numbers', 'damage numbers'], ['slowmo', 'slow-mo on perfect dodge'], ['recoil', 'recoil'],
];
type HeldKey = 'up' | 'down' | 'left' | 'right';
const MOVE: Record<string, HeldKey> = { w: 'up', s: 'down', a: 'left', d: 'right', ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right' };

export default function Map21({ width = 800 }: { width?: number; height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const [settings, setSettings] = useState<Settings>(DEFAULT_SETTINGS);
    const [paused, setPaused] = useState(false);
    const [run, setRun] = useState(0); // bump to restart
    const [theme, setTheme] = useState(0);
    const [level, setLevel] = useState(0);
    const [hud, setHud] = useState('');
    const simRef = useRef<Sim | null>(null);
    const settingsRef = useRef(settings);
    const pausedRef = useRef(paused);
    const mouseScreen = useRef({ x: 0.5, y: 0.3 }); // fraction of the canvas: the world point under it moves with the camera
    const input = useRef<Input>({ up: false, down: false, left: false, right: false, fire: false, dash: false, pop: false, fizz: false, mouse: { x: 14, y: 10 } });

    useEffect(() => {
        settingsRef.current = settings;
        if (simRef.current) simRef.current.settings = settings;
    }, [settings]);
    useEffect(() => {
        pausedRef.current = paused;
    }, [paused]);
    useEffect(() => {
        simRef.current = createSim(settingsRef.current, theme, level);
    }, [run, theme, level]);

    // Keyboard (ignored while typing in a field).
    useEffect(() => {
        const key = (down: boolean) => (ev: KeyboardEvent) => {
            if ((ev.target as HTMLElement)?.tagName === 'INPUT') return;
            const k = ev.key.length === 1 ? ev.key.toLowerCase() : ev.key;
            if (k === ' ' || k.startsWith('Arrow')) ev.preventDefault();
            if (MOVE[k]) input.current[MOVE[k]] = down;
            else if (down && (k === ' ' || k === 'Shift')) input.current.dash = true;
            else if (down && k === 'e') input.current.pop = true;
            else if (down && k === 'q') input.current.fizz = true;
            else if (down && k === 'r') setRun((r) => r + 1);
            else if (down && k === 'p') setPaused((p) => !p);
        };
        const kd = key(true);
        const ku = key(false);
        window.addEventListener('keydown', kd);
        window.addEventListener('keyup', ku);
        return () => {
            window.removeEventListener('keydown', kd);
            window.removeEventListener('keyup', ku);
        };
    }, []);

    // The loop: fixed 60 Hz steps, draw every frame, HUD text a few times a second.
    useEffect(() => {
        let raf = 0;
        let last = performance.now();
        let acc = 0;
        let hudLeft = 0;
        const frame = (now: number) => {
            const sim = simRef.current;
            const ctx = canvasRef.current?.getContext('2d');
            const elapsed = Math.min(0.1, (now - last) / 1000);
            last = now;
            if (sim && ctx) {
                if (!pausedRef.current) {
                    acc += elapsed;
                    while (acc >= DT) {
                        input.current.mouse = screenToWorld(sim, mouseScreen.current.x, mouseScreen.current.y);
                        step(sim, input.current, DT);
                        acc -= DT;
                    }
                }
                drawSim(ctx, sim, input.current.mouse);
                if (readyForNext(sim)) { // level clear: on to the next one, keeping nothing but your skill
                    simRef.current = null;
                    setLevel(sim.level + 1);
                }
                hudLeft -= elapsed;
                if (hudLeft <= 0) {
                    hudLeft = 0.25;
                    setHud(hudText(sim));
                }
            }
            raf = requestAnimationFrame(frame);
        };
        raf = requestAnimationFrame(frame);
        return () => cancelAnimationFrame(raf);
    }, []);

    const toScreen = (ev: React.MouseEvent) => {
        const r = canvasRef.current!.getBoundingClientRect();
        mouseScreen.current = { x: (ev.clientX - r.left) / r.width, y: (ev.clientY - r.top) / r.height };
    };
    const set = <K extends keyof Settings>(k: K) => (value: Settings[K]) => setSettings((s) => ({ ...s, [k]: value }));
    const num = (k: keyof Settings, label: string, min: number, max: number, stepSize: number, unit = '') => (
        <Slider key={k} label={`${label} ${(settings[k] as number).toFixed(stepSize < 1 ? 2 : 0)}${unit}`} min={min} max={max} step={stepSize} value={settings[k] as number} onChange={set(k) as (v: number) => void} />
    );

    return (
        <div style={{ width: '100%', color: '#e2e8f0', fontFamily: 'Inter, sans-serif', background: '#0b1220' }}>
            <canvas
                ref={canvasRef}
                width={width}
                height={width}
                style={{ display: 'block', width: '100%', maxWidth: width, aspectRatio: '1', cursor: 'none' }}
                onMouseMove={toScreen}
                onMouseDown={(ev) => {
                    toScreen(ev);
                    if (ev.button === 2) input.current.pop = true;
                    else input.current.fire = true;
                }}
                onMouseUp={(ev) => { if (ev.button !== 2) input.current.fire = false; }}
                onMouseLeave={() => (input.current.fire = false)}
                onContextMenu={(ev) => ev.preventDefault()}
            />
            <div style={{ padding: '10px 14px', display: 'flex', flexDirection: 'column', gap: 8 }}>
                <pre style={{ margin: 0, fontSize: '0.75rem', color: '#94a3b8', whiteSpace: 'pre-wrap' }}>{hud}</pre>
                <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap' }}>
                    <button style={btn(true)} onClick={() => setRun((r) => r + 1)}>Restart (R)</button>
                    <button style={btn(false)} onClick={() => setPaused((p) => !p)}>{paused ? 'Play' : 'Pause'} (P)</button>
                    <button style={btn(false)} onClick={() => setSettings(DEFAULT_SETTINGS)}>Default settings</button>
                    <button style={btn(false)} onClick={() => simRef.current && skipWaves(simRef.current)}>Skip level (test)</button>
                </div>
                <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap', alignItems: 'center' }}>
                    <span style={{ fontSize: '0.75rem', color: '#94a3b8' }}>THEME</span>
                    {THEMES.map((t, i) => (
                        <button key={t.id} style={{ ...btn(i === theme), padding: '6px 10px' }} onClick={() => { setTheme(i); setLevel(0); setRun((r) => r + 1); }}>
                            {t.name}
                        </button>
                    ))}
                </div>
                <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap', alignItems: 'center' }}>
                    <span style={{ fontSize: '0.75rem', color: '#94a3b8' }}>LEVEL</span>
                    {THEMES[theme].levels.map((l, i) => (
                        <button key={l.name} style={{ ...btn(i === level), padding: '6px 10px' }} onClick={() => { setLevel(i); setRun((r) => r + 1); }}>
                            {i + 1}. {l.name}
                        </button>
                    ))}
                </div>
                <Section title="FEEL (switch off to see what each one carries)">
                    {FEEL.map(([k, label]) => (
                        <label key={k} style={{ fontSize: '0.75rem', display: 'flex', gap: 6, alignItems: 'center', cursor: 'pointer' }}>
                            <input type="checkbox" checked={settings[k]} onChange={(e) => set(k)(e.target.checked)} />
                            {label}
                        </label>
                    ))}
                    {num('shakeAmount', 'shake amount', 0, 2, 0.05)}
                    {num('hitStopMs', 'hit-stop', 0, 120, 5, ' ms')}
                </Section>
                <Section title="PLAYER">
                    {num('playerSpeed', 'speed', 3, 12, 0.5, ' m/s')}
                    {num('dashDistance', 'dash', 1, 8, 0.25, ' m')}
                    {num('dashTime', 'dash time', 0.06, 0.4, 0.01, ' s')}
                    {num('iframes', 'invincible', 0, 0.5, 0.01, ' s')}
                    {num('dashCooldown', 'dash cooldown', 0, 1.5, 0.05, ' s')}
                    {num('fireRate', 'fire rate', 2, 20, 1, '/s')}
                    {num('bubbleSpeed', 'bubble speed', 8, 40, 1, ' m/s')}
                    {num('spread', 'spread', 0, 25, 1, '°')}
                    {num('popRadius', 'pop radius', 1, 5, 0.1, ' m')}
                    {num('popDamage', 'pop damage', 1, 10, 1)}
                    {num('popCooldown', 'pop cooldown', 0.2, 4, 0.1, ' s')}
                </Section>
                <Section title="CAMERA">
                    {num('viewSpan', 'view width', 12, 34, 1, ' m')}
                    {num('aimLead', 'lean toward aim', 0, 8, 0.25, ' m')}
                </Section>
                <Section title="TOYS">
                    {num('enemySpeed', 'move speed', 1, 8, 0.25, ' m/s')}
                    {num('aimTell', 'rifle tell', 0.1, 1.5, 0.05, ' s')}
                    {num('boltSpeed', 'bolt speed', 5, 30, 1, ' m/s')}
                    {num('windTime', 'wind-up (weak)', 0.3, 4, 0.1, ' s')}
                    {num('chargeSpeed', 'riot charge', 6, 25, 1, ' m/s')}
                </Section>
                <Section title="PRESSURE">
                    {num('gasSpeed', 'gas creep', 0, 1.5, 0.05, ' m/s')}
                    {num('gasPushBack', 'gas push-back per wave', 0, 15, 0.5, ' m')}
                    {num('maxJoy', 'max joy', 10, 150, 5)}
                </Section>
            </div>
        </div>
    );
}

function hudText(sim: Sim): string {
    const counts = new Map<string, number>();
    for (const e of sim.enemies) counts.set(`${e.kind} ${e.state}`, (counts.get(`${e.kind} ${e.state}`) ?? 0) + 1);
    const states = [...counts].map(([s, n]) => (n > 1 ? `${s} ×${n}` : s)).join(' · ');
    const { kills, perfects, hitsTaken, bestStreak } = sim.stats;
    const def = sim.arena.def;
    const a = sim.arena;
    const left = a.objectives.filter((o) => !o.broken).length;
    const goal = a.exitOpen ? 'EXIT OPEN — get to the green gate' : `break ${left}/${a.objectives.length} machines${a.core && a.core.hp > 0 ? ' and the core' : ''}`;
    return 'WASD move · mouse aim · hold click: bubbles (cost joy) · Q fizz bubble · Space / Shift dash · right click / E pop · R restart level · P pause\n' +
        `${THEMES[sim.theme].name} · ${sim.level + 1}. ${def.name} — ${def.tagline}\n` +
        `${goal} · keys: ${sim.player.keys.join(', ') || '—'} · fizz ${sim.player.fizz}\n` +
        `${sim.phase} · wave ${sim.wave}/${def.waves.length} · kills ${kills} · perfect dodges ${perfects} · hits taken ${hitsTaken} · best streak ${bestStreak}\n` +
        `toys: ${states || '—'}`;
}
