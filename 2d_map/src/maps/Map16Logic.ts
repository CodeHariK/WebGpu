import { Vector2 } from '../lib/Vector2';

export interface PackedCircle {
    x: number;
    y: number;
    r: number;
}

export interface GridTile {
    x: number;
    y: number;
    c: PackedCircle[];
}

export interface PackedShape {
    type: 'R' | 'T' | 'C';
    x: number;
    y: number;
    scale: number;
    rotation: number;
    circles: PackedCircle[];
    sizeW?: number;
    sizeH?: number;
    side?: number;
    radius?: number;
}

export function mulberry32(a: number) {
    return function() {
        let t = a += 0x6D2B79F5;
        t = Math.imul(t ^ (t >>> 15), t | 1);
        t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
}

export class CirclePacker {
    wid: number;
    hei: number;
    gridDivs: number;
    pad: number;
    gridSizeX: number;
    gridSizeY: number;
    grid: GridTile[][] = [];
    items: PackedCircle[] = [];
    shapes: PackedShape[] = [];

    constructor(wid: number, hei: number, gridDivs: number, gridPadding: number) {
        this.wid = wid;
        this.hei = hei;
        this.gridDivs = gridDivs || 50;
        this.pad = gridPadding;
        this.gridSizeX = this.wid / this.gridDivs;
        this.gridSizeY = this.hei / this.gridDivs;
        this.generateGrid();
    }

    private generateGrid() {
        const grid: GridTile[][] = [];
        for (let x = 0; x < this.gridDivs; x++) {
            grid[x] = [];
            for (let y = 0; y < this.gridDivs; y++) {
                grid[x][y] = { x, y, c: [] };
            }
        }
        this.grid = grid;
    }

    getGridTilesAround(x: number, y: number, r: number): GridTile[] {
        const tl = [
            Math.floor((x - r - this.pad) / this.gridSizeX),
            Math.floor((y - r - this.pad) / this.gridSizeY),
        ];

        const br = [
            Math.floor((x + r + this.pad) / this.gridSizeX),
            Math.floor((y + r + this.pad) / this.gridSizeY),
        ];

        const tiles: GridTile[] = [];
        for (let i = tl[0]; i <= br[0]; i++) {
            for (let j = tl[1]; j <= br[1]; j++) {
                if (i < 0 || j < 0 || i >= this.gridDivs || j >= this.gridDivs) continue;
                tiles.push(this.grid[i][j]);
            }
        }
        return tiles;
    }

    circleDistance(c1: PackedCircle, c2: PackedCircle): number {
        const dx = c1.x - c2.x;
        const dy = c1.y - c2.y;
        return Math.sqrt(dx * dx + dy * dy) - (c1.r + c2.r);
    }

    tryToAddCircle(x: number, y: number, minRadius = 5, maxRadius = 200, actuallyAdd = true): PackedCircle | null {
        let c1: PackedCircle = { x, y, r: minRadius };

        while (true) {
            if (c1.x - c1.r < 0 || c1.x + c1.r > this.wid || c1.y - c1.r < 0 || c1.y + c1.r > this.hei) {
                return null;
            }

            const gridTiles = this.getGridTilesAround(x, y, c1.r);

            for (const tile of gridTiles) {
                for (const c2 of tile.c) {
                    const d = this.circleDistance(c1, c2);
                    if (d - this.pad < 0) {
                        if (c1.r === minRadius) {
                            return null;
                        } else {
                            if (actuallyAdd) {
                                this.finalizeCircle(c1, gridTiles);
                            }
                            return c1;
                        }
                    }
                }
            }

            c1.r += 1;
            if (c1.r > maxRadius) {
                if (actuallyAdd) {
                    this.finalizeCircle(c1, gridTiles);
                }
                return c1;
            }
        }
    }

    private finalizeCircle(c: PackedCircle, tiles: GridTile[]) {
        tiles.forEach(t => {
            this.grid[t.x][t.y].c.push(c);
        });
        this.items.push(c);
    }

    addCircleExplicitly(c: PackedCircle) {
        if (c.x - c.r < 0 || c.x + c.r > this.wid || c.y - c.r < 0 || c.y + c.r > this.hei) {
            return null;
        }
        const gridTiles = this.getGridTilesAround(c.x, c.y, c.r);
        this.finalizeCircle(c, gridTiles);
        return c;
    }

    tryToAddShape(circles: PackedCircle[], metadata: Omit<PackedShape, 'circles'>, actuallyAdd = true): PackedShape | null {
        for (const c of circles) {
            if (!this.tryToAddCircle(c.x, c.y, c.r, c.r, false)) {
                return null;
            }
        }
        const packed: PackedShape = { ...metadata, circles };
        if (actuallyAdd) {
            circles.forEach(c => this.addCircleExplicitly(c));
            this.shapes.push(packed);
        }
        return packed;
    }
}

export class RectangularShape {
    original: PackedCircle[] = [];
    circles: PackedCircle[] = [];
    sizeW: number;
    sizeH: number;

    constructor(sizeW: number, sizeH: number) {
        this.sizeW = sizeW;
        this.sizeH = sizeH;
        this.makeOriginal();
    }

    makeOriginal() {
        const w2 = this.sizeW / 2;
        const h2 = this.sizeH / 2;
        
        // Define vertices in clockwise order
        const pts = [
            { x: w2, y: -h2 },  // Top Right
            { x: -w2, y: -h2 }, // Top Left
            { x: -w2, y: h2 },  // Bottom Left
            { x: w2, y: h2 }    // Bottom Right
        ];

        for (let i = 0; i < 4; i++) {
            const p1 = pts[i];
            const p2 = pts[(i + 1) % 4];
            
            // Steps proportional to edge length to maintain density
            const dist = Math.sqrt(Math.pow(p1.x - p2.x, 2) + Math.pow(p1.y - p2.y, 2));
            const steps = Math.max(5, Math.floor(dist / (this.sizeW / 12)));

            for (let j = 0; j < steps; j++) {
                const ratio = j / steps;
                const xi = p1.x * (1 - ratio) + p2.x * ratio;
                const yi = p1.y * (1 - ratio) + p2.y * ratio;
                this.original.push({ x: xi, y: yi, r: this.sizeW / 24 });
            }
        }
        this.circles = this.original.map(c => ({ ...c }));
    }

    scaleRotateTranslate(scale: number, rotateRadians: number, tx: number, ty: number) {
        this.circles = this.original.map(c => {
            const sx = c.x * scale;
            const sy = c.y * scale;
            const r = c.r * scale;
            const rx = sx * Math.cos(rotateRadians) - sy * Math.sin(rotateRadians) + tx;
            const ry = sx * Math.sin(rotateRadians) + sy * Math.cos(rotateRadians) + ty;
            return { x: rx, y: ry, r };
        });
    }
}

export class TriangularShape {
    original: PackedCircle[] = [];
    circles: PackedCircle[] = [];
    side: number;

    constructor(side: number) {
        this.side = side;
        this.makeOriginal();
    }

    makeOriginal() {
        const h = (this.side * Math.sqrt(3)) / 2;
        const pts = [
            new Vector2(0, -h * (2/3)),            // Top
            new Vector2(-this.side / 2, h * (1/3)), // Bottom Left
            new Vector2(this.side / 2, h * (1/3))  // Bottom Right
        ];

        for (let i = 0; i < 3; i++) {
            const p1 = pts[i];
            const p2 = pts[(i + 1) % 3];
            
            const steps = 15;
            for (let j = 0; j < steps; j++) {
                const ratio = j / steps;
                const xi = p1.x * (1 - ratio) + p2.x * ratio;
                const yi = p1.y * (1 - ratio) + p2.y * ratio;
                this.original.push({ x: xi, y: yi, r: this.side / 24 });
            }
        }
        this.circles = this.original.map(c => ({ ...c }));
    }

    scaleRotateTranslate(scale: number, rotateRadians: number, tx: number, ty: number) {
        this.circles = this.original.map(c => {
            const sx = c.x * scale;
            const sy = c.y * scale;
            const r = c.r * scale;
            const rx = sx * Math.cos(rotateRadians) - sy * Math.sin(rotateRadians) + tx;
            const ry = sx * Math.sin(rotateRadians) + sy * Math.cos(rotateRadians) + ty;
            return { x: rx, y: ry, r };
        });
    }
}

export class CircularShape {
    original: PackedCircle[] = [];
    circles: PackedCircle[] = [];
    radius: number;

    constructor(radius: number) {
        this.radius = radius;
        this.makeOriginal();
    }

    makeOriginal() {
        const PI = Math.PI;
        const TAU = PI * 2;
        const steps = 40;
        
        for (let i = 0; i < steps; i++) {
            const a = (i / steps) * TAU;
            const xi = Math.cos(a) * this.radius;
            const yi = Math.sin(a) * this.radius;
            this.original.push({ x: xi, y: yi, r: this.radius / 10 });
        }
        this.circles = this.original.map(c => ({ ...c }));
    }

    scaleRotateTranslate(scale: number, rotateRadians: number, tx: number, ty: number) {
        this.circles = this.original.map(c => {
            const sx = c.x * scale;
            const sy = c.y * scale;
            const r = c.r * scale;
            const rx = sx * Math.cos(rotateRadians) - sy * Math.sin(rotateRadians) + tx;
            const ry = sx * Math.sin(rotateRadians) + sy * Math.cos(rotateRadians) + ty;
            return { x: rx, y: ry, r };
        });
    }
}
