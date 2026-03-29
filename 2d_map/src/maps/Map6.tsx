import { useEffect, useRef, useState, useCallback } from 'react';

const plantBorder = 50;
const nodeBorder = 10;
const minBranchLength = 10;
const maxBranchLength = 25;
const minSizeMultiplier = 0.75;

class Node {
    parent: Node | null;
    parentSize: number;
    size: number;
    angle: number;
    branchLength: number;
    children: Node[];
    x: number;
    y: number;
    g: number;

    constructor(parent: Node | null, parentSize: number, parentG: number, angle: number, plantX: number, plantY: number) {
        this.parent = parent;
        this.parentSize = parentSize;
        this.size = (Math.random() * (1 - minSizeMultiplier) + minSizeMultiplier) * parentSize;
        this.angle = angle;
        this.branchLength = Math.random() * (maxBranchLength - minBranchLength) + minBranchLength;
        this.children = [];

        if (parent) {
            this.x = parent.x + Math.cos(this.angle) * (this.parentSize / 2 + this.branchLength + this.size / 2);
            this.y = parent.y + Math.sin(this.angle) * (this.parentSize / 2 + this.branchLength + this.size / 2);
        } else {
            this.x = plantX;
            this.y = plantY;
        }

        this.g = parentG + (Math.random() * 50 - 25);
        this.g = Math.max(128, Math.min(255, this.g));
    }

    grow(plant: Node, width: number, plantY: number): boolean {
        const childAngle = this.angle + (Math.random() * (Math.PI / 1.5) - Math.PI / 3);
        const child = new Node(this, this.size, this.g, childAngle, 0, 0);

        if (child.size < 10) return false;
        if (child.x < plantBorder || child.x > width - plantBorder) return false;
        if (child.y < plantBorder || child.y > plantY - 25) return false;
        if (plant.intersects(this, child.x, child.y, child.size)) return false;

        this.children.push(child);
        return true;
    }

    maybeGrow(plant: Node, width: number, plantY: number) {
        const grew = this.grow(plant, width, plantY);
        if (!grew && this.children.length > 0) {
            const randomChild = this.children[Math.floor(Math.random() * this.children.length)];
            randomChild.maybeGrow(plant, width, plantY);
        }
    }

    prune() {
        if (!this.parent) return;
        const index = this.parent.children.indexOf(this);
        if (index > -1) {
            this.parent.children.splice(index, 1);
        }
    }

    getClickedNode(clickedX: number, clickedY: number): Node | null {
        const d = Math.sqrt((this.x - clickedX) ** 2 + (this.y - clickedY) ** 2);
        if (d < this.size / 2) return this;

        for (const child of this.children) {
            const result = child.getClickedNode(clickedX, clickedY);
            if (result) return result;
        }
        return null;
    }

    intersects(parentNode: Node, otherX: number, otherY: number, otherSize: number): boolean {
        // Distance check between this node and other node
        if (this !== parentNode) {
            const d = Math.sqrt((this.x - otherX) ** 2 + (this.y - otherY) ** 2);
            if (d < this.size / 2 + otherSize / 2 + nodeBorder) return true;
        }
        // Recursively check children
        for (const child of this.children) {
            if (child.intersects(parentNode, otherX, otherY, otherSize)) return true;
        }
        return false;
    }

    draw(ctx: CanvasRenderingContext2D) {
        // Branches
        ctx.strokeStyle = 'rgb(139, 69, 19)';
        ctx.lineWidth = 10;
        ctx.lineCap = 'round';
        for (const child of this.children) {
            ctx.beginPath();
            ctx.moveTo(this.x, this.y);
            ctx.lineTo(child.x, child.y);
            ctx.stroke();
        }

        // Node Circle
        ctx.fillStyle = `rgb(0, ${this.g}, 0)`;
        ctx.strokeStyle = `rgb(0, ${this.g * 0.8}, 0)`;
        ctx.lineWidth = 4;
        ctx.beginPath();
        ctx.arc(this.x, this.y, this.size / 2, 0, Math.PI * 2);
        ctx.fill();
        ctx.stroke();

        // Children
        for (const child of this.children) {
            child.draw(ctx);
        }
    }
}

export default function Map6({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const plantRef = useRef<Node | null>(null);
    const [seed, setSeed] = useState(0);

    const plantX = width / 2;
    const plantY = height * 0.9;

    const initPlant = useCallback(() => {
        plantRef.current = new Node(null, 50, 200 + Math.random() * 55, -Math.PI / 2, plantX, plantY);
    }, [plantX, plantY]);

    const handleRegenerate = () => {
        initPlant();
        setSeed(s => s + 1);
    };

    const handleCanvasClick = (e: React.MouseEvent<HTMLCanvasElement>) => {
        if (!canvasRef.current || !plantRef.current) return;
        const rect = canvasRef.current.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;

        const clickedNode = plantRef.current.getClickedNode(x, y);
        if (clickedNode && clickedNode !== plantRef.current) {
            clickedNode.prune();
        }
    };

    useEffect(() => {
        initPlant();
    }, [initPlant]);

    useEffect(() => {
        const canvas = canvasRef.current;
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        let animationFrameId: number;

        const render = () => {
            // Draw Background
            ctx.fillStyle = '#202020';
            ctx.fillRect(0, 0, width, height);

            if (plantRef.current) {
                // Growth
                plantRef.current.maybeGrow(plantRef.current, width, plantY);

                // Draw Plant
                plantRef.current.draw(ctx);

                // Pot
                ctx.strokeStyle = 'rgb(200, 0, 0)';
                ctx.fillStyle = 'rgb(128, 0, 0)';
                ctx.lineWidth = 4;
                ctx.beginPath();
                ctx.moveTo(plantX - 50, plantY);
                ctx.lineTo(plantX + 50, plantY);
                ctx.lineTo(plantX + 25, plantY + 25);
                ctx.lineTo(plantX - 25, plantY + 25);
                ctx.closePath();
                ctx.fill();
                ctx.stroke();
            }

            animationFrameId = requestAnimationFrame(render);
        };

        render();

        return () => {
            cancelAnimationFrame(animationFrameId);
        };
    }, [width, height, plantX, plantY, seed]);

    return (
        <div style={{ position: 'relative', width, height }}>
            {/* Control Panel Overlay */}
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
                gap: '12px',
                zIndex: 10
            }}>
                <button
                    onClick={handleRegenerate}
                    style={{ background: '#3b82f6', color: 'white', border: 'none', padding: '8px', borderRadius: '4px', cursor: 'pointer', fontWeight: 'bold' }}>
                    Regenerate Plant
                </button>
                <div style={{ fontSize: '12px', opacity: 0.8 }}>
                    Tip: Click a green node to prune branches!
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
