import { useEffect, useRef, useState } from 'react';
import type { CSSProperties } from 'react';
import { Canvas } from '../lib/Canvas';
import { Vector2 } from '../lib/Vector2';

export default function Map12({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const canvasInstanceRef = useRef<Canvas | null>(null);
    const [renderTrigger, setRenderTrigger] = useState(0);

    // Init Canvas
    useEffect(() => {
        if (!canvasRef.current) return;
        const instance = new Canvas(canvasRef.current);
        instance.initInteractions({ onInteraction: () => setRenderTrigger(t => t + 1) });
        canvasInstanceRef.current = instance;
    }, [width, height]);

    // Draw Loop
    useEffect(() => {
        if (!canvasInstanceRef.current) return;
        const canvas = canvasInstanceRef.current;
        canvas.clear();
        canvas.rect(-1000, -1000, width + 2000, height + 2000, { fill: '#111111' });
        canvas.grid(20, 'rgba(255, 255, 255, 0.05)');

        // One line as requested
        canvas.text('Marching Squares implementation coming soon...', new Vector2(40, 40), { fill: '#60a5fa', font: '14px "Outfit", sans-serif' });
    }, [width, height, renderTrigger]);

    return (
        <div style={UI_STYLES.container}>
            <div style={UI_STYLES.panel}>
                <div style={UI_STYLES.group}>
                    <label style={UI_STYLES.label}>STATUS</label>
                    <p style={{ color: '#94a3b8', fontSize: '13px', margin: 0 }}>Development Phase: Initial Placeholder</p>
                </div>
            </div>
            <canvas ref={canvasRef} width={width} height={height} style={UI_STYLES.canvas} />
        </div>
    );
}

const UI_STYLES = {
    container: { position: 'relative', width: '100%', height: '100%', background: '#0a0b12', overflow: 'hidden' } as CSSProperties,
    panel: { position: 'absolute', top: 20, left: 20, background: 'rgba(6, 11, 19, 0.95)', backdropFilter: 'blur(16px)', padding: '24px', borderRadius: '16px', color: 'white', fontFamily: '"Outfit", sans-serif', display: 'flex', flexDirection: 'column', gap: '20px', zIndex: 10, border: '1px solid rgba(59, 130, 246, 0.2)', width: '280px', boxShadow: '0 12px 40px rgba(0, 0, 0, 0.5)' } as CSSProperties,
    group: { display: 'flex', flexDirection: 'column', gap: '6px' } as CSSProperties,
    label: { fontSize: '11px', color: '#60a5fa', fontWeight: '700', letterSpacing: '1.5px', textTransform: 'uppercase' } as CSSProperties,
    canvas: { display: 'block', width: '100%', height: '100%', cursor: 'crosshair' } as CSSProperties
};
