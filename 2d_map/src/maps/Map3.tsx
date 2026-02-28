import { useEffect, useRef } from 'react';

// Random helper
function random(min: any, max?: number) {
    if (Array.isArray(min)) return min[Math.floor(Math.random() * min.length)];
    if (max === undefined) return Math.random() * min;
    return min + Math.random() * (max - min);
}

export default function Map3({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);

    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        // Configuration
        const baseCols = 15; // Increased to fill out the 800x800 canvas better
        const baseRows = 15;
        // Tile size calculation to fit canvas (tileCols = baseCols*2 + 1)
        const tileCols = baseCols * 2 + 1;
        const tileRows = baseRows * 2 + 1;

        // Calculate appropriate tile size so that the maze fits within the width/height
        const tileSize = Math.min(Math.floor(width / tileCols), Math.floor(height / tileRows));

        // Center the maze if it doesn't perfectly fit the canvas
        const offsetX = (width - (tileCols * tileSize)) / 2;
        const offsetY = (height - (tileRows * tileSize)) / 2;

        let logicGrid: any[] = [];
        let tileGrid: any[][] = [];

        // Clear and background (Cute sand/dirt path color: 240, 220, 180)
        ctx.fillStyle = 'rgb(240, 220, 180)';
        ctx.fillRect(0, 0, width, height);

        class BaseCell {
            i: number;
            j: number;
            walls: boolean[];
            visited: boolean;

            constructor(i: number, j: number) {
                this.i = i;
                this.j = j;
                this.walls = [true, true, true, true]; // Top, Right, Bottom, Left
                this.visited = false;
            }

            checkNeighbors() {
                let neighbors = [];
                let top = getBaseCell(this.i, this.j - 1);
                let right = getBaseCell(this.i + 1, this.j);
                let bottom = getBaseCell(this.i, this.j + 1);
                let left = getBaseCell(this.i - 1, this.j);

                if (top && !top.visited) neighbors.push(top);
                if (right && !right.visited) neighbors.push(right);
                if (bottom && !bottom.visited) neighbors.push(bottom);
                if (left && !left.visited) neighbors.push(left);

                if (neighbors.length > 0) return random(neighbors);
                return undefined;
            }
        }

        function getBaseCell(i: number, j: number) {
            if (i < 0 || j < 0 || i > baseCols - 1 || j > baseRows - 1) return undefined;
            return logicGrid[i + j * baseCols];
        }

        function removeWalls(a: BaseCell, b: BaseCell) {
            let x = a.i - b.i;
            if (x === 1) { a.walls[3] = false; b.walls[1] = false; }
            if (x === -1) { a.walls[1] = false; b.walls[3] = false; }
            let y = a.j - b.j;
            if (y === 1) { a.walls[0] = false; b.walls[2] = false; }
            if (y === -1) { a.walls[2] = false; b.walls[0] = false; }
        }

        function generateInvisibleMaze() {
            // Initialize base logic grid
            for (let j = 0; j < baseRows; j++) {
                for (let i = 0; i < baseCols; i++) {
                    logicGrid.push(new BaseCell(i, j));
                }
            }

            let current = logicGrid[0];
            let stack: BaseCell[] = [];
            current.visited = true;

            // Run the Recursive Backtracker
            let unvisitedCount = logicGrid.length - 1;
            while (unvisitedCount > 0) {
                let next = current.checkNeighbors();
                if (next) {
                    next.visited = true;
                    stack.push(current);
                    removeWalls(current, next);
                    current = next;
                    unvisitedCount--;
                } else if (stack.length > 0) {
                    current = stack.pop();
                }
            }
        }

        function expandGrid() {
            // Initialize tile grid completely filled with walls (1)
            for (let x = 0; x < tileCols; x++) {
                tileGrid[x] = [];
                for (let y = 0; y < tileRows; y++) {
                    tileGrid[x][y] = 1; // 1 = Wall, 0 = Path
                }
            }

            // Carve out the paths based on the logic grid blueprint
            for (let cell of logicGrid) {
                let tx = cell.i * 2 + 1;
                let ty = cell.j * 2 + 1;

                tileGrid[tx][ty] = 0; // Center is path

                // Carve connecting corridors
                if (!cell.walls[0]) tileGrid[tx][ty - 1] = 0; // Top
                if (!cell.walls[1]) tileGrid[tx + 1][ty] = 0; // Right
                if (!cell.walls[2]) tileGrid[tx][ty + 1] = 0; // Bottom
                if (!cell.walls[3]) tileGrid[tx - 1][ty] = 0; // Left
            }
        }

        function drawCartoonMaze() {
            if (!ctx) return;

            // Setup drop shadow exactly as requested
            ctx.shadowOffsetX = 3;
            ctx.shadowOffsetY = 5;
            ctx.shadowBlur = 4;
            ctx.shadowColor = 'rgba(0, 0, 0, 0.2)';

            // Helper to draw a circle
            const circle = (x: number, y: number, d: number) => {
                ctx.beginPath();
                ctx.arc(x, y, d / 2, 0, Math.PI * 2);
                ctx.fill();
            };

            for (let x = 0; x < tileCols; x++) {
                for (let y = 0; y < tileRows; y++) {
                    if (tileGrid[x][y] === 1) { // If it's a WALL
                        let px = offsetX + x * tileSize;
                        let py = offsetY + y * tileSize;

                        // Bitmask calculation
                        let n = (y > 0 && tileGrid[x][y - 1] === 1) ? 1 : 0;
                        let e = (x < tileCols - 1 && tileGrid[x + 1][y] === 1) ? 2 : 0;
                        let s = (y < tileRows - 1 && tileGrid[x][y + 1] === 1) ? 4 : 0;
                        let w = (x > 0 && tileGrid[x - 1][y] === 1) ? 8 : 0;

                        let mask = n + e + s + w;

                        ctx.fillStyle = 'rgb(90, 190, 110)'; // Hedge green

                        // Base circle for rounded corners at every wall tile center
                        circle(px + tileSize / 2, py + tileSize / 2, tileSize);

                        // Draw connecting rects based on bitmask
                        if (mask & 1) ctx.fillRect(px, py, tileSize, tileSize / 2);               // North
                        if (mask & 2) ctx.fillRect(px + tileSize / 2, py, tileSize / 2, tileSize); // East
                        if (mask & 4) ctx.fillRect(px, py + tileSize / 2, tileSize, tileSize / 2); // South
                        if (mask & 8) ctx.fillRect(px, py, tileSize / 2, tileSize);               // West

                        // Spawning Decorations -> tiny leaves using tiny circles
                        if (Math.random() < 0.2) {
                            ctx.fillStyle = 'rgb(60, 140, 80)';
                            let ltx = px + tileSize / 2 + random(-5, 5) * (tileSize / 25);
                            let lty = py + tileSize / 2 + random(-5, 5) * (tileSize / 25);
                            circle(ltx, lty, tileSize * 0.3);
                        }
                    }
                }
            }

            // Turn off shadow for the character/goal
            ctx.shadowColor = 'transparent';

            // Fixed character spawning point calculations (Start Top-Left, End Bottom-Right)
            let startX = offsetX + 1 * tileSize + tileSize / 2;
            let startY = offsetY + 1 * tileSize + tileSize / 2;
            let endX = offsetX + (tileCols - 2) * tileSize + tileSize / 2;
            let endY = offsetY + (tileRows - 2) * tileSize + tileSize / 2;

            // Draw Blue Hero
            ctx.fillStyle = 'rgb(80, 150, 255)';
            ctx.strokeStyle = 'rgb(40, 80, 150)';
            ctx.lineWidth = 3;

            ctx.beginPath();
            ctx.arc(startX, startY, (tileSize * 0.7) / 2, 0, Math.PI * 2);
            ctx.fill();
            ctx.stroke();

            // Draw Yellow Goal Star/Target
            ctx.fillStyle = 'rgb(255, 220, 50)';
            ctx.strokeStyle = 'rgb(200, 150, 0)';
            // Simulate the rect drawn from CENTER by shifting and adjusting size
            let goalDim = tileSize * 0.7;
            ctx.beginPath();
            ctx.rect(endX - goalDim / 2, endY - goalDim / 2, goalDim, goalDim);
            ctx.fill();
            ctx.stroke();

            // Inner white circle
            ctx.fillStyle = '#fff';
            ctx.beginPath();
            ctx.arc(endX, endY, (tileSize * 0.3) / 2, 0, Math.PI * 2);
            ctx.fill();
        }

        // Execute generation sequence
        generateInvisibleMaze();
        expandGrid();
        drawCartoonMaze();

    }, [width, height]);

    return <canvas ref={canvasRef} width={width} height={height} style={{ borderRadius: 8, display: 'block', margin: '0 auto', boxShadow: '0 4px 6px rgba(0,0,0,0.3)' }} />;
}
