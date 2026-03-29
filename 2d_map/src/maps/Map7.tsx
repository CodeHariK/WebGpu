import { useEffect, useRef, useState, useCallback } from 'react';

const cityBorder = 50;
const junctionBorder = 30; // Increased spacing to prevent "collapsing"

// Segment intersection utility
function segmentsIntersect(x1: number, y1: number, x2: number, y2: number, x3: number, y3: number, x4: number, y4: number) {
    const den = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);
    if (den === 0) return false;
    const uA = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / den;
    const uB = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / den;
    return (uA > 0.01 && uA < 0.99 && uB > 0.01 && uB < 0.99);
}

type NodeType = 'artery' | 'street';

class Building {
    x: number;
    y: number;
    w: number;
    h: number;
    angle: number;
    color: string;

    constructor(x: number, y: number, w: number, h: number, angle: number) {
        this.x = x;
        this.y = y;
        this.w = w;
        this.h = h;
        this.angle = angle;
        const shades = ['#4a4a4a', '#5a5a5a', '#6a6a6a', '#3a3a3a', '#ef584e', '#3b82f6'];
        this.color = shades[Math.floor(Math.random() * shades.length)];
    }

    draw(ctx: CanvasRenderingContext2D) {
        ctx.save();
        ctx.translate(this.x, this.y);
        ctx.rotate(this.angle);
        ctx.fillStyle = this.color;
        ctx.strokeStyle = '#222';
        ctx.lineWidth = 1;
        ctx.fillRect(-this.w / 2, -this.h / 2, this.w, this.h);
        ctx.strokeRect(-this.w / 2, -this.h / 2, this.w, this.h);
        
        ctx.fillStyle = 'rgba(255, 255, 255, 0.3)';
        const winSize = Math.max(2, this.w / 4);
        for (let i = -this.w / 2 + 2; i < this.w / 2 - 2; i += winSize + 2) {
            for (let j = -this.h / 2 + 2; j < this.h / 2 - 2; j += winSize + 2) {
                if (Math.random() > 0.3) ctx.fillRect(i, j, winSize, winSize);
            }
        }
        ctx.restore();
    }
}

class Junction {
    parent: Junction | null;
    x: number;
    y: number;
    type: NodeType;
    angle: number;
    children: Junction[];
    buildings: Building[];

    constructor(parent: Junction | null, x: number, y: number, angle: number, type: NodeType) {
        this.parent = parent;
        this.x = x;
        this.y = y;
        this.angle = angle;
        this.type = type;
        this.children = [];
        this.buildings = [];
    }

    grow(city: Junction, width: number, height: number): Junction | null {
        let nextType: NodeType = this.type;
        let nextAngle = this.angle;
        let nextLen = this.type === 'artery' ? 50 + Math.random() * 30 : 25 + Math.random() * 15;

        if (this.type === 'artery' && Math.random() > 0.7) {
            nextType = 'street';
            nextAngle += (Math.random() > 0.5 ? 1 : -1) * Math.PI / 2;
        } else {
            nextAngle += (Math.random() - 0.5) * 0.2; // Reduced wobble for straighter city roads
        }

        const nextX = this.x + Math.cos(nextAngle) * nextLen;
        const nextY = this.y + Math.sin(nextAngle) * nextLen;

        if (nextX < cityBorder || nextX > width - cityBorder) return null;
        if (nextY < cityBorder || nextY > height - cityBorder) return null;
        
        // 1. Check junction proximity
        if (city.intersects(this, nextX, nextY, junctionBorder)) return null;
        
        // 2. Check road segment intersections (prevents crossing)
        if (city.intersectsSegment(this.x, this.y, nextX, nextY)) return null;
        
        const child = new Junction(this, nextX, nextY, nextAngle, nextType);
        this.zoneBuildings(child);
        this.children.push(child);
        return child;
    }

    zoneBuildings(toNode: Junction) {
        const dist = Math.sqrt((this.x - toNode.x) ** 2 + (this.y - toNode.y) ** 2);
        const dirX = (toNode.x - this.x) / dist;
        const dirY = (toNode.y - this.y) / dist;
        const normX = -dirY;
        const normY = dirX;

        const bW = toNode.type === 'artery' ? 25 : 15;
        const bH = toNode.type === 'artery' ? 35 : 20;
        const bDist = toNode.type === 'artery' ? 25 : 15;

        for (let s of [-1, 1]) {
            if (Math.random() > 0.4) {
                const bx = this.x + dirX * (dist / 2) + normX * s * bDist;
                const by = this.y + dirY * (dist / 2) + normY * s * bDist;
                this.buildings.push(new Building(bx, by, bW, bH, toNode.angle));
            }
        }
    }

    intersectsSegment(x1: number, y1: number, x2: number, y2: number): boolean {
        for (let child of this.children) {
            if (segmentsIntersect(x1, y1, x2, y2, this.x, this.y, child.x, child.y)) {
                return true;
            }
            if (child.intersectsSegment(x1, y1, x2, y2)) {
                return true;
            }
        }
        return false;
    }

    intersects(parent: Junction, ox: number, oy: number, radius: number): boolean {
        if (this !== parent) {
            const d = Math.sqrt((this.x - ox) ** 2 + (this.y - oy) ** 2);
            if (d < radius) return true;
        }
        for (let child of this.children) {
            if (child.intersects(parent, ox, oy, radius)) return true;
        }
        return false;
    }

    getClickedJunction(cx: number, cy: number): Junction | null {
        const d = Math.sqrt((this.x - cx) ** 2 + (this.y - cy) ** 2);
        if (d < 15) return this;
        for (let child of this.children) {
            const res = child.getClickedJunction(cx, cy);
            if (res) return res;
        }
        return null;
    }

    prune() {
        if (!this.parent) return;
        const idx = this.parent.children.indexOf(this);
        if (idx > -1) this.parent.children.splice(idx, 1);
    }

    draw(ctx: CanvasRenderingContext2D) {
        for (let b of this.buildings) b.draw(ctx);
        for (let child of this.children) {
            ctx.strokeStyle = '#333';
            ctx.lineWidth = child.type === 'artery' ? 12 : 6;
            ctx.lineCap = 'butt';
            ctx.beginPath();
            ctx.moveTo(this.x, this.y);
            ctx.lineTo(child.x, child.y);
            ctx.stroke();

            if (child.type === 'artery') {
                ctx.strokeStyle = '#fff';
                ctx.lineWidth = 1;
                ctx.setLineDash([5, 5]);
                ctx.beginPath();
                ctx.moveTo(this.x, this.y);
                ctx.lineTo(child.x, child.y);
                ctx.stroke();
                ctx.setLineDash([]);
            }
        }
        ctx.fillStyle = '#666';
        ctx.beginPath();
        ctx.arc(this.x, this.y, 4, 0, Math.PI * 2);
        ctx.fill();

        for (let child of this.children) child.draw(ctx);
    }
}

export default function Map7({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const cityRef = useRef<Junction | null>(null);
    const activeNodesRef = useRef<Junction[]>([]);
    const [seed, setSeed] = useState(0);

    const initCity = useCallback(() => {
        const root = new Junction(null, width / 2, height / 2, Math.random() * Math.PI * 2, 'artery');
        cityRef.current = root;
        activeNodesRef.current = [root];
    }, [width, height]);

    const handleRegenerate = () => {
        initCity();
        setSeed(s => s + 1);
    };

    const handleCanvasClick = (e: React.MouseEvent<HTMLCanvasElement>) => {
        if (!canvasRef.current || !cityRef.current) return;
        const rect = canvasRef.current.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;

        const junction = cityRef.current.getClickedJunction(x, y);
        if (junction && junction !== cityRef.current) {
            junction.prune();
            // Filter out nodes that are no longer part of the root tree
            activeNodesRef.current = activeNodesRef.current.filter(n => {
                let curr: Junction | null = n;
                while (curr && curr !== cityRef.current) {
                    curr = curr.parent;
                }
                return curr === cityRef.current;
            });
        }
    };

    useEffect(() => {
        initCity();
    }, [initCity]);

    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        let animationFrameId: number;
        const render = () => {
            ctx.fillStyle = '#e5e5e5';
            ctx.fillRect(0, 0, width, height);

            if (cityRef.current && activeNodesRef.current.length > 0) {
                // Pick a random active node to grow
                const idx = Math.floor(Math.random() * activeNodesRef.current.length);
                const node = activeNodesRef.current[idx];
                const newChild = node.grow(cityRef.current, width, height);
                
                if (newChild) {
                    activeNodesRef.current.push(newChild);
                } else if (Math.random() > 0.95) {
                    // Slow decay of "growth potential" if it keeps failing
                    // but we keep some for streets branching later
                }
                
                // Keep the list manageable but large enough for a city
                if (activeNodesRef.current.length > 2000) {
                    activeNodesRef.current.shift();
                }

                cityRef.current.draw(ctx);
            } else if (cityRef.current) {
                cityRef.current.draw(ctx);
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
                background: 'rgba(0, 0, 0, 0.7)',
                padding: '12px',
                borderRadius: '8px',
                color: 'white',
                fontFamily: 'sans-serif',
                display: 'flex',
                flexDirection: 'column',
                gap: '8px',
                zIndex: 10
            }}>
                <button
                    onClick={handleRegenerate}
                    style={{ background: '#3b82f6', color: 'white', border: 'none', padding: '8px', borderRadius: '4px', cursor: 'pointer', fontWeight: 'bold' }}>
                    Regenerate City
                </button>
                <div style={{ fontSize: '11px', opacity: 0.8 }}>
                    • Growth is now non-recursive (faster & stable)<br/>
                    • Click junctions to demolish!<br/>
                    • Total junctions tracked: {activeNodesRef.current.length}
                </div>
            </div>

            <canvas
                ref={canvasRef}
                width={width}
                height={height}
                onClick={handleCanvasClick}
                style={{ borderRadius: 8, display: 'block', margin: '0 auto', boxShadow: '0 4px 6px rgba(0,0,0,0.3)', cursor: 'crosshair' }}
            />
        </div>
    );
}
