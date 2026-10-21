// Wick — a top-down 2D prototype of a spooky light-and-dark game: you are a living candle walking a
// drowned valley at night, and your wax is your health, your ammo and your light. The rules are
// plain TypeScript (Map22Sim and friends) so they port to Godot once they're fun; this file only
// runs the loop, reads input and draws.
import { useEffect, useRef, useState } from 'react';
import { DEFAULT_SETTINGS, type Settings } from './Map22Settings';
import { createSim, screenToWorld, step } from './Map22Sim';
import { LEVELS } from './Map22Levels';
import { drawSim } from './Map22Render';
import type { Input, Sim } from './Map22State';

import { Section, Slider } from './Map19Ui';
import { btn } from './Map19UiStyle';

const DT = 1 / 60;
type HeldKey = 'up' | 'down' | 'left' | 'right';
const MOVE: Record<string, HeldKey> = { w: 'up', s: 'down', a: 'left', d: 'right', ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right' };

export default function Map22({ width = 800 }: { width?: number; height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const [settings, setSettings] = useState<Settings>(DEFAULT_SETTINGS);
    const [paused, setPaused] = useState(false);
    const [run, setRun] = useState(0);
    const [level, setLevel] = useState(0);
    const [hud, setHud] = useState('');
    const simRef = useRef<Sim | null>(null);
    const settingsRef = useRef(settings);
    const pausedRef = useRef(paused);
    const mouseScreen = useRef({ x: 0.5, y: 0.3 });
    const input = useRef<Input>({ up: false, down: false, left: false, right: false, throw: false, dash: false, flare: false, mouse: { x: 0, y: 0 } });

    useEffect(() => {
        settingsRef.current = settings;
        if (simRef.current) simRef.current.settings = settings;
    }, [settings]);
    useEffect(() => { pausedRef.current = paused; }, [paused]);
    useEffect(() => { simRef.current = createSim(settingsRef.current, LEVELS[level]); }, [run, level]);

    useEffect(() => {
        const key = (down: boolean) => (ev: KeyboardEvent) => {
            if ((ev.target as HTMLElement)?.tagName === 'INPUT') return;
            const k = ev.key.length === 1 ? ev.key.toLowerCase() : ev.key;
            if (k === ' ' || k.startsWith('Arrow')) ev.preventDefault();
            if (MOVE[k]) input.current[MOVE[k]] = down;
            else if (down && (k === ' ' || k === 'Shift')) input.current.dash = true;
            else if (down && k === 'e') input.current.flare = true;
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
                const next = LEVELS.indexOf(sim.level) + 1;
                if (sim.phase === 'won' && sim.phaseT > 4 && next < LEVELS.length) { simRef.current = null; setLevel(next); }
                hudLeft -= elapsed;
                if (hudLeft <= 0) { hudLeft = 0.25; setHud(hudText(sim)); }
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
        <div style={{ width: '100%', color: '#e2e8f0', fontFamily: 'Inter, sans-serif', background: '#05070b' }}>
            <canvas
                ref={canvasRef}
                width={width}
                height={width}
                style={{ display: 'block', width: '100%', maxWidth: width, aspectRatio: '1', cursor: 'none' }}
                onMouseMove={toScreen}
                onMouseDown={(ev) => {
                    toScreen(ev);
                    if (ev.button === 2) input.current.flare = true;
                    else input.current.throw = true;
                }}
                onMouseUp={(ev) => { if (ev.button !== 2) input.current.throw = false; }}
                onMouseLeave={() => (input.current.throw = false)}
                onContextMenu={(ev) => ev.preventDefault()}
            />
            <div style={{ padding: '10px 14px', display: 'flex', flexDirection: 'column', gap: 8 }}>
                <pre style={{ margin: 0, fontSize: '0.75rem', color: '#a8a29e', whiteSpace: 'pre-wrap' }}>{hud}</pre>
                <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap' }}>
                    <button style={btn(true)} onClick={() => setRun((r) => r + 1)}>Restart (R)</button>
                    <button style={btn(false)} onClick={() => setPaused((p) => !p)}>{paused ? 'Play' : 'Pause'} (P)</button>
                    <button style={btn(false)} onClick={() => setSettings(DEFAULT_SETTINGS)}>Default settings</button>
                    {LEVELS.map((l, i) => (
                        <button key={l.name} style={{ ...btn(i === level), padding: '6px 10px' }} onClick={() => { setLevel(i); setRun((r) => r + 1); }}>
                            {i + 1}. {l.name}
                        </button>
                    ))}
                    <label style={{ fontSize: '0.75rem', display: 'flex', gap: 6, alignItems: 'center' }}>
                        <input type="checkbox" checked={settings.shake} onChange={(e) => set('shake')(e.target.checked)} /> screen shake
                    </label>
                </div>
                <Section title="WAX AND LIGHT">
                    {num('lightMin', 'light at no wax', 1, 5, 0.1, ' m')}
                    {num('lightMax', 'light at full wax', 3, 12, 0.25, ' m')}
                    {num('throwCost', 'wax per flame', 0, 15, 1)}
                    {num('throwRate', 'flames per second', 1, 8, 0.5)}
                    {num('emberTime', 'ember burns', 0, 12, 0.5, ' s')}
                    {num('flareCost', 'flare cost', 0, 40, 1)}
                    {num('flareRadius', 'flare radius', 2, 10, 0.25, ' m')}
                    {num('lanternRadius', 'lantern light', 3, 12, 0.25, ' m')}
                    {num('lanternHeal', 'lantern refill', 0, 40, 1, '/s')}
                </Section>
                <Section title="YOU">
                    {num('playerSpeed', 'speed', 3, 12, 0.5, ' m/s')}
                    {num('dashDistance', 'dash', 1, 8, 0.25, ' m')}
                    {num('viewSpan', 'view width', 12, 40, 1, ' m')}
                </Section>
                <Section title="THE DARK">
                    {num('shadeSpeed', 'shade speed', 1, 9, 0.25, ' m/s')}
                    {num('shadeHit', 'shade lunge', 0, 30, 1, ' wax')}
                    {num('burnRate', 'light burns shades', 0, 20, 0.5, '/s')}
                    {num('mothDrain', 'moth drinks', 0, 10, 0.5, ' wax/s')}
                    {num('weeperSpeed', 'weeper speed', 1, 10, 0.25, ' m/s')}
                    {num('weeperHit', 'weeper hit', 0, 60, 1, ' wax')}
                    {num('ambientEvery', 'dark sends a shade every', 0, 20, 0.5, ' s')}
                </Section>
            </div>
        </div>
    );
}

function hudText(sim: Sim): string {
    const p = sim.player;
    const counts = new Map<string, number>();
    for (const e of sim.enemies) counts.set(`${e.kind} ${e.state}`, (counts.get(`${e.kind} ${e.state}`) ?? 0) + 1);
    const states = [...counts].map(([s, n]) => (n > 1 ? `${s} ×${n}` : s)).join(' · ');
    return 'WASD walk · mouse aim · hold click: throw flames (cost wax) · right click / E: flare · Space dash (shakes off moths) · stand at a lantern to kindle it · R restart · P pause\n' +
        `${sim.phase} · wax ${Math.max(0, Math.round(p.wax))} · lanterns ${sim.lanterns.filter((l) => l.lit).length}/${sim.lanterns.length} · shades ${sim.stats.shades} · moths ${sim.stats.moths} · weepers ${sim.stats.weepers} · snuffed out ${sim.stats.snuffed}\n` +
        `in the dark: ${states || '—'}`;
}
