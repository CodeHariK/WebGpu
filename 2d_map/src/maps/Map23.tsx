// Lockpick — a top-down 2D prototype of a dungeon sneak: no weapons, just patrols to slip past,
// noises to make in the right place, and the dungeon itself (torches, doors, barrels, creaky floors,
// glass, water, alarm plates). Find the keys and get out. The rules are plain TypeScript (Map23Sim
// and friends) so they port to Godot once they're fun; this file only runs the loop, reads input
// and draws.
import { useEffect, useRef, useState } from 'react';
import { DEFAULT_SETTINGS, type Settings } from './Map23Settings';
import { createSim, screenToWorld, step } from './Map23Sim';
import { KEEP } from './Map23Level';
import { drawSim } from './Map23Render';
import type { Input, Sim } from './Map23State';

import { Section, Slider } from './Map19Ui';
import { btn } from './Map19UiStyle';

const DT = 1 / 60;
type HeldKey = 'up' | 'down' | 'left' | 'right';
const MOVE: Record<string, HeldKey> = { w: 'up', s: 'down', a: 'left', d: 'right', ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right' };

export default function Map23({ width = 800 }: { width?: number; height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const [settings, setSettings] = useState<Settings>(DEFAULT_SETTINGS);
    const [paused, setPaused] = useState(false);
    const [run, setRun] = useState(0);
    const [hud, setHud] = useState('');
    const simRef = useRef<Sim | null>(null);
    const settingsRef = useRef(settings);
    const pausedRef = useRef(paused);
    const mouseScreen = useRef({ x: 0.5, y: 0.3 });
    const input = useRef<Input>({ up: false, down: false, left: false, right: false, sneak: false, run: false, use: false, throw: false, mouse: { x: 0, y: 0 } });

    useEffect(() => {
        settingsRef.current = settings;
        if (simRef.current) { simRef.current.settings = settings; simRef.current.lightDirty = true; }
    }, [settings]);
    useEffect(() => { pausedRef.current = paused; }, [paused]);
    useEffect(() => { simRef.current = createSim(settingsRef.current, KEEP); }, [run]);

    useEffect(() => {
        const key = (down: boolean) => (ev: KeyboardEvent) => {
            if ((ev.target as HTMLElement)?.tagName === 'INPUT') return;
            const k = ev.key.length === 1 ? ev.key.toLowerCase() : ev.key;
            if (k === ' ' || k.startsWith('Arrow')) ev.preventDefault();
            if (MOVE[k]) input.current[MOVE[k]] = down;
            else if (k === 'Shift') input.current.sneak = down;
            else if (k === ' ') input.current.run = down;
            else if (down && k === 'e') input.current.use = true;
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
        <div style={{ width: '100%', color: '#e2e8f0', fontFamily: 'Inter, sans-serif', background: '#050407' }}>
            <canvas
                ref={canvasRef}
                width={width}
                height={width}
                style={{ display: 'block', width: '100%', maxWidth: width, aspectRatio: '1', cursor: 'crosshair' }}
                onMouseMove={toScreen}
                onMouseDown={(ev) => { toScreen(ev); input.current.throw = true; }}
                onContextMenu={(ev) => ev.preventDefault()}
            />
            <div style={{ padding: '10px 14px', display: 'flex', flexDirection: 'column', gap: 8 }}>
                <pre style={{ margin: 0, fontSize: '0.75rem', color: '#a8a29e', whiteSpace: 'pre-wrap' }}>{hud}</pre>
                <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap', alignItems: 'center' }}>
                    <button style={btn(true)} onClick={() => setRun((r) => r + 1)}>Restart (R)</button>
                    <button style={btn(false)} onClick={() => setPaused((p) => !p)}>{paused ? 'Play' : 'Pause'} (P)</button>
                    <button style={btn(false)} onClick={() => setSettings(DEFAULT_SETTINGS)}>Default settings</button>
                    <label style={{ fontSize: '0.75rem', display: 'flex', gap: 6, alignItems: 'center' }}>
                        <input type="checkbox" checked={settings.showCones} onChange={(e) => set('showCones')(e.target.checked)} /> show sight cones
                    </label>
                </div>
                <Section title="YOU">
                    {num('walkSpeed', 'walk', 1, 8, 0.1, ' m/s')}
                    {num('sneakSpeed', 'sneak', 0.5, 5, 0.1, ' m/s')}
                    {num('runSpeed', 'run', 2, 10, 0.1, ' m/s')}
                    {num('throwRange', 'throw range', 2, 16, 0.5, ' m')}
                    {num('pebbleNoise', 'pebble heard within', 1, 15, 0.5, ' m')}
                    {num('viewSpan', 'view width', 12, 48, 1, ' m')}
                </Section>
                <Section title="GUARDS">
                    {num('sightRange', 'see you in light', 2, 16, 0.5, ' m')}
                    {num('darkSight', 'see you in the dark', 0.5, 8, 0.1, ' m')}
                    {num('sightAngle', 'cone half-angle', 10, 90, 1, '°')}
                    {num('noticeRate', 'notice speed', 0.2, 4, 0.1)}
                    {num('patrolSpeed', 'patrol', 0.5, 4, 0.1, ' m/s')}
                    {num('chaseSpeed', 'chase', 1, 8, 0.1, ' m/s')}
                    {num('houndSpeed', 'hound', 1, 8, 0.1, ' m/s')}
                    {num('torchRadius', 'torch light', 2, 10, 0.25, ' m')}
                </Section>
            </div>
        </div>
    );
}

function hudText(sim: Sim): string {
    const counts = new Map<string, number>();
    for (const g of sim.guards) counts.set(`${g.kind} ${g.state}`, (counts.get(`${g.kind} ${g.state}`) ?? 0) + 1);
    const states = [...counts].map(([s, n]) => (n > 1 ? `${s} ×${n}` : s)).join(' · ');
    return 'WASD move · hold Shift: sneak (quiet, hard to see) · hold Space: run (loud) · click: throw a pebble (a distraction) · E: doors, torches, barrels, keys · R restart · P pause\n' +
        `${sim.phase} · keys: ${sim.player.keys.join(', ') || '—'} · pebbles ${sim.player.pebbles} · seen ${sim.stats.spotted} · caught ${sim.stats.caught}\n` +
        `guards: ${states}`;
}
