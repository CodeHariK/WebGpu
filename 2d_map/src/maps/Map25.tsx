// Islands — 4 islands joined by bridges. Each island is a noise map with a falloff round its middle
// (land above sea level), plus a headland out to each of its bridges. The bridges are chosen first,
// so each island reaches out toward each of its bridges.
import { useEffect, useRef, useState } from 'react';
import { DEFAULT_SETTINGS, type Settings } from './Map25Settings';
import { drawIslands, makeIslands } from './Map25Render';

import { Section, Slider } from './Map19Ui';
import { btn } from './Map19UiStyle';

const REDRAW_DELAY = 60; // ms after the last slider move

export default function Map25({ width = 800, height = 800 }: { width?: number; height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const timeRef = useRef<HTMLSpanElement>(null);
    const [settings, setSettings] = useState<Settings>(DEFAULT_SETTINGS);

    useEffect(() => {
        const timer = setTimeout(() => {
            const ctx = canvasRef.current?.getContext('2d');
            if (!ctx) return;
            const start = performance.now();
            const islands = makeIslands(settings, width, height);
            const made = performance.now();
            drawIslands(ctx, islands);
            if (timeRef.current) timeRef.current.textContent = `made in ${(made - start).toFixed(0)} ms, drawn in ${(performance.now() - made).toFixed(0)} ms`;
        }, REDRAW_DELAY);
        return () => clearTimeout(timer);
    }, [settings, width, height]);

    const set = <K extends keyof Settings>(k: K) => (value: Settings[K]) => setSettings((s) => ({ ...s, [k]: value }));
    const num = (k: keyof Settings, label: string, min: number, max: number, stepSize: number, unit = '') => (
        <Slider key={k} label={`${label} ${settings[k]}${unit}`} min={min} max={max} step={stepSize} value={settings[k] as number} onChange={set(k) as (v: number) => void} />
    );

    return (
        <div style={{ width: '100%', color: '#e2e8f0', fontFamily: 'Inter, sans-serif', background: '#050407' }}>
            <canvas ref={canvasRef} width={width} height={height} style={{ display: 'block', width: '100%', maxWidth: width, aspectRatio: `${width} / ${height}` }} />
            <div style={{ padding: '10px 14px', display: 'flex', flexDirection: 'column', gap: 8 }}>
                <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap', alignItems: 'center' }}>
                    <button style={btn(true)} onClick={() => set('seed')(Math.floor(Math.random() * 100000))}>New islands</button>
                    <button style={btn(false)} onClick={() => setSettings(DEFAULT_SETTINGS)}>Default settings</button>
                    <label style={{ fontSize: '0.75rem', display: 'flex', gap: 6, alignItems: 'center' }}>
                        <input type="checkbox" checked={settings.showParts} onChange={(e) => set('showParts')(e.target.checked)} /> show how it's built
                    </label>
                    <span ref={timeRef} style={{ fontSize: '0.7rem', color: '#78716c', marginLeft: 'auto' }} />
                </div>
                <Section title="ISLANDS">
                    {num('islandSize', 'size', 70, 170, 1, ' px')}
                    {num('channel', 'channel', 16, 90, 1, ' px')}
                </Section>
                <Section title="SHAPE">
                    {num('noiseScale', 'noise size', 30, 300, 5, ' px')}
                    {num('noiseStrength', 'noise strength', 0, 1, 0.01)}
                    {num('blend', 'headland blend', 2, 50, 1, ' px')}
                </Section>
            </div>
        </div>
    );
}
