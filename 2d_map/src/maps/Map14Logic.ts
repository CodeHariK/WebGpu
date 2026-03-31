export interface TileConfig {
    name: string;
    connections: number[][]; // [edge1, edge2][]
    sideHashes: boolean[]; // True if a road is on that face (0-5)
}

// 4 Fundamental Truchet Patterns (Indices: 0-5)
export const PATTERNS = [
    { name: 'EMPTY', conns: [] },
    { name: 'ADJACENT', conns: [[0, 1]] },
    { name: 'MEDIUM', conns: [[0, 2]] },
    { name: 'DIAMETRICAL', conns: [[0, 3]] }
];

export const TILE_REGISTRY: TileConfig[] = [];

// Populate Registry with 24 Variations (4 Patterns * 6 Rotations)
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
 */
export const getNeighborPos = (q: number, r: number, side: number): { q: number, r: number } => {
    const directions = [
        { q: 1, r: -1 }, // Side 0 (Top-Right)
        { q: 0, r: -1 }, // Side 1 (Top-Left)
        { q: -1, r: 0 }, // Side 2 (Left)
        { q: -1, r: 1 }, // Side 3 (Bottom-Left)
        { q: 0, r: 1 },  // Side 4 (Bottom-Right)
        { q: 1, r: 0 }   // Side 5 (Right)
    ];
    return { q: q + directions[side].q, r: r + directions[side].r };
};
