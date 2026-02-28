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

function noise(x: number, y: number, z: number) {
    // Basic pseudo-random noise approximation since p5's noise isn't available
    return (Math.sin(x * 12.9898 + y * 78.233 + z * 1.341) * 43758.5453) % 1;
}

// HSL to RGB conversion helper (output string rgb(r, g, b))
function hslToRgbString(h: number, s: number, l: number, a: number = 1) {
    s /= 100;
    l /= 100;
    const k = (n: number) => (n + h / 30) % 12;
    const aVal = s * Math.min(l, 1 - l);
    const f = (n: number) =>
        l - aVal * Math.max(-1, Math.min(k(n) - 3, Math.min(9 - k(n), 1)));
    return `rgba(${Math.round(255 * f(0))}, ${Math.round(255 * f(8))}, ${Math.round(255 * f(4))}, ${a})`;
}

export default function Map2({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);

    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        let shapes: any[] = [];
        let connections: any[] = [];
        let allSegments: any[] = [];

        // Clear and background (Warm paper/ground background matching p5: 240, 235, 225 rgb)
        ctx.fillStyle = 'rgb(240, 235, 225)';
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

                // Childish City Palettes: Grass, Water, Sand, Concrete
                // Adapted from HSB to HSL approximations
                let palettes = [
                    hslToRgbString(100, 40, 40),  // Green (Parks)
                    hslToRgbString(200, 60, 50),  // Blue (Lakes)
                    hslToRgbString(40, 50, 60),   // Orange/Sand (Playgrounds)
                    hslToRgbString(0, 0, 50)      // Grey (Building Plots)
                ];
                this.col = random(palettes) as string;

                let numVertices = type === 'star' ? 12 : (type === 'blocky' ? 8 : 30);
                let noiseOffset = random(1000) as number;

                for (let i = 0; i < numVertices; i++) {
                    let angle = (i / numVertices) * Math.PI * 2;
                    let r = maxR;

                    if (type === 'star') {
                        r = i % 2 === 0 ? maxR : maxR * 0.5;
                    } else if (type === 'blocky') {
                        r = maxR * (random(0.7, 1) as number); // rougher edges
                    } else {
                        // Math.abs on our fake noise to keep it positive 0-1
                        let nVal = Math.abs(noise(Math.cos(angle) + 1, Math.sin(angle) + 1, noiseOffset));
                        r = lerp(maxR * 0.5, maxR, nVal);
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
            while (shapes.length < maxShapes && attempts < 3000) {
                let r = random(30, 90) as number;
                let x = random(r + 20, width - r - 20) as number;
                let y = random(r + 20, height - r - 20) as number;

                let overlapping = false;
                for (let s of shapes) {
                    if (dist(x, y, s.x, s.y) < r + s.maxR + 25) { // Wider padding for roads
                        overlapping = true;
                        break;
                    }
                }

                if (!overlapping) {
                    let type = random(['blob', 'star', 'blocky']) as string;
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
            let maxRoadLength = 250; // Restrict how long a road can be

            for (let i = 0; i < attempts; i++) {
                let pA = random(points) as any;
                if (pA.used) continue; // Skip if A is already a road

                // Find all valid, unused points on OTHER shapes within max road length
                let candidates = points.filter(pB =>
                    !pB.used &&
                    pB.shapeId !== pA.shapeId &&
                    dist(pA.x, pA.y, pB.x, pB.y) < maxRoadLength
                );

                if (candidates.length === 0) continue;

                // Sort candidates so the closest ones are at the start of the array
                candidates.sort((a, b) => dist(pA.x, pA.y, a.x, a.y) - dist(pA.x, pA.y, b.x, b.y));

                // Pick from the top 3 closest points to allow a little natural variation
                let pB = candidates[Math.floor(random(Math.min(3, candidates.length)) as number)];

                let distAB = dist(pA.x, pA.y, pB.x, pB.y);
                let controlMag = distAB * 0.35; // Slightly tighter curves for roads

                let cp1x = pA.x + pA.nx * controlMag;
                let cp1y = pA.y + pA.ny * controlMag;
                let cp2x = pB.x + pB.nx * controlMag;
                let cp2y = pB.y + pB.ny * controlMag;

                let steps = 25;
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
                        if (dist(c1.x, c1.y, pA.x, pA.y) < 3 || dist(c2.x, c2.y, pB.x, pB.y) < 3) continue;

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

        generateShapes(30);

        let allPoints: any[] = [];
        for (let s of shapes) {
            let sampled = s.samplePoints(35); // Slightly larger spacing for city plots
            allPoints = allPoints.concat(sampled);
        }

        generateConnections(allPoints, 1500);

        // 1. Draw City Plots (Shapes)
        for (let s of shapes) {
            if (!ctx) break;
            // Draw an outline/sidewalk for the plots
            ctx.strokeStyle = '#222'; // Dark outline approximation
            ctx.lineWidth = 4;
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
            ctx.stroke();
        }

        // 2. Draw Roads (Connections)
        if (ctx) {
            for (let c of connections) {
                // Road Base (Thick Dark Grey)
                ctx.strokeStyle = '#222';
                ctx.lineWidth = 8;
                ctx.setLineDash([]); // Ensure no dash for base

                ctx.beginPath();
                if (c.length > 0) {
                    ctx.moveTo(c[0].x, c[0].y);
                    for (let i = 1; i < c.length; i++) {
                        ctx.lineTo(c[i].x, c[i].y);
                    }
                }
                ctx.stroke();

                // Road Centerline (Dashed Yellow)
                ctx.strokeStyle = '#e6c200'; // Yellowish
                ctx.lineWidth = 2;
                ctx.setLineDash([10, 10]); // Create the dashed effect
                ctx.beginPath();
                if (c.length > 0) {
                    ctx.moveTo(c[0].x, c[0].y);
                    for (let i = 1; i < c.length; i++) {
                        ctx.lineTo(c[i].x, c[i].y);
                    }
                }
                ctx.stroke();
                ctx.setLineDash([]); // Reset dash
            }
        }

    }, [width, height]);

    return <canvas ref={canvasRef} width={width} height={height} style={{ borderRadius: 8, display: 'block', margin: '0 auto', boxShadow: '0 4px 6px rgba(0,0,0,0.3)' }} />;
}
