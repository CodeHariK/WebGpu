// Street Map — a hand-drawn-looking city map: boulevards across the map, districts of small streets
// lined up with them, green hills with winding roads, lakes with parks. Built up in steps (Prev /
// Next); the sliders change the map. Map24Render makes it (ground, roads, districts) and draws it.
import { useEffect, useRef, useState } from 'react';
import { DEFAULT_SETTINGS, STEPS, type Settings } from './Map24Settings';
import { drawMap, makeMap } from './Map24Render';

import { Section, Slider } from './Map19Ui';
import { btn } from './Map19UiStyle';

const REDRAW_DELAY = 120; // ms after the last slider move: dragging doesn't queue up redraws

export default function Map24({ width = 800, height = 800 }: { width?: number; height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const timeRef = useRef<HTMLSpanElement>(null);
    const [settings, setSettings] = useState<Settings>(DEFAULT_SETTINGS);

    useEffect(() => {
        const timer = setTimeout(() => {
            const ctx = canvasRef.current?.getContext('2d');
            if (!ctx) return;
            const start = performance.now();
            const map = makeMap(settings, width, height);
            const made = performance.now();
            drawMap(ctx, map);
            if (timeRef.current) timeRef.current.textContent = `made in ${(made - start).toFixed(0)} ms, drawn in ${(performance.now() - made).toFixed(0)} ms`;
        }, REDRAW_DELAY);
        return () => clearTimeout(timer);
    }, [settings, width, height]);

    const set = <K extends keyof Settings>(k: K) => (value: Settings[K]) => setSettings((s) => ({ ...s, [k]: value }));
    const goStep = (delta: number) => setSettings((s) => ({ ...s, step: Math.min(STEPS.length, Math.max(1, s.step + delta)) }));
    const num = (k: keyof Settings, label: string, min: number, max: number, stepSize: number, shown?: string) => (
        <Slider key={k} label={`${label} ${shown ?? settings[k]}`} min={min} max={max} step={stepSize} value={settings[k] as number} onChange={set(k) as (v: number) => void} />
    );
    const current = STEPS[settings.step - 1];

    return (
        <div style={{ width: '100%', color: '#e2e8f0', fontFamily: 'Inter, sans-serif', background: '#050407' }}>
            <canvas ref={canvasRef} width={width} height={height} style={{ display: 'block', width: '100%', maxWidth: width, aspectRatio: `${width} / ${height}` }} />
            <div style={{ padding: '10px 14px', display: 'flex', flexDirection: 'column', gap: 8 }}>
                <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap', alignItems: 'center' }}>
                    <button style={btn(false)} onClick={() => goStep(-1)}>Prev</button>
                    <button style={btn(true)} onClick={() => goStep(1)}>Next</button>
                    <strong style={{ fontSize: '0.85rem' }}>Step {settings.step} / {STEPS.length}: {current.title}</strong>
                    <span ref={timeRef} style={{ fontSize: '0.7rem', color: '#78716c', marginLeft: 'auto' }} />
                </div>
                <p style={{ margin: 0, fontSize: '0.72rem', color: '#a8a29e', lineHeight: 1.5 }}>{current.text}</p>
                <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap' }}>
                    <button style={btn(false)} onClick={() => set('seed')(Math.floor(Math.random() * 100000))}>New map</button>
                    <button style={btn(false)} onClick={() => setSettings(DEFAULT_SETTINGS)}>Default settings</button>
                    <label style={{ fontSize: '0.75rem', display: 'flex', gap: 6, alignItems: 'center' }}>
                        <input type="checkbox" checked={settings.freeway} onChange={(e) => set('freeway')(e.target.checked)} /> freeway
                    </label>
                </div>
                <Section title="ROADS">
                    {num('boulevards', 'boulevards', 2, 12, 1)}
                    {num('block', 'block size', 12, 40, 1, `${settings.block} px`)}
                    {num('gaps', 'street gaps', 0, 0.5, 0.01, `${Math.round(settings.gaps * 100)}%`)}
                </Section>
                <Section title="GROUND">
                    {num('hills', 'hills', 0, 0.5, 0.01, `${Math.round(settings.hills * 100)}%`)}
                    {num('lakes', 'lakes', 0, 4, 1)}
                    {num('lakeSize', 'lake size', 15, 70, 1, `${settings.lakeSize} px`)}
                </Section>
            </div>
        </div>
    );
}
