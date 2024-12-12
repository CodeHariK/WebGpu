import { Vector2, Vector3 } from "three"
import { createTerrainHeight, strategySearch } from "./terrain"

class Circle {
    pos: Vector2 = new Vector2()
    radius: number
    startAngle: number
    endAngle: number
    usedTangent?: Vector2
    color: string
    tangent: { start: Vector2; end: Vector2 }

    constructor(x: number, y: number, radius: number, color: string) {
        this.pos.x = x
        this.pos.y = y
        this.radius = radius
        this.color = color
    }
}

class DirPoint {
    pos: Vector2
    dir: Vector2

    constructor(pos: Vector2, dir: Vector2) {
        this.pos = pos
        this.dir = dir
    }
}

export class Canvas {
    canvasElement: HTMLCanvasElement;
    context: CanvasRenderingContext2D;

    constructor(width: number, height: number, id: string, append: boolean) {
        this.canvasElement = document.createElement('canvas') as HTMLCanvasElement;
        this.canvasElement.id = id
        this.canvasElement.width = width;
        this.canvasElement.height = height;
        this.context = this.canvasElement.getContext('2d')!;

        if (!this.context) {
            throw new Error('Failed to get 2D rendering context');
        }

        if (append) {
            document.getElementById('ui').appendChild(this.canvasElement);
        }
    }

    width(): number {
        return this.canvasElement.width
    }
    height(): number {
        return this.canvasElement.height
    }

    getImageData(): ImageData {
        return this.context.getImageData(0, 0, this.canvasElement.width, this.canvasElement.height)
    }

    clearRect() {
        this.context.clearRect(0, 0, this.width(), this.height());
    }

    blur(blur: number) {
        this.context.filter = `blur(${blur}px)`;
    }

    drawLine(point1: Vector2, point2: Vector2, lineWidth: number, color: string): void {
        this.context.beginPath();
        this.context.moveTo(point1.x, point1.y);
        this.context.lineTo(point2.x, point2.y);
        this.context.strokeStyle = color;
        this.context.lineWidth = lineWidth
        this.context.stroke();
    }

    drawGrid(
        cellSize: number,
        lineWidth: number,
        color: string
    ): void {
        const { width, height } = this.canvasElement;

        // Draw vertical lines
        for (let x = 0; x <= width; x += cellSize) {
            this.drawLine(new Vector2(x, 0), new Vector2(x, height), lineWidth, color);
        }

        // Draw horizontal lines
        for (let y = 0; y <= height; y += cellSize) {
            this.drawLine(new Vector2(0, y), new Vector2(width, y), lineWidth, color);
        }
    }

    sampleLine(point1: Vector2, point2: Vector2, sampleDistance: number): DirPoint[] {
        const len = point1.distanceTo(point2);

        let n = Math.floor(len / sampleDistance)
        // if (n < 2) {
        //     throw new Error("The number of samples (n) must be at least 2.");
        // }

        const samples: DirPoint[] = [];
        const offset = len / n;
        const direction = point2.sub(point1).normalize();

        for (let i = 0; i < n; i++) {
            samples.push(
                new DirPoint(
                    point1.clone().add(direction.clone().multiplyScalar((i + 0.5) * offset)),
                    direction,
                )
            );
        }

        return samples;
    }

    drawCircle(center: Vector2, radius: number, fill: boolean, lineWidth: number, color: string, dashLength: number | null): void {
        this.context.beginPath();
        if (dashLength) {
            this.context.setLineDash([dashLength, dashLength]);
        }
        this.context.arc(center.x, center.y, radius, 0, 2 * Math.PI);
        this.context.lineWidth = lineWidth
        if (fill) {
            this.context.fillStyle = color;
            this.context.fill();
        } else {
            this.context.strokeStyle = color;
            this.context.stroke()
        }
        if (dashLength) {
            this.context.setLineDash([]);
        }
    }

    drawArc(center: Vector2, radius: number, startAngle: number, endAngle: number, lineWidth: number, color: string): void {
        this.context.beginPath();
        this.context.arc(center.x, center.y, radius, startAngle, endAngle);
        this.context.strokeStyle = color;
        this.context.lineWidth = lineWidth
        this.context.stroke();
    }

    sampleArc(center: Vector2, radius: number, startAngle: number, endAngle: number, sampleDistance: number): DirPoint[] {

        let angleDiff = (endAngle - startAngle) > 0 ? (endAngle - startAngle) : 2 * Math.PI + (endAngle - startAngle)
        let len = radius * angleDiff
        let n = Math.floor(len / sampleDistance)
        // if (n < 2) {
        //     throw new Error("The number of samples (n) must be at least 2.");
        // }

        const samples: DirPoint[] = [];
        let angleStep = angleDiff / n;

        for (let i = 0; i < n; i++) {
            const angle = startAngle + (i + 0.5) * angleStep;
            const x = center.x + radius * Math.cos(angle);
            const y = center.y + radius * Math.sin(angle);

            // Calculate tangent vector
            const tx = -Math.sin(angle);
            const ty = Math.cos(angle);

            // Create a normalized tangent vector
            const tangentVector = new Vector2(tx, ty).normalize();

            samples.push(new DirPoint(
                new Vector2(x, y),
                tangentVector,
            ));
        }

        return samples.reverse();
    }

    drawRectangle(topLeft: Vector2, width: number, height: number, lineWidth: number, color: string): void {
        this.context.fillStyle = color;
        this.context.lineWidth = lineWidth

        this.context.fillRect(topLeft.x, topLeft.y, width, height);
    }

    drawTriangle(center: Vector2, dw: number, dh: number, rotation: number, lineWidth: number, color: string): void {
        const ctx = this.context;

        // Save the context state
        ctx.save();

        // Translate to the triangle's center
        ctx.translate(center.x, center.y);

        // Rotate the context
        ctx.rotate(rotation);

        // Set styles
        ctx.fillStyle = color;
        ctx.lineWidth = lineWidth;

        // Draw the triangle
        ctx.beginPath();
        ctx.moveTo(0, -dh); // Top vertex
        ctx.lineTo(-dw, dh); // Bottom-left vertex
        ctx.lineTo(dw, dh); // Bottom-right vertex
        ctx.closePath();

        // Fill and stroke the triangle
        ctx.fill();
        if (lineWidth) {
            ctx.stroke();
        }

        // Restore the context state
        ctx.restore();
    }

    drawEllipse(center: Vector2, radiusX: number, radiusY: number, rotation: number, startAngle: number, endAngle: number, lineWidth: number, color: string): void {
        this.context.beginPath();
        this.context.ellipse(center.x, center.y, radiusX, radiusY, rotation, startAngle, endAngle);
        this.context.fillStyle = color;
        this.context.lineWidth = lineWidth
        this.context.fill();
    }

    drawText(text: string, position: Vector2, font: number, color: string): void {
        this.context.font = font + "px Monospace"
        this.context.fillStyle = color;
        this.context.fillText(text, position.x, position.y);
    }
}

export class Track {
    circles: Circle[] = new Array<Circle>();

    canvas: Canvas

    isDragging: boolean
    isResizing: boolean

    selectedCircle: Circle

    minRadius: number
    maxRadius: number
    numCircle: number

    trackWidth: number
    trackColor255: number

    debugDraw: boolean

    gridSize: number
    grid: Array<string | null>

    sampleDistance: number
    samples: DirPoint[] = []

    constructor(width: number, height: number, numCircle: number, minRadius: number, maxRadius: number, trackWidth: number, trackColor255: number, blur: number, sampleDistance: number, gridSize: number, debugDraw: boolean) {

        this.canvas = new Canvas(width, height, 'Track', this.debugDraw)

        this.canvas.blur(blur)

        this.minRadius = minRadius
        this.maxRadius = maxRadius
        this.numCircle = numCircle

        this.trackWidth = trackWidth
        this.trackColor255 = trackColor255

        this.sampleDistance = sampleDistance

        this.gridSize = gridSize
        this.debugDraw = debugDraw

        let numTry = 0
        while (numTry <= 1000) {
            if (numTry == 1000) {
                numTry = 0
                this.numCircle--
            }
            try {
                numTry++
                this.NewTrack()
                this.drawTrack()
                break
            } catch (error) {
                console.log(numTry)
            }
        }

        this.addListeners()
    }


    NewTrack() {
        const gridXNum = this.canvas.width() / this.gridSize;
        const gridYNum = this.canvas.height() / this.gridSize;
        this.grid = new Array(Math.ceil(gridXNum) * Math.ceil(gridYNum)).fill(null);

        this.circles = [];
        let attempts = 0;
        for (let i = 0; i < this.numCircle; i++) {
            let placed = false;

            while (!placed) {
                if (attempts > 1000) {
                    console.warn("Could not place all circles without overlap.");
                    break;
                }

                let radius = Math.random() * (this.maxRadius - this.minRadius) + this.minRadius; // Random radius between 40 and 100
                let x = Math.random() * (this.canvas.width() - 4 * radius) + 2 * radius; // Random X position
                let y = Math.random() * (this.canvas.height() - 4 * radius) + 2 * radius; // Random Y position

                // Calculate the cells occupied by the circle
                const startX = Math.floor((x - radius) / this.gridSize);
                const startY = Math.floor((y - radius) / this.gridSize);
                const endX = Math.ceil((x + radius) / this.gridSize);
                const endY = Math.ceil((y + radius) / this.gridSize);

                let canPlace = true;
                for (let gx = startX; gx < endX; gx++) {
                    for (let gy = startY; gy < endY; gy++) {
                        const index = gy * Math.ceil(gridXNum) + gx;
                        if (this.grid[index]) {
                            canPlace = false;
                            break;
                        }
                    }
                    if (!canPlace) break;
                }

                if (canPlace) {

                    let color = `rgba(${Math.random() * 155}, ${Math.random() * 155}, ${Math.random() * 155}, ${0.2})`;

                    // Mark the cells as occupied
                    for (let gx = startX; gx < endX; gx++) {
                        for (let gy = startY; gy < endY; gy++) {
                            const index = gy * Math.ceil(gridXNum) + gx;
                            this.grid[index] = color;
                        }
                    }

                    // Place the circle
                    this.circles.push(new Circle(x, y, radius, color));
                    placed = true;
                }

                attempts++;
            }
        }
    }

    checkGridLine(start: Vector2, end: Vector2, currColor: string, nextColor: string) {
        let x0 = Math.floor(start.x / this.gridSize);
        let y0 = Math.floor(start.y / this.gridSize);
        const x1 = Math.floor(end.x / this.gridSize);
        const y1 = Math.floor(end.y / this.gridSize);

        const deltaX = Math.abs(x1 - x0);
        const deltaY = Math.abs(y1 - y0);
        const signX = x0 < x1 ? 1 : -1;
        const signY = y0 < y1 ? 1 : -1;

        const gridXNum = this.canvas.width() / this.gridSize;

        let error = deltaX - deltaY;

        while (true) {
            // Calculate rectangle position based on grid cell
            const rectX = x0 * this.gridSize;
            const rectY = y0 * this.gridSize;

            const index = y0 * Math.ceil(gridXNum) + x0;

            if (this.grid[index] == currColor || this.grid[index] == nextColor || this.grid[index] === null) {
                if (this.debugDraw) {
                    this.canvas.drawRectangle(
                        new Vector2(rectX, rectY),
                        this.gridSize,
                        this.gridSize,
                        10,
                        `rgb(200,200,200)`
                    );
                }
            }
            else {
                if (this.debugDraw) {
                    this.canvas.drawRectangle(
                        new Vector2(rectX, rectY),
                        this.gridSize,
                        this.gridSize,
                        10,
                        `rgb(200,0,0)`
                    );
                }
                throw new Error('Collapse');
            }

            // Stop if the line has reached its endpoint
            if (x0 === x1 && y0 === y1) break;

            const error2 = error * 2;
            if (error2 > -deltaY) {
                error -= deltaY;
                x0 += signX;
            }
            if (error2 < deltaX) {
                error += deltaX;
                y0 += signY;
            }
        }
    }

    fillGridCircle(circle: Circle) {

        const gridXNum = this.canvas.width() / this.gridSize;

        const startX = Math.floor((circle.pos.x - circle.radius) / this.gridSize);
        const startY = Math.floor((circle.pos.y - circle.radius) / this.gridSize);
        const endX = Math.ceil((circle.pos.x + circle.radius) / this.gridSize);
        const endY = Math.ceil((circle.pos.y + circle.radius) / this.gridSize);

        for (let gx = startX; gx < endX; gx++) {
            for (let gy = startY; gy < endY; gy++) {

                // const rectX = gx * this.gridSize;
                // const rectY = gy * this.gridSize;

                const index = gy * Math.ceil(gridXNum) + gx;
                this.grid[index] = circle.color

                // this.canvas.drawRectangle(new Vector2(rectX, rectY), this.gridSize, this.gridSize, 10, circle.color);
            }
        }
    }

    debugGrid() {
        const gridXNum = this.canvas.width() / this.gridSize;
        const gridYNum = this.canvas.height() / this.gridSize;

        for (let gx = 0; gx < gridXNum; gx++) {
            for (let gy = 0; gy < gridYNum; gy++) {

                const rectX = gx * this.gridSize;
                const rectY = gy * this.gridSize;

                const index = gy * Math.ceil(gridXNum) + gx;

                if (this.debugDraw && this.grid[index]) {
                    this.canvas.drawRectangle(new Vector2(rectX, rectY), this.gridSize, this.gridSize, 10, this.grid[index]);
                }
            }
        }
    }

    // Function to calculate tangents and return their points
    calculateTangents(x1, y1, r1, x2, y2, r2) {
        let dx = x2 - x1;
        let dy = y2 - y1;
        let d = Math.sqrt(dx * dx + dy * dy); // Distance between circle centers
        let tangents = new Array<{ start: Vector2, end: Vector2 }>(); // Array to store tangent points

        if (d > Math.abs(r1 - r2)) {
            // Calculate the angle between the centers
            let angle = Math.atan2(dy, dx);

            // External tangents
            let deltaExt = Math.acos((r1 - r2) / d); // Angle adjustment for external tangents

            // First external tangent
            tangents.push({
                start: new Vector2(
                    x1 + r1 * Math.cos(angle + deltaExt),
                    y1 + r1 * Math.sin(angle + deltaExt),
                ),
                end: new Vector2(
                    x2 + r2 * Math.cos(angle + deltaExt),
                    y2 + r2 * Math.sin(angle + deltaExt),
                ),
            });

            // Second external tangent
            tangents.push({
                start: new Vector2(
                    x1 + r1 * Math.cos(angle - deltaExt),
                    y1 + r1 * Math.sin(angle - deltaExt),
                ),
                end: new Vector2(
                    x2 + r2 * Math.cos(angle - deltaExt),
                    y2 + r2 * Math.sin(angle - deltaExt),
                ),
            });
        }

        return tangents;
    }

    // Function to choose a valid tangent (does not reuse the previous one)
    chooseValidTangent(
        tangents: {
            start: Vector2;
            end: Vector2;
        }[],
        usedTangent?: Vector2
    ) {
        for (let tangent of tangents) {
            if (usedTangent && (tangent.start.x === usedTangent.x && tangent.start.y === usedTangent.y)) {
                continue; // Skip if it's the same as the previous tangent's start point
            }
            return tangent; // Return the first valid tangent
        }
        return null; // No valid tangent found
    }

    drawTrack() {
        this.canvas.clearRect()

        this.samples = []

        if (this.debugDraw) {
            this.canvas.drawGrid(this.gridSize, 2, `rgb(230,230,230)`)
        }
        this.debugGrid()

        for (let i = 0; i < this.circles.length; i++) {

            let curr = this.circles[i];
            let next = this.circles[(i + 1) % this.circles.length]; // Wrap around to connect last to first

            // Calculate all tangents
            let tangents = this.calculateTangents(curr.pos.x, curr.pos.y, curr.radius, next.pos.x, next.pos.y, next.radius);

            // Choose a tangent that doesn't reuse the previous point
            let tangent = this.chooseValidTangent(tangents, curr.usedTangent);
            curr.tangent = tangent
            if (!tangent) {
                console.log('No Tangent');
                return;
            }

            this.checkGridLine(tangent.start, tangent.end, curr.color, next.color)
            this.canvas.drawLine(tangent.start, tangent.end, this.trackWidth, `rgb(${this.trackColor255}, ${this.trackColor255}, ${this.trackColor255})`);

            // Angle at the start tangent point
            let angle = Math.atan2(tangent.start.y - curr.pos.y, tangent.start.x - curr.pos.x) * 180 / Math.PI;

            if (!curr.startAngle) {
                curr.startAngle = angle + (angle < 0 ? 360 : 0)
            } else {
                curr.endAngle = angle + (angle < 0 ? 360 : 0)
            }
            if (!next.startAngle) {
                next.startAngle = angle + (angle < 0 ? 360 : 0);
            } else {
                next.endAngle = angle + (angle < 0 ? 360 : 0);
            }

            // Mark the tangent as used for the next circle
            curr.usedTangent = tangent.start; // Store the start point of the tangent
            next.usedTangent = tangent.end; // Ensure a different tangent for the next circle
        }

        {
            let s = this.circles[0].startAngle
            this.circles[0].startAngle = this.circles[0].endAngle
            this.circles[0].endAngle = s
        }

        // Display angle differences at circle centers
        for (let i = 0; i < this.circles.length; i++) {
            let curr = this.circles[i];
            let angleEnd = curr.startAngle * Math.PI / 180;
            let angleStart = curr.endAngle * Math.PI / 180;

            // Draw the arc using the Canvas API
            this.canvas.drawArc(curr.pos, curr.radius, angleStart, angleEnd, this.trackWidth, `rgb(${this.trackColor255}, ${this.trackColor255}, ${this.trackColor255})`)

            const arcSamples = this.canvas.sampleArc(curr.pos, curr.radius, angleStart, angleEnd, this.sampleDistance)
            this.samples.push(...arcSamples)

            const lineSamples = this.canvas.sampleLine(curr.tangent.start, curr.tangent.end, this.sampleDistance)
            this.samples.push(...lineSamples)

        }

        if (this.debugDraw) {
            for (let i = 0; i < this.samples.length; i++) {
                let dirpoint = this.samples[i]
                this.canvas.drawCircle(dirpoint.pos, 2, true, 1, 'rgb(30,30,30)', null)
                this.canvas.drawLine(dirpoint.pos, dirpoint.pos.clone().add(dirpoint.dir.clone().multiplyScalar(10)), 1, `rgb(100,0,0)`)
                this.canvas.drawText(`${i}`, dirpoint.pos, 12, 'rgb(30,30,30)')
            }
        }

    }

    addListeners(): void {
        this.canvas.canvasElement.addEventListener('mousedown', (event) => this.onMouseDown(event));
        this.canvas.canvasElement.addEventListener('mousemove', (event) => this.onMouseMove(event));
        this.canvas.canvasElement.addEventListener('mouseup', () => this.onMouseUp());
    }

    onMouseDown(event: MouseEvent): void {
        const mousePos = this.getMousePosition(event);
        const circle = this.findCircleAtPosition(mousePos);

        if (circle) {
            const distance = this.getDistance(mousePos, new Vector2(circle.pos.x, circle.pos.y));

            if (distance <= circle.radius) {
                this.selectedCircle = circle;
                this.isDragging = true;
            } else if (Math.abs(distance - circle.radius) <= 30) {
                this.selectedCircle = circle;
                this.isResizing = true;
            }
        }
    }

    onMouseMove(event: MouseEvent): void {
        if (!this.selectedCircle) return;

        const mousePos = this.getMousePosition(event);

        if (this.isDragging) {
            this.selectedCircle.pos.x = mousePos.x;
            this.selectedCircle.pos.y = mousePos.y;
        } else if (this.isResizing) {
            this.selectedCircle.radius = this.getDistance(mousePos, new Vector2(this.selectedCircle.pos.x, this.selectedCircle.pos.y));
        }

        this.grid.fill(null)

        for (const c of this.circles) {
            c.startAngle = null
            c.endAngle = null

            this.fillGridCircle(c)
        }

        this.drawTrack()
    }

    onMouseUp(): void {
        this.isDragging = false;
        this.isResizing = false;
        this.selectedCircle = null;
    }

    findCircleAtPosition(position: Vector2): Circle | null {
        for (const circle of this.circles) {
            const distance = this.getDistance(position, new Vector2(circle.pos.x, circle.pos.y));
            if (distance <= circle.radius + 5) {
                return circle;
            }
        }
        return null;
    }

    getMousePosition(event: MouseEvent): Vector2 {
        const rect = this.canvas.canvasElement.getBoundingClientRect();
        return new Vector2(
            event.clientX - rect.left,
            event.clientY - rect.top
        )
    }

    getDistance(point1: Vector2, point2: Vector2): number {
        return Math.sqrt(Math.pow(point2.x - point1.x, 2) + Math.pow(point2.y - point1.y, 2));
    }
}










function visualizeHeightMap(
    heightMap: Array<number> | Float32Array<ArrayBufferLike>,
    width: number,
    height: number,
    blur: number = 0,
    contour: boolean = false,
    grad?: (c: number) => { r: number; g: number; b: number; }
): ImageData {
    const canvas = document.createElement('canvas');
    const ctx = canvas.getContext('2d');

    if (!ctx) {
        console.error('Unable to get 2D context.');
        return;
    }

    // Set canvas dimensions
    canvas.width = width;
    canvas.height = height;

    // Create ImageData
    const imageData = ctx.createImageData(width, height);
    const data = imageData.data; // Pixel data array (RGBA)

    // Map the heightMap values to RGBA
    for (let i = 0; i < heightMap.length; i++) {
        const normalizedValue = Math.floor((heightMap[i] + 1) * 127.5);

        if (grad) {
            let c = grad(heightMap[i])
            // Fill RGBA channels for the pixel
            data[i * 4] = c.r; // Red
            data[i * 4 + 1] = c.g; // Green
            data[i * 4 + 2] = c.b; // Blue
        } else {
            data[i * 4] = normalizedValue; // Red
            data[i * 4 + 1] = normalizedValue; // Green
            data[i * 4 + 2] = normalizedValue; // Blue
        }
        data[i * 4 + 3] = 255; // Alpha
    }

    // Create a temporary canvas to hold the ImageData
    const tempCanvas = document.createElement('canvas');
    tempCanvas.width = imageData.width;
    tempCanvas.height = imageData.height;
    const tempCtx = tempCanvas.getContext('2d');

    // Put the ImageData onto the temporary canvas
    tempCtx.putImageData(imageData, 0, 0);

    // Apply the filter and draw the temporary canvas onto the final scaled canvas
    ctx.filter = `blur(${blur}px)`;

    // If no scaling, just put the original ImageData on the canvas
    ctx.drawImage(tempCanvas, 0, 0);

    if (contour) {

        let newImageData = ctx.getImageData(0, 0, imageData.width, imageData.height)
        for (let row = 0; row < imageData.height; row++) {
            for (let col = 0; col < imageData.width; col++) {
                let index = row * imageData.width * 4 + col * 4
                let d = newImageData.data[index] - imageData.data[index] != 0 ? 100 : 0
                newImageData.data[index] = d; // Red
                newImageData.data[index + 1] = d; // Green
                newImageData.data[index + 2] = d; // Blue
            }
        }
        ctx.putImageData(newImageData, 0, 0);
    }


    // Append the canvas to the body
    // document.getElementById('ui').appendChild(canvas);

    return ctx.getImageData(0, 0, imageData.width, imageData.height)
}


const noiseScale = .4
const lowresSegments = 400
const s = 10
const scale = new Vector3(s, 1, s)

let rows = 1, cols = 1

let lowresHeightMap = new Array<number>(rows * cols * lowresSegments * lowresSegments).fill(.5)

let biomeMap = createTerrainHeight(
    lowresSegments,
    scale,
    new Vector2(0, 0),
    [
        { seed: 1700, noiseScale: 0.1 },
    ],
    'ADDITIVE'
)
let biomeFunction = strategySearch(-.5, -.1, .5)
let biomeImageData = visualizeHeightMap(biomeMap, lowresSegments, lowresSegments, 2, true, biomeFunction);

console.log(biomeMap)

for (let row = 0; row < rows; row++) {
    for (let col = 0; col < cols; col++) {

        let strategy: 'MIN' | 'MAX' | 'ADDITIVE' = 'ADDITIVE'

        let lowresHeights = createTerrainHeight(
            lowresSegments,
            scale,
            new Vector2(row, col),
            [
                { seed: 17, noiseScale },
                { seed: 1700, noiseScale },
                { seed: 17000, noiseScale },
            ],
            'ADDITIVE',
            biomeMap,
            biomeFunction,
        )

        for (let i = 0; i < lowresSegments; i++) {
            for (let j = 0; j < lowresSegments; j++) {
                lowresHeightMap[
                    col * lowresSegments + i +
                    (cols * lowresSegments) * (row * lowresSegments + j - 1)
                ] = lowresHeights[i + j * lowresSegments];
            }
        }
    }
}



const width = lowresSegments * cols; // Overall width
const height = lowresSegments * rows; // Overall height

let heightMapImageData = visualizeHeightMap(lowresHeightMap, width, height);


function maskBlurImageData(srcImageData: ImageData, maskImageData: ImageData, radius: number) {
    const width = srcImageData.width;
    const height = srcImageData.height;

    const srcData = srcImageData.data;
    const maskData = maskImageData.data;
    const resultImageData = new ImageData(width, height);
    const resultData = resultImageData.data;

    // Helper function: Apply a box blur around a pixel
    function getBlurredValue(x, y, channel) {
        let sum = 0, count = 0;

        for (let dy = -radius; dy <= radius; dy++) {
            for (let dx = -radius; dx <= radius; dx++) {
                const nx = x + dx;
                const ny = y + dy;

                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                    const index = (ny * width + nx) * 4 + channel;
                    sum += srcData[index];
                    count++;
                }
            }
        }

        return count > 0 ? sum / count : 0;
    }

    // Iterate through each pixel
    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            const index = (y * width + x) * 4;

            // Check the mask value
            const mask = maskData[index]; // Using red channel of mask
            if (mask > 0) {
                // Apply blur only if the mask is active
                for (let channel = 0; channel < 3; channel++) { // R, G, B channels
                    resultData[index + channel] = getBlurredValue(x, y, channel);
                }
            } else {
                // Use original source value if mask is not active
                for (let channel = 0; channel < 3; channel++) { // R, G, B channels
                    resultData[index + channel] = srcData[index + channel];
                }
            }

            // Set alpha channel to fully opaque
            resultData[index + 3] = 255;
        }
    }

    return resultImageData;
}

const MaskCanvas = new Canvas(width, height, 'MaskCanvas', false)
const blendedImageData = maskBlurImageData(heightMapImageData, biomeImageData, 5);
MaskCanvas.context.putImageData(blendedImageData, 0, 0)

const track = new Track(
    lowresSegments, lowresSegments,
    8,
    20, 50,
    10,
    150,
    0,
    25,
    10,
    false
);
let raceTrackHeights = track.canvas.getImageData()

export const RaceTrackBlendCanvas = new Canvas(width, height, 'RaceTrackBlendCanvas', true)
const raceTrackBlendedImageData = maskBlurImageData(blendedImageData, raceTrackHeights, 5);
RaceTrackBlendCanvas.context.putImageData(raceTrackBlendedImageData, 0, 0)

function mapImageDataRange(imageData: ImageData, h: number) {
    const data = imageData.data;
    const result = new Float32Array(data.length / 4); // Use Float32Array for the mapped data

    for (let i = 0; i < data.length; i++) {
        // Map each value from [0, 255] to [-h, h]
        result[i] = (data[i * 4] / 255) * (2 * h) - h;
    }

    return result;
}

export const raceTrackMap = mapImageDataRange(raceTrackBlendedImageData, .5)


export const RaceTrackMinimapCanvas = new Canvas(200, 200, 'RaceTrackMinimapCanvas', false)
RaceTrackMinimapCanvas.canvasElement.style.borderRadius = '50%'

// // main.ts
// type WorkerResponse = {
//     result: number;
// };

// const worker = new Worker(new URL('./worker.ts', import.meta.url));

// worker.onmessage = (event: MessageEvent<WorkerResponse>) => {
//     console.log('Result from worker:', event.data.result);
// };

// worker.postMessage({ type: 'multiply', payload: 21 });
