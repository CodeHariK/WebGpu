import { Vector2 } from "three"

class Circle {
    pos: Vector2 = new Vector2()
    radius: number
    startAngle: number
    endAngle: number
    usedTangent?: Vector2

    constructor(x: number, y: number, radius: number) {
        this.pos.x = x
        this.pos.y = y
        this.radius = radius
    }
}

export class Canvas {
    canvasElement: HTMLCanvasElement;
    context: CanvasRenderingContext2D;

    constructor(width: number, height: number) {
        this.canvasElement = document.createElement('canvas') as HTMLCanvasElement;
        this.canvasElement.width = width;
        this.canvasElement.height = height;
        this.context = this.canvasElement.getContext('2d')!;

        if (!this.context) {
            throw new Error('Failed to get 2D rendering context');
        }

        // document.body.appendChild(this.canvasElement);
    }

    width(): number {
        return this.canvasElement.width
    }
    height(): number {
        return this.canvasElement.height
    }

    getImageData(normalized: boolean): Float32Array<ArrayBuffer> {
        const data = new Array<number>(this.canvasElement.width * this.canvasElement.height)
        const img = this.context.getImageData(0, 0, this.canvasElement.width, this.canvasElement.height).data

        for (let i = 0; i < this.canvasElement.width * this.canvasElement.height; i++) {
            if (normalized) {
                data[i] = img[4 * i] / 255
            } else {
                data[i] = img[4 * i]
            }
        }

        return new Float32Array(data)
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

    sampleLine(point1: Vector2, point2: Vector2, n: number): Vector2[] {
        const distance = point1.distanceTo(point2);

        if (n < 2) {
            throw new Error("The number of samples (n) must be at least 2.");
        }

        const samples: Vector2[] = [];
        const offset = distance / (n - 1);
        const direction = point2.sub(point1).normalize();

        for (let i = 0; i < n; i++) {
            samples.push(point1.clone().add(direction.clone().multiplyScalar(i * offset)));
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

    sampleArc(center: Vector2, radius: number, startAngle: number, endAngle: number, n: number): Vector2[] {
        if (n < 2) {
            throw new Error("The number of samples (n) must be at least 2.");
        }

        const samples: Vector2[] = [];
        let angleStep = ((endAngle - startAngle) / (n - 1));

        angleStep = angleStep + (angleStep > 0 ? 0 : Math.PI * 2 / (n - 1))

        for (let i = 0; i < n; i++) {
            const angle = startAngle + i * angleStep;
            const x = center.x + radius * Math.cos(angle);
            const y = center.y + radius * Math.sin(angle);
            samples.push(new Vector2(x, y));
        }

        return samples;
    }

    drawRectangle(topLeft: Vector2, width: number, height: number, lineWidth: number, color: string): void {
        this.context.fillStyle = color;
        this.context.lineWidth = lineWidth

        this.context.fillRect(topLeft.x, topLeft.y, width, height);
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
    trackHeight: number

    constructor(width: number, height: number, numCircle: number, minRadius: number, maxRadius: number, trackWidth: number, trackHeight: number, blur: number) {

        this.canvas = new Canvas(width, height)

        this.canvas.blur(blur)

        this.minRadius = minRadius
        this.maxRadius = maxRadius
        this.numCircle = numCircle

        this.trackWidth = trackWidth
        this.trackHeight = trackHeight

        this.NewTrack()

        this.addListeners()

        this.drawTrack()
    }

    NewTrack() {
        this.circles = []
        for (let i = 0; i < this.numCircle; i++) {
            let radius = Math.random() * (this.maxRadius - this.minRadius) + this.minRadius; // Random radius between 40 and 100
            let x = Math.random() * (this.canvas.width() - 200) + 100; // Random X position
            let y = Math.random() * (this.canvas.height() - 200) + 100; // Random Y position
            this.circles.push(new Circle(x, y, radius));
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
    chooseValidTangent(tangents: any[], usedTangent?: Vector2) {
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

        for (let i = 0; i < this.circles.length; i++) {
            let curr = this.circles[i];
            let next = this.circles[(i + 1) % this.circles.length]; // Wrap around to connect last to first

            // Calculate all tangents
            let tangents = this.calculateTangents(curr.pos.x, curr.pos.y, curr.radius, next.pos.x, next.pos.y, next.radius);

            // Choose a tangent that doesn't reuse the previous point
            let tangent = this.chooseValidTangent(tangents, curr.usedTangent);
            if (!tangent) {
                console.log('No Tangent');
                return;
            }

            this.canvas.drawLine(tangent.start, tangent.end, this.trackWidth, `rgb(${this.trackHeight}, ${this.trackHeight}, ${this.trackHeight})`)

            // for (let i of this.canvas.sampleLine(tangent.start, tangent.end, 20)) {
            //     this.canvas.drawCircle(i, 2, true, 1, 'rgb(30,30,30)', null)
            // }

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
            this.canvas.drawArc(curr.pos, curr.radius, angleStart, angleEnd, this.trackWidth, `rgb(${this.trackHeight}, ${this.trackHeight}, ${this.trackHeight})`)
            // this.canvas.drawCircle(curr.pos, curr.radius, false, lineWidth, `rgb(${height}, ${height}, ${height})`, 4)

            // for (let i of this.canvas.sampleArc(curr.pos, curr.radius, angleStart, angleEnd, 20)) {
            //     this.canvas.drawCircle(i, 2, true, 1, 'rgb(30,30,30)', null)
            //     this.canvas.drawText(`s:${curr.startAngle.toFixed(2)}, e: ${curr.endAngle.toFixed(2)}`, curr.pos, 16, 'rgb(30,30,30)')
            // }
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

        for (const c of this.circles) {
            c.startAngle = null
            c.endAngle = null
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
