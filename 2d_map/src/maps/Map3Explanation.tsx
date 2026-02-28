export default function Map3Explanation() {
    return (
        <div style={{ color: '#fff', fontSize: '15px', lineHeight: 1.6 }}>
            <h2 style={{ marginTop: 0, color: '#e0e0e0' }}>Autotiled Hedge Maze</h2>
            <p>
                Unlike the freeform continuous collision shapes in previous maps, this algorithm uses distinct grid-based architectural logic mapped cleanly onto HTML5 Canvas.
            </p>

            <ul style={{ paddingLeft: '20px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                <li>
                    <strong>1. Recursive Backtracker Logic Grid:</strong>
                    First, we generate an invisible "Blueprint" logic maze (for example `15x15`) using a classic Recursive Backtracker algorithm, digging through arrays and knocking down walls.
                </li>
                <li>
                    <strong>2. Space Expansion (Zooming Array Space):</strong>
                    If you just rendered a 15x15 backtracker block-by-block, walls take up 0 pixel width. To fix this, we create a new grid that is `31x31` `((15 * 2) + 1)`, effectively making the maze paths 1 whole grid-square wide and the physical walls 1 grid-square thick.
                </li>
                <li>
                    <strong>3. Autotiling (Bitmasking):</strong>
                    Instead of just drawing square blocks, we check the Top, Right, Bottom, Left neighbors of every wall tile to build a bitmask between `0` and `15`. Canvas `arc()` and `rect()` calls combine to draw beautiful, seamless bubbling hedges depending on whether a wall continues into an adjacent tile.
                </li>
            </ul>
        </div>
    );
}
