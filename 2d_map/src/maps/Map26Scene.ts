// Colour Blocks — the 3D preview's city (three.js), built from the same data as the 2D map.
//
// One unit is one pixel of the 2D map; x is the map's x, z its y, y is up. The ground is a flat
// square with the 2D map painted on it (only what lies flat: coast, roads, zones, car parks,
// ponds, dock yards). On it, everything with a height, each kind one instanced mesh (one draw call
// for all of them, a colour each): houses (walls a floor or two high, a pyramid roof in the roof's
// colour), the placeholder buildings (boxes as tall as their floors, the round towers cylinders),
// warehouses and ships at the docks, trees (cones), and the motorway (a strip just above the
// ground, rising onto its bridge over the water, with piers under the bridge).
import * as THREE from 'three';
import { DETAIL } from './Map26Districts';
import { ROOF_COLOURS } from './Map26Draw';
import type { Rect } from './Map26Fit';
import { coastDistance } from './Map26Island';
import { drawBlocks, type Blocks } from './Map26Render';

export const FLOOR = 2.2; // units (pixels) a floor
const BRIDGE_HEIGHT = 4.5; // units: the bridge's deck above the water
const DECK_WIDTH = 12;
const RAMP = 6; // points (each a quarter block cell) the motorway takes to climb onto its bridge

type Piece = { x: number; z: number; width: number; depth: number; height: number; angle: number; base?: number; colour: string };

// The 2D map, flat parts only, as the ground's texture.
function groundTexture(p_b: Blocks, p_size: number): THREE.CanvasTexture {
    const canvas = document.createElement('canvas');
    canvas.width = p_size;
    canvas.height = p_size;
    drawBlocks(canvas.getContext('2d')!, p_b, 1, { queue: false, ground: false, island: 'map', rings: false, field: false, flat: true });
    const texture = new THREE.CanvasTexture(canvas);
    texture.colorSpace = THREE.SRGBColorSpace;
    texture.anisotropy = 8;
    return texture;
}

// All p_pieces as one instanced mesh of p_geometry (a unit shape standing on y = 0), each moved,
// turned and stretched to its piece, in its colour.
function instanced(p_geometry: THREE.BufferGeometry, p_pieces: Piece[], p_size: number): THREE.InstancedMesh {
    const material = new THREE.MeshLambertMaterial({ color: 0xffffff });
    const mesh = new THREE.InstancedMesh(p_geometry, material, Math.max(1, p_pieces.length));
    mesh.count = p_pieces.length;
    const m = new THREE.Matrix4();
    const q = new THREE.Quaternion();
    const up = new THREE.Vector3(0, 1, 0);
    const colour = new THREE.Color();
    p_pieces.forEach((p, k) => {
        q.setFromAxisAngle(up, -p.angle); // the map's angle runs x towards z
        m.compose(new THREE.Vector3(p.x - p_size / 2, p.base ?? 0, p.z - p_size / 2), q, new THREE.Vector3(p.width, p.height, p.depth));
        mesh.setMatrixAt(k, m);
        mesh.setColorAt(k, colour.set(p.colour));
    });
    mesh.castShadow = true;
    mesh.receiveShadow = true;
    return mesh;
}

function rectPiece(p_r: Rect, p_cell: number, p_height: number, p_colour: string, p_base = 0): Piece {
    return { x: p_r.x * p_cell, z: p_r.y * p_cell, width: p_r.width * p_cell, depth: p_r.depth * p_cell, height: p_height, angle: p_r.angle, base: p_base, colour: p_colour };
}

// A random number 0..1 for a whole number.
function hash(p_k: number): number {
    let h = Math.imul(p_k + 3, 2654435761);
    h ^= h >>> 15;
    return ((h >>> 0) % 1000) / 1000;
}

// The motorway as pieces: a strip from point to point, just above the ground, BRIDGE_HEIGHT up
// over the water (climbing over RAMP points either side); a pier under every 4th bridge point.
function motorwayPieces(p_b: Blocks, p_size: number, r_deck: Piece[], r_piers: Piece[]): void {
    if (!p_b.motorway || !p_b.island) return;
    const cell = p_size / p_b.spread.n;
    const pts: { x: number; z: number }[] = [];
    const road = p_b.motorway.road.points;
    for (let k = 0; k < road.length - 1; k++) {
        const a = road[k];
        const b = road[k + 1];
        const steps = Math.max(1, Math.ceil(Math.hypot(b.x - a.x, b.y - a.y) * 4));
        for (let s = 0; s < steps; s++) pts.push({ x: (a.x + ((b.x - a.x) * s) / steps) * cell, z: (a.y + ((b.y - a.y) * s) / steps) * cell });
    }
    const water = pts.map((p) => coastDistance(p_b.island!, p.x, p.z) > 0);
    const height = pts.map((_, k) => { // up over water and within RAMP points of it, eased
        let near = RAMP + 1;
        for (let d = 0; d <= RAMP && near > RAMP; d++) if (water[k - d] || water[k + d]) near = d;
        const t = Math.max(0, 1 - near / (RAMP + 1));
        return 0.3 + (BRIDGE_HEIGHT - 0.3) * t * t * (3 - 2 * t);
    });
    for (let k = 0; k < pts.length - 1; k++) {
        const a = pts[k];
        const b = pts[k + 1];
        const length = Math.hypot(b.x - a.x, b.z - a.z);
        const top = (height[k] + height[k + 1]) / 2;
        r_deck.push({ x: (a.x + b.x) / 2, z: (a.z + b.z) / 2, width: length + 0.6, depth: DECK_WIDTH, height: 1, base: top - 1, angle: Math.atan2(b.z - a.z, b.x - a.x), colour: water[k] ? '#d9d3c7' : '#8f8a84' });
        if (water[k] && k % 4 === 0) r_piers.push({ x: a.x, z: a.z, width: 1.6, depth: DECK_WIDTH * 0.7, height: top - 1 + 2, base: -2, angle: Math.atan2(b.z - a.z, b.x - a.x), colour: '#6d655d' });
    }
}

// Everything that stands up, by kind.
function pieces(p_b: Blocks, p_size: number) {
    const fine = p_b.districts ? p_size / p_b.districts.n : p_size / (p_b.spread.n * DETAIL);
    const walls: Piece[] = [];
    const roofs: Piece[] = [];
    const blocks: Piece[] = [];
    const towers: Piece[] = [];
    const trees: Piece[] = [];
    const deck: Piece[] = [];
    const piers: Piece[] = [];
    p_b.houses?.houses.forEach((h, k) => {
        const floors = 1 + (hash(k) < 0.35 ? 1 : 0);
        const r: Rect = { x: h.x, y: h.y, angle: h.angle, width: h.width, depth: h.depth };
        walls.push(rectPiece(r, fine, floors * FLOOR, '#f3ead8'));
        roofs.push(rectPiece(r, fine, Math.min(h.width, h.depth) * fine * 0.45, ROOF_COLOURS[h.roof % ROOF_COLOURS.length], floors * FLOOR));
    });
    const colours = { downtown: ['#b9b3d6', '#a7b4cf', '#c9c2b3', '#9fa9b8'], industry: ['#a9adb3'], mall: ['#e9a39b'], tower: ['#e8d9a8'] };
    p_b.buildings?.buildings.forEach((b, k) => {
        const palette = colours[b.kind];
        const colour = palette[Math.floor(hash(k * 7) * palette.length)];
        if (b.rect) blocks.push(rectPiece(b.rect, fine, b.floors * FLOOR, colour));
        else if (b.circle) towers.push({ x: b.circle.x * fine, z: b.circle.y * fine, width: b.circle.radius * fine * 2, depth: b.circle.radius * fine * 2, height: b.floors * FLOOR, angle: 0, colour });
    });
    p_b.buildings?.trees.forEach((t, k) => {
        const r = t.radius * fine;
        trees.push({ x: t.x * fine, z: t.y * fine, width: r * 2, depth: r * 2, height: r * 3 + 1.5, angle: 0, colour: hash(k) < 0.5 ? '#3f7d33' : '#4f9a3c' });
    });
    for (const w of p_b.docks?.warehouses ?? []) blocks.push(rectPiece(w, fine, 2 * FLOOR, '#c98d6a'));
    for (const s of p_b.docks?.ships ?? []) blocks.push(rectPiece(s, fine, 1.8, '#f4f1ea', -0.4));
    motorwayPieces(p_b, p_size, deck, piers);
    return { walls, roofs, blocks, towers, trees, deck, piers };
}

// The city as a group, to put in the scene; dispose() frees its GPU memory. Round the map, a sea
// to the horizon in the colour of the map's corner (its deepest water).
export function buildCity(p_b: Blocks, p_size: number): { group: THREE.Group; dispose: () => void } {
    const group = new THREE.Group();
    const texture = groundTexture(p_b, p_size);
    const ground = new THREE.Mesh(new THREE.PlaneGeometry(p_size, p_size).rotateX(-Math.PI / 2), new THREE.MeshLambertMaterial({ map: texture }));
    ground.receiveShadow = true;
    const [r, g, b] = (texture.image as HTMLCanvasElement).getContext('2d')!.getImageData(1, 1, 1, 1).data;
    const sea = new THREE.Mesh(new THREE.PlaneGeometry(p_size * 12, p_size * 12).rotateX(-Math.PI / 2).translate(0, -0.2, 0), new THREE.MeshLambertMaterial({ color: new THREE.Color(`rgb(${r}, ${g}, ${b})`) }));
    group.add(ground, sea);
    const p = pieces(p_b, p_size);
    const box = new THREE.BoxGeometry(1, 1, 1).translate(0, 0.5, 0);
    const roof = new THREE.ConeGeometry(Math.SQRT1_2, 1, 4).rotateY(Math.PI / 4).translate(0, 0.5, 0); // a pyramid on a unit square
    const cylinder = new THREE.CylinderGeometry(0.5, 0.5, 1, 24).translate(0, 0.5, 0);
    const cone = new THREE.ConeGeometry(0.5, 1, 8).translate(0, 0.5, 0);
    group.add(instanced(box, p.walls, p_size), instanced(roof, p.roofs, p_size), instanced(box, p.blocks, p_size), instanced(cylinder, p.towers, p_size), instanced(cone, p.trees, p_size), instanced(box, p.deck, p_size), instanced(box, p.piers, p_size));
    const dispose = () => {
        group.traverse((o) => {
            if (o instanceof THREE.Mesh) {
                o.geometry.dispose();
                (o.material as THREE.Material).dispose();
            }
        });
        texture.dispose();
    };
    return { group, dispose };
}
