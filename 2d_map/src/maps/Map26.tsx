// Colour Blocks — a square grid, a few colours spreading from seeds until they fill it, the colours
// as area blocks, and their borders as highways. Built up in steps (Prev / Next, or ← →). Step 2
// plays the spread; it can be paused and stepped a tick at a time (◀ ▶, or ← → while on step 2).
// Map26Spread grows the colours, Map26Borders traces the highways, Map26Draw draws.
import { useEffect, useRef, useState } from 'react';
import { DEFAULT_SETTINGS, STEPS, type Settings } from './Map26Settings';
import { drawBlocks, makeBlocks, type Blocks, type Show } from './Map26Render';

import { Section, Slider } from './Map19Ui';
import { btn } from './Map19UiStyle';

const TICKS = 60; // the spread's clock: from 0 (only the seeds) to TICKS (full)
const TICK_MS = 66; // how long a tick lasts while playing

export default function Map26({ width = 800 }: { width?: number; height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const timeRef = useRef<HTMLSpanElement>(null);
    const blocksRef = useRef<Blocks | null>(null);
    const [settings, setSettings] = useState<Settings>(DEFAULT_SETTINGS);
    const [tick, setTick] = useState(0);
    const [playing, setPlaying] = useState(true);
    const [show, setShow] = useState<Show>({ queue: true, ground: true });
    const queueRef = useRef<HTMLSpanElement>(null);

    // a new map whenever the settings change
    useEffect(() => {
        const start = performance.now();
        blocksRef.current = makeBlocks(settings);
        if (timeRef.current) timeRef.current.textContent = `made in ${(performance.now() - start).toFixed(0)} ms`;
    }, [settings]);

    // draw: step 2 at the clock's tick, the others whole
    useEffect(() => {
        const ctx = canvasRef.current?.getContext('2d');
        if (!ctx || !blocksRef.current) return;
        const b = blocksRef.current;
        const queue = drawBlocks(ctx, b, tick / TICKS, show);
        if (!queueRef.current) return;
        const clock = settings.step === 1 ? 0 : (b.spread.lastTime * tick) / TICKS;
        queueRef.current.textContent = queue
            ? `clock ${clock.toFixed(1)} · ${queue.waiting.length} cells waiting · next burns at ${queue.next ? queue.next.at.toFixed(2) : '—'} (red ring)`
            : '';
    }, [settings, tick, show]);

    // playing: one tick every TICK_MS, stopping at the end
    useEffect(() => {
        if (!playing || settings.step !== 2) return;
        const timer = setInterval(() => {
            setTick((t) => {
                if (t >= TICKS) {
                    setPlaying(false);
                    return t;
                }
                return t + 1;
            });
        }, TICK_MS);
        return () => clearInterval(timer);
    }, [playing, settings.step]);

    const goStep = (delta: number) => {
        setSettings((s) => ({ ...s, step: Math.min(STEPS.length, Math.max(1, s.step + delta)) }));
        setTick(0);
        setPlaying(true);
    };
    const goTick = (delta: number) => {
        setPlaying(false);
        setTick((t) => Math.min(TICKS, Math.max(0, t + delta)));
    };
    const play = () => {
        if (tick >= TICKS) setTick(0);
        setPlaying((p) => !p || tick >= TICKS);
    };

    // ← →: a tick while on step 2, a step otherwise (not while a slider has focus)
    const step = settings.step;
    useEffect(() => {
        const onKey = (ev: KeyboardEvent) => {
            if ((ev.target as HTMLElement)?.tagName === 'INPUT') return;
            if (ev.key !== 'ArrowLeft' && ev.key !== 'ArrowRight') return;
            ev.preventDefault();
            const delta = ev.key === 'ArrowRight' ? 1 : -1;
            if (step === 2) {
                setPlaying(false);
                setTick((t) => Math.min(TICKS, Math.max(0, t + delta)));
            } else {
                setSettings((s) => ({ ...s, step: Math.min(STEPS.length, Math.max(1, s.step + delta)) }));
                setTick(0);
                setPlaying(true);
            }
        };
        window.addEventListener('keydown', onKey);
        return () => window.removeEventListener('keydown', onKey);
    }, [step]);

    const set = <K extends keyof Settings>(k: K) => (value: Settings[K]) => setSettings((s) => ({ ...s, [k]: value }));
    const num = (k: keyof Settings, label: string, min: number, max: number, stepSize: number) => (
        <Slider key={k} label={`${label} ${settings[k]}`} min={min} max={max} step={stepSize} value={settings[k] as number} onChange={set(k) as (v: number) => void} />
    );
    const current = STEPS[settings.step - 1];

    return (
        <div style={{ width: '100%', color: '#e2e8f0', fontFamily: 'Inter, sans-serif', background: '#050407' }}>
            <canvas ref={canvasRef} width={width} height={width} style={{ display: 'block', width: '100%', maxWidth: width, aspectRatio: '1' }} />
            <div style={{ padding: '10px 14px', display: 'flex', flexDirection: 'column', gap: 8 }}>
                <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap', alignItems: 'center' }}>
                    <button style={btn(false)} onClick={() => goStep(-1)}>Prev</button>
                    <button style={btn(true)} onClick={() => goStep(1)}>Next</button>
                    <strong style={{ fontSize: '0.85rem' }}>Step {settings.step} / {STEPS.length}: {current.title}</strong>
                    <span ref={timeRef} style={{ fontSize: '0.7rem', color: '#78716c', marginLeft: 'auto' }} />
                </div>
                {settings.step <= 2 && (
                    <div style={{ display: 'flex', gap: 12, flexWrap: 'wrap', alignItems: 'center', fontSize: '0.75rem' }}>
                        <label style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
                            <input type="checkbox" checked={show.queue} onChange={(e) => setShow((v) => ({ ...v, queue: e.target.checked }))} /> show the queue (outlined: waiting, in the colour it will take; red ring: next to burn)
                        </label>
                        <label style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
                            <input type="checkbox" checked={show.ground} onChange={(e) => setShow((v) => ({ ...v, ground: e.target.checked }))} /> show the ground's cost (light: quick, brown: slow)
                        </label>
                        <span ref={queueRef} style={{ color: '#a8a29e' }} />
                    </div>
                )}
                {settings.step === 2 && (
                    <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap', alignItems: 'center' }}>
                        <button style={btn(false)} onClick={() => goTick(-1)}>◀ tick</button>
                        <button style={btn(true)} onClick={play}>{playing && tick < TICKS ? 'Pause' : tick >= TICKS ? 'Play again' : 'Play'}</button>
                        <button style={btn(false)} onClick={() => goTick(1)}>tick ▶</button>
                        <span style={{ fontSize: '0.75rem' }}>tick {tick} / {TICKS}</span>
                        <input type="range" min={0} max={TICKS} value={tick} onChange={(e) => goTick(Number(e.target.value) - tick)} style={{ flex: 1, accentColor: '#22b07d' }} />
                    </div>
                )}
                <p style={{ margin: 0, fontSize: '0.72rem', color: '#a8a29e', lineHeight: 1.5 }}>{current.text}</p>
                <p style={{ margin: 0, fontSize: '0.68rem', color: '#78716c' }}>← →: {settings.step === 2 ? 'a tick back / forward' : 'previous / next step'}</p>
                <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap' }}>
                    <button style={btn(false)} onClick={() => set('seed')(Math.floor(Math.random() * 100000))}>New map</button>
                    <button style={btn(false)} onClick={() => setSettings(DEFAULT_SETTINGS)}>Default settings</button>
                </div>
                <Section title="SPREAD">
                    {num('cells', 'cells', 30, 200, 1)}
                    {num('seeds', 'colours', 2, 20, 1)}
                    {num('seedsPerColour', 'seeds per colour, up to', 1, 3, 1)}
                    {num('chainStep', 'chain step', 8, 50, 1)}
                    {num('roughness', 'roughness', 0, 8, 0.1)}
                    {num('noiseSize', 'noise size', 3, 40, 1)}
                    {num('speedVariety', 'speed variety', 0, 1, 0.01)}
                </Section>
                <Section title="DISTRICTS">
                    {num('districtSize', 'district size', 6, 40, 1)}
                </Section>
                <Section title="ROADS">
                    {num('straighten', 'straighten', 0, 5, 0.1)}
                    {num('smoothing', 'rounding passes', 0, 5, 1)}
                </Section>
            </div>
        </div>
    );
}
