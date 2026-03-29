import { useEffect, useRef, useState, useCallback } from 'react';

const PAD = 80;
const MAX_DEPTH = 35;
const N = 3;
const minBranchLength = 30;
const maxBranchLength = 40;

interface Point {
    x: number;
    y: number;
}

interface Edge {
    p1: Point;
    p2: Point;
}

// Math Utilities from the provided p5 script
function dist(x1: number, y1: number, x2: number, y2: number) {
    return Math.sqrt((x1 - x2) ** 2 + (y1 - y2) ** 2);
}

function dist2(v: Point, w: Point) {
    return (v.x - w.x) ** 2 + (v.y - w.y) ** 2;
}

function distToSegmentSquared(p: Point, edge: Edge) {
    const v = edge.p1;
    const w = edge.p2;
    const l2 = dist2(v, w);
    if (l2 === 0) return dist2(p, v);
    let t = ((p.x - v.x) * (w.x - v.x) + (p.y - v.y) * (w.y - v.y)) / l2;
    t = Math.max(0, Math.min(1, t));
    return dist2(p, { x: v.x + t * (w.x - v.x), y: v.y + t * (w.y - v.y) });
}

function nodeEdgeCollision(p: Point, edge: Edge) {
    return Math.sqrt(distToSegmentSquared(p, edge));
}

function edgeIntersect(edge1: Edge, edge2: Edge) {
    const a = edge1.p1.x, b = edge1.p1.y;
    const c = edge1.p2.x, d = edge1.p2.y;
    const p = edge2.p1.x, q = edge2.p1.y;
    const r = edge2.p2.x, s = edge2.p2.y;

    const det = (c - a) * (s - q) - (r - p) * (d - b);
    if (det === 0) return false;
    const lambda = ((s - q) * (r - a) + (p - r) * (s - b)) / det;
    const gamma = ((b - d) * (r - a) + (c - a) * (s - b)) / det;
    return (0 < lambda && lambda < 1) && (0 < gamma && gamma < 1);
}

class Node {
    parentNode: Node | null;
    childrenNodes: Node[];
    position: Point;
    radius: number;
    depth: number;
    id: number;
    color: string;

    constructor(parentNode: Node | null, position: Point, radius: number, depth: number, id: number, color: string) {
        this.parentNode = parentNode;
        this.childrenNodes = [];
        this.position = position;
        this.radius = radius;
        this.depth = depth;
        this.id = id;
        this.color = color;
    }

    attemptGrowth(allNodes: Node[], edgesList: Edge[], width: number, height: number): Node | null {
        if (this.depth > 0) {
            const angle = Math.random() * Math.PI * 2;
            const len = minBranchLength + Math.random() * (maxBranchLength - minBranchLength);
            const childRadius = Math.max(this.radius * 0.85, 2);
            const childPos = {
                x: this.position.x + len * Math.cos(angle),
                y: this.position.y + len * Math.sin(angle)
            };

            const child = new Node(this, childPos, childRadius, this.depth - 1, -1, this.color); // Temp ID

            // 1. Boundary Check
            if (child.boundaryCheck(width, height)) return null;

            // 2. Intersection Check with All Other Nodes
            for (let n of allNodes) {
                if (child.intersects(n)) return null;
            }

            const currentEdge: Edge = { p1: childPos, p2: this.position };

            // 3. Edge Proximity & Crossing Checks
            for (let e of edgesList) {
                if (nodeEdgeCollision(childPos, e) < childRadius / 2 + 20) return null;
                if (edgeIntersect(currentEdge, e)) return null;
            }

            return child;
        }
        return null;
    }

    grow(allNodes: Node[], edgesList: Edge[], width: number, height: number, counter: { val: number }): boolean {
        const newChild = this.attemptGrowth(allNodes, edgesList, width, height);
        if (newChild) {
            newChild.id = counter.val++;
            allNodes.push(newChild);
            this.childrenNodes.push(newChild);
            edgesList.push({ p1: newChild.position, p2: this.position });
            return true;
        } else {
            if (this.childrenNodes.length > 0) {
                const randChild = this.childrenNodes[Math.floor(Math.random() * this.childrenNodes.length)];
                return randChild.grow(allNodes, edgesList, width, height, counter);
            }
        }
        return false;
    }

    intersects(other: Node): boolean {
        const d = dist(this.position.x, this.position.y, other.position.x, other.position.y);
        return d < this.radius / 2 + other.radius / 2 + 10;
    }

    boundaryCheck(width: number, height: number): boolean {
        return (
            this.position.x - this.radius < PAD ||
            this.position.x + this.radius > width - PAD ||
            this.position.y - this.radius < PAD ||
            this.position.y + this.radius > height - PAD
        );
    }

    display(ctx: CanvasRenderingContext2D) {
        if (this.parentNode) {
            ctx.strokeStyle = this.color;
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.moveTo(this.position.x, this.position.y);
            ctx.lineTo(this.parentNode.position.x, this.parentNode.position.y);
            ctx.stroke();

            ctx.fillStyle = this.color;
            ctx.beginPath();
            ctx.arc(this.parentNode.position.x, this.parentNode.position.y, this.parentNode.radius / 2, 0, Math.PI * 2);
            ctx.fill();
        }
        ctx.fillStyle = this.color;
        ctx.beginPath();
        ctx.arc(this.position.x, this.position.y, this.radius / 2, 0, Math.PI * 2);
        ctx.fill();
    }
}

export default function Map8({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const allNodesRef = useRef<Node[]>([]);
    const sourceNodesRef = useRef<Node[]>([]);
    const edgesRef = useRef<Edge[]>([]);
    const counterRef = useRef({ val: 0 });
    const [seed, setSeed] = useState(0);

    const initMap = useCallback(() => {
        allNodesRef.current = [];
        sourceNodesRef.current = [];
        edgesRef.current = [];
        counterRef.current.val = 0;

        const biomes = ['#52b788', '#00b4d8', '#ffb703', '#f72585', '#7209b7', '#e63946'];
        let availableBiomes = [...biomes];

        let attempts = 0;
        while (sourceNodesRef.current.length < N && attempts < 100) {
            const pos = {
                x: PAD * 1.5 + Math.random() * (width - PAD * 3),
                y: PAD * 1.5 + Math.random() * (height - PAD * 3)
            };
            const radius = 22 + Math.random() * 8;
            
            const biomeIdx = Math.floor(Math.random() * availableBiomes.length);
            const color = availableBiomes.splice(biomeIdx, 1)[0] || '#fff';
            
            const source = new Node(null, pos, radius, MAX_DEPTH, counterRef.current.val++, color);

            let placeable = true;
            for (let s of sourceNodesRef.current) {
                if (dist(source.position.x, source.position.y, s.position.x, s.position.y) < source.radius / 2 + s.radius / 2 + 25) {
                    placeable = false;
                    break;
                }
            }
            if (placeable) {
                sourceNodesRef.current.push(source);
                allNodesRef.current.push(source);
            }
            attempts++;
        }
    }, [width, height]);

    const handleRegenerate = () => {
        initMap();
        setSeed(s => s + 1);
    };

    useEffect(() => {
        initMap();
    }, [initMap]);

    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        let animationFrameId: number;
        const render = () => {
            // Draw Background (Black)
            ctx.fillStyle = '#000';
            ctx.fillRect(0, 0, width, height);

            // Growth
            for (let node of sourceNodesRef.current) {
                node.grow(allNodesRef.current, edgesRef.current, width, height, counterRef.current);
            }

            // Display
            for (let node of allNodesRef.current) {
                node.display(ctx);
            }

            animationFrameId = requestAnimationFrame(render);
        };

        render();
        return () => cancelAnimationFrame(animationFrameId);
    }, [width, height, seed]);

    return (
        <div style={{ position: 'relative', width, height }}>
            <div style={{
                position: 'absolute',
                top: 10,
                left: 10,
                background: 'rgba(255, 255, 255, 0.1)',
                backdropFilter: 'blur(4px)',
                padding: '12px',
                borderRadius: '8px',
                color: 'white',
                fontFamily: 'sans-serif',
                display: 'flex',
                flexDirection: 'column',
                gap: '8px',
                zIndex: 10,
                border: '1px solid rgba(255, 255, 255, 0.2)'
            }}>
                <button
                    onClick={handleRegenerate}
                    style={{ background: '#fff', color: '#000', border: 'none', padding: '8px', borderRadius: '4px', cursor: 'pointer', fontWeight: 'bold' }}>
                    Regenerate Trees
                </button>
                <div style={{ fontSize: '11px', opacity: 0.8 }}>
                    • Stark white-on-black growth<br/>
                    • Space-aware avoidance<br/>
                    • Competitive seeds: {sourceNodesRef.current.length}
                </div>
            </div>

            <canvas
                ref={canvasRef}
                width={width}
                height={height}
                style={{ borderRadius: 8, display: 'block', margin: '0 auto', boxShadow: '0 4px 6px rgba(0,0,0,0.5)' }}
            />
        </div>
    );
}
