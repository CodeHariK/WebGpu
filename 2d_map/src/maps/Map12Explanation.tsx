import type { CSSProperties } from 'react';

export default function Map12Explanation() {
    return (
        <div style={EXPLANATION_STYLES.container}>
            <h2 style={EXPLANATION_STYLES.title}>Marching Squares</h2>
            <p style={EXPLANATION_STYLES.paragraph}>
                In this segment, we will explore the **Marching Squares** algorithm, a computer graphics algorithm
                used for generating isolines from a 2D scalar field.
            </p>
            <div style={EXPLANATION_STYLES.tip}>
                <strong>Coming Soon:</strong> Implementation based on the Prideout guide.

                <a href="https://prideout.net/marching-squares">https://prideout.net/marching-squares</a>
            </div>
        </div>
    );
}

const EXPLANATION_STYLES = {
    container: { fontFamily: '"Outfit", sans-serif', color: '#94a3b8', lineHeight: '1.6' } as CSSProperties,
    title: { color: '#f8fafc', fontSize: '24px', marginBottom: '16px', fontWeight: '700' } as CSSProperties,
    paragraph: { fontSize: '15px', marginBottom: '12px' } as CSSProperties,
    tip: { marginTop: '32px', padding: '16px', background: 'rgba(59, 130, 246, 0.1)', borderRadius: '12px', border: '1px solid rgba(59, 130, 246, 0.2)', fontSize: '14px', color: '#bfdbfe' } as CSSProperties
};
