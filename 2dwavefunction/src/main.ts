class Tile {
    imageData: Uint8ClampedArray<ArrayBufferLike>
    hash: number
    // hashLeft: number
    // hashRight: number
    // hashFront: number
    // hashBack: number

    constructor(imageData: Uint8ClampedArray<ArrayBufferLike>) {
        this.imageData = imageData

        this.hash = fnv1aHash(imageData)
    }
}

// Get the canvas element and context
class WFC {

    sampleTileImageData: ImageData | null = null

    unitTileSize: number = 16
    tileSize: number = 320
    tileX: number = 0
    tileY: number = 0
    distanceX: number = 0
    distanceY: number = 0

    img: HTMLImageElement | null = null

    canvas: HTMLCanvasElement
    tileCanvas: HTMLCanvasElement
    sampleTileCanvas: HTMLCanvasElement

    ctx: CanvasRenderingContext2D
    sampleTileCtx: CanvasRenderingContext2D
    tileCtx: CanvasRenderingContext2D

    tiles: Map<number, Tile> = new Map()

    constructor() {
        this.canvas = document.getElementById('myCanvas') as HTMLCanvasElement;
        this.tileCanvas = document.getElementById("tiles") as HTMLCanvasElement;
        this.sampleTileCanvas = document.getElementById("sampletile") as HTMLCanvasElement;

        const sampleTileCtx = this.sampleTileCanvas.getContext("2d", { willReadFrequently: true });
        const ctx = this.canvas.getContext('2d', { willReadFrequently: true });
        const tileCtx = this.tileCanvas.getContext('2d', { willReadFrequently: true });

        if (!ctx || !sampleTileCtx || !tileCtx) {
            throw new DOMException("canvas context not found")
        } else {
            this.ctx = ctx
            this.sampleTileCtx = sampleTileCtx
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
            UI.createSlider("tileSize", "Tile Size", 320, Math.min(this.img.width, this.img.height), this.tileSize, (value) => {
                this.tileSize = value; this.draw();
            });
            UI.createSlider("tileX", "Tile X", 0, this.img.width / this.tileSize - 1, 0, (value) => {
                this.tileX = value; this.draw();
            });
            UI.createSlider("tileY", "Tile Y", 0, this.img.height / this.tileSize - 1, 0, (value) => {
                this.tileY = value; this.draw();
            });
            UI.createSlider("distanceX", "Distance X", 0, this.img.width - this.tileSize, 0, (value) => {
                this.distanceX = value; this.draw();
            });
            UI.createSlider("distanceY", "Distance Y", 0, this.img.height - this.tileSize, 0, (value) => {
                this.distanceY = value; this.draw();
            });

            this.makeTiles()

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

        const sampleX = this.tileSize * this.tileX + this.distanceX;
        const sampleY = this.tileSize * this.tileY + this.distanceY;

        // Draw a rectangle to highlight the sample area
        ctx.beginPath();
        ctx.rect(sampleX, sampleY, this.tileSize, this.tileSize);
        ctx.lineWidth = 3;
        ctx.strokeStyle = 'red';
        ctx.stroke();
        ctx.closePath();

        this.sampleTileImageData = WFC.getSampleImageData(ctx, this.tileSize, this.tileX, this.tileY, this.distanceX, this.distanceY)

        this.sampleTileCanvas.width = this.tileSize;
        this.sampleTileCanvas.height = this.tileSize;

        this.sampleTileCtx.putImageData(this.sampleTileImageData, 0, 0);
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

        const numCols = this.tileCanvas.width / this.unitTileSize - 1
        const numRows = this.tileCanvas.height / this.unitTileSize - 1

        for (let i = 0; i < numRows; i++) {
            for (let j = 0; j < numCols; j++) {

                // WFC.getSampleImageData(this.tileCtx, this.unitTileSize, i, j, 0, 0)

                // Extract the sub-tile ImageData
                const tileImageData = this.sampleTileCtx.getImageData(
                    i * this.unitTileSize,
                    j * this.unitTileSize,
                    this.tileSize, this.tileSize);

                // Draw the sub-tile on the target tile canvas
                this.tileCtx.putImageData(tileImageData, i * this.unitTileSize, j * this.unitTileSize);
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


function fnv1aHash(bytes: Uint8Array | Uint8ClampedArray): number {
    let hash = 0x811c9dc5; // FNV offset basis

    for (let i = 0; i < bytes.length; i++) {
        hash ^= bytes[i];
        hash += (hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) + (hash << 24);
    }

    // Ensure 32-bit unsigned integer overflow behavior
    return hash >>> 0;
}

// Example usage:
const data = new Uint8Array([1, 2, 3, 4, 5]); // Replace with your byte array
console.log("Hash:", fnv1aHash(data).toString(16)); // Display hash as hex
