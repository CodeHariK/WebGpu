import { Vector2 } from './Vector2';

export interface DrawOptions {
    stroke?: string;
    fill?: string;
    lineWidth?: number;
    dashed?: boolean;
    dashPattern?: number[];
    alpha?: number;
}

export class Canvas {
    public readonly element: HTMLCanvasElement;
    public readonly ctx: CanvasRenderingContext2D;
    private _zoom: number = 1.0;
    private _offset: Vector2 = new Vector2(0, 0);

    constructor(canvasElement: HTMLCanvasElement) {
        this.element = canvasElement;
        const context = this.element.getContext('2d');
        if (!context) throw new Error('Could not get 2D context from canvas element.');
        this.ctx = context;
    }

    public get width(): number { return this.element.width; }
    public get height(): number { return this.element.height; }

    public get zoom(): number { return this._zoom; }
    public set zoom(value: number) { this._zoom = value; }

    public get offset(): Vector2 { return this._offset; }
    public set offset(value: Vector2) { this._offset = value; }

    public clear(): void {
        this.ctx.setTransform(1, 0, 0, 1, 0, 0);
        this.ctx.clearRect(0, 0, this.width, this.height);
        this.ctx.scale(this._zoom, this._zoom);
        this.ctx.translate(this._offset.x, this._offset.y);
    }

    private applyOptions(options?: DrawOptions): void {
        this.ctx.strokeStyle = options?.stroke ?? '#000';
        this.ctx.fillStyle = options?.fill ?? 'transparent';
        this.ctx.lineWidth = options?.lineWidth ?? 1;
        this.ctx.globalAlpha = options?.alpha ?? 1;

        if (options?.dashed) {
            this.ctx.setLineDash(options.dashPattern ?? [5, 5]);
        } else {
            this.ctx.setLineDash([]);
        }
    }

    public save(): void {
        this.ctx.save();
    }

    public restore(): void {
        this.ctx.restore();
    }

    public translate(offset: Vector2): void {
        this.ctx.translate(offset.x, offset.y);
    }

    public rotate(angle: number): void {
        this.ctx.rotate(angle);
    }

    public setTransform(a: number, b: number, c: number, d: number, e: number, f: number): void {
        this.ctx.setTransform(a, b, c, d, e, f);
    }

    public line(p1: Vector2, p2: Vector2, options?: DrawOptions): void {
        this.applyOptions(options);
        this.ctx.beginPath();
        this.ctx.moveTo(p1.x, p1.y);
        this.ctx.lineTo(p2.x, p2.y);
        this.ctx.stroke();
    }

    public linePath(points: Vector2[], options?: DrawOptions): void {
        if (points.length < 2) return;
        this.applyOptions(options);
        this.ctx.beginPath();
        this.ctx.moveTo(points[0].x, points[0].y);
        for (let i = 1; i < points.length; i++) {
            this.ctx.lineTo(points[i].x, points[i].y);
        }
        if (options?.fill) this.ctx.fill();
        if (options?.stroke && options.stroke !== 'transparent') this.ctx.stroke();
    }

    public quadBezier(p1: Vector2, cp: Vector2, p2: Vector2, options?: DrawOptions): void {
        this.applyOptions(options);
        this.ctx.beginPath();
        this.ctx.moveTo(p1.x, p1.y);
        this.ctx.quadraticCurveTo(cp.x, cp.y, p2.x, p2.y);
        if (options?.fill) this.ctx.fill();
        if (options?.stroke && options.stroke !== 'transparent') this.ctx.stroke();
    }

    public rect(x: number, y: number, w: number, h: number, options?: DrawOptions): void {
        this.applyOptions(options);
        this.ctx.beginPath();
        this.ctx.rect(x, y, w, h);
        if (options?.fill) this.ctx.fill();
        if (options?.stroke && options.stroke !== 'transparent') this.ctx.stroke();
    }

    public circle(center: Vector2, radius: number, options?: DrawOptions): void {
        this.applyOptions(options);
        this.ctx.beginPath();
        this.ctx.arc(center.x, center.y, radius, 0, Math.PI * 2);
        if (options?.fill) this.ctx.fill();
        if (options?.stroke && options.stroke !== 'transparent') this.ctx.stroke();
    }

    public capsule(p1: Vector2, p2: Vector2, radius: number, options?: DrawOptions): void {
        this.applyOptions(options);
        const dx = p2.x - p1.x;
        const dy = p2.y - p1.y;
        const angle = Math.atan2(dy, dx);

        this.ctx.beginPath();
        // Start with arc at p1
        this.ctx.arc(p1.x, p1.y, radius, angle + Math.PI / 2, angle - Math.PI / 2);
        // Line to side of p2
        // arc will automatically add a line from previous point
        // End with arc at p2
        this.ctx.arc(p2.x, p2.y, radius, angle - Math.PI / 2, angle + Math.PI / 2);
        this.ctx.closePath();

        if (options?.fill) this.ctx.fill();
        if (options?.stroke && options.stroke !== 'transparent') this.ctx.stroke();
    }

    public ellipse(center: Vector2, radiusX: number, radiusY: number, rotation: number = 0, options?: DrawOptions): void {
        this.applyOptions(options);
        this.ctx.beginPath();
        this.ctx.ellipse(center.x, center.y, radiusX, radiusY, rotation, 0, Math.PI * 2);
        if (options?.fill) this.ctx.fill();
        if (options?.stroke && options.stroke !== 'transparent') this.ctx.stroke();
    }

    public arc(center: Vector2, radius: number, startAngle: number, endAngle: number, anticlockwise: boolean = false, options?: DrawOptions): void {
        this.applyOptions(options);
        this.ctx.beginPath();
        this.ctx.arc(center.x, center.y, radius, startAngle, endAngle, anticlockwise);
        if (options?.fill) this.ctx.fill();
        if (options?.stroke && options.stroke !== 'transparent') this.ctx.stroke();
    }

    public triangle(p1: Vector2, p2: Vector2, p3: Vector2, options?: DrawOptions): void {
        this.polygon([p1, p2, p3], options);
    }

    public polygon(points: Vector2[], options?: DrawOptions): void {
        if (points.length < 2) return;
        this.applyOptions(options);
        this.ctx.beginPath();
        this.ctx.moveTo(points[0].x, points[0].y);
        for (let i = 1; i < points.length; i++) {
            this.ctx.lineTo(points[i].x, points[i].y);
        }
        this.ctx.closePath();
        if (options?.fill) this.ctx.fill();
        if (options?.stroke && options.stroke !== 'transparent') this.ctx.stroke();
    }

    public text(str: string, p: Vector2, options?: DrawOptions & { font?: string, align?: CanvasTextAlign }): void {
        this.applyOptions(options);
        this.ctx.font = options?.font ?? '14px Arial';
        this.ctx.textAlign = options?.align ?? 'left';
        if (options?.fill) this.ctx.fillText(str, p.x, p.y);
        if (options?.stroke && options.stroke !== 'transparent') this.ctx.strokeText(str, p.x, p.y);
    }

    public point(p: Vector2, radius: number = 3, options?: DrawOptions): void {
        this.circle(p, radius, { fill: options?.fill ?? options?.stroke, stroke: 'white', ...options });
    }

    public arrow(p1: Vector2, p2: Vector2, headSize: number = 10, options?: DrawOptions): void {
        this.line(p1, p2, options);

        const dir = p2.clone().sub(p1).normalize();
        const perp = new Vector2(-dir.y, dir.x).mult(headSize * 0.6);

        this.applyOptions(options);
        this.ctx.beginPath();
        this.ctx.moveTo(p2.x, p2.y);
        this.ctx.lineTo(p2.x - dir.x * headSize + perp.x, p2.y - dir.y * headSize + perp.y);
        this.ctx.moveTo(p2.x, p2.y);
        this.ctx.lineTo(p2.x - dir.x * headSize - perp.x, p2.y - dir.y * headSize - perp.y);
        this.ctx.stroke();
    }

    private _isPanning: number = -1; // -1: none, 1: middle click
    private _onInteraction?: () => void;

    private handleMouseDownBound = this.handleMouseDown.bind(this);
    private handleMouseMoveBound = this.handleMouseMove.bind(this);
    private handleMouseUpBound = this.handleMouseUp.bind(this);
    private handleWheelBound = this.handleWheel.bind(this);
    private handleContextMenuBound = this.handleContextMenu.bind(this);

    public get isPanning(): boolean { return this._isPanning === 1; }

    public getMousePos(e: MouseEvent): Vector2 {
        const rect = this.element.getBoundingClientRect();
        const scaleX = this.element.width / rect.width;
        const scaleY = this.element.height / rect.height;
        const x = (e.clientX - rect.left) * scaleX;
        const y = (e.clientY - rect.top) * scaleY;
        return new Vector2(
            x / this._zoom - this._offset.x,
            y / this._zoom - this._offset.y
        );
    }

    public initInteractions(options?: { onInteraction?: () => void }): void {
        this._onInteraction = options?.onInteraction;
        this.element.addEventListener('mousedown', this.handleMouseDownBound);
        this.element.addEventListener('wheel', this.handleWheelBound, { passive: false });
        this.element.addEventListener('contextmenu', this.handleContextMenuBound);
        window.addEventListener('mousemove', this.handleMouseMoveBound);
        window.addEventListener('mouseup', this.handleMouseUpBound);
    }

    public destroyInteractions(): void {
        this.element.removeEventListener('mousedown', this.handleMouseDownBound);
        this.element.removeEventListener('wheel', this.handleWheelBound);
        this.element.removeEventListener('contextmenu', this.handleContextMenuBound);
        window.removeEventListener('mousemove', this.handleMouseMoveBound);
        window.removeEventListener('mouseup', this.handleMouseUpBound);
    }

    private handleMouseDown(e: MouseEvent): void {
        if (e.button === 1) { // Middle click
            e.preventDefault();
            this._isPanning = 1;
            this._onInteraction?.();
        }
    }

    private handleMouseMove(e: MouseEvent): void {
        if (this._isPanning === 1) {
            const rect = this.element.getBoundingClientRect();
            const scaleX = this.element.width / rect.width;
            const scaleY = this.element.height / rect.height;

            this._offset.x += (e.movementX * scaleX) / this._zoom;
            this._offset.y += (e.movementY * scaleY) / this._zoom;
            this._onInteraction?.();
        }
    }

    private handleMouseUp(): void {
        if (this._isPanning === 1) {
            this._isPanning = -1;
            this._onInteraction?.();
        }
    }

    private handleWheel(e: WheelEvent): void {
        e.preventDefault();
        const delta = e.deltaY > 0 ? 0.9 : 1.1;
        this._zoom = Math.min(Math.max(this._zoom * delta, 0.2), 5.0);
        this._onInteraction?.();
    }

    private handleContextMenu(e: MouseEvent): void {
        if (this._isPanning === 1) e.preventDefault();
    }

    public grid(spacing: number = 50, color: string = 'rgba(255, 255, 255, 0.1)'): void {
        this.ctx.strokeStyle = color;
        this.ctx.lineWidth = 1;
        this.ctx.setLineDash([]);

        // Grid should fill the viewport even when zoomed and panned
        const scaledWidth = this.width / this._zoom;
        const scaledHeight = this.height / this._zoom;

        const startX = Math.floor(-this._offset.x / spacing) * spacing;
        const startY = Math.floor(-this._offset.y / spacing) * spacing;
        const endX = startX + scaledWidth + spacing;
        const endY = startY + scaledHeight + spacing;

        for (let x = startX; x <= endX; x += spacing) {
            this.ctx.beginPath();
            this.ctx.moveTo(x, startY);
            this.ctx.lineTo(x, endY);
            this.ctx.stroke();
        }

        for (let y = startY; y <= endY; y += spacing) {
            this.ctx.beginPath();
            this.ctx.moveTo(startX, y);
            this.ctx.lineTo(endX, y);
            this.ctx.stroke();
        }
    }
}
