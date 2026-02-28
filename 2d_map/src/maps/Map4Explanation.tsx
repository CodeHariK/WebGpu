export default function Map4Explanation() {
    return (
        <div style={{ color: '#fff', fontSize: '15px', lineHeight: 1.6 }}>
            <h2 style={{ marginTop: 0, color: '#e0e0e0' }}>Seamless City Tile</h2>
            <p>
                This map utilizes an entirely different technique focused on creating a perfectly looping, infinite texture. It leverages offscreen rendering to pre-compose a single 800x800 blueprint tile.
            </p>

            <ul style={{ paddingLeft: '20px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                <li>
                    <strong>1. 3x3 Ghost Buffering:</strong>
                    When you draw lines in Canvas that extend past the edge, they don't natively loop. To solve this and allow thick strokes (like `strokeWeight(60)`) to cleanly wrap around, we draw every single road and asset 9 times (`dx`, `dy` from `-TILE_SIZE` to `+TILE_SIZE`). This creates a flawless wrap on the tile.
                </li>
                <li>
                    <strong>2. Layer Stacking:</strong>
                    Vector data cannot just be drawn indiscriminately because objects overlap depending on their Z-index. The script manages layer queues: thick dark roads first, entirely overwritten by thinner grey roads, topped with dashed centerlines, and finally the civic assets (houses, hospitals) are placed on top.
                </li>
                <li>
                    <strong>3. Zoom Scaling:</strong>
                    Once the perfect 800x800 offscreen tile is generated, the main canvas shrinks its viewport scale to `35%`.
                </li>
                <li>
                    <strong>4. Infinite Grid Render:</strong>
                    By dividing the canvas width by the `0.35` zoom level, you calculate algebraically how many 800px tiles are needed to fill the screen bounds. `ctx.drawImage()` repeats the blueprint endlessly across the view.
                </li>
            </ul>
        </div>
    );
}
