// Colour Blocks — the 3D preview as a React element: the view (Map26View3D) made once for its size,
// and shown the map again whenever it changes.
import { useEffect, useRef } from 'react';
import type { Blocks } from './Map26Render';
import { createView, type View3D } from './Map26View3D';

export default function Map26Three({ blocks, size }: { blocks: Blocks; size: number }) {
    const mountRef = useRef<HTMLDivElement>(null);
    const viewRef = useRef<View3D | null>(null);
    useEffect(() => {
        const view = createView(mountRef.current!, size);
        viewRef.current = view;
        return () => {
            view.dispose();
            viewRef.current = null;
        };
    }, [size]);
    useEffect(() => {
        viewRef.current?.show(blocks);
    }, [blocks, size]);
    return <div ref={mountRef} style={{ width: size, maxWidth: '100%', aspectRatio: '1', overflow: 'hidden' }} />;
}
