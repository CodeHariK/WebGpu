import { Vector2 } from '../lib/Vector2';
import Delaunator from 'delaunator';
import { createNoise2D } from 'simplex-noise';

export function mulberry32(a: number) {
    return function () {
        let t = (a += 0x6d2b79f5);
        t = Math.imul(t ^ (t >>> 15), t | 1);
        t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
}

export interface VoronoiCell {
    index: number;
    site: Vector2;
    vertices: Vector2[];
    neighborIndices: number[];
    isWater: boolean;
    isCoast: boolean;
    isRiver: boolean;
    elevation: number;
    wardType: 'WATER' | 'CASTLE' | 'MARKET' | 'RESIDENTIAL' | 'SLUM' | 'FARMLAND' | 'CRAFTSMEN' | 'COUNTRYSIDE';
    isInsideWalls: boolean;
    buildings: BuildingParcel[];
}

export interface BuildingParcel {
    polygon: Vector2[];
    ridge?: [Vector2, Vector2]; // Gable roof ridge line
    color: string;
    roofColor: string;
    height: number;
}

export interface CityGate {
    point: Vector2;
    angle: number;
}

export interface WatabouCity {
    cells: VoronoiCell[];
    coastlines: Vector2[][];
    riverPath: Vector2[];
    riverWidth: number[];
    cityWalls: Vector2[];
    towers: Vector2[];
    gates: CityGate[];
    arterialRoads: Vector2[][];
    alleys: Vector2[][];
    castleCellIndex: number;
    marketCellIndex: number;
}

function getCircumcenter(a: Vector2, b: Vector2, c: Vector2): Vector2 {
    const ad = a.x * a.x + a.y * a.y;
    const bd = b.x * b.x + b.y * b.y;
    const cd = c.x * c.x + c.y * c.y;
    const D = 2 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
    if (Math.abs(D) < 1e-7) return new Vector2((a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3);
    return new Vector2(
        (1 / D) * (ad * (b.y - c.y) + bd * (c.y - a.y) + cd * (a.y - b.y)),
        (1 / D) * (ad * (c.x - b.x) + bd * (a.x - c.x) + cd * (b.x - a.x))
    );
}

function polygonCentroid(pts: Vector2[]): Vector2 {
    if (pts.length === 0) return new Vector2(0, 0);
    let cx = 0;
    let cy = 0;
    let signedArea = 0;
    for (let i = 0; i < pts.length; i++) {
        const p0 = pts[i];
        const p1 = pts[(i + 1) % pts.length];
        const a = p0.x * p1.y - p1.x * p0.y;
        signedArea += a;
        cx += (p0.x + p1.x) * a;
        cy += (p0.y + p1.y) * a;
    }
    signedArea *= 0.5;
    if (Math.abs(signedArea) < 1e-5) {
        let sx = 0, sy = 0;
        pts.forEach(p => { sx += p.x; sy += p.y; });
        return new Vector2(sx / pts.length, sy / pts.length);
    }
    return new Vector2(cx / (6 * signedArea), cy / (6 * signedArea));
}

function insetPolygon(pts: Vector2[], distance: number): Vector2[] {
    if (pts.length < 3) return pts;
    const n = pts.length;
    const result: Vector2[] = [];

    // Calculate inward edge normals
    const edgeNormals: Vector2[] = [];
    for (let i = 0; i < n; i++) {
        const p0 = pts[i];
        const p1 = pts[(i + 1) % n];
        const dx = p1.x - p0.x;
        const dy = p1.y - p0.y;
        const len = Math.hypot(dx, dy);
        if (len < 1e-4) {
            edgeNormals.push(new Vector2(0, 0));
        } else {
            // Left-hand normal (inward for CCW polygons)
            edgeNormals.push(new Vector2(-dy / len, dx / len));
        }
    }

    for (let i = 0; i < n; i++) {
        const prevEdge = (i + n - 1) % n;
        const nextEdge = i;
        const n1 = edgeNormals[prevEdge];
        const n2 = edgeNormals[nextEdge];

        const bisector = new Vector2(n1.x + n2.x, n1.y + n2.y);
        const bLen = bisector.mag();
        if (bLen < 1e-4) {
            result.push(pts[i].clone().add(n2.clone().mult(distance)));
        } else {
            bisector.normalize();
            const dot = n1.x * bisector.x + n1.y * bisector.y;
            const dist = distance / Math.max(0.3, dot);
            result.push(pts[i].clone().add(bisector.mult(Math.min(dist, distance * 2.5))));
        }
    }

    return result;
}

// Split a polygon into building parcels along its longest axis (Watabou OBB style)
function subdivideWardIntoParcels(
    poly: Vector2[],
    rand: () => number,
    wardType: string,
    depth = 0
): Vector2[][] {
    if (poly.length < 3) return [];

    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    poly.forEach(p => {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
    });

    const width = maxX - minX;
    const height = maxY - minY;
    const maxDim = Math.max(width, height);

    const targetSize = wardType === 'CASTLE' ? 45 : (wardType === 'SLUM' ? 14 : (wardType === 'RESIDENTIAL' ? 20 : 28));

    if (maxDim <= targetSize || depth >= 4) {
        return [poly];
    }

    const center = polygonCentroid(poly);
    const splitHorizontal = width > height;

    const jitter = (rand() - 0.5) * 0.2;
    const normal = splitHorizontal ? new Vector2(1 + jitter, jitter) : new Vector2(jitter, 1 + jitter);
    normal.normalize();

    const poly1: Vector2[] = [];
    const poly2: Vector2[] = [];

    for (let i = 0; i < poly.length; i++) {
        const p1 = poly[i];
        const p2 = poly[(i + 1) % poly.length];

        const side1 = (p1.x - center.x) * normal.x + (p1.y - center.y) * normal.y;
        const side2 = (p2.x - center.x) * normal.x + (p2.y - center.y) * normal.y;

        if (side1 >= 0) poly1.push(p1);
        else poly2.push(p1);

        if ((side1 > 0 && side2 < 0) || (side1 < 0 && side2 > 0)) {
            const t = Math.abs(side1) / (Math.abs(side1) + Math.abs(side2));
            const intersect = new Vector2(p1.x + t * (p2.x - p1.x), p1.y + t * (p2.y - p1.y));
            poly1.push(intersect);
            poly2.push(intersect);
        }
    }

    if (poly1.length < 3 || poly2.length < 3) {
        return [poly];
    }

    return [
        ...subdivideWardIntoParcels(poly1, rand, wardType, depth + 1),
        ...subdivideWardIntoParcels(poly2, rand, wardType, depth + 1)
    ];
}

/**
 * Main Watabou City Generator Algorithm
 */
export function generateWatabouCity(options: {
    width: number;
    height: number;
    seed: number;
    cellCount: number;
    relaxationPasses: number;
    hasCoast: boolean;
    hasRiver: boolean;
    wallRadius: number;
}): WatabouCity {
    const { width, height, seed, cellCount, relaxationPasses, hasCoast, hasRiver, wallRadius } = options;
    const rand = mulberry32(seed);
    const noise2D = createNoise2D(rand);

    // 1. Generate Poisson/Jittered Seed Points
    let points: Vector2[] = [];
    const gridDim = Math.ceil(Math.sqrt(cellCount));
    const stepX = width / gridDim;
    const stepY = height / gridDim;

    for (let gy = 0; gy < gridDim; gy++) {
        for (let gx = 0; gx < gridDim; gx++) {
            if (points.length >= cellCount) break;
            const x = (gx + 0.5) * stepX + (rand() - 0.5) * stepX * 0.85;
            const y = (gy + 0.5) * stepY + (rand() - 0.5) * stepY * 0.85;
            points.push(new Vector2(x, y));
        }
    }

    // 2. Delaunay & Lloyd's Relaxation Passes
    let cellVerticesMap = new Map<number, Vector2[]>();
    let cellNeighborsMap = new Map<number, number[]>();

    for (let pass = 0; pass <= relaxationPasses; pass++) {
        const coords: number[] = [];
        points.forEach(p => coords.push(p.x, p.y));
        const delaunay = new Delaunator(coords);

        const { triangles, halfedges } = delaunay;
        const circumcenters: Vector2[] = [];
        for (let i = 0; i < triangles.length; i += 3) {
            circumcenters.push(
                getCircumcenter(points[triangles[i]], points[triangles[i + 1]], points[triangles[i + 2]])
            );
        }

        cellVerticesMap.clear();
        cellNeighborsMap.clear();

        for (let i = 0; i < points.length; i++) {
            cellVerticesMap.set(i, []);
            cellNeighborsMap.set(i, []);
        }

        for (let i = 0; i < triangles.length; i++) {
            const pIdx = triangles[i];
            const cc = circumcenters[Math.floor(i / 3)];
            cellVerticesMap.get(pIdx)!.push(cc);

            const oppHalf = halfedges[i];
            if (oppHalf !== -1) {
                cellNeighborsMap.get(pIdx)!.push(triangles[oppHalf]);
            }
        }

        const newPoints: Vector2[] = [];
        for (let i = 0; i < points.length; i++) {
            const p = points[i];
            const rawVerts = cellVerticesMap.get(i)!;
            rawVerts.sort((a, b) => Math.atan2(a.y - p.y, a.x - p.x) - Math.atan2(b.y - p.y, b.x - p.x));

            const cleanVerts = rawVerts.filter(
                (v, idx, self) => idx === self.findIndex(ov => Math.hypot(ov.x - v.x, ov.y - v.y) < 1.0)
            );
            cellVerticesMap.set(i, cleanVerts);

            if (pass < relaxationPasses && cleanVerts.length >= 3) {
                const centroid = polygonCentroid(cleanVerts);
                centroid.x = Math.max(20, Math.min(width - 20, centroid.x));
                centroid.y = Math.max(20, Math.min(height - 20, centroid.y));
                newPoints.push(centroid);
            } else {
                newPoints.push(p);
            }
        }
        points = newPoints;
    }

    // 3. Elevation, Coastline & Water Assignment
    const center = new Vector2(width / 2, height / 2);

    const elevation: number[] = [];
    const isWater: boolean[] = [];

    points.forEach((p, idx) => {
        const nx = p.x * 0.003;
        const ny = p.y * 0.003;
        const n = noise2D(nx, ny) + 0.5 * noise2D(nx * 2, ny * 2);

        const coastGrad = hasCoast ? (p.x + p.y * 0.6) / (width * 1.2) : 0;
        const elev = n - coastGrad;
        elevation[idx] = elev;

        isWater[idx] = hasCoast && (elev < -0.35 || p.x > width * 0.88);
    });

    // 4. River Generation (Steepest Descent from Inland to Coast / Sea)
    const riverPath: Vector2[] = [];
    const riverCells = new Set<number>();
    const riverWidth: number[] = [];

    if (hasRiver) {
        let bestSource = -1;
        let maxElev = -Infinity;

        points.forEach((p, idx) => {
            if (!isWater[idx] && p.x < width * 0.35 && elevation[idx] > maxElev) {
                maxElev = elevation[idx];
                bestSource = idx;
            }
        });

        if (bestSource !== -1) {
            let curr = bestSource;
            const visitedRiver = new Set<number>([curr]);
            let currentWidth = 8;

            while (curr !== -1 && riverPath.length < 80) {
                riverCells.add(curr);
                riverPath.push(points[curr]);
                riverWidth.push(currentWidth);
                currentWidth += 0.4;

                if (isWater[curr]) break;

                const neighbors = cellNeighborsMap.get(curr) || [];
                let next = -1;
                let lowestElev = elevation[curr];

                neighbors.forEach(nIdx => {
                    if (!visitedRiver.has(nIdx)) {
                        const score = elevation[nIdx] - (points[nIdx].x / width) * 0.2;
                        if (score < lowestElev) {
                            lowestElev = score;
                            next = nIdx;
                        }
                    }
                });

                if (next === -1 || visitedRiver.has(next)) break;
                visitedRiver.add(next);
                curr = next;
            }
        }
    }

    // 5. City Center, Castle, and Market Placement
    let castleIdx = -1;
    let marketIdx = -1;
    let bestCastleScore = -Infinity;

    points.forEach((p, idx) => {
        if (isWater[idx] || riverCells.has(idx)) return;
        const distToCenter = Vector2.dist(p, center);
        if (distToCenter < wallRadius * 0.5) {
            const score = elevation[idx] * 2 - distToCenter * 0.01;
            if (score > bestCastleScore) {
                bestCastleScore = score;
                castleIdx = idx;
            }
        }
    });

    if (castleIdx === -1) castleIdx = Math.floor(points.length / 2);

    (cellNeighborsMap.get(castleIdx) || []).forEach(nIdx => {
        if (!isWater[nIdx] && !riverCells.has(nIdx)) {
            marketIdx = nIdx;
        }
    });
    if (marketIdx === -1) marketIdx = castleIdx;

    // 6. City Wall Perimeter & Gates
    const insideWalls = new Set<number>();
    points.forEach((p, idx) => {
        if (!isWater[idx] && Vector2.dist(p, points[castleIdx]) < wallRadius) {
            insideWalls.add(idx);
        }
    });

    const wallEdges: [Vector2, Vector2][] = [];
    const cityWallVertices: Vector2[] = [];
    const towers: Vector2[] = [];
    const gates: CityGate[] = [];

    insideWalls.forEach(cIdx => {
        const cNeighbors = cellNeighborsMap.get(cIdx) || [];
        const cVerts = cellVerticesMap.get(cIdx) || [];

        cNeighbors.forEach(nIdx => {
            if (!insideWalls.has(nIdx) && !isWater[nIdx]) {
                const nVerts = cellVerticesMap.get(nIdx) || [];
                const sharedVerts = cVerts.filter(v1 =>
                    nVerts.some(v2 => Math.hypot(v1.x - v2.x, v1.y - v2.y) < 1.0)
                );
                if (sharedVerts.length >= 2) {
                    wallEdges.push([sharedVerts[0], sharedVerts[1]]);
                    towers.push(sharedVerts[0]);
                }
            }
        });
    });

    if (wallEdges.length > 0) {
        let curr = wallEdges[0][1];
        cityWallVertices.push(wallEdges[0][0], curr);
        const used = new Set<number>([0]);

        for (let step = 0; step < wallEdges.length; step++) {
            let found = false;
            for (let i = 0; i < wallEdges.length; i++) {
                if (used.has(i)) continue;
                const [p0, p1] = wallEdges[i];
                if (Math.hypot(curr.x - p0.x, curr.y - p0.y) < 2.0) {
                    cityWallVertices.push(p1);
                    curr = p1;
                    used.add(i);
                    found = true;
                    break;
                } else if (Math.hypot(curr.x - p1.x, curr.y - p1.y) < 2.0) {
                    cityWallVertices.push(p0);
                    curr = p0;
                    used.add(i);
                    found = true;
                    break;
                }
            }
            if (!found) break;
        }
    }

    const arterialRoads: Vector2[][] = [];
    const gateDirections = [0.2, 1.8, 3.6, 5.2];

    gateDirections.forEach(angle => {
        let bestWallIdx = -1;
        let minAngleDiff = Infinity;

        cityWallVertices.forEach((wv, i) => {
            const vAngle = (Math.atan2(wv.y - points[castleIdx].y, wv.x - points[castleIdx].x) + Math.PI * 2) % (Math.PI * 2);
            const diff = Math.abs(vAngle - angle);
            if (diff < minAngleDiff) {
                minAngleDiff = diff;
                bestWallIdx = i;
            }
        });

        if (bestWallIdx !== -1) {
            const gatePos = cityWallVertices[bestWallIdx];
            gates.push({ point: gatePos, angle });

            const extX = gatePos.x + Math.cos(angle) * (width * 0.45);
            const extY = gatePos.y + Math.sin(angle) * (height * 0.45);
            const exitPoint = new Vector2(
                Math.max(20, Math.min(width - 20, extX)),
                Math.max(20, Math.min(height - 20, extY))
            );

            arterialRoads.push([points[marketIdx], gatePos, exitPoint]);
        }
    });

    // 7. Ward Categorization, Parcels & Buildings
    const cells: VoronoiCell[] = [];
    const alleys: Vector2[][] = [];

    const ROOF_COLORS = ['#b45309', '#9a3412', '#7c2d12', '#c2410c', '#854d0e', '#78350f', '#475569'];
    const WALL_COLORS = ['#e2e8f0', '#cbd5e1', '#94a3b8', '#f8fafc', '#fed7aa'];

    points.forEach((p, idx) => {
        const rawVerts = cellVerticesMap.get(idx) || [];
        const isW = isWater[idx];
        const isR = riverCells.has(idx);
        const inWall = insideWalls.has(idx);

        let wardType: VoronoiCell['wardType'];
        if (isW) {
            wardType = 'WATER';
        } else if (idx === castleIdx) {
            wardType = 'CASTLE';
        } else if (idx === marketIdx) {
            wardType = 'MARKET';
        } else if (inWall) {
            const distToCastle = Vector2.dist(p, points[castleIdx]);
            if (distToCastle < wallRadius * 0.45) {
                wardType = 'CRAFTSMEN';
            } else {
                wardType = 'RESIDENTIAL';
            }
        } else {
            const distToWall = Vector2.dist(p, points[castleIdx]);
            if (distToWall < wallRadius * 1.5) {
                wardType = 'SLUM';
            } else if (distToWall < wallRadius * 2.2) {
                wardType = 'FARMLAND';
            } else {
                wardType = 'COUNTRYSIDE';
            }
        }

        const buildings: BuildingParcel[] = [];

        if (!isW && !isR && rawVerts.length >= 3) {
            const streetInset = inWall ? 4.5 : 3.0;
            const wardBlock = insetPolygon(rawVerts, streetInset);

            if (wardType !== 'FARMLAND' && wardType !== 'COUNTRYSIDE') {
                const parcels = subdivideWardIntoParcels(wardBlock, rand, wardType);

                parcels.forEach(poly => {
                    if (poly.length >= 3) {
                        const buildingPoly = insetPolygon(poly, 1.8);
                        if (buildingPoly.length >= 3) {
                            const roofColor = ROOF_COLORS[Math.floor(rand() * ROOF_COLORS.length)];
                            const wallColor = WALL_COLORS[Math.floor(rand() * WALL_COLORS.length)];
                            buildings.push({
                                polygon: buildingPoly,
                                color: wallColor,
                                roofColor: roofColor,
                                height: inWall ? 1.5 + rand() * 1.5 : 1.0
                            });
                        }
                    }
                });
            }
        }

        cells.push({
            index: idx,
            site: p,
            vertices: rawVerts,
            neighborIndices: cellNeighborsMap.get(idx) || [],
            isWater: isW,
            isCoast: false,
            isRiver: isR,
            elevation: elevation[idx],
            wardType,
            isInsideWalls: inWall,
            buildings
        });
    });

    // 8. Extract Coastline Paths
    const coastlines: Vector2[][] = [];
    if (hasCoast) {
        cells.forEach(c => {
            if (!c.isWater) {
                c.neighborIndices.forEach(nIdx => {
                    const neighbor = cells[nIdx];
                    if (neighbor && neighbor.isWater) {
                        c.isCoast = true;
                        const shared = c.vertices.filter(v1 =>
                            neighbor.vertices.some(v2 => Math.hypot(v1.x - v2.x, v1.y - v2.y) < 1.0)
                        );
                        if (shared.length >= 2) {
                            coastlines.push([shared[0], shared[1]]);
                        }
                    }
                });
            }
        });
    }

    return {
        cells,
        coastlines,
        riverPath,
        riverWidth,
        cityWalls: cityWallVertices,
        towers,
        gates,
        arterialRoads,
        alleys,
        castleCellIndex: castleIdx,
        marketCellIndex: marketIdx
    };
}
