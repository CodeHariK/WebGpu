import { useEffect, useRef } from 'react';

export default function Map4({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);

    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        const TILE_SIZE = 800;
        let zoom = 0.35; // 35% of original size

        // Create the hidden buffer for our seamless tile (offscreen canvas)
        const tileCanvas = document.createElement('canvas');
        tileCanvas.width = TILE_SIZE;
        tileCanvas.height = TILE_SIZE;
        const tileCtx = tileCanvas.getContext('2d');

        if (!tileCtx) return;

        // --- DRAWING THE TILE ---
        function generateCityTile(tCtx: CanvasRenderingContext2D) {
            tCtx.fillStyle = '#ffffff';
            tCtx.fillRect(0, 0, TILE_SIZE, TILE_SIZE);

            // To ensure thick road strokes wrap perfectly around the edges of our tile,
            // we draw the entire map 9 times in a 3x3 grid around the center.
            let offsets = [-TILE_SIZE, 0, TILE_SIZE];

            // Helper to handle transformations
            const drawWithOffset = (dx: number, dy: number, drawFunc: () => void) => {
                tCtx.save();
                tCtx.translate(dx, dy);
                drawFunc();
                tCtx.restore();
            };

            // LAYER 1: Road Base (Dark Grey)
            for (let dx of offsets) for (let dy of offsets) {
                drawWithOffset(dx, dy, () => drawRoadPaths(tCtx, 60, '#acb4b5'));
            }

            // LAYER 2: Road Inner (Light Grey)
            for (let dx of offsets) for (let dy of offsets) {
                drawWithOffset(dx, dy, () => drawRoadPaths(tCtx, 44, '#cfd5d6'));
            }

            // LAYER 3: Dashed Centerlines
            for (let dx of offsets) for (let dy of offsets) {
                drawWithOffset(dx, dy, () => {
                    tCtx.setLineDash([15, 15]);
                    drawRoadPaths(tCtx, 4, '#ffffff');
                    tCtx.setLineDash([]);
                });
            }

            // LAYER 4: Roundabouts
            for (let dx of offsets) for (let dy of offsets) {
                drawWithOffset(dx, dy, () => {
                    drawRoundabout(tCtx, 250, 150);
                    drawRoundabout(tCtx, 550, 650);
                });
            }

            // LAYER 5: Buildings, Decor & Vehicles
            for (let dx of offsets) for (let dy of offsets) {
                drawWithOffset(dx, dy, () => {
                    drawCityElements(tCtx);
                });
            }
        }

        // --- ROAD PATH ALGORITHM ---
        function drawRoadPaths(tCtx: CanvasRenderingContext2D, weight: number, col: string) {
            tCtx.strokeStyle = col;
            tCtx.lineWidth = weight;
            tCtx.lineJoin = 'round';
            tCtx.lineCap = 'round';

            // Hardcoded blueprint for a seamlessly tiling road network
            let paths = [
                [[0, 150], [250, 150], [250, 300], [550, 300], [550, 150], [800, 150]], // Top wave
                [[0, 650], [250, 650], [250, 500], [550, 500], [550, 650], [800, 650]], // Bottom wave
                [[250, 300], [250, 500]], // Left vertical connection
                [[550, 300], [550, 500]], // Right vertical connection
                [[100, 0], [100, 150]],   // Top-left vertical
                [[700, 0], [700, 150]],   // Top-right vertical
                [[100, 650], [100, 800]], // Bottom-left vertical
                [[700, 650], [700, 800]]  // Bottom-right vertical
            ];

            for (let p of paths) {
                tCtx.beginPath();
                if (p.length > 0) {
                    tCtx.moveTo(p[0][0], p[0][1]);
                    for (let i = 1; i < p.length; i++) {
                        tCtx.lineTo(p[i][0], p[i][1]);
                    }
                }
                tCtx.stroke();
            }
        }

        // --- PROCEDURAL ASSET PLACEMENT ---
        function drawCityElements(tCtx: CanvasRenderingContext2D) {
            // 1. Civic Hubs
            drawHospital(tCtx, 400, 400);
            drawFireStation(tCtx, 400, 70);
            drawFireStation(tCtx, 400, 730);

            // 2. Small Residential Houses
            let houseColors = ['#f4938f', '#e5c56c', '#98b0ce', '#d4a193'];
            let houseCoords = [
                [170, 400], [80, 500], [680, 400], [740, 250], [720, 550],
                [330, 220], [470, 220], [170, 80], [680, 80], [80, 250]
            ];
            for (let i = 0; i < houseCoords.length; i++) {
                drawHouse(tCtx, houseCoords[i][0], houseCoords[i][1], houseColors[i % houseColors.length]);
            }

            // 3. Nature (Trees, Bushes, Stumps)
            let treeCoords = [[120, 110], [680, 110], [120, 710], [680, 710], [400, 260], [400, 540]];
            for (let t of treeCoords) drawPine(tCtx, t[0], t[1]);

            let bushCoords = [[160, 320], [640, 320], [160, 580], [640, 580], [330, 480], [470, 480]];
            for (let b of bushCoords) drawBush(tCtx, b[0], b[1]);

            // 4. Vehicles 
            drawCar(tCtx, 150, 150, 0, '#ef584e');         // Red car top-left
            drawCar(tCtx, 650, 150, Math.PI, '#a2acad');        // Grey car top-right
            drawCar(tCtx, 250, 220, Math.PI / 2, '#fcdb5d', true); // Yellow Bus left
            drawCar(tCtx, 650, 650, 0, '#ef584e');         // Red car bottom-right
            drawCar(tCtx, 550, 400, -Math.PI / 2, '#a2acad');  // Grey car right center
            drawCar(tCtx, 100, 650, Math.PI, '#fcdb5d', true);  // Yellow Bus bottom-left

            // 5. Traffic Signs
            drawStopSign(tCtx, 210, 120);
            drawStopSign(tCtx, 590, 620);
        }

        // --- VECTOR ART DRAWING FUNCTIONS ---
        function circle(tCtx: CanvasRenderingContext2D, x: number, y: number, r: number) {
            tCtx.beginPath();
            tCtx.arc(x, y, r, 0, Math.PI * 2);
            tCtx.fill();
        }

        function drawRoundabout(tCtx: CanvasRenderingContext2D, x: number, y: number) {
            tCtx.save();
            tCtx.translate(x, y);
            tCtx.fillStyle = '#acb4b5'; circle(tCtx, 0, 0, 70); // Radius is diam/2
            tCtx.fillStyle = '#cfd5d6'; circle(tCtx, 0, 0, 60);
            tCtx.fillStyle = '#ffffff'; circle(tCtx, 0, 0, 35);
            tCtx.fillStyle = '#0d7cbe'; circle(tCtx, 0, 0, 30); // Water
            tCtx.restore();
        }

        function drawHospital(tCtx: CanvasRenderingContext2D, x: number, y: number) {
            tCtx.save();
            tCtx.translate(x, y);
            tCtx.fillStyle = '#1b9db4'; tCtx.fillRect(-70, -45, 140, 90); // Main Building
            tCtx.fillStyle = '#14798c'; tCtx.fillRect(-75, -45, 150, 8); // Roof Rim
            // Windows
            tCtx.fillStyle = '#fcdb5d';
            for (let i = -40; i <= 40; i += 30) {
                for (let j = -20; j <= 20; j += 25) {
                    if (i === -10 && j === 20) continue; // Leave space for door
                    tCtx.fillRect(i - 7.5, j - 7.5, 15, 15); // Adjust for centering
                }
            }
            // Door & Red Cross
            tCtx.fillStyle = '#ffffff'; tCtx.fillRect(-10, 30 - 15, 20, 30); // Door (adjusted from rect(..., 30) where 30 is height)
            tCtx.fillRect(0 - 15, -65 - 15, 30, 30); // Note original p5 rect is CENTER, here it's corner
            tCtx.fillStyle = '#ef584e'; tCtx.fillRect(0 - 4, -65 - 10, 8, 20); tCtx.fillRect(0 - 10, -65 - 4, 20, 8);
            tCtx.restore();
        }

        function drawFireStation(tCtx: CanvasRenderingContext2D, x: number, y: number) {
            tCtx.save();
            tCtx.translate(x, y);
            tCtx.fillStyle = '#ef584e'; tCtx.fillRect(-80, -40, 160, 80);
            tCtx.fillStyle = '#d34035'; tCtx.fillRect(-85, -40, 170, 8);

            // Garage Doors (simulating rounded top via arc + rect)
            tCtx.fillStyle = '#d34035';

            // Left Garage (-40x, 10y center -> -65, -20 corner)
            tCtx.beginPath();
            tCtx.roundRect(-65, -20, 50, 60, [5, 5, 0, 0]);
            tCtx.fill();
            // Right Garage (40x, 10y center -> 15, -20 corner)
            tCtx.beginPath();
            tCtx.roundRect(15, -20, 50, 60, [5, 5, 0, 0]);
            tCtx.fill();

            // Door Detail Lines
            tCtx.strokeStyle = '#ef584e'; tCtx.lineWidth = 2;
            for (let i = 0; i < 60; i += 10) {
                tCtx.beginPath();
                tCtx.moveTo(-65, -15 + i); tCtx.lineTo(-15, -15 + i);
                tCtx.stroke();

                tCtx.beginPath();
                tCtx.moveTo(15, -15 + i); tCtx.lineTo(65, -15 + i);
                tCtx.stroke();
            }

            // Sign
            tCtx.fillStyle = '#d34035'; tCtx.fillRect(-45, -55 - 7.5, 90, 15);
            tCtx.fillStyle = '#ffffff';
            tCtx.font = "10px sans-serif";
            tCtx.textAlign = "center";
            tCtx.textBaseline = "middle";
            tCtx.fillText("FIRE STATION", 0, -55);
            tCtx.restore();
        }

        function drawHouse(tCtx: CanvasRenderingContext2D, x: number, y: number, col: string) {
            tCtx.save();
            tCtx.translate(x, y);
            tCtx.fillStyle = col; tCtx.fillRect(-20, 10 - 20, 40, 40);
            tCtx.fillStyle = '#807c7a';
            tCtx.beginPath(); tCtx.moveTo(-25, -10); tCtx.lineTo(25, -10); tCtx.lineTo(0, -35); tCtx.fill(); // Roof
            tCtx.fillStyle = '#ffffff'; tCtx.fillRect(-8 - 5, 5 - 5, 10, 10); tCtx.fillRect(8 - 5, 5 - 5, 10, 10); // Windows
            tCtx.fillStyle = '#fcdb5d'; tCtx.fillRect(0 - 6, 22 - 8, 12, 16); // Door
            tCtx.restore();
        }

        function drawCar(tCtx: CanvasRenderingContext2D, x: number, y: number, angle: number, col: string, isBus = false) {
            tCtx.save();
            tCtx.translate(x, y);
            tCtx.rotate(angle);
            if (isBus) {
                tCtx.fillStyle = col;
                tCtx.beginPath(); tCtx.roundRect(-30, -12, 60, 24, 5); tCtx.fill();
                tCtx.fillStyle = '#ffffff';
                tCtx.fillRect(-15 - 4, 0 - 9, 8, 18); tCtx.fillRect(0 - 4, 0 - 9, 8, 18); tCtx.fillRect(15 - 4, 0 - 9, 8, 18);
            } else {
                tCtx.fillStyle = col;
                tCtx.beginPath(); tCtx.roundRect(-17, -10, 34, 20, 5); tCtx.fill();
                tCtx.fillStyle = '#ffffff';
                tCtx.beginPath(); tCtx.roundRect(4 - 6, 0 - 8, 12, 16, 2); tCtx.fill();
            }
            // Wheels
            tCtx.fillStyle = '#4a4a4a'; let w = isBus ? 20 : 10;
            const drawWheel = (wx: number, wy: number) => {
                tCtx.beginPath(); tCtx.roundRect(wx - 4, wy - 2, 8, 4, 2); tCtx.fill();
            };
            drawWheel(-w, -12); drawWheel(w, -12);
            drawWheel(-w, 12); drawWheel(w, 12);
            tCtx.restore();
        }

        function drawPine(tCtx: CanvasRenderingContext2D, x: number, y: number) {
            tCtx.save();
            tCtx.translate(x, y);
            tCtx.fillStyle = '#23575b';
            tCtx.beginPath(); tCtx.moveTo(-10, 10); tCtx.lineTo(10, 10); tCtx.lineTo(0, -10); tCtx.fill();
            tCtx.beginPath(); tCtx.moveTo(-12, 0); tCtx.lineTo(12, 0); tCtx.lineTo(0, -20); tCtx.fill();
            tCtx.beginPath(); tCtx.moveTo(-15, -10); tCtx.lineTo(15, -10); tCtx.lineTo(0, -35); tCtx.fill();
            tCtx.restore();
        }

        function drawBush(tCtx: CanvasRenderingContext2D, x: number, y: number) {
            tCtx.fillStyle = '#7cb8a4';
            circle(tCtx, x, y, 10); circle(tCtx, x - 8, y + 5, 7.5); circle(tCtx, x + 8, y + 5, 7.5);
        }

        function drawStopSign(tCtx: CanvasRenderingContext2D, x: number, y: number) {
            tCtx.save();
            tCtx.translate(x, y);
            tCtx.fillStyle = '#acb4b5'; tCtx.fillRect(-2, 10 - 10, 4, 20); // Pole
            tCtx.fillStyle = '#ef584e'; circle(tCtx, 0, 0, 7); // Sign base
            tCtx.restore();
        }

        // --- RENDER TILES ---
        generateCityTile(tileCtx);

        // --- DRAW TO MAIN CANVAS ---
        ctx.fillStyle = '#ffffff';
        ctx.fillRect(0, 0, width, height);

        ctx.save();
        ctx.scale(zoom, zoom); // This zooms the entire canvas out

        // We divide the screen width/height by zoom to fill screen
        let cols = Math.ceil((width / zoom) / TILE_SIZE) + 1;
        let rows = Math.ceil((height / zoom) / TILE_SIZE) + 1;

        // Draw the seamless grid 
        for (let i = 0; i < cols; i++) {
            for (let j = 0; j < rows; j++) {
                ctx.drawImage(tileCanvas, i * TILE_SIZE, j * TILE_SIZE);
            }
        }
        ctx.restore();

    }, [width, height]);

    return <canvas ref={canvasRef} width={width} height={height} style={{ borderRadius: 8, display: 'block', margin: '0 auto', boxShadow: '0 4px 6px rgba(0,0,0,0.3)' }} />;
}
