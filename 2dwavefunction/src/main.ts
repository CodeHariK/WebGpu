class Tile {
    imageData: ImageData

    occurrence: number
    probability: number

    hash: number
    hashLeft: number
    hashRight: number
    hashFront: number
    hashBack: number

    constructor(imageData: ImageData) {

        let t = Tile.extractEdges(imageData)

        this.imageData = imageData
        this.occurrence = 1
        this.probability = 0
        this.hash = Tile.fnv1aHash(imageData.data)
        this.hashRight = Tile.fnv1aHash(t.lastColumn)
        this.hashLeft = Tile.fnv1aHash(t.firstColumn)
        this.hashFront = Tile.fnv1aHash(t.firstRow)
        this.hashBack = Tile.fnv1aHash(t.lastRow)
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

    static extractEdges(imageData: ImageData) {
        const pixels = imageData.data;
        const width = imageData.width;
        const height = imageData.height;

        // Arrays to store the pixel data of the regions
        let firstRow: number[] = [];
        let lastRow: number[] = [];
        let firstColumn: number[] = [];
        let lastColumn: number[] = [];

        // First row (pixels in the first row of the image)
        for (let x = 0; x < width; x++) {
            const idx = (0 * width + x) * 4; // Row 0, Column x
            firstRow.push(...pixels.slice(idx, idx + 4)); // RGBA for each pixel
        }

        // Last row (pixels in the last row of the image)
        for (let x = 0; x < width; x++) {
            const idx = ((height - 1) * width + x) * 4; // Last row, Column x
            lastRow.push(...pixels.slice(idx, idx + 4)); // RGBA for each pixel
        }

        // First column (pixels in the first column of each row)
        for (let y = 0; y < height; y++) {
            const idx = (y * width + 0) * 4; // Row y, Column 0
            firstColumn.push(...pixels.slice(idx, idx + 4)); // RGBA for each pixel
        }

        // Last column (pixels in the last column of each row)
        for (let y = 0; y < height; y++) {
            const idx = (y * width + (width - 1)) * 4; // Row y, Last column
            lastColumn.push(...pixels.slice(idx, idx + 4)); // RGBA for each pixel
        }

        return { firstRow, lastRow, firstColumn, lastColumn };
    }

    static getRandomTile(tiles: Tile[]): Tile | null {
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
    currentTileHash: number | null
    tilesLeft: Set<Number>
    tilesRight: Set<Number>
    tilesFront: Set<Number>
    tilesBack: Set<Number>

    constructor(currentTile: number | null, tilesLeft: Set<Number>, tilesRight: Set<Number>, tilesFront: Set<Number>, tilesBack: Set<Number>) {
        this.currentTileHash = currentTile
        this.tilesLeft = tilesLeft
        this.tilesRight = tilesRight
        this.tilesFront = tilesFront
        this.tilesBack = tilesBack
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

    canvas: HTMLCanvasElement
    wfcCanvas: HTMLCanvasElement
    tileCanvas: HTMLCanvasElement
    sampleCanvas: HTMLCanvasElement

    ctx: CanvasRenderingContext2D
    wfcCtx: CanvasRenderingContext2D
    sampleCtx: CanvasRenderingContext2D
    tileCtx: CanvasRenderingContext2D

    tiles: Map<number, Tile> = new Map()
    tilesLeftHash: Map<number, Set<number>> = new Map()
    tilesRightHash: Map<number, Set<number>> = new Map()
    tilesFrontHash: Map<number, Set<number>> = new Map()
    tilesBackHash: Map<number, Set<number>> = new Map()

    wfcTiles: WFCTile[][] = []

    constructor() {
        this.canvas = document.getElementById('myCanvas') as HTMLCanvasElement;
        this.wfcCanvas = document.getElementById('wfc') as HTMLCanvasElement;
        this.tileCanvas = document.getElementById("tiles") as HTMLCanvasElement;
        this.sampleCanvas = document.getElementById("sampletile") as HTMLCanvasElement;

        const ctx = this.canvas.getContext('2d', { willReadFrequently: true });
        const wfcCtx = this.wfcCanvas.getContext('2d', { willReadFrequently: true });
        const sampleTileCtx = this.sampleCanvas.getContext("2d", { willReadFrequently: true });
        const tileCtx = this.tileCanvas.getContext('2d', { willReadFrequently: true });

        if (!ctx || !sampleTileCtx || !tileCtx || !wfcCtx) {
            throw new DOMException("canvas context not found")
        } else {
            this.ctx = ctx
            this.wfcCtx = wfcCtx
            this.sampleCtx = sampleTileCtx
            this.tileCtx = tileCtx
        }
    }

    async load(path: string) {

        if (!this.ctx) {
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

            // this.oldCode()

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
        let canvas = this.canvas
        let ctx = this.ctx

        // Clear the canvas
        ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

        canvas.width = img.width;
        canvas.height = img.height;

        // Draw the image onto the canvas
        ctx.drawImage(this.img!, 0, 0, this.canvas.width, this.canvas.height);

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
        this.tilesLeftHash = new Map()

        for (let i = 0; i < numRows; i++) {
            for (let j = 0; j < numCols; j++) {

                // WFC.getSampleImageData(this.tileCtx, this.unitTileSize, i, j, 0, 0)

                // Extract the sub-tile ImageData
                const tileImageData = this.sampleCtx.getImageData(
                    i * this.unitTileSize,
                    j * this.unitTileSize,
                    this.unitTileSize, this.unitTileSize);

                let t = new Tile(tileImageData)
                if (!this.tiles.get(t.hash)) {
                    this.tiles.set(t.hash, t)

                    this.tileCtx.putImageData(tileImageData, i * this.unitTileSize, j * this.unitTileSize);

                    let leftHash = this.tilesLeftHash.get(t.hashLeft) ?? new Set<number>()
                    leftHash.add(t.hash)
                    this.tilesLeftHash.set(t.hashLeft, leftHash)

                    let rightHash = this.tilesRightHash.get(t.hashRight) ?? new Set<number>()
                    rightHash.add(t.hash)
                    this.tilesRightHash.set(t.hashRight, rightHash)

                    let frontHash = this.tilesFrontHash.get(t.hashFront) ?? new Set<number>()
                    frontHash.add(t.hash)
                    this.tilesFrontHash.set(t.hashFront, frontHash)

                    let backHash = this.tilesBackHash.get(t.hashBack) ?? new Set<number>()
                    backHash.add(t.hash)
                    this.tilesBackHash.set(t.hashBack, backHash)
                } else {
                    let tt = this.tiles.get(t.hash)!
                    tt.occurrence = tt.occurrence + 1
                    this.tiles.set(t.hash, tt)
                }
                let tt = this.tiles.get(t.hash)!
                tt.probability = tt.occurrence / (numRows * numCols)
            }
        }

        console.log(this.tiles)
        console.log(this.tilesLeftHash)
        console.log(this.tilesRightHash)
        console.log(this.tilesFrontHash)
        console.log(this.tilesBackHash)
    }

    collapse() {
        this.wfcCanvas.width = 320
        this.wfcCanvas.height = 320

        let rows = this.wfcCanvas.width / this.unitTileSize;
        let cols = this.wfcCanvas.height / this.unitTileSize;

        this.wfcTiles = [];
        for (let j = 0; j < cols; j++) {
            this.wfcTiles[j] = []; // Create a new row for each column index
            for (let i = 0; i < rows; i++) {
                this.wfcTiles[j][i] = new WFCTile(null, new Set(), new Set(), new Set(), new Set());
            }
        }

        // Initialization
        {
            // for (let i = 0; i < rows; i++) {
            //     for (let j = 0; j < cols; j++) {

            const randomTile = Tile.getRandomTile(Array.from(this.tiles.values()))

            let randomWFCTile = this.wfcTiles[rows / 2][cols / 2]
            if (randomTile && randomWFCTile) {
                randomWFCTile.currentTileHash = randomTile.hash

                randomWFCTile.tilesRight = this.tilesLeftHash.get(this.tiles.get(randomTile.hash)!.hashLeft) ?? new Set()
                randomWFCTile.tilesLeft = this.tilesRightHash.get(this.tiles.get(randomTile.hash)!.hashRight) ?? new Set()
                randomWFCTile.tilesFront = this.tilesBackHash.get(this.tiles.get(randomTile.hash)!.hashBack) ?? new Set()
                randomWFCTile.tilesBack = this.tilesFrontHash.get(this.tiles.get(randomTile.hash)!.hashFront) ?? new Set()
            }

            console.log(randomWFCTile)
            // //     }
            // // }

            // let i = 12
            // randomWFCTile.tilesRight.forEach((t) => {
            //     i += 1
            //     this.wfcCtx.putImageData(this.tiles.get(t.valueOf())!.imageData, i * 16, 160)
            // })
            // i = 0
            // randomWFCTile.tilesLeft.forEach((t) => {
            //     i += 1
            //     this.wfcCtx.putImageData(this.tiles.get(t.valueOf())!.imageData, i * 16, 160)
            // })
            // i = 8
            // randomWFCTile.tilesFront.forEach((t) => {
            //     i += 1
            //     this.wfcCtx.putImageData(this.tiles.get(t.valueOf())!.imageData, i * 16, 120)
            // })
            // i = 8
            // randomWFCTile.tilesBack.forEach((t) => {
            //     i += 1
            //     this.wfcCtx.putImageData(this.tiles.get(t.valueOf())!.imageData, i * 16, 200)
            // })
        }

        console.log(this.wfcTiles)

        for (let i = 0; i < rows; i++) {
            for (let j = 0; j < cols; j++) {
                let hello = this.wfcTiles[i][j]
                if (hello && hello.currentTileHash) {
                    this.wfcCtx.putImageData(this.tiles.get(hello.currentTileHash)!.imageData, i * 16, j * 16)
                }
            }
        }
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
