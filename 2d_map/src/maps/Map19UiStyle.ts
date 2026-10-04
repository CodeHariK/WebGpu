// Puzzle Islands — shared UI colours and button style.

export const UI = { accent: '#22b07d', text: '#94a3b8' };

export const btn = (accent: boolean) => ({
    background: accent ? UI.accent : 'rgba(255,255,255,0.08)',
    color: accent ? 'white' : '#e2e8f0',
    border: accent ? 'none' : '1px solid rgba(255,255,255,0.14)',
    padding: '10px 16px',
    borderRadius: 8,
    cursor: 'pointer',
    fontWeight: 700,
    fontSize: '0.78rem',
});
