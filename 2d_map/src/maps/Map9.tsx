import React, { useEffect, useRef, useState, useCallback } from 'react';
import type { CSSProperties } from 'react';
import { Canvas } from '../lib/Canvas';
import { Vector2 } from '../lib/Vector2';

// TODO: Track room margin, dont connect room within this margin, maintain distance
// TODO: Maintain grid color index matrix

function mulberry32(a: number) {
    return function () {
        let t = a += 0x6D2B79F5;
        t = Math.imul(t ^ t >>> 15, t | 1);
        t ^= t + Math.imul(t ^ t >>> 7, t | 61);
        return ((t ^ t >>> 14) >>> 0) / 4294967296;
    }
}

// --- HELPER FUNCTIONS FOR MANSION PROCESSING ---

/** Area in grid units (width) */
const rm_gw = (r: Room) => Math.round(r.w / GRID_SIZE);
/** Area in grid units (height) */
const rm_gh = (r: Room) => Math.round(r.h / GRID_SIZE);

/**
 * Identifies clusters of adjacent rooms with the same zone color using BFS.
 * Assigns a unique `islandId` to each room in a cluster.
 */
function identifyIslands(rooms: Room[]) {
    const visited = new Set<Room>();
    let islandIdCounter = 0;
    rooms.forEach(r => { r.islandId = -1; r.showLabel = false; }); // Reset metadata

    rooms.forEach(root => {
        if (visited.has(root)) return;

        const currentId = islandIdCounter++;
        const queue: Room[] = [root];
        visited.add(root);

        while (queue.length > 0) {
            const curr = queue.shift()!;
            curr.islandId = currentId;

            // Check neighbors for same-color adjacency
            rooms.forEach(other => {
                if (visited.has(other) || other.zoneColor !== curr.zoneColor) return;

                const isAdjacent = (
                    (Math.abs(curr.x - (other.x + other.w)) < EPSILON || Math.abs(other.x - (curr.x + curr.w)) < EPSILON) &&
                    Math.min(curr.y + curr.h, other.y + other.h) - Math.max(curr.y, other.y) >= GRID_SIZE / 4
                ) || (
                        (Math.abs(curr.y - (other.y + other.h)) < EPSILON || Math.abs(other.y - (curr.y + curr.h)) < EPSILON) &&
                        Math.min(curr.x + curr.w, other.x + other.w) - Math.max(curr.x, other.x) >= GRID_SIZE / 4
                    );

                if (isAdjacent) {
                    visited.add(other);
                    queue.push(other);
                }
            });
        }
    });
}

/**
 * Assigns deterministic names to islands based on their ID and the seed.
 * Also finds the largest room in each island to host the label.
 */
function assignIslandMetadata(rooms: Room[], seed: number) {
    const islands = new Map<number, Room[]>();
    rooms.forEach(r => {
        if (!islands.has(r.islandId)) islands.set(r.islandId, []);
        islands.get(r.islandId)!.push(r);
    });

    islands.forEach((islandRooms, id) => {
        const nameSeed = seed + id;
        const nameRng = mulberry32(nameSeed);
        const name = islandRooms[0].zoneColor === '#cbd5e1' ? 'Secret Path' : ROOM_NAMES[Math.floor(nameRng() * ROOM_NAMES.length)];

        let totalArea = 0;
        let bestRoom = islandRooms[0];

        islandRooms.forEach(r => {
            r.name = name;
            const area = rm_gw(r) * rm_gh(r);
            totalArea += area;
            if (area > rm_gw(bestRoom) * rm_gh(bestRoom)) bestRoom = r;
        });

        islandRooms.forEach(r => {
            r.totalArea = totalArea;
            r.showLabel = false;
        });
        bestRoom.showLabel = true;
    });
}

/**
 * Iteratively merges tiny islands (area < 3 grid units) into their largest neighbors.
 */
function consolidateTinyRooms(rooms: Room[]) {
    let changed = true;
    while (changed) {
        changed = false;
        const islands = Array.from(new Set(rooms.map(r => r.islandId)));
        for (const id of islands) {
            const islandRooms = rooms.filter(r => r.islandId === id);
            const area = islandRooms.reduce((sum, r) => sum + (rm_gw(r) * rm_gh(r)), 0);

            if (area < 3) {
                const neighbors = new Map<number, number>(); // Map<islandId, sharedPixelLength>
                islandRooms.forEach(curr => {
                    rooms.forEach(other => {
                        if (other.islandId === id) return;
                        let overlap = 0;
                        if (Math.min(curr.x + curr.w, other.x + other.w) - Math.max(curr.x, other.x) >= GRID_SIZE / 4 && (Math.abs(curr.y - (other.y + other.h)) < EPSILON || Math.abs(other.y - (curr.y + curr.h)) < EPSILON))
                            overlap = Math.min(curr.x + curr.w, other.x + other.w) - Math.max(curr.x, other.x);
                        else if (Math.min(curr.y + curr.h, other.y + other.h) - Math.max(curr.y, other.y) >= GRID_SIZE / 4 && (Math.abs(curr.x - (other.x + other.w)) < EPSILON || Math.abs(other.x - (curr.x + curr.w)) < EPSILON))
                            overlap = Math.min(curr.y + curr.h, other.y + other.h) - Math.max(curr.y, other.y);

                        if (overlap > 0) neighbors.set(other.islandId, (neighbors.get(other.islandId) || 0) + overlap);
                    });
                });

                if (neighbors.size > 0) {
                    const targetId = Array.from(neighbors.entries()).sort((a, b) => b[1] - a[1])[0][0];
                    const targetRoom = rooms.find(r => r.islandId === targetId)!;
                    islandRooms.forEach(rm => {
                        rm.islandId = targetId;
                        rm.zoneColor = targetRoom.zoneColor;
                        rm.name = targetRoom.name;
                    });
                    changed = true;
                    break;
                }
            }
        }
    }
}

/**
 * Final pass to ensure area calculation and label visibility are consistent after consolidation.
 */
function refreshIslandLabels(rooms: Room[]) {
    const finalIslands = new Map<number, Room[]>();
    rooms.forEach(r => {
        if (!finalIslands.has(r.islandId)) finalIslands.set(r.islandId, []);
        finalIslands.get(r.islandId)!.push(r);
        r.showLabel = false;
    });

    finalIslands.forEach(island => {
        const totalArea = island.reduce((sum, r) => sum + (rm_gw(r) * rm_gh(r)), 0);
        let bestRoom = island[0];
        island.forEach(rm => {
            rm.totalArea = totalArea;
            if (rm_gw(rm) * rm_gh(rm) > rm_gw(bestRoom) * rm_gh(bestRoom)) bestRoom = rm;
        });
        bestRoom.showLabel = true;
    });
}

/**
 * Scans room boundaries and generates doors between adjacent but different islands.
 */
function generateDoorsForIslands(rooms: Room[]): Door[] {
    const newDoors: Door[] = [];
    const islandPairDoors = new Map<string, Door[]>();

    rooms.forEach((r1, i) => {
        rooms.slice(i + 1).forEach(r2 => {
            if (r1.islandId === r2.islandId) return;
            const hO = Math.min(r1.x + r1.w, r2.x + r2.w) - Math.max(r1.x, r2.x);
            const vO = Math.min(r1.y + r1.h, r2.y + r2.h) - Math.max(r1.y, r2.y);

            let door: Door | null = null;
            if (hO >= GRID_SIZE * 0.75 && (Math.abs(r1.y - (r2.y + r2.h)) < EPSILON || Math.abs(r2.y - (r1.y + r1.h)) < EPSILON)) {
                door = {
                    x: Math.max(r1.x, r2.x) + hO / 2 - 12,
                    y: (Math.abs(r1.y - (r2.y + r2.h)) < EPSILON ? r1.y : r2.y) - 3,
                    w: 24, h: 6
                };
            }
            else if (vO >= GRID_SIZE * 0.75 && (Math.abs(r1.x - (r2.x + r2.w)) < EPSILON || Math.abs(r2.x - (r1.x + r1.w)) < EPSILON)) {
                door = {
                    x: (Math.abs(r1.x - (r2.x + r2.w)) < EPSILON ? r1.x : r2.x) - 3,
                    y: Math.max(r1.y, r2.y) + vO / 2 - 12,
                    w: 6, h: 24
                };
            }

            if (door) {
                const pairId = [r1.islandId, r2.islandId].sort((a, b) => a - b).join('-');
                if (!islandPairDoors.has(pairId)) islandPairDoors.set(pairId, []);
                const existing = islandPairDoors.get(pairId)!;
                if (existing.length < 4) {
                    const minDist = 4 * GRID_SIZE;
                    if (!existing.some(d => Math.sqrt((d.x - door!.x) ** 2 + (d.y - door!.y) ** 2) < minDist)) existing.push(door);
                }
            }
        });
    });

    islandPairDoors.forEach(list => newDoors.push(...list));
    return newDoors;
}

type RoomStatus = 'unexplored' | 'clear' | 'locked';
const Direction = { North: 0, East: 1, South: 2, West: 3 } as const;
type Direction = (typeof Direction)[keyof typeof Direction];

const GRID_SIZE = 20;
const EPSILON = 1; // Tolerance for grid-aligned adjacency checks

const ROOM_NAMES = [
    'Entrance Hall', 'Dining Room', 'Gallery', 'Library', 'Kitchen',
    'Save Room', 'Master BR', 'Attic', 'Basement', 'Laboratory',
    'Corridor', 'Study', 'Greenhouse', 'Storage', 'Observatory', 'Salon'
];

const ZONE_COLORS = [
    '#3b82f6', // Blue (Standard)
    '#8b5cf6', // Violet (Upper Floor)
    '#ec4899', // Pink (Gallery)
    '#10b981', // Emerald (Garden)
    '#f59e0b', // Amber (Laboratory)
    '#ef4444'  // Crimson (Restricted)
];



function rectRectOverlap(r1: { x: number, y: number, w: number, h: number }, r2: { x: number, y: number, w: number, h: number }) {
    // Standard grid collision with small epsilon to allow neighbor detection without false overlap
    return !(r1.x >= r2.x + r2.w - EPSILON || r1.x + r1.w <= r2.x + EPSILON || r1.y >= r2.y + r2.h - EPSILON || r1.y + r1.h <= r2.y + EPSILON);
}

class Room {
    x: number; // Top-left x coordinate
    y: number; // Top-left y coordinate
    w: number; // Width
    h: number; // Height
    name: string;
    status: RoomStatus;
    zoneColor: string;

    showLabel: boolean;
    totalArea: number;
    islandId: number;
    isCorridor: boolean;

    get topLeft() { return new Vector2(this.x, this.y); }
    get topRight() { return new Vector2(this.x + this.w, this.y); }
    get bottomLeft() { return new Vector2(this.x, this.y + this.h); }
    get bottomRight() { return new Vector2(this.x + this.w, this.y + this.h); }
    get center() { return new Vector2(this.x + this.w / 2, this.y + this.h / 2); }

    constructor(x: number, y: number, w: number, h: number, zoneColor: string, isCorridor: boolean = false) {
        this.x = x;
        this.y = y;
        this.w = w;
        this.h = h;
        this.name = '';
        this.status = 'unexplored';
        this.zoneColor = zoneColor;
        this.isCorridor = isCorridor;
        this.showLabel = false;
        this.totalArea = 0;
        this.islandId = -1;
    }

    draw(canvas: Canvas, allRooms: Room[]) {
        const baseColor = this.status === 'unexplored' ? this.zoneColor :
            this.status === 'clear' ? '#22c55e' : '#ef4444';

        canvas.rect(this.x, this.y, this.w, this.h, { fill: baseColor + '44' });

        if (false) {
            this.drawSides(canvas, allRooms, baseColor);
        }

        if (this.showLabel && this.name) {
            // Draw Room Name
            canvas.text(this.name.toUpperCase(), new Vector2(this.x + this.w / 2, this.y + this.h / 2 - 7), {
                fill: '#ffffff',
                font: 'bold 11px "Courier New", monospace',
                align: 'center'
            });

            // Draw Total Area (e.g., AREA: 12)
            canvas.text(`AREA: ${this.totalArea}`, new Vector2(this.x + this.w / 2, this.y + this.h / 2 + 8), {
                fill: '#ffffff',
                alpha: 0.8,
                font: '10px "Courier New", monospace',
                align: 'center'
            });
        }
    }

    drawSides(canvas: Canvas, allRooms: Room[], color: string) {
        const strokeOpts = { stroke: color + 'aa', lineWidth: 1.5 };

        if (!this.hasSideMerge(allRooms, Direction.North)) {
            canvas.line(new Vector2(this.x, this.y), new Vector2(this.x + this.w, this.y), strokeOpts);
        }
        if (!this.hasSideMerge(allRooms, Direction.East)) {
            canvas.line(new Vector2(this.x + this.w, this.y), new Vector2(this.x + this.w, this.y + this.h), strokeOpts);
        }
        if (!this.hasSideMerge(allRooms, Direction.South)) {
            canvas.line(new Vector2(this.x + this.w, this.y + this.h), new Vector2(this.x, this.y + this.h), strokeOpts);
        }
        if (!this.hasSideMerge(allRooms, Direction.West)) {
            canvas.line(new Vector2(this.x, this.y + this.h), new Vector2(this.x, this.y), strokeOpts);
        }
    }

    // check if the room has a side merge with another room
    hasSideMerge(allRooms: Room[], side: Direction): boolean {
        for (let other of allRooms) {
            // Only merge with DIFFERENT rooms of the SAME zone color
            if (other === this || other.zoneColor !== this.zoneColor) continue;

            if (side === Direction.North && Math.abs((other.y + other.h) - this.y) < EPSILON) {
                // North: Neighbor's bottom (y+h) touches our top (y)
                // Need at least half a cell overlap to suppress a visual wall
                if (Math.min(this.x + this.w, other.x + other.w) - Math.max(this.x, other.x) >= GRID_SIZE / 2) return true;
            }
            if (side === Direction.South && Math.abs(other.y - (this.y + this.h)) < EPSILON) {
                // South: Neighbor's top (y) touches our bottom (y+h)
                if (Math.min(this.x + this.w, other.x + other.w) - Math.max(this.x, other.x) >= GRID_SIZE / 2) return true;
            }
            if (side === Direction.East && Math.abs(other.x - (this.x + this.w)) < EPSILON) {
                // East: Neighbor's left (x) touches our right (x+w)
                if (Math.min(this.y + this.h, other.y + other.h) - Math.max(this.y, other.y) >= GRID_SIZE / 2) return true;
            }
            if (side === Direction.West && Math.abs((other.x + other.w) - this.x) < EPSILON) {
                // West: Neighbor's right (x+w) touches our left (x)
                if (Math.min(this.y + this.h, other.y + other.h) - Math.max(this.y, other.y) >= GRID_SIZE / 2) return true;
            }
        }
        return false;
    }

    contains(px: number, py: number): boolean {
        return px >= this.x && px <= this.x + this.w && py >= this.y && py <= this.y + this.h;
    }
}

interface Door {
    x: number;
    y: number;
    w: number;
    h: number;
}

export default function Map9({ width = 800, height = 800 }: { width?: number, height?: number }) {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const canvasInstanceRef = useRef<Canvas | null>(null);
    const [rooms, setRooms] = useState<Room[]>([]);
    const [doors, setDoors] = useState<Door[]>([]);
    const [seed, setSeed] = useState(12345);
    const [targetStep, setTargetStep] = useState(35);
    const [renderTrigger, setRenderTrigger] = useState(0);

    /**
     * post-generation pipeline that organizes raw rooms into a coherent mansion layout.
     */
    const processMansionData = useCallback((currentRooms: Room[]) => {
        identifyIslands(currentRooms);
        assignIslandMetadata(currentRooms, seed);
        consolidateTinyRooms(currentRooms);
        refreshIslandLabels(currentRooms);

        const newDoors = generateDoorsForIslands(currentRooms);
        setDoors(newDoors);
    }, [seed]);

    const applyVoidFill = useCallback(() => {
        const newRooms = [...rooms];
        let voidFilled = true;
        let pass = 0;
        while (voidFilled && pass < 5) {
            voidFilled = false;
            pass++;
            const minX = Math.min(...newRooms.map(r => r.x));
            const minY = Math.min(...newRooms.map(r => r.y));
            const maxX = Math.max(...newRooms.map(r => r.x + r.w));
            const maxY = Math.max(...newRooms.map(r => r.y + r.h));

            for (let x = minX; x < maxX; x += GRID_SIZE) {
                for (let y = minY; y < maxY; y += GRID_SIZE) {
                    const centerX = x + GRID_SIZE / 2;
                    const centerY = y + GRID_SIZE / 2;
                    if (!newRooms.some(r => r.contains(centerX, centerY))) {
                        const neighbors = [
                            { dx: 0, dy: -GRID_SIZE }, { dx: GRID_SIZE, dy: 0 },
                            { dx: 0, dy: GRID_SIZE }, { dx: -GRID_SIZE, dy: 0 }
                        ];
                        const colorCounts = new Map<string, number>();
                        neighbors.forEach(n => {
                            const nx_val = centerX + n.dx;
                            const ny_val = centerY + n.dy;
                            const neighboringRoom = newRooms.find(r => r.contains(nx_val, ny_val));
                            if (neighboringRoom) colorCounts.set(neighboringRoom.zoneColor, (colorCounts.get(neighboringRoom.zoneColor) || 0) + 1);
                        });
                        for (const [color, count] of colorCounts.entries()) {
                            if (count >= 3) {
                                newRooms.push(new Room(x, y, GRID_SIZE, GRID_SIZE, color));
                                voidFilled = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
        processMansionData(newRooms);
        setRooms(newRooms);
    }, [rooms, processMansionData]);

    const trimDeadEnds = useCallback(() => {
        const newRooms = [...rooms].map(r => new Room(r.x, r.y, r.w, r.h, r.zoneColor, r.isCorridor));
        let overallChanged = false;
        let changed = true;

        const checkAir = (x: number, y: number, w: number, h: number, exclude: Room) => {
            const target = { x, y, w, h };
            return !newRooms.some(r => r !== exclude && rectRectOverlap(target, r));
        };

        while (changed) {
            changed = false;
            for (let i = newRooms.length - 1; i >= 0; i--) {
                const r = newRooms[i];
                if (r.w < GRID_SIZE || r.h < GRID_SIZE) {
                    newRooms.splice(i, 1);
                    changed = true;
                    overallChanged = true;
                    continue;
                }

                // Check Vertical Danglers
                if (Math.abs(r.w - GRID_SIZE) < EPSILON) {
                    if (checkAir(r.x, r.y - GRID_SIZE, GRID_SIZE, GRID_SIZE, r) && 
                        checkAir(r.x - GRID_SIZE, r.y, GRID_SIZE, GRID_SIZE, r) && 
                        checkAir(r.x + GRID_SIZE, r.y, GRID_SIZE, GRID_SIZE, r)) {
                        r.y += GRID_SIZE; r.h -= GRID_SIZE;
                        changed = true; overallChanged = true;
                    }
                    else if (checkAir(r.x, r.y + r.h, GRID_SIZE, GRID_SIZE, r) && 
                             checkAir(r.x - GRID_SIZE, r.y + r.h - GRID_SIZE, GRID_SIZE, GRID_SIZE, r) && 
                             checkAir(r.x + GRID_SIZE, r.y + r.h - GRID_SIZE, GRID_SIZE, GRID_SIZE, r)) {
                        r.h -= GRID_SIZE;
                        changed = true; overallChanged = true;
                    }
                }
                // Check Horizontal Danglers
                if (Math.abs(r.h - GRID_SIZE) < EPSILON) {
                    if (checkAir(r.x - GRID_SIZE, r.y, GRID_SIZE, GRID_SIZE, r) && 
                        checkAir(r.x, r.y - GRID_SIZE, GRID_SIZE, GRID_SIZE, r) && 
                        checkAir(r.x, r.y + GRID_SIZE, GRID_SIZE, GRID_SIZE, r)) {
                        r.x += GRID_SIZE; r.w -= GRID_SIZE;
                        changed = true; overallChanged = true;
                    }
                    else if (checkAir(r.x + r.w, r.y, GRID_SIZE, GRID_SIZE, r) && 
                             checkAir(r.x + r.w - GRID_SIZE, r.y - GRID_SIZE, GRID_SIZE, GRID_SIZE, r) && 
                             checkAir(r.x + r.w - GRID_SIZE, r.y + GRID_SIZE, GRID_SIZE, GRID_SIZE, r)) {
                        r.w -= GRID_SIZE;
                        changed = true; overallChanged = true;
                    }
                }

                if (r.w < GRID_SIZE || r.h < GRID_SIZE) {
                    newRooms.splice(i, 1);
                    changed = true; overallChanged = true;
                }
            }
        }

        if (overallChanged) {
            processMansionData(newRooms);
            setRooms(newRooms);
        }
    }, [rooms, processMansionData]);

    const generateMansion = useCallback(() => {
        // 1. Initialize Seeded Random Number Generator
        const rng = mulberry32(seed);
        const rand = () => rng();

        const newRooms: Room[] = [];

        // --- STAGE 1: ANCHOR ROOMS (Large Hubs) ---
        const anchorCount = 3 + Math.floor(rand() * 4); // 3 to 6 anchors
        const anchors: Room[] = [];

        for (let i = 0; i < anchorCount; i++) {
            if (newRooms.length >= targetStep) break;
            const gw = 4 + Math.floor(rand() * 7); // 4 to 10 grid units
            const gh = 4 + Math.floor(rand() * 7);
            const rw = gw * GRID_SIZE;
            const rh = gh * GRID_SIZE;

            // Random position within safe bounds
            const rx = (1 + Math.floor(rand() * Math.floor((width - rw - 40) / GRID_SIZE))) * GRID_SIZE;
            const ry = (1 + Math.floor(rand() * Math.floor((height - rh - 40) / GRID_SIZE))) * GRID_SIZE;

            const cand = new Room(rx, ry, rw, rh, ZONE_COLORS[i % ZONE_COLORS.length]);

            // Simple overlap check: anchors shouldn't overlap too much
            if (!newRooms.some(r => rectRectOverlap({ x: rx, y: ry, w: rw, h: rh }, r))) {
                newRooms.push(cand);
                anchors.push(cand);
            }
        }

        // --- STAGE 2: MANHATTAN CORRIDORS (MST-style Connectivity) ---
        // Greedy Nearest-Neighbor approach ensures a fully connected skeleton from the start.
        const connectedAnchors = [anchors[0]];
        const disconnectedAnchors = anchors.slice(1);

        while (disconnectedAnchors.length > 0 && newRooms.length < targetStep) {
            let bestDist = Infinity;
            let bestPair: [Room, Room] | null = null;
            let bestIndex = -1;

            // Find the closest (one from connected, one from disconnected)
            for (let cA of connectedAnchors) {
                for (let idx = 0; idx < disconnectedAnchors.length; idx++) {
                    const dA = disconnectedAnchors[idx];
                    const dist = Math.abs(cA.x - dA.x) + Math.abs(cA.y - dA.y);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestPair = [cA, dA];
                        bestIndex = idx;
                    }
                }
            }

            if (bestPair && bestIndex !== -1) {
                const [a1, a2] = bestPair;
                const c1 = { x: Math.floor((a1.x + a1.w / 2) / GRID_SIZE), y: Math.floor((a1.y + a1.h / 2) / GRID_SIZE) };
                const c2 = { x: Math.floor((a2.x + a2.w / 2) / GRID_SIZE), y: Math.floor((a2.y + a2.h / 2) / GRID_SIZE) };

                // Scan Horizontal Gap
                const startX = Math.min(c1.x, c2.x);
                const endX = Math.max(c1.x, c2.x);
                let hSegStart = -1;
                for (let x = startX; x <= endX; x++) {
                    const cand = { x: x * GRID_SIZE, y: c1.y * GRID_SIZE, w: GRID_SIZE, h: GRID_SIZE };
                    const isOccupied = newRooms.some(r => rectRectOverlap(cand, r));
                    if (!isOccupied) {
                        if (hSegStart === -1) hSegStart = x;
                    } else {
                        if (hSegStart !== -1) {
                            if (newRooms.length < targetStep) {
                                const r = new Room(hSegStart * GRID_SIZE, c1.y * GRID_SIZE, (x - hSegStart) * GRID_SIZE, GRID_SIZE, '#cbd5e1', true);
                                newRooms.push(r);
                            }
                            hSegStart = -1;
                        }
                    }
                }
                if (hSegStart !== -1 && newRooms.length < targetStep) {
                    newRooms.push(new Room(hSegStart * GRID_SIZE, c1.y * GRID_SIZE, (endX - hSegStart + 1) * GRID_SIZE, GRID_SIZE, '#cbd5e1', true));
                }

                // Scan Vertical Gap
                const startY = Math.min(c1.y, c2.y);
                const endY = Math.max(c1.y, c2.y);
                let vSegStart = -1;
                for (let y = startY; y <= endY; y++) {
                    const cand = { x: c2.x * GRID_SIZE, y: y * GRID_SIZE, w: GRID_SIZE, h: GRID_SIZE };
                    const isOccupied = newRooms.some(r => rectRectOverlap(cand, r));
                    if (!isOccupied) {
                        if (vSegStart === -1) vSegStart = y;
                    } else {
                        if (vSegStart !== -1) {
                            if (newRooms.length < targetStep) {
                                newRooms.push(new Room(c2.x * GRID_SIZE, vSegStart * GRID_SIZE, GRID_SIZE, (y - vSegStart) * GRID_SIZE, '#cbd5e1', true));
                            }
                            vSegStart = -1;
                        }
                    }
                }
                if (vSegStart !== -1 && newRooms.length < targetStep) {
                    newRooms.push(new Room(c2.x * GRID_SIZE, vSegStart * GRID_SIZE, GRID_SIZE, (endY - vSegStart + 1) * GRID_SIZE, '#cbd5e1', true));
                }

                // Add to connected set
                connectedAnchors.push(disconnectedAnchors[bestIndex]);
                disconnectedAnchors.splice(bestIndex, 1);
            }
        }

        // --- STAGE 3: PROCEDURAL EXPANSION ---
        let attempts = 0;
        while (newRooms.length < targetStep && attempts < 5000) {
            // A. Pick a random existing room to grow from
            const parent = newRooms[Math.floor(rand() * newRooms.length)];
            
            // B. Pick a random side (0: N, 1: E, 2: S, 3: W)
            const side = Math.floor(rand() * 4);
            // C. Determine room footprint (1 to 3 grid cells)
            const gridW = 1 + Math.floor(rand() * 3), gridH = 1 + Math.floor(rand() * 3);
            const nw = gridW * GRID_SIZE, nh = gridH * GRID_SIZE;
            let nx = 0, ny = 0;

            // D. Align the new room to the parent's chosen side
            if (side === Direction.North) {
                nx = parent.x + (Math.floor((parent.w / GRID_SIZE - gridW) / 2)) * GRID_SIZE; ny = parent.y - nh;
            }
            else if (side === Direction.East) {
                nx = parent.x + parent.w; ny = parent.y + (Math.floor((parent.h / GRID_SIZE - gridH) / 2)) * GRID_SIZE;
            }
            else if (side === Direction.South) {
                nx = parent.x + (Math.floor((parent.w / GRID_SIZE - gridW) / 2)) * GRID_SIZE; ny = parent.y + parent.h;
            }
            else if (side === Direction.West) {
                nx = parent.x - nw; ny = parent.y + (Math.floor((parent.h / GRID_SIZE - gridH) / 2)) * GRID_SIZE;
            }

            // E. Validate against bounds and existing rooms
            const cand = { x: nx, y: ny, w: nw, h: nh };
            if (nx > 20 && nx + nw < width - 20 && ny > 20 && ny + nh < height - 20) {
                if (!newRooms.some(r => rectRectOverlap(cand, r))) {
                    // F. Assign zone: corridors always spawn a NEW colored zone
                    const zoneColor = parent.isCorridor ? 
                        ZONE_COLORS[Math.floor(rand() * ZONE_COLORS.length)] : 
                        (rand() > 0.6 ? parent.zoneColor : ZONE_COLORS[Math.floor(rand() * ZONE_COLORS.length)]);
                    newRooms.push(new Room(nx, ny, nw, nh, zoneColor));
                }
            }
            attempts++;
        }

        // 4. Update data structures and recalculate islands/doors
        processMansionData(newRooms);
        setRooms(newRooms);
    }, [width, height, seed, targetStep, processMansionData]);

    const handleCanvasClick = (e: React.MouseEvent<HTMLCanvasElement>) => {
        if (!canvasInstanceRef.current) return;
        const pos = canvasInstanceRef.current.getMousePos(e.nativeEvent);
        const clicked = [...rooms].reverse().find(r => r.contains(pos.x, pos.y));
        if (clicked) {
            clicked.status = (clicked.status === 'unexplored') ? 'clear' : (clicked.status === 'clear' ? 'locked' : 'unexplored');
            setRooms([...rooms]);
        }
    };

    useEffect(() => { generateMansion(); }, [generateMansion, seed]);

    // 1. Unified Interaction Setup
    useEffect(() => {
        const canvasEl = canvasRef.current;
        if (!canvasEl) return;

        const instance = new Canvas(canvasEl);
        instance.initInteractions({
            onInteraction: () => setRenderTrigger(t => t + 1)
        });
        canvasInstanceRef.current = instance;

        return () => {
            instance.destroyInteractions();
        };
    }, []);

    // 2. Separate Drawing Loop
    useEffect(() => {
        const canvas = canvasInstanceRef.current;
        if (!canvas) return;

        canvas.clear();

        // Background
        canvas.rect(-1000, -1000, width + 2000, height + 2000, { fill: '#060b13' });
        canvas.grid(GRID_SIZE, 'rgba(59, 130, 246, 0.08)');

        rooms.forEach(r => r.draw(canvas, rooms));

        // Doors
        doors.forEach(d => {
            canvas.rect(d.x, d.y, d.w, d.h, { fill: '#fff' });
        });
    }, [rooms, doors, width, height, renderTrigger]);

    return (
        <div style={UI_STYLES.container}>
            <div style={UI_STYLES.panel}>
                <div style={UI_STYLES.group}>
                    <label style={UI_STYLES.label}>MAP SEED</label>
                    <div style={UI_STYLES.inputRow}>
                        <input
                            type="number"
                            value={seed}
                            onChange={(e) => setSeed(parseInt(e.target.value) || 0)}
                            style={UI_STYLES.input}
                        />
                        <button onClick={() => setSeed(Math.floor(Math.random() * 1000000))} style={UI_STYLES.buttonPrimary}>🎲</button>
                    </div>
                </div>

                <div style={UI_STYLES.group}>
                    <label style={UI_STYLES.label}>GENERATION STEP: {rooms.length}</label>
                    <input
                        type="range"
                        min="1"
                        max="200"
                        value={targetStep}
                        onChange={(e) => setTargetStep(parseInt(e.target.value))}
                        style={UI_STYLES.slider}
                    />
                    <div style={UI_STYLES.inputRow}>
                        <button onClick={() => setTargetStep(Math.max(1, targetStep - 1))} style={UI_STYLES.buttonAlt}>PREV</button>
                        <button onClick={() => setTargetStep(targetStep + 1)} style={UI_STYLES.buttonAlt}>NEXT</button>
                    </div>
                    <div style={UI_STYLES.inputRow}>
                        <button onClick={applyVoidFill} style={UI_STYLES.buttonAlt}>FILL VOIDS</button>
                        <button onClick={trimDeadEnds} style={UI_STYLES.buttonAlt}>TRIM DEAD ENDS</button>
                    </div>
                </div>
            </div>
            <canvas
                ref={canvasRef}
                width={width}
                height={height}
                onClick={handleCanvasClick}
                style={UI_STYLES.canvas}
            />
        </div>
    );
}

const UI_STYLES = {
    container: {
        position: 'relative',
        width: '100%',
        height: '100%'
    } as CSSProperties,
    panel: {
        position: 'absolute',
        top: 20,
        left: 20,
        background: 'rgba(6, 11, 19, 0.95)',
        backdropFilter: 'blur(12px)',
        padding: '20px',
        borderRadius: '12px',
        color: 'white',
        fontFamily: '"Courier New", monospace',
        display: 'flex',
        flexDirection: 'column',
        gap: '16px',
        zIndex: 10,
        border: '1px solid #3b82f6',
        width: '240px',
        boxShadow: '0 8px 32px rgba(0, 0, 0, 0.4)'
    } as CSSProperties,
    group: {
        display: 'flex',
        flexDirection: 'column',
        gap: '6px'
    } as CSSProperties,
    label: {
        fontSize: '10px',
        color: '#3b82f6',
        fontWeight: 'bold',
        letterSpacing: '1px',
        textTransform: 'uppercase'
    } as CSSProperties,
    inputRow: {
        display: 'flex',
        gap: '6px'
    } as CSSProperties,
    input: {
        background: '#0a101f',
        border: '1px solid #3b82f6',
        color: 'white',
        padding: '6px 10px',
        borderRadius: '6px',
        width: '100%',
        outline: 'none',
        fontSize: '13px'
    } as CSSProperties,
    slider: {
        width: '100%',
        accentColor: '#3b82f6',
        margin: '4px 0',
        cursor: 'pointer'
    } as CSSProperties,
    buttonAlt: {
        background: '#1e293b',
        border: '1px solid #3b82f6',
        color: 'white',
        padding: '8px',
        borderRadius: '6px',
        cursor: 'pointer',
        fontSize: '11px',
        fontWeight: 'bold',
        flex: 1,
        transition: 'all 0.2s ease'
    } as CSSProperties,
    buttonPrimary: {
        background: '#3b82f6',
        border: 'none',
        color: 'white',
        padding: '6px 12px',
        borderRadius: '6px',
        cursor: 'pointer',
        fontWeight: 'bold'
    } as CSSProperties,
    buttonAction: {
        background: '#10b981',
        color: '#fff',
        border: 'none',
        padding: '12px',
        borderRadius: '6px',
        cursor: 'pointer',
        fontWeight: 'bold',
        marginTop: '8px',
        letterSpacing: '1px'
    } as CSSProperties,
    canvas: {
        borderRadius: 12,
        display: 'block',
        margin: '0 auto',
        boxShadow: '0 10px 40px rgba(59, 130, 246, 0.5)',
        cursor: 'pointer',
        background: '#060b13'
    } as React.CSSProperties
};
