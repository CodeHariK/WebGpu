///
///
///
///   ---->right +1
///   <----left  -1
///   ^
///   |  up      -1
///   *  down    +1
///
///    col =>right
///    row !down
///

import { PriorityQueue } from "./queue"
class Tile {
    imageData: ImageData

    occurrence: number
    probability: number

    hash: number

    leftNeighbours: Set<number> = new Set<number>()
    rightNeighbours: Set<number> = new Set<number>()
    upNeighbours: Set<number> = new Set<number>()
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
    }

    static getMosLikelyTile(tiles: Tile[]): Tile | null {
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

    r: number
    c: number

    entropy: number = 9999

    tiles: Set<number>

    constructor(currentTile: number | null, i: number, j: number, tiles: Set<number>) {
        this.currentTile = currentTile

        this.r = i
        this.c = j

        this.tiles = tiles
    }

    calculateEntropy(tiles: Map<number, Tile>, entropyPQ: PriorityQueue<WFCTile>) {
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

        // console.log(probabilities, entropy)

        this.entropy = entropy

        try {
            entropyPQ.updatePriority(this, entropy)
        } catch (error) {
            entropyPQ.insert(this, entropy)
        }
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
    entropyPQ = new PriorityQueue<WFCTile>();

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
        const wfcCtx = this.wfcCanvas.getContext('2d', { willReadFrequently: true });

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

            this.debugDraw()

            this.collapse()

        } catch (error) {
            console.error("Error loading image:", error);
        }
    }

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

    makeTiles() {

        const updateNewTile = (imageData: ImageData, row: number, col: number): Tile => {
            let tile = new Tile(imageData)

            if (!this.tiles.get(tile.hash)) {
                this.tiles.set(tile.hash, tile)
                this.tileCtx.putImageData(imageData, col * 16, row * 16);
            }
            tile = this.tiles.get(tile.hash)!

            return tile
        }

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

        for (let r = 0; r < numRows; r++) {
            for (let c = 0; c < numCols; c++) {

                // WFC.getSampleImageData(this.tileCtx, this.unitTileSize, i, j, 0, 0)

                const tileImageData = this.sampleCtx.getImageData(
                    c * this.unitTileSize,
                    r * this.unitTileSize,
                    this.unitTileSize, this.unitTileSize);

                {

                    let tile = updateNewTile(tileImageData, r, c)
                    tile.occurrence += 1
                    tile.probability = tile.occurrence / (numRows * numCols)

                    // Check and add the left neighbor if available
                    if (c > 0) {
                        const leftTileImageData = this.sampleCtx.getImageData(
                            (c - 1) * this.unitTileSize,
                            r * this.unitTileSize,
                            this.unitTileSize, this.unitTileSize
                        );

                        let leftTile = updateNewTile(leftTileImageData, r, c - 1)

                        tile.leftNeighbours.add(leftTile.hash); // Add left tile to the current tile's leftNeighbors
                        // leftTile.rightNeighbours.add(tile.hash); // Add current tile to the left tile's rightNeighbors
                    }

                    // Check and add the right neighbor if available
                    if (c < numCols - 1) {
                        const rightTileImageData = this.sampleCtx.getImageData(
                            (c + 1) * this.unitTileSize,
                            r * this.unitTileSize,
                            this.unitTileSize, this.unitTileSize
                        );

                        let rightTile = updateNewTile(rightTileImageData, r, c + 1)

                        tile.rightNeighbours.add(rightTile.hash); // Add right tile to the current tile's rightNeighbors
                        // rightTile.leftNeighbours.add(tile.hash); // Add current tile to the right tile's leftNeighbors
                    }

                    // Check and add the top neighbor if available
                    if (r > 0) {
                        const topTileImageData = this.sampleCtx.getImageData(
                            c * this.unitTileSize,
                            (r - 1) * this.unitTileSize,
                            this.unitTileSize, this.unitTileSize
                        );

                        let topTile = updateNewTile(topTileImageData, r - 1, c)

                        tile.upNeighbours.add(topTile.hash); // Add top tile to the current tile's topNeighbors
                        // topTile.downNeighbours.add(tile.hash); // Add current tile to the top tile's downNeighbours
                    }

                    // Check and add the down neighbor if available
                    if (r < numRows - 1) {
                        const downTileImageData = this.sampleCtx.getImageData(
                            c * this.unitTileSize,
                            (r + 1) * this.unitTileSize,
                            this.unitTileSize, this.unitTileSize
                        );
                        let downTile = updateNewTile(downTileImageData, r + 1, c)

                        tile.downNeighbours.add(downTile.hash); // Add down tile to the current tile's downNeighbours
                        // downTile.upNeighbours.add(tile.hash); // Add current tile to the down tile's topNeighbours
                    }
                }
            }
        }

        console.log(this.tiles)
    }

    debugDraw() {
        this.singleTileCanvas.width = 1000
        this.singleTileCanvas.height = this.tiles.size * 16

        let i = 0

        this.tiles.forEach((tile) => {

            this.singleTileCtx.putImageData(tile.imageData, 500, i * 16)

            let ll = 0
            tile.leftNeighbours.forEach((l) => {
                ll++
                let ltile = this.tiles.get(l)!
                this.singleTileCtx.putImageData(ltile.imageData, 250 + ll * 16, i * 16)
            })

            let rr = 0
            tile.rightNeighbours.forEach((r) => {
                rr++
                let rtile = this.tiles.get(r)!
                this.singleTileCtx.putImageData(rtile.imageData, 520 + rr * 16, i * 16)
            })

            let tt = 0
            tile.upNeighbours.forEach((t) => {
                tt++
                let ttile = this.tiles.get(t)!
                this.singleTileCtx.putImageData(ttile.imageData, 750 + tt * 16, i * 16)
            })

            let dd = 0
            tile.downNeighbours.forEach((t) => {
                dd++
                let dtile = this.tiles.get(t)!
                this.singleTileCtx.putImageData(dtile.imageData, 0 + dd * 16, i * 16)
            })

            i++
        })
    }

    collapse() {

        this.wfcCanvas.width = 320
        this.wfcCanvas.height = 320
        let rows = this.wfcCanvas.width / this.unitTileSize;
        let cols = this.wfcCanvas.height / this.unitTileSize;

        this.wfcTiles = [];
        for (let r = 0; r < rows; r++) {
            this.wfcTiles[r] = []; // Create a new row for each column index
            for (let c = 0; c < cols; c++) {
                this.wfcTiles[r][c] = new WFCTile(null, r, c, new Set());
            }
        }

        // Initialization
        {
            this.collapseTile(rows / 2, cols / 2, rows, cols, true)
        }

        document.onkeydown = (e) => {
            e.preventDefault()
            if (e.key == "ArrowRight") {
                if (this.entropyPQ.size() > 0) {
                    let wfctile = this.entropyPQ.extractMin()

                    // console.log(wfctile)

                    this.collapseTile(wfctile!.r, wfctile!.c, rows, cols, false)
                }

                this.drawWFC(rows, cols)
            }
        }

        this.drawWFC(rows, cols)
    }

    drawWFC(rows: number, cols: number) {

        this.wfcCtx.clearRect(0, 0, this.wfcCanvas.width, this.wfcCanvas.height)

        for (let r = 0; r < rows; r++) {
            for (let c = 0; c < cols; c++) {

                let wfcTile = this.wfcTiles[r][c]
                if (wfcTile.currentTile) {

                    let tile = this.tiles.get(wfcTile.currentTile)!

                    // console.log(r, c, tile, this.entropyPQ)

                    this.wfcCtx.putImageData(tile!.imageData, wfcTile!.c * 16, wfcTile!.r * 16)
                } else {
                    this.wfcCtx.fillText(wfcTile.tiles.size.toString(), wfcTile!.c * 16 + 4, wfcTile!.r * 16 + 12, 16)
                }
            }
        }
    }

    collapseRandomTile(rows: number, cols: number) {
        for (let i = 0; i < rows; i++) {
            for (let j = 0; j < cols; j++) {
                if (this.collapseTile(i, j, rows, cols, true)) {
                    return
                }
            }
        }
    }

    // Method to get a Set of Tile objects based on a set of hash numbers
    getTileSetFromHashes(hashes: Set<number>): Set<Tile> {
        // Filter the tiles map based on the keys in the hashes set and return a Set of Tiles
        return new Set(
            Array.from(this.tiles)
                .filter(([hash, _]) => hashes.has(hash)) // Filter entries where the key is in the hashes set
                .map(([_, tile]) => tile) // Map to just the Tile objects
        );
    }

    collapseTile(r: number, c: number, rows: number, cols: number, random: boolean): boolean {

        // const randomTile = Tile.getMosLikelyTile(Array.from(this.tiles.values()))

        let wfcTile = this.wfcTiles[r][c]

        let randomTile: Tile | null
        let possibleTiles = this.getTileSetFromHashes(wfcTile.tiles)
        if (possibleTiles.size > 0) {
            randomTile = Tile.getMosLikelyTile(Array.from(possibleTiles))
        } else {
            if (random) {
                randomTile = Tile.getMosLikelyTile(Array.from(this.tiles.values()))
            } else {
                console.log("No Possible Tiles")
                return false
            }
        }

        if (wfcTile.currentTile === null && randomTile) {

            // console.log("Collapsed", r, c, wfcTile)

            // RightTile
            if (c + 1 < cols) {
                const rightTile = this.wfcTiles[r][c + 1];
                const possibleTiles = randomTile.rightNeighbours;

                if (!rightTile.currentTile) {
                    if (rightTile.tiles.size === 0) {
                        rightTile.tiles = possibleTiles;
                    } else {
                        rightTile.tiles = new Set(Array.from(possibleTiles).filter(num => rightTile.tiles.has(num)));
                    }

                    // console.log("right")
                    rightTile.calculateEntropy(this.tiles, this.entropyPQ)
                }
            }

            // LeftTile
            if (c - 1 >= 0) {
                const leftTile = this.wfcTiles[r][c - 1];
                const possibleTiles = randomTile.leftNeighbours

                if (!leftTile.currentTile) {
                    if (leftTile.tiles.size === 0) {
                        leftTile.tiles = possibleTiles;
                    } else {
                        leftTile.tiles = new Set(Array.from(possibleTiles).filter(num => leftTile.tiles.has(num)));
                    }

                    // console.log("left")
                    leftTile.calculateEntropy(this.tiles, this.entropyPQ)
                }
            }

            // UpTile
            if (r - 1 >= 0) {
                const upTile = this.wfcTiles[r - 1][c];
                const possibleTiles = randomTile.upNeighbours

                if (!upTile.currentTile) {
                    if (upTile.tiles.size === 0) {
                        upTile.tiles = possibleTiles;
                    } else {
                        upTile.tiles = new Set(Array.from(possibleTiles).filter(num => upTile.tiles.has(num)));
                    }

                    // console.log("up")
                    upTile.calculateEntropy(this.tiles, this.entropyPQ)
                }
            }

            // DownTile
            if (r + 1 < rows) {
                const downTile = this.wfcTiles[r + 1][c];
                const possibleTiles = randomTile.downNeighbours

                if (!downTile.currentTile) {
                    if (downTile.tiles.size === 0) {
                        downTile.tiles = possibleTiles;
                    } else {
                        downTile.tiles = new Set(Array.from(possibleTiles).filter(num => downTile.tiles.has(num)));
                    }

                    // console.log("down")
                    downTile.calculateEntropy(this.tiles, this.entropyPQ)
                }
            }

            wfcTile.entropy = -1
            wfcTile.currentTile = randomTile.hash
            wfcTile.tiles = new Set()

            return true
        }

        console.log(this.wfcTiles)
        console.log(this.entropyPQ)

        return false
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
