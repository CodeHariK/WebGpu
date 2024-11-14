import { PriorityQueue } from "./queue"
class Tile {
    imageData: ImageData

    occurrence: number
    probability: number

    hash: number

    leftNeighbours: Set<number> = new Set<number>()
    rightNeighbours: Set<number> = new Set<number>()
    topNeighbours: Set<number> = new Set<number>()
    downNeighbours: Set<number> = new Set<number>()

    constructor(imageData: ImageData) {

        this.imageData = imageData

        this.occurrence = 0
        this.probability = 0

        this.hash = Tile.fnv1aHash(imageData.data)
    }

    static fnv1aHash(bytes: Uint8Array | number[] | Uint8ClampedArray): number {
        let hash = 0x811c9dc5; // FNV offset basis

        for (let i = 0; i < bytes.length; i++) {
            hash ^= bytes[i];
            hash += (hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) + (hash << 24);
        }

        // Ensure 32-bit unsigned integer overflow behavior
        return hash >>> 0;
    }

    static getRandomTile(tiles: Tile[]): Tile | null {

        return tiles[Math.floor(Math.random() * tiles.length)]

        const totalOccurrences = tiles.reduce((sum, tile) => sum + tile.occurrence, 0);
        const randomValue = Math.random() * totalOccurrences;

        let cumulativeSum = 0;
        for (const tile of tiles) {
            cumulativeSum += tile.occurrence;
            if (randomValue < cumulativeSum) {
                return tile;
            }
        }
        return null; // In case the list is empty
    }
}

class WFCTile {
    currentTile: number | null

    i: number
    j: number

    entropy: number = 9999

    tiles: Set<number>

    constructor(currentTile: number | null, i: number, j: number, tiles: Set<number>) {
        this.currentTile = currentTile

        this.i = i
        this.j = j

        this.tiles = tiles
    }

    calculateEntropy(tiles: Map<number, Tile>) {
        const probabilities: number[] = [];

        for (const hash of this.tiles) {
            const tile = tiles.get(hash);
            if (tile) {
                probabilities.push(tile.probability);
            }
        }

        let entropy = 0;
        for (const prob of probabilities) {
            if (prob > 0) {
                entropy -= prob * Math.log2(prob);
            }
        }

        console.log(probabilities, entropy)

        this.entropy = entropy
    }
}

class WFC {

    sampleTileImageData: ImageData | null = null

    unitTileSize: number = 16
    sampleSize: number = 320
    tileX: number = 0
    tileY: number = 0
    distanceX: number = 0
    distanceY: number = 0

    img: HTMLImageElement | null = null

    fullCanvas: HTMLCanvasElement
    singleTileCanvas: HTMLCanvasElement
    tileCanvas: HTMLCanvasElement
    sampleCanvas: HTMLCanvasElement
    wfcCanvas: HTMLCanvasElement

    fullCtx: CanvasRenderingContext2D
    singleTileCtx: CanvasRenderingContext2D
    sampleCtx: CanvasRenderingContext2D
    tileCtx: CanvasRenderingContext2D
    wfcCtx: CanvasRenderingContext2D

    tiles: Map<number, Tile> = new Map()

    wfcTiles: WFCTile[][] = []
    entropyPQ = new PriorityQueue<{ i: number, j: number }>();

    constructor() {
        this.fullCanvas = document.getElementById('myCanvas') as HTMLCanvasElement;
        this.singleTileCanvas = document.getElementById('singleTile') as HTMLCanvasElement;
        this.tileCanvas = document.getElementById("tiles") as HTMLCanvasElement;
        this.sampleCanvas = document.getElementById("sampletile") as HTMLCanvasElement;
        this.wfcCanvas = document.getElementById("wfc") as HTMLCanvasElement;

        const ctx = this.fullCanvas.getContext('2d', { willReadFrequently: true });
        const singleTileCtx = this.singleTileCanvas.getContext('2d', { willReadFrequently: true });
        const sampleTileCtx = this.sampleCanvas.getContext("2d", { willReadFrequently: true });
        const tileCtx = this.tileCanvas.getContext('2d', { willReadFrequently: true });
        const wfcCtx = this.tileCanvas.getContext('2d', { willReadFrequently: true });

        if (!ctx || !sampleTileCtx || !tileCtx || !singleTileCtx || !wfcCtx) {
            throw new DOMException("canvas context not found")
        } else {
            this.fullCtx = ctx
            this.singleTileCtx = singleTileCtx
            this.sampleCtx = sampleTileCtx
            this.tileCtx = tileCtx
            this.wfcCtx = wfcCtx
        }
    }

    async load(path: string) {

        if (!this.fullCtx) {
            console.error("Canvas rendering context not found.");
            return;
        }

        try {
            // Await image loading
            this.img = await WFC.loadImage(path); // Replace with your image path

            // Display image dimensions
            (document.getElementById("imageWidth") as HTMLSpanElement).textContent = this.img.width.toString();
            (document.getElementById("imageHeight") as HTMLSpanElement).textContent = this.img.height.toString();

            this.draw()

            UI.createSlider("unitTileSize", "Unit Tile Size", 16, 300, 16, (value) => {
                this.unitTileSize = value; this.draw();
            });
            UI.createSlider("tileSize", "Tile Size", 320, Math.min(this.img.width, this.img.height), this.sampleSize, (value) => {
                this.sampleSize = value; this.draw();
            });
            UI.createSlider("tileX", "Tile X", 0, this.img.width / this.sampleSize - 1, 0, (value) => {
                this.tileX = value; this.draw();
            });
            UI.createSlider("tileY", "Tile Y", 0, this.img.height / this.sampleSize - 1, 0, (value) => {
                this.tileY = value; this.draw();
            });
            UI.createSlider("distanceX", "Distance X", 0, this.img.width - this.sampleSize, 0, (value) => {
                this.distanceX = value; this.draw();
            });
            UI.createSlider("distanceY", "Distance Y", 0, this.img.height - this.sampleSize, 0, (value) => {
                this.distanceY = value; this.draw();
            });

            this.makeTiles()

            this.collapse()

        } catch (error) {
            console.error("Error loading image:", error);
        }
    }

    static getPixel = (pixels: ImageData, x: number, y: number): [number, number, number, number] => {
        const index = (y * pixels.width + x) * 4;
        const r = pixels.data[index];
        const g = pixels.data[index + 1];
        const b = pixels.data[index + 2];
        const a = pixels.data[index + 3];
        return [r, g, b, a];
    };

    static loadImage(src: string): Promise<HTMLImageElement> {
        return new Promise((resolve, reject) => {
            const img = new Image();
            img.src = src;
            img.onload = () => resolve(img);
            img.onerror = (err) => reject(err);
        });
    }

    // Function to redraw the image with the updated tile size and distance
    draw() {
        if (!this.img) {
            throw new DOMException("NO Image")
        }
        let img = this.img
        let canvas = this.fullCanvas
        let ctx = this.fullCtx

        // Clear the canvas
        ctx.clearRect(0, 0, this.fullCanvas.width, this.fullCanvas.height);

        canvas.width = img.width;
        canvas.height = img.height;

        // Draw the image onto the canvas
        ctx.drawImage(this.img!, 0, 0, this.fullCanvas.width, this.fullCanvas.height);

        const sampleX = this.sampleSize * this.tileX + this.distanceX;
        const sampleY = this.sampleSize * this.tileY + this.distanceY;

        // Draw a rectangle to highlight the sample area
        ctx.globalAlpha = 0.2; // Set opacity to 10%
        ctx.fillStyle = `rgb(255, 255, 255)`; // Assuming the background is white
        ctx.fillRect(sampleX, sampleY, this.sampleSize, this.sampleSize);
        ctx.globalAlpha = 1.0; // Reset globalAlpha to full opacity for subsequent drawing

        this.sampleTileImageData = WFC.getSampleImageData(ctx, this.sampleSize, this.tileX, this.tileY, this.distanceX, this.distanceY)

        this.sampleCanvas.width = this.sampleSize;
        this.sampleCanvas.height = this.sampleSize;

        this.sampleCtx.putImageData(this.sampleTileImageData, 0, 0);
    }

    static getSampleImageData(ctx: CanvasRenderingContext2D, unitTileSize: number, tileX: number, tileY: number, distanceX: number, distanceY: number) {
        return ctx.getImageData(
            unitTileSize * tileX + distanceX,
            unitTileSize * tileY + distanceY,
            unitTileSize,
            unitTileSize);
    }

    updateNewTile(imageData: ImageData, i: number, j: number): Tile {
        let tile = new Tile(imageData)

        if (!this.tiles.get(tile.hash)) {
            this.tiles.set(tile.hash, tile)

            this.tileCtx.putImageData(imageData, i * 16, j * 16);
        }
        tile = this.tiles.get(tile.hash)!

        return tile
    }

    makeTiles() {
        if (!this.sampleTileImageData) {
            console.error("No sample tile image data available");
            return
        }

        this.tileCanvas.width = this.sampleTileImageData.width
        this.tileCanvas.height = this.sampleTileImageData.height

        this.tileCtx.clearRect(0, 0, this.tileCanvas.width, this.tileCanvas.height);

        const numCols = this.tileCanvas.width / this.unitTileSize
        const numRows = this.tileCanvas.height / this.unitTileSize

        this.tiles = new Map()

        for (let i = 0; i < numRows; i++) {
            for (let j = 0; j < numCols; j++) {

                // WFC.getSampleImageData(this.tileCtx, this.unitTileSize, i, j, 0, 0)

                const tileImageData = this.sampleCtx.getImageData(
                    i * this.unitTileSize,
                    j * this.unitTileSize,
                    this.unitTileSize, this.unitTileSize);

                {

                    let tile = this.updateNewTile(tileImageData, i, j)
                    tile.occurrence += 1
                    tile.probability = tile.occurrence / (numRows * numCols)

                    // Check and add the left neighbor if available
                    if (i > 0) {
                        const leftTileImageData = this.sampleCtx.getImageData(
                            (i - 1) * this.unitTileSize,
                            j * this.unitTileSize,
                            this.unitTileSize, this.unitTileSize
                        );

                        let leftTile = this.updateNewTile(leftTileImageData, i, j)

                        tile.leftNeighbours.add(leftTile.hash); // Add left tile to the current tile's leftNeighbors
                        leftTile.rightNeighbours.add(tile.hash); // Add current tile to the left tile's rightNeighbors
                    }

                    // Check and add the right neighbor if available
                    if (i < numCols - 1) {
                        const rightTileImageData = this.sampleCtx.getImageData(
                            (i + 1) * this.unitTileSize,
                            j * this.unitTileSize,
                            this.unitTileSize, this.unitTileSize
                        );

                        let rightTile = this.updateNewTile(rightTileImageData, i, j)

                        tile.rightNeighbours.add(rightTile.hash); // Add right tile to the current tile's rightNeighbors
                        rightTile.leftNeighbours.add(tile.hash); // Add current tile to the right tile's leftNeighbors
                    }

                    // Check and add the top neighbor if available
                    if (j > 0) {
                        const topTileImageData = this.sampleCtx.getImageData(
                            i * this.unitTileSize,
                            (j - 1) * this.unitTileSize,
                            this.unitTileSize, this.unitTileSize
                        );

                        let topTile = this.updateNewTile(topTileImageData, i, j)

                        tile.topNeighbours.add(topTile.hash); // Add top tile to the current tile's topNeighbors
                        topTile.downNeighbours.add(tile.hash); // Add current tile to the top tile's downNeighbours
                    }

                    // Check and add the down neighbor if available
                    if (j < numRows - 1) {
                        const downTileImageData = this.sampleCtx.getImageData(
                            i * this.unitTileSize,
                            (j + 1) * this.unitTileSize,
                            this.unitTileSize, this.unitTileSize
                        );
                        let downTile = this.updateNewTile(downTileImageData, i, j)

                        tile.downNeighbours.add(downTile.hash); // Add down tile to the current tile's downNeighbours
                        downTile.topNeighbours.add(tile.hash); // Add current tile to the down tile's topNeighbours
                    }
                }
            }
        }

        console.log(this.tiles)
    }

    collapse() {
        this.singleTileCanvas.width = 320
        this.singleTileCanvas.height = 320

        let rows = this.singleTileCanvas.width / this.unitTileSize;
        let cols = this.singleTileCanvas.height / this.unitTileSize;

        this.wfcTiles = [];
        for (let j = 0; j < cols; j++) {
            this.wfcTiles[j] = []; // Create a new row for each column index
            for (let i = 0; i < rows; i++) {
                this.wfcTiles[j][i] = new WFCTile(null, i, j, new Set());
            }
        }

        // Initialization
        {
            this.collapseTile(rows / 2, cols / 2, rows, cols)
        }

        for (let i = 0; i < rows; i++) {
            for (let j = 0; j < cols; j++) {
                let hello = this.wfcTiles[i][j]
                if (!hello.currentTile) {

                }
            }
        }
    }

    collapseRandomTile(rows: number, cols: number) {

        for (let i = 0; i < rows; i++) {
            for (let j = 0; j < cols; j++) {
                this.collapseTile(i, j, rows, cols)

                return
            }
        }
    }

    // Method to get a Set of Tile objects based on a set of hash numbers
    getTileSetFromHash(hashes: Set<number>): Set<Tile> {
        // Filter the tiles map based on the keys in the hashes set and return a Set of Tiles
        return new Set(
            Array.from(this.tiles)
                .filter(([hash, _]) => hashes.has(hash)) // Filter entries where the key is in the hashes set
                .map(([_, tile]) => tile) // Map to just the Tile objects
        );
    }

    collapseTile(i: number, j: number, rows: number, cols: number) {
        const randomTile = Tile.getRandomTile(Array.from(this.tiles.values()))

        let wfcTile = this.wfcTiles[i][j]
        wfcTile.entropy = 0

        if (wfcTile.currentTile === null && randomTile) {

            wfcTile.currentTile = randomTile.hash

            // RightTile
            if (i + 1 < rows) {
                const rightTile = this.wfcTiles[i + 1][j];
                const possibleTiles = randomTile.rightNeighbours;

                if (rightTile.tiles.size === 0) {
                    rightTile.tiles = possibleTiles;
                } else {
                    rightTile.tiles = new Set(Array.from(possibleTiles).filter(num => rightTile.tiles.has(num)));
                }

                rightTile.calculateEntropy(this.tiles)
            }

            // LeftTile
            if (i - 1 >= 0) {
                const leftTile = this.wfcTiles[i - 1][j];
                const possibleTiles = randomTile.leftNeighbours

                if (leftTile.tiles.size === 0) {
                    leftTile.tiles = possibleTiles;
                } else {
                    leftTile.tiles = new Set(Array.from(possibleTiles).filter(num => leftTile.tiles.has(num)));
                }

                leftTile.calculateEntropy(this.tiles)
            }

            // UpTile
            if (j + 1 < cols) {
                const upTile = this.wfcTiles[i][j - 1];
                const possibleTiles = randomTile.topNeighbours

                if (upTile.tiles.size === 0) {
                    upTile.tiles = possibleTiles;
                } else {
                    upTile.tiles = new Set(Array.from(possibleTiles).filter(num => upTile.tiles.has(num)));
                }

                upTile.calculateEntropy(this.tiles)
            }

            // DownTile
            if (j - 1 >= 0) {
                const downTile = this.wfcTiles[i][j + 1];
                const possibleTiles = randomTile.downNeighbours

                if (downTile.tiles.size === 0) {
                    downTile.tiles = possibleTiles;
                } else {
                    downTile.tiles = new Set(Array.from(possibleTiles).filter(num => downTile.tiles.has(num)));
                }

                downTile.calculateEntropy(this.tiles)
            }

            this.debugDraw(randomTile, rows, cols)
        }

        console.log(this.wfcTiles)
    }

    debugDraw(randomTile: Tile, rows: number, cols: number) {

        let i = rows / 2
        let j = cols / 2

        this.singleTileCtx.putImageData(randomTile!.imageData, i * 16, j * 16)

        let h = 1;
        // Right (i + 1, j)
        this.wfcTiles[i + 1][j].tiles.forEach((t) => {
            h += 1;
            this.singleTileCtx.putImageData(this.tiles.get(t)!.imageData, (i + h) * 16, j * 16);
        });

        // Left (i - 1, j)
        h = 1; // Reset h for the left direction
        this.wfcTiles[i - 1][j].tiles.forEach((t) => {
            h += 1;
            this.singleTileCtx.putImageData(this.tiles.get(t)!.imageData, (i - h) * 16, j * 16);
        });

        // Front (i, j - 1) - Top
        h = 1; // Reset h for the front (top) direction
        this.wfcTiles[i][j + 1].tiles.forEach((t) => {
            h += 1;
            this.singleTileCtx.putImageData(this.tiles.get(t)!.imageData, i * 16, (j + h) * 16);
        });

        // Back (i, j + 1) - Bottom
        h = 1; // Reset h for the back (bottom) direction
        this.wfcTiles[i][j - 1].tiles.forEach((t) => {
            h += 1;
            this.singleTileCtx.putImageData(this.tiles.get(t)!.imageData, i * 16, (j - h) * 16);
        });
    }

}

class UI {
    static updateSliderValue(id: string, value: number) {
        const element = document.getElementById(id) as HTMLSpanElement;
        element.textContent = value.toString();
    }

    static createSlider(id: string, label: string, min: number, max: number, defaultValue: number, onChange: (value: number) => void) {
        // Create the slider container
        const container = document.createElement("div");
        container.innerHTML = `
            <div>
                <label>${label}: <input type="range" id="${id}" min="${min}" max="${max}" value="${defaultValue}"></label>
                <span id="${id}Value">${defaultValue}</span>
            </div>
        `;

        // Access the slider and value display elements
        const slider = container.querySelector(`#${id}`) as HTMLInputElement;
        const valueDisplay = container.querySelector(`#${id}Value`) as HTMLSpanElement;

        // Add an input event listener to update value display and call onChange
        slider.addEventListener("input", () => {
            const value = Number(slider.value);
            valueDisplay.textContent = value.toString();
            onChange(value);
        });

        // Append the container to the UI div
        const ui = document.getElementById("ui");
        if (ui) {
            ui.appendChild(container);
        } else {
            console.error("UI container not found");
        }
    }
}

const w = new WFC()
await w.load('./assets/zelda.jpg')

const [red, green, blue, alpha] = WFC.getPixel(w.sampleTileImageData!, 0, 0);
console.log(`Pixel at (0, 0) - R: ${red}, G: ${green}, B: ${blue}, A: ${alpha}`);
