// Puzzle Islands — small shared UI pieces (buttons, sliders, collapsible sections).
import type { ReactNode } from 'react';
import { UI } from './Map19UiStyle';

export const Slider = ({ label, min, max, step = 1, value, onChange }: { label: string; min: number; max: number; step?: number; value: number; onChange: (v: number) => void }) => (
    <div style={{ padding: '8px 14px', background: 'rgba(255,255,255,0.05)', borderRadius: 8, display: 'flex', alignItems: 'center', gap: 10, color: UI.text, border: '1px solid rgba(255,255,255,0.1)' }}>
        <span style={{ fontSize: '0.72rem', fontWeight: 600, whiteSpace: 'nowrap' }}>{label}</span>
        <input type="range" min={min} max={max} step={step} value={value} onChange={(e) => onChange(Number(e.target.value))} style={{ cursor: 'pointer', accentColor: UI.accent }} />
    </div>
);

/** A collapsible group of controls. */
export const Section = ({ title, open = true, children }: { title: string; open?: boolean; children: ReactNode }) => (
    <details open={open} style={{ width: '100%', borderTop: '1px solid rgba(255,255,255,0.06)', padding: '6px 0' }}>
        <summary style={{ cursor: 'pointer', color: '#e2e8f0', fontSize: '0.72rem', fontWeight: 700, letterSpacing: 1, padding: '4px 0 8px' }}>{title}</summary>
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: 10, alignItems: 'center' }}>{children}</div>
    </details>
);
