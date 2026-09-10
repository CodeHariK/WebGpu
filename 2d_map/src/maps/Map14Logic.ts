import { Vector2 } from '../lib/Vector2';

export interface TileConfig {
    name: string;
    connections: number[][]; // [edge1, edge2][]
    sideHashes: boolean[]; // True if a road is on that face (0-5)
}

// 6 Fundamental Truchet Patterns (Indices: 0-5)
export const PATTERNS = [
    { name: 'EMPTY', conns: [] },
    { name: 'ADJACENT', conns: [[0, 1]] },
    { name: 'MEDIUM', conns: [[0, 2]] },
    { name: 'DIAMETRICAL', conns: [[0, 3]] },
    { name: 'DOUBLE ARC', conns: [[0, 1], [3, 4]] },
    { name: 'TRI-ARC', conns: [[0, 1], [2, 3], [4, 5]] }
];

export const TILE_REGISTRY: TileConfig[] = [];

// Populate Registry with 36 Variations (6 Patterns * 6 Rotations)
PATTERNS.forEach((p) => {
    for (let rot = 0; rot < 6; rot++) {
        const rotatedConns = p.conns.map(c => c.map(e => (e + rot) % 6));

        // Calculate boolean signatures analytically
        const sideHashes = [false, false, false, false, false, false];
        rotatedConns.forEach(conn => {
            sideHashes[conn[0]] = true;
            sideHashes[conn[1]] = true;
        });

        const tile: TileConfig = {
            name: `${p.name} #${rot}`,
            connections: rotatedConns,
            sideHashes: sideHashes
        };
        TILE_REGISTRY.push(tile);
    }
});

/**
 * Standard Neighbor Directions for Pointy Top Hexagons (Clockwise)
 *
 * Hex center: x = size * (sqrt(3)*q + sqrt(3)/2*r), y = size * (1.5*r)
 * Vertex angles: i * 60 + 30 deg (30°, 90°, 150°, 210°, 270°, 330°)
 * Edge midpoints & directions:
 *   Side 0: 60° (Bottom-Right) -> (0, 1)   [Opposite is Side 3]
 *   Side 1: 120° (Bottom-Left)  -> (-1, 1)  [Opposite is Side 4]
 *   Side 2: 180° (Left)         -> (-1, 0)  [Opposite is Side 5]
 *   Side 3: 240° (Top-Left)     -> (0, -1)  [Opposite is Side 0]
 *   Side 4: 300° (Top-Right)    -> (1, -1)  [Opposite is Side 1]
 *   Side 5: 0°   (Right)        -> (1, 0)   [Opposite is Side 2]
 */
export const DIRECTIONS = [
    { q: 0, r: 1 },   // Side 0 (Bottom-Right, 60°)
    { q: -1, r: 1 },  // Side 1 (Bottom-Left, 120°)
    { q: -1, r: 0 },  // Side 2 (Left, 180°)
    { q: 0, r: -1 },  // Side 3 (Top-Left, 240°)
    { q: 1, r: -1 },  // Side 4 (Top-Right, 300°)
    { q: 1, r: 0 }    // Side 5 (Right, 0°)
];

export const getNeighborPos = (q: number, r: number, side: number): { q: number, r: number } => {
    const dir = DIRECTIONS[side];
    return { q: q + dir.q, r: r + dir.r };
};

export const getHexCenter = (q: number, r: number, size: number): Vector2 => {
    const x = size * (Math.sqrt(3) * q + (Math.sqrt(3) / 2) * r);
    const y = size * (1.5 * r);
    return new Vector2(x, y);
};

export const getHexVertices = (center: Vector2, size: number): Vector2[] => {
    const vertices: Vector2[] = [];
    for (let i = 0; i < 6; i++) {
        const angle = (i * 60 + 30) * (Math.PI / 180);
        vertices.push(new Vector2(
            center.x + size * Math.cos(angle),
            center.y + size * Math.sin(angle)
        ));
    }
    return vertices;
};

export const drawTileRoads = (
    ctx: CanvasRenderingContext2D,
    center: Vector2,
    size: number,
    tile: TileConfig,
    roadColor = '#f8fafc',
    lineWidthScale = 0.15
): void => {
    const v = getHexVertices(center, size);
    ctx.lineCap = 'round';
    ctx.lineWidth = Math.max(1.5, size * lineWidthScale);
    ctx.strokeStyle = roadColor;

    tile.connections.forEach(conn => {
        const e1 = conn[0];
        const e2 = conn[1];
        const m1 = new Vector2((v[e1].x + v[(e1 + 1) % 6].x) / 2, (v[e1].y + v[(e1 + 1) % 6].y) / 2);
        const m2 = new Vector2((v[e2].x + v[(e2 + 1) % 6].x) / 2, (v[e2].y + v[(e2 + 1) % 6].y) / 2);
        const d1 = center.clone().sub(m1).normalize();
        const d2 = center.clone().sub(m2).normalize();
        const dist = Math.abs(e1 - e2) > 3 ? 6 - Math.abs(e1 - e2) : Math.abs(e1 - e2);
        const sScale = dist === 1 ? size * 0.35 : (dist === 2 ? size * 0.55 : size * 0.7);
        const cp1 = m1.clone().add(d1.mult(sScale));
        const cp2 = m2.clone().add(d2.mult(sScale));

        ctx.beginPath();
        ctx.moveTo(m1.x, m1.y);
        ctx.bezierCurveTo(cp1.x, cp1.y, cp2.x, cp2.y, m2.x, m2.y);
        ctx.stroke();
    });
};
