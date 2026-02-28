import { useEffect, useRef } from 'react';

// Math: Distance
function dist(x1: number, y1: number, x2: number, y2: number) {
    return Math.hypot(x2 - x1, y2 - y1);
}

// Math: Lerp
function lerp(a: number, b: number, t: number) {
    return a + (b - a) * t;
}

// Math: Cubic Bezier Point
function bezierPoint(a: number, b: number, c: number, d: number, t: number) {
    const t1 = 1.0 - t;
    return Math.pow(t1, 3) * a +
        3 * Math.pow(t1, 2) * t * b +
        3 * t1 * Math.pow(t, 2) * c +
        Math.pow(t, 3) * d;
}

// Math: Check if two line segments intersect
function segmentsIntersect(x1: number, y1: number, x2: number, y2: number, x3: number, y3: number, x4: number, y4: number) {
    let den = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);
    if (den === 0) return false;
    let uA = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / den;
    let uB = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / den;
    return (uA > 0.01 && uA < 0.99 && uB > 0.01 && uB < 0.99); // 0.01 to ignore endpoints
}

// Math: Ray-casting point-in-polygon algorithm
function pointInPolygon(px: number, py: number, poly: { x: number, y: number }[]) {
    let isInside = false;
    for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
        let xi = poly[i].x, yi = poly[i].y;
        let xj = poly[j].x, yj = poly[j].y;
        let intersect = ((yi > py) !== (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi);
        if (intersect) isInside = !isInside;
    }
    return isInside;
}

function random(min: any, max?: number) {
    if (Array.isArray(min)) return min[Math.floor(Math.random() * min.length)];
    if (max === undefined) return Math.random() * min;
    return min + Math.random() * (max - min);
}

export default function Map1({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);

    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        let shapes: any[] = [];
        let connections: any[] = [];
        let allSegments: any[] = [];

        // Clear and background
        ctx.fillStyle = '#141414';
        ctx.fillRect(0, 0, width, height);

        class Shape {
            x: number;
            y: number;
            maxR: number;
            type: string;
            id: number;
            vertices: { x: number, y: number }[];
            sampledPoints: any[];
            col: string;

            constructor(x: number, y: number, maxR: number, type: string, id: number) {
                this.x = x;
                this.y = y;
                this.maxR = maxR;
                this.type = type;
                this.id = id;
                this.vertices = [];
                this.sampledPoints = [];

                let h = Math.floor(Math.random() * 360);
                let s = Math.floor(60 + Math.random() * 30);
                let l = Math.floor(40 + Math.random() * 20);
                this.col = `hsl(${h}, ${s}%, ${l}%)`;

                let numVertices = type === 'star' ? 14 : 40;
                let offset1 = Math.random() * 1000;
                let offset2 = Math.random() * 1000;

                for (let i = 0; i < numVertices; i++) {
                    let angle = (i / numVertices) * Math.PI * 2;
                    let r = maxR;

                    if (type === 'star') {
                        r = i % 2 === 0 ? maxR : maxR * 0.4;
                    } else {
                        let blobiness = Math.sin(angle * 3 + offset1) * 0.15 + Math.cos(angle * 5 + offset2) * 0.1;
                        r = maxR * 0.75 + maxR * blobiness;
                    }
                    this.vertices.push({ x: x + Math.cos(angle) * r, y: y + Math.sin(angle) * r });
                }
            }

            samplePoints(spacing: number) {
                let accumulatedDist = 0;
                for (let i = 0; i < this.vertices.length; i++) {
                    let p1 = this.vertices[i];
                    let p2 = this.vertices[(i + 1) % this.vertices.length];
                    let d = dist(p1.x, p1.y, p2.x, p2.y);

                    let segmentsNeeded = Math.floor((accumulatedDist + d) / spacing);
                    for (let j = 0; j < segmentsNeeded; j++) {
                        let t = (spacing * (j + 1) - accumulatedDist) / d;
                        let ptX = lerp(p1.x, p2.x, t);
                        let ptY = lerp(p1.y, p2.y, t);

                        let dx = p2.x - p1.x;
                        let dy = p2.y - p1.y;
                        let mag = Math.sqrt(dx * dx + dy * dy);
                        let nx = dy / mag;
                        let ny = -dx / mag;

                        this.sampledPoints.push({
                            x: ptX, y: ptY,
                            nx: nx, ny: ny,
                            shapeId: this.id, used: false
                        });
                    }
                    accumulatedDist = (accumulatedDist + d) % spacing;
                }
                return this.sampledPoints;
            }
        }

        function generateShapes(maxShapes: number) {
            let attempts = 0;
            while (shapes.length < maxShapes && attempts < 2000) {
                let r = random(30, 80) as number;
                let x = random(r + 10, width - r - 10) as number;
                let y = random(r + 10, height - r - 10) as number;

                let overlapping = false;
                for (let s of shapes) {
                    if (dist(x, y, s.x, s.y) < r + s.maxR + 15) {
                        overlapping = true;
                        break;
                    }
                }

                if (!overlapping) {
                    let type = random(['blob', 'star']) as string;
                    let newShape = new Shape(x, y, r, type, shapes.length);
                    shapes.push(newShape);

                    for (let i = 0; i < newShape.vertices.length; i++) {
                        let p1 = newShape.vertices[i];
                        let p2 = newShape.vertices[(i + 1) % newShape.vertices.length];
                        allSegments.push({ x1: p1.x, y1: p1.y, x2: p2.x, y2: p2.y });
                    }
                }
                attempts++;
            }
        }

        function generateConnections(points: any[], attempts: number) {
            for (let i = 0; i < attempts; i++) {
                let pA = random(points) as any;
                let pB = random(points) as any;

                if (pA.shapeId === pB.shapeId || pA.used || pB.used) continue;

                let distAB = dist(pA.x, pA.y, pB.x, pB.y);
                let controlMag = distAB * 0.4;

                let cp1x = pA.x + pA.nx * controlMag;
                let cp1y = pA.y + pA.ny * controlMag;
                let cp2x = pB.x + pB.nx * controlMag;
                let cp2y = pB.y + pB.ny * controlMag;

                let steps = 20;
                let curvePoints = [];
                for (let t = 0; t <= steps; t++) {
                    let frac = t / steps;
                    let bx = bezierPoint(pA.x, cp1x, cp2x, pB.x, frac);
                    let by = bezierPoint(pA.y, cp1y, cp2y, pB.y, frac);
                    curvePoints.push({ x: bx, y: by });
                }

                let isOverlapping = false;
                let newSegments = [];

                for (let j = 0; j < curvePoints.length - 1; j++) {
                    let c1 = curvePoints[j];
                    let c2 = curvePoints[j + 1];
                    newSegments.push({ x1: c1.x, y1: c1.y, x2: c2.x, y2: c2.y });

                    for (let seg of allSegments) {
                        if (dist(c1.x, c1.y, pA.x, pA.y) < 2 || dist(c2.x, c2.y, pB.x, pB.y) < 2) continue;
                        if (segmentsIntersect(c1.x, c1.y, c2.x, c2.y, seg.x1, seg.y1, seg.x2, seg.y2)) {
                            isOverlapping = true;
                            break;
                        }
                    }
                    if (isOverlapping) break;

                    for (let s of shapes) {
                        if (pointInPolygon(c1.x, c1.y, s.vertices)) {
                            isOverlapping = true;
                            break;
                        }
                    }
                    if (isOverlapping) break;
                }

                if (!isOverlapping) {
                    connections.push(curvePoints);
                    allSegments = allSegments.concat(newSegments);
                    pA.used = true;
                    pB.used = true;
                }
            }
        }

        generateShapes(25);

        let allPoints: any[] = [];
        for (let s of shapes) {
            let sampled = s.samplePoints(30);
            allPoints = allPoints.concat(sampled);
        }

        generateConnections(allPoints, 1000);

        for (let s of shapes) {
            if (!ctx) break;
            ctx.fillStyle = s.col;
            ctx.beginPath();
            if (s.vertices.length > 0) {
                ctx.moveTo(s.vertices[0].x, s.vertices[0].y);
                for (let i = 1; i < s.vertices.length; i++) {
                    ctx.lineTo(s.vertices[i].x, s.vertices[i].y);
                }
            }
            ctx.closePath();
            ctx.fill();

            ctx.fillStyle = '#ffffff';
            for (let pt of s.sampledPoints) {
                ctx.beginPath();
                ctx.arc(pt.x, pt.y, 2, 0, Math.PI * 2);
                ctx.fill();
            }
        }

        if (ctx) {
            ctx.strokeStyle = 'rgba(255, 255, 255, 0.6)';
            ctx.lineWidth = 1.5;
            for (let c of connections) {
                ctx.beginPath();
                if (c.length > 0) {
                    ctx.moveTo(c[0].x, c[0].y);
                    for (let i = 1; i < c.length; i++) {
                        ctx.lineTo(c[i].x, c[i].y);
                    }
                }
                ctx.stroke();
            }
        }

    }, [width, height]);

    return <canvas ref={canvasRef} width={width} height={height} style={{ borderRadius: 8, display: 'block', margin: '0 auto', boxShadow: '0 4px 6px rgba(0,0,0,0.3)' }} />;
}
