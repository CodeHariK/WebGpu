// Enemy Arena — playable lab for simple, cheap enemy AI (Far Cry / Batman style): patrol + leash,
// cone + detection meter, hearing, shout, slot-ring surround, context steering, attack tokens with
// a wind-up, ranged enemies using cover markers, search and return. Every behaviour can be
// switched off to see what it adds. The AI is plain TypeScript (Map20Sim and friends) so it ports
// to Godot as is; this file only runs the loop, reads input and draws.
import { useEffect, useRef, useState } from 'react';
import { DEFAULT_SETTINGS, type Settings, type Toggles } from './Map20Settings';
import { createSim, step } from './Map20Sim';
import { drawSim, type View } from './Map20Render';
import type { Input, Sim } from './Map20State';
import { SIZE } from './Map20World';
import { Section, Slider } from './Map19Ui';
import { btn } from './Map19UiStyle';

const DT = 1 / 60;
const TOGGLES: [keyof Toggles, string][] = [
    ['hearing', 'hearing'], ['shout', 'shout for help'], ['leash', 'leash (give up far from home)'],
    ['slots', 'slot ring (surround)'], ['takeTurns', 'attack tokens (take turns)'], ['windup', 'wind-up before hits'],
    ['cover', 'ranged use cover'], ['retreat', 'retreat when hurt'], ['search', 'search when lost'],
];
const KEYS: Record<string, keyof Input> = { w: 'up', s: 'down', a: 'left', d: 'right', ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right', Shift: 'sneak', ' ': 'shout' };

export default function Map20({ width = 800 }: { width?: number; height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const [settings, setSettings] = useState<Settings>(DEFAULT_SETTINGS);
    const [view, setView] = useState<View>({ markers: true, slots: true, cones: true, patrols: true });
    const [paused, setPaused] = useState(false);
    const [run, setRun] = useState(0); // bump to restart
    const [hud, setHud] = useState('');
    const simRef = useRef<Sim | null>(null);
    const settingsRef = useRef(settings);
    const viewRef = useRef(view);
    const pausedRef = useRef(paused);
    const input = useRef<Input>({ up: false, down: false, left: false, right: false, sneak: false, shout: false, attack: false, mouse: { x: SIZE / 2, y: 0 } });

    useEffect(() => {
        settingsRef.current = settings;
        if (simRef.current) simRef.current.settings = settings;
    }, [settings]);
    useEffect(() => {
        viewRef.current = view;
    }, [view]);
    useEffect(() => {
        pausedRef.current = paused;
    }, [paused]);
    useEffect(() => {
        simRef.current = createSim(settingsRef.current);
    }, [run]);

    // Keyboard (ignored while typing in a field) and mouse.
    useEffect(() => {
        const key = (down: boolean) => (ev: KeyboardEvent) => {
            if ((ev.target as HTMLElement)?.tagName === 'INPUT') return;
            const k = KEYS[ev.key.length === 1 ? ev.key.toLowerCase() : ev.key];
            if (!k) return;
            if (ev.key === ' ' || ev.key.startsWith('Arrow')) ev.preventDefault();
            (input.current[k] as boolean) = down;
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
                        step(sim, input.current, DT);
                        acc -= DT;
                    }
                }
                drawSim(ctx, sim, viewRef.current);
                hudLeft -= elapsed;
                if (hudLeft <= 0) {
                    hudLeft = 0.2;
                    setHud(hudText(sim));
                }
            }
            raf = requestAnimationFrame(frame);
        };
        raf = requestAnimationFrame(frame);
        return () => cancelAnimationFrame(raf);
    }, []);

    const toMetres = (ev: React.MouseEvent) => {
        const r = canvasRef.current!.getBoundingClientRect();
        return { x: ((ev.clientX - r.left) / r.width) * SIZE, y: ((ev.clientY - r.top) / r.height) * SIZE };
    };
    const set = <K extends keyof Settings>(k: K) => (value: Settings[K]) => setSettings((s) => ({ ...s, [k]: value }));

    return (
        <div style={{ width: '100%', color: '#e2e8f0', fontFamily: 'Inter, sans-serif', background: '#0b1220' }}>
            <canvas
                ref={canvasRef}
                width={width}
                height={width}
                style={{ display: 'block', width: '100%', maxWidth: width, aspectRatio: '1', cursor: 'crosshair' }}
                onMouseMove={(ev) => (input.current.mouse = toMetres(ev))}
                onMouseDown={(ev) => {
                    input.current.mouse = toMetres(ev);
                    input.current.attack = true;
                }}
                onMouseUp={() => (input.current.attack = false)}
                onMouseLeave={() => (input.current.attack = false)}
            />
            <div style={{ padding: '10px 14px', display: 'flex', flexDirection: 'column', gap: 8 }}>
                <pre style={{ margin: 0, fontSize: '0.75rem', color: '#94a3b8', whiteSpace: 'pre-wrap' }}>{hud}</pre>
                <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap' }}>
                    <button style={btn(true)} onClick={() => setRun((r) => r + 1)}>Restart</button>
                    <button style={btn(false)} onClick={() => setPaused((p) => !p)}>{paused ? 'Play' : 'Pause'}</button>
                    <button style={btn(false)} onClick={() => setSettings(DEFAULT_SETTINGS)}>Default settings</button>
                </div>
                <Section title="BEHAVIOURS (switch off to see what each adds)">
                    {TOGGLES.map(([k, label]) => (
                        <label key={k} style={{ fontSize: '0.75rem', display: 'flex', gap: 6, alignItems: 'center', cursor: 'pointer' }}>
                            <input type="checkbox" checked={settings[k]} onChange={(e) => set(k)(e.target.checked)} />
                            {label}
                        </label>
                    ))}
                </Section>
                <Section title="NUMBERS">
                    <Slider label={`attack tokens ${settings.tokens}`} min={1} max={4} value={settings.tokens} onChange={set('tokens')} />
                    <Slider label={`wind-up ${settings.windupTime.toFixed(2)} s`} min={0.1} max={1.2} step={0.05} value={settings.windupTime} onChange={set('windupTime')} />
                    <Slider label={`ring ${settings.slotRadius.toFixed(1)} m`} min={1.5} max={5} step={0.1} value={settings.slotRadius} onChange={set('slotRadius')} />
                    <Slider label={`slots ${settings.slotCount}`} min={4} max={12} value={settings.slotCount} onChange={set('slotCount')} />
                    <Slider label={`view ${settings.viewAngle}°`} min={40} max={180} step={5} value={settings.viewAngle} onChange={set('viewAngle')} />
                    <Slider label={`range ${settings.viewRange} m`} min={4} max={20} value={settings.viewRange} onChange={set('viewRange')} />
                    <Slider label={`spot time ${settings.detectTime.toFixed(1)} s`} min={0.2} max={3} step={0.1} value={settings.detectTime} onChange={set('detectTime')} />
                    <Slider label={`enemy speed ${settings.enemySpeed.toFixed(1)}`} min={1} max={8} step={0.1} value={settings.enemySpeed} onChange={set('enemySpeed')} />
                    <Slider label={`shout ${settings.shoutRadius} m`} min={0} max={30} value={settings.shoutRadius} onChange={set('shoutRadius')} />
                    <Slider label={`leash ${settings.leashRadius} m`} min={6} max={40} value={settings.leashRadius} onChange={set('leashRadius')} />
                </Section>
                <Section title="SHOW" open={false}>
                    {(Object.keys(view) as (keyof View)[]).map((k) => (
                        <label key={k} style={{ fontSize: '0.75rem', display: 'flex', gap: 6, alignItems: 'center' }}>
                            <input type="checkbox" checked={view[k]} onChange={(e) => setView((v) => ({ ...v, [k]: e.target.checked }))} />
                            {k}
                        </label>
                    ))}
                </Section>
            </div>
        </div>
    );
}

function hudText(sim: Sim): string {
    const counts = new Map<string, number>();
    for (const e of sim.enemies) counts.set(e.state, (counts.get(e.state) ?? 0) + 1);
    const states = [...counts].map(([s, n]) => `${s} ${n}`).join(' · ');
    const { hitsTaken, hitsGiven, kills } = sim.stats;
    return `WASD move · mouse aim · click swing · Shift sneak · Space shout\n` +
        `hp ${sim.player.hp} · hits taken ${hitsTaken} · hits given ${hitsGiven} · kills ${kills} · tokens free ${Math.max(0, sim.settings.tokens - sim.tokens.holders.length - sim.tokens.resting.length)}\n` +
        `enemies: ${states}`;
}

