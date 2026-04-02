import { Vector2 } from '../lib/Vector2';

export function mulberry32(a: number) {
    return function() {
        let t = a += 0x6D2B79F5;
        t = Math.imul(t ^ (t >>> 15), t | 1);
        t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
}

export interface Node {
    id: number;
    pos: Vector2;
    splitNumber: number;
    degree: number;
    connectedTo: number[];
    level: number; // 0: Yellow, 1: Red, 2: Orange
}

export interface Edge {
    u: number;
    v: number;
    level: number; // 0: Blue, 1: Red-generated, 2: Orange-generated
}

export interface GenConfig {
    minDist: number;
    maxDist: number;
    connectionRadius: number;
    minRoadLen: number;
    maxRoadLen: number;
    minAngle: number; // in degrees
    maxDegree: number;
    splitChance: number;
}

export class SpatialHash {
    private grid: Map<string, number[]> = new Map();
    private cellSize: number;

    constructor(cellSize: number) {
        this.cellSize = cellSize;
    }

    private getKey(pos: Vector2): string {
        const gx = Math.floor(pos.x / this.cellSize);
        const gy = Math.floor(pos.y / this.cellSize);
        return `${gx},${gy}`;
    }

    insert(id: number, pos: Vector2) {
        const key = this.getKey(pos);
        if (!this.grid.has(key)) this.grid.set(key, []);
        this.grid.get(key)!.push(id);
    }

    query(pos: Vector2, radius: number): number[] {
        const results: number[] = [];
        const startX = Math.floor((pos.x - radius) / this.cellSize);
        const endX = Math.floor((pos.x + radius) / this.cellSize);
        const startY = Math.floor((pos.y - radius) / this.cellSize);
        const endY = Math.floor((pos.y + radius) / this.cellSize);

        for (let x = startX; x <= endX; x++) {
            for (let y = startY; y <= endY; y++) {
                const key = `${x},${y}`;
                const ids = this.grid.get(key);
                if (ids) results.push(...ids);
            }
        }
        return results;
    }

    clear() {
        this.grid.clear();
    }
}

/**
 * Calculates distance from point P to segment [A, B]
 */
export function distToSegment(p: Vector2, a: Vector2, b: Vector2): { dist: number, point: Vector2, t: number } {
    const l2 = Vector2.dist(a, b) ** 2;
    if (l2 === 0) return { dist: Vector2.dist(p, a), point: a.clone(), t: 0 };
    let t = ((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / l2;
    t = Math.max(0, Math.min(1, t));
    const projection = new Vector2(a.x + t * (b.x - a.x), a.y + t * (b.y - a.y));
    return { dist: Vector2.dist(p, projection), point: projection, t };
}

/**
 * Checks if segment (p1, p2) intersects with (p3, p4)
 */
export function intersect(p1: Vector2, p2: Vector2, p3: Vector2, p4: Vector2): boolean {
    const ccw = (a: Vector2, b: Vector2, c: Vector2) => {
        return (c.y - a.y) * (b.x - a.x) > (b.y - a.y) * (c.x - a.x);
    };
    // Standard segment intersection check
    // We use a small epsilon to avoid returning true for segments that share an endpoint
    const res = ccw(p1, p3, p4) !== ccw(p2, p3, p4) && ccw(p1, p2, p3) !== ccw(p1, p2, p4);
    
    if (!res) return false;

    // Check if they share an endpoint - if so, it's not a "crossing"
    const epsilon = 0.001;
    const isEndpoint = (a: Vector2, b: Vector2) => Vector2.dist(a, b) < epsilon;
    if (isEndpoint(p1, p3) || isEndpoint(p1, p4) || isEndpoint(p2, p3) || isEndpoint(p2, p4)) return false;

    return true;
}

export function generateNodes(seeds: { pos: Vector2, s: number }[], config: GenConfig, seedValue: number): Node[] {
    // Simple Mulberry32 PRNG
    const random = () => {
        let t = (seedValue += 0x6D2B79F5);
        t = Math.imul(t ^ (t >>> 15), t | 1);
        t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };

    const nodes: Node[] = [];
    const open: Node[] = [];
    const spatial = new SpatialHash(config.maxDist);

    let idCounter = 0;

    for (const seed of seeds) {
        const node: Node = {
            id: idCounter++,
            pos: seed.pos.clone(),
            splitNumber: seed.s,
            degree: 0,
            connectedTo: [],
            level: 0
        };
        nodes.push(node);
        open.push(node);
        spatial.insert(node.id, node.pos);
    }

    while (open.length > 0) {
        const curr = open.shift()!;
        const s = curr.splitNumber;
        
        let shouldSplit = false;
        if (s >= 1) shouldSplit = true;
        else if (s > 0 && random() < s) shouldSplit = true;

        if (shouldSplit) {
            const numCandidates = 10 + Math.floor(random() * 7);
            for (let i = 0; i < numCandidates; i++) {
                const angle = random() * Math.PI * 2;
                const dist = config.minDist + random() * (config.maxDist - config.minDist);
                const pos = new Vector2(
                    curr.pos.x + Math.cos(angle) * dist,
                    curr.pos.y + Math.sin(angle) * dist
                );

                // Check distance from ALL other points
                const nearby = spatial.query(pos, config.minDist);
                let tooClose = false;
                for (const otherId of nearby) {
                    if (Vector2.dist(pos, nodes[otherId].pos) < config.minDist) {
                        tooClose = true;
                        break;
                    }
                }

                if (!tooClose) {
                    const nextNode: Node = {
                        id: idCounter++,
                        pos: pos,
                        splitNumber: s - 1,
                        degree: 0,
                        connectedTo: [],
                        level: 0
                    };
                    nodes.push(nextNode);
                    open.push(nextNode);
                    spatial.insert(nextNode.id, nextNode.pos);
                }
            }
        }
    }

    return nodes;
}

export function generateEdges(allNodes: Node[], config: GenConfig, initialEdges: Edge[], targetLevel: number, seed: number, startNodeIds?: number[]): Edge[] {
    const prng = mulberry32(seed);
    const rand = () => prng();
    
    const edges = [...initialEdges];
    const nodeMap = new Map<number, Node>();
    for (const n of allNodes) {
        nodeMap.set(n.id, n);
    }

    const openNodes = startNodeIds 
        ? startNodeIds.map(id => nodeMap.get(id)).filter(Boolean) as Node[]
        : [...allNodes].filter(n => n.level === targetLevel);

    const minAngleRad = config.minAngle * (Math.PI / 180);
    
    // Initialize spatial hash for performance
    const spatial = new SpatialHash(config.maxRoadLen);
    for (const n of allNodes) {
        spatial.insert(n.id, n.pos);
    }

    // Helper to get edge vector using the Map
    const getEdgeDir = (uId: number, vId: number) => {
        const u = nodeMap.get(uId);
        const v = nodeMap.get(vId);
        if (!u || !v) return new Vector2(0, 0);
        return v.pos.clone().sub(u.pos).normalize();
    };

    while (openNodes.length > 0) {
        const idx = Math.floor(rand() * openNodes.length);
        const startNode = openNodes[idx];
        
        if (startNode.degree >= config.maxDegree) {
            openNodes.splice(idx, 1);
            continue;
        }

        // Try to create a street
        let curr = startNode;
        let prevDir: Vector2 | null = null;
        let streetCreated = false;

        while (curr.degree < config.maxDegree) {
            const candidateIds = spatial.query(curr.pos, config.maxRoadLen);
            const candidates = candidateIds.map(id => nodeMap.get(id)!).filter(n => {
                if (n.id === curr.id) return false;
                if (n.degree >= config.maxDegree) return false;
                
                const dist = Vector2.dist(curr.pos, n.pos);
                if (dist < config.minRoadLen || dist > config.maxRoadLen) return false;

                // Constraint: No doubling up
                if (curr.connectedTo.includes(n.id)) return false;

                // Constraint: No crossing
                for (const edge of edges) {
                    const u = nodeMap.get(edge.u);
                    const v = nodeMap.get(edge.v);
                    if (!u || !v) continue;

                    if (intersect(curr.pos, n.pos, u.pos, v.pos)) {
                        return false;
                    }
                }

                // Constraint: No triangles
                // If curr and n share a neighbor, it forms a triangle
                const sharedNeighbor = curr.connectedTo.some(nbId => n.connectedTo.includes(nbId));
                if (sharedNeighbor) return false;

                // Constraint: No small angles
                const dir = n.pos.clone().sub(curr.pos).normalize();
                
                // Check angles with existing edges of START node
                for (const nbId of curr.connectedTo) {
                    const existingDir = getEdgeDir(curr.id, nbId);
                    const dot = dir.dot(existingDir);
                    if (dot > Math.cos(minAngleRad)) return false; // dot > cos(angle) means actual angle < minAngle
                }

                // Check angles with existing edges of TARGET node
                for (const nbId of n.connectedTo) {
                    const existingDir = getEdgeDir(n.id, nbId);
                    const dot = dir.clone().mult(-1).dot(existingDir); // reverse dir because we are at N looking at curr
                    if (dot > Math.cos(minAngleRad)) return false;
                }

                return true;
            });

            if (candidates.length === 0) break;

            // Score candidates
            // 1. Alignment with prevDir (straightness)
            // 2. Distance (shorter preferred but not too much)
            let bestCandidate = candidates[0];
            let bestScore = -Infinity;

            for (const cand of candidates) {
                const dir = cand.pos.clone().sub(curr.pos).normalize();
                const dist = Vector2.dist(curr.pos, cand.pos);
                
                let alignment = 0;
                if (prevDir) {
                    alignment = dir.dot(prevDir);
                } else {
                    alignment = 1.0; 
                }

                // Increased weight on distance penalty (prefer shorter)
                // Alignment still matters for street straightness
                const score = (alignment * 5) - (dist / config.connectionRadius) * 8;
                
                if (score > bestScore) {
                    bestScore = score;
                    bestCandidate = cand;
                }
            }

            // Create edge
            const edge: Edge = { u: curr.id, v: bestCandidate.id, level: targetLevel };
            edges.push(edge);
            
            curr.connectedTo.push(bestCandidate.id);
            curr.degree++;
            bestCandidate.connectedTo.push(curr.id);
            bestCandidate.degree++;

            prevDir = bestCandidate.pos.clone().sub(curr.pos).normalize();
            curr = bestCandidate;
            streetCreated = true;

            // If we reached a dead-end or max degree, stop street
            if (curr.degree >= config.maxDegree) break;
        }

        if (!streetCreated) {
            openNodes.splice(idx, 1);
        }
    }

    // --- NEW: Snap Dead Ends to Edges (Edge Splitting) ---
    // This implements the "extra step (splitting edges)" mentioned in the algorithm description.
    const snapDist = config.minDist * 0.8;

    for (let i = 0; i < allNodes.length; i++) {
        const node = allNodes[i];
        if (node.degree !== 1) continue; // Only snap dead ends

        let bestEdgeIdx = -1;
        let bestPoint: Vector2 | null = null;
        let minDist = snapDist;

        for (let j = 0; j < edges.length; j++) {
            const edge = edges[j];
            if (edge.u === node.id || edge.v === node.id) continue;

            const u = nodeMap.get(edge.u);
            const v = nodeMap.get(edge.v);
            if (!u || !v) continue;

            const res = distToSegment(node.pos, u.pos, v.pos);
            if (res.dist < minDist && res.t > 0.1 && res.t < 0.9) {
                // Check if the new segment would cross anything else
                let crosses = false;
                for (const otherEdge of edges) {
                    const ou = nodeMap.get(otherEdge.u);
                    const ov = nodeMap.get(otherEdge.v);
                    if (!ou || !ov) continue;

                    if (intersect(node.pos, res.point, ou.pos, ov.pos)) {
                        crosses = true;
                        break;
                    }
                }
                if (!crosses) {
                    minDist = res.dist;
                    bestEdgeIdx = j;
                    bestPoint = res.point;
                }
            }
        }

        if (bestEdgeIdx !== -1 && bestPoint) {
            const edge = edges[bestEdgeIdx];
            const uId = edge.u;
            const vId = edge.v;
            
            // Split edge: remove UV, add UN, NV, AN
            const newNodeId = Math.max(...Array.from(nodeMap.keys()), 0) + 1; // Correct ID to avoid collisions
            const newNode: Node = {
                id: newNodeId,
                pos: bestPoint,
                splitNumber: 0,
                degree: 3,
                connectedTo: [uId, vId, node.id],
                level: edge.level
            };
            allNodes.push(newNode);
            nodeMap.set(newNode.id, newNode);

            // Update original node A
            node.connectedTo.push(newNode.id);
            node.degree++;

            // Replace edge UV with UN and NV
            edges.splice(bestEdgeIdx, 1);
            edges.push({ u: uId, v: newNode.id, level: edge.level });
            edges.push({ u: vId, v: newNode.id, level: edge.level });
            edges.push({ u: node.id, v: newNode.id, level: edge.level });

            // Update U and V neighbor lists
            const uNode = nodeMap.get(uId);
            const vNode = nodeMap.get(vId);
            if (uNode) {
                uNode.connectedTo = uNode.connectedTo.filter(id => id !== vId);
                uNode.connectedTo.push(newNode.id);
            }
            if (vNode) {
                vNode.connectedTo = vNode.connectedTo.filter(id => id !== uId);
                vNode.connectedTo.push(newNode.id);
            }
        }
    }

    return edges;
}

export function removeIsolatedNodes(nodes: Node[], edges: Edge[]): Node[] {
    const connectedIds = new Set<number>();
    for (const edge of edges) {
        connectedIds.add(edge.u);
        connectedIds.add(edge.v);
    }
    return nodes.filter(n => connectedIds.has(n.id));
}

/**
 * Subdivides existing edges into smaller segments to create anchor points.
 */
export function subdivideEdges(nodes: Node[], edges: Edge[], maxLen: number): Edge[] {
    const newEdges: Edge[] = [];
    let idCounter = nodes.length;

    const nodeMap = new Map<number, Node>();
    for (const n of nodes) nodeMap.set(n.id, n);

    for (const edge of edges) {
        const u = nodeMap.get(edge.u);
        const v = nodeMap.get(edge.v);
        if (!u || !v) continue;
        const dist = Vector2.dist(u.pos, v.pos);

        if (dist > maxLen) {
            const numDivs = Math.ceil(dist / maxLen);
            let prevNode = u;
            for (let i = 1; i < numDivs; i++) {
                const t = i / numDivs;
                const pos = new Vector2(
                    u.pos.x + (v.pos.x - u.pos.x) * t,
                    u.pos.y + (v.pos.y - u.pos.y) * t
                );
                const newNode: Node = {
                    id: idCounter++,
                    pos: pos,
                    splitNumber: 0,
                    degree: 2,
                    connectedTo: [],
                    level: 0
                };
                nodes.push(newNode);
                nodeMap.set(newNode.id, newNode);

                newEdges.push({ u: prevNode.id, v: newNode.id, level: edge.level });
                prevNode.connectedTo.push(newNode.id);
                newNode.connectedTo.push(prevNode.id);
                prevNode = newNode;
            }
            newEdges.push({ u: prevNode.id, v: v.id, level: edge.level });
            prevNode.connectedTo.push(v.id);
            v.connectedTo = v.connectedTo.filter(id => id !== u.id);
            v.connectedTo.push(prevNode.id);
            u.connectedTo = u.connectedTo.filter(id => id !== v.id);
        } else {
            newEdges.push(edge);
        }
    }
    return newEdges;
}

/**
 * Finds all faces (chordless cycles) in a planar graph embedding.
 */
export function findFaces(nodes: Node[], edges: Edge[]): number[][] {
    const nodeMap = new Map<number, Node>();
    for (const n of nodes) nodeMap.set(n.id, n);

    const faces: number[][] = [];
    const used = new Set<string>(); // "u,v" directed edges

    // Pre-sort outgoing edges at each node by angle
    const sortedEdges = nodes.map(u => {
        return u.connectedTo.map(vId => {
            const v = nodeMap.get(vId);
            if (!v) return { id: vId, angle: 0 };
            return { id: vId, angle: Math.atan2(v.pos.y - u.pos.y, v.pos.x - u.pos.x) };
        }).sort((a, b) => a.angle - b.angle);
    });

    for (const edge of edges) {
        const directed = [[edge.u, edge.v], [edge.v, edge.u]];
        for (const [startU, startV] of directed) {
            const key = `${startU},${startV}`;
            if (used.has(key)) continue;

            // Found a new face
            const face: number[] = [];
            let currU = startU;
            let currV = startV;

            while (!used.has(`${currU},${currV}`)) {
                used.add(`${currU},${currV}`);
                face.push(currU);

                // Find "rightmost" turn at currV
                // We came from currU, we are at currV, we want to go out to nextW
                const currVNode = nodeMap.get(currV);
                const currUNode = nodeMap.get(currU);
                if (!currVNode || !currUNode) break;

                const options = sortedEdges.find((_, i) => nodes[i].id === currV);
                if (!options) break;
                
                // Find index of incoming edge U in the sorted list of V
                let idx = options.findIndex(o => o.id === currU);
                if (idx === -1) break;
                
                // The "rightmost" edge is the one immediately CLOCKWISE from the incoming edge
                // In our sorted list (counter-clockwise), this is the one at index - 1
                let nextIdx = (idx - 1 + options.length) % options.length;
                const nextW = options[nextIdx].id;

                currU = currV;
                currV = nextW;
            }

            // Calculate signed area to skip the outer face
            let area = 0;
            for (let i = 0; i < face.length; i++) {
                const u = nodeMap.get(face[i]);
                const v = nodeMap.get(face[(i + 1) % face.length]);
                if (u && v) {
                    area += (u.pos.x * v.pos.y - v.pos.x * u.pos.y);
                }
            }
            if (area < 0) { // Keep only inner faces (assuming clockwise/ccw order)
                faces.push(face);
            }
        }
    }
    return faces;
}

/**
 * Checks if a point is inside a polygon
 */
export function pointInPolygon(p: Vector2, polygon: Vector2[]): boolean {
    let inside = false;
    for (let i = 0, j = polygon.length - 1; i < polygon.length; j = i++) {
        const pi = polygon[i];
        const pj = polygon[j];
        const intersect = ((pi.y > p.y) !== (pj.y > p.y)) &&
            (p.x < (pj.x - pi.x) * (p.y - pi.y) / (pj.y - pi.y) + pi.x);
        if (intersect) inside = !inside;
    }
    return inside;
}

export function generateHierarchy(nodes: Node[], edges: Edge[], config: GenConfig, depth: number, seed: number): { nodes: Node[], edges: Edge[] } {
    let currentNodes = [...nodes];
    let currentEdges = [...edges];

    // Stage 1: Level 1 (Red Midpoints & Branching Roads)
    if (depth >= 1) {
        const result = internalBranchingSubdivision(currentNodes, currentEdges, config, 1, seed + 1);
        currentNodes = result.nodes;
        currentEdges = result.edges;
    }

    // Stage 2: Level 2 (Orange Midpoints & Branching Roads)
    if (depth >= 2) {
        const result = internalBranchingSubdivision(currentNodes, currentEdges, config, 2, seed + 2);
        currentNodes = result.nodes;
        currentEdges = result.edges;
    }

    return { nodes: currentNodes, edges: currentEdges };
}

function internalBranchingSubdivision(nodes: Node[], edges: Edge[], config: GenConfig, targetLevel: number, seed: number): { nodes: Node[], edges: Edge[] } {
    const prng = mulberry32(seed);
    const rand = () => prng();
    
    const nodeMap = new Map<number, Node>();
    nodes.forEach(n => nodeMap.set(n.id, n));

    const resultNodes = [...nodes];
    const resultEdges = [...edges];
    const newMidpointIds: number[] = [];

    // 1. Create Midpoints
    const subConfig: GenConfig = {
        ...config,
        connectionRadius: config.connectionRadius * (targetLevel === 1 ? 0.7 : 0.4),
        minRoadLen: config.minRoadLen * (targetLevel === 1 ? 0.7 : 0.4),
        maxRoadLen: config.maxRoadLen * (targetLevel === 1 ? 0.7 : 0.4)
    };

    const originalEdges = [...resultEdges];
    for (const edge of originalEdges) {
        const u = nodeMap.get(edge.u);
        const v = nodeMap.get(edge.v);
        if (!u || !v) continue;

        let split = false;
        if (targetLevel === 1 && edge.level === 0) split = true;
        if (targetLevel === 2 && ((u.level === 0 && v.level === 1) || (u.level === 1 && v.level === 0))) split = true;

        if (split) {
            const midPos = u.pos.clone().add(v.pos).mult(0.5);
            const midId = Math.max(...Array.from(nodeMap.keys()), 0) + 1 + newMidpointIds.length;
            const midNode: Node = {
                id: midId,
                pos: midPos,
                splitNumber: 0,
                degree: 0,
                connectedTo: [],
                level: targetLevel
            };
            resultNodes.push(midNode);
            nodeMap.set(midId, midNode);
            newMidpointIds.push(midId);

            // Replace
            const idx = resultEdges.findIndex(e => e.u === edge.u && e.v === edge.v && e.level === edge.level);
            if (idx !== -1) {
                resultEdges.splice(idx, 1, 
                    { u: edge.u, v: midId, level: edge.level },
                    { u: midId, v: edge.v, level: edge.level }
                );
                // Update topology
                u.connectedTo = u.connectedTo.filter(id => id !== v.id);
                u.connectedTo.push(midId);
                v.connectedTo = v.connectedTo.filter(id => id !== u.id);
                v.connectedTo.push(midId);
                midNode.connectedTo.push(u.id, v.id);
                midNode.degree = 2;
            }
        }
    }

    // 1.5 Supplemental Spawning (Face-Based Infilling)
    if (targetLevel === 1) {
        const innerFaces = findFaces(resultNodes, resultEdges.filter(e => e.level === 0)); // Only arterial blocks
        for (const face of innerFaces) {
            const facePoints = face.map(id => nodeMap.get(id)!.pos);
            const bb = getBoundingBox(facePoints);
            const area = (bb.maxX - bb.minX) * (bb.maxY - bb.minY);
            
            // Adjust spawn count based on block size
            const target = Math.min(5, Math.floor(area / (config.minDist * config.minDist * 4)));
            let spawns = 0;
            let attempts = 0;

            while (spawns < target && attempts < 100) {
                attempts++;
                const rx = bb.minX + rand() * (bb.maxX - bb.minX);
                const ry = bb.minY + rand() * (bb.maxY - bb.minY);
                const rpos = new Vector2(rx, ry);

                if (pointInPolygon(rpos, facePoints)) {
                    let tooClose = false;
                    // For Red in-filling, we can be slightly denser (0.7x)
                    const limit = config.minDist * 0.7;
                    for (const node of resultNodes) {
                        if (Vector2.dist(rpos, node.pos) < limit) {
                            tooClose = true;
                            break;
                        }
                    }

                    if (!tooClose) {
                        const midId = Math.max(...Array.from(nodeMap.keys()), 0) + 1 + newMidpointIds.length;
                        const newNode: Node = {
                            id: midId,
                            pos: rpos,
                            splitNumber: 0,
                            degree: 0,
                            connectedTo: [],
                            level: targetLevel
                        };
                        resultNodes.push(newNode);
                        nodeMap.set(midId, newNode);
                        newMidpointIds.push(midId);
                        spawns++;
                    }
                }
            }
        }
    }

    // 2. Generate Branching Streets from these midpoints and seeds
    const branchedEdges = generateEdges(resultNodes, subConfig, resultEdges, targetLevel, seed + 10, newMidpointIds);
    
    // 3. Supplemental: Add midpoints to the newly branched roads for extra anchor density
    const finalEdges = [...branchedEdges];
    const branchedOnly = branchedEdges.filter(e => e.level === targetLevel && !resultEdges.includes(e));
    const secondaryMidpointIds: number[] = [];
    
    for (const edge of branchedOnly) {
        const u = nodeMap.get(edge.u);
        const v = nodeMap.get(edge.v);
        if (!u || !v) continue;

        const midPos = u.pos.clone().add(v.pos).mult(0.5);
        const midId = Math.max(...Array.from(nodeMap.keys()), 0) + 1;
        const midNode: Node = {
            id: midId,
            pos: midPos,
            splitNumber: 0,
            degree: 2,
            connectedTo: [u.id, v.id],
            level: targetLevel
        };
        resultNodes.push(midNode);
        nodeMap.set(midId, midNode);
        secondaryMidpointIds.push(midId);

        // Replace in final list
        const idx = finalEdges.findIndex(e => e.u === edge.u && e.v === edge.v && e.level === targetLevel);
        if (idx !== -1) {
            finalEdges.splice(idx, 1,
                { u: edge.u, v: midId, level: targetLevel },
                { u: midId, v: edge.v, level: targetLevel }
            );
            // Update topology
            u.connectedTo = u.connectedTo.filter(id => id !== v.id);
            u.connectedTo.push(midId);
            v.connectedTo = v.connectedTo.filter(id => id !== u.id);
            v.connectedTo.push(midId);
        }
    }

    // 4. Secondary Branching: Use these new midpoints to create even more roads
    const finalBranchedEdges = generateEdges(resultNodes, subConfig, finalEdges, targetLevel, seed + 20, secondaryMidpointIds);

    return { nodes: resultNodes, edges: finalBranchedEdges };
}

function getBoundingBox(points: Vector2[]) {
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    for (const p of points) {
        minX = Math.min(minX, p.x); maxX = Math.max(maxX, p.x);
        minY = Math.min(minY, p.y); maxY = Math.max(maxY, p.y);
    }
    return { minX, maxX, minY, maxY };
}
