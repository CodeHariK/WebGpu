import { Keyboard } from './input';
import { Game } from './game';
import { AxesHelper, Mesh, Vector3, CatmullRomCurve3, MeshStandardMaterial, BoxGeometry, TubeGeometry, MeshBasicMaterial, Vector2, Matrix4, Quaternion, Object3D, SphereGeometry, Scene, BufferGeometry, Float32BufferAttribute } from 'three';
import { createTerrain } from './terrain';
import { CAR_TOY_CAR } from './prefab';
import { GenerateCanvasTrack, mapCanvasUpdate } from './canvas_track';
import { UV_Shader } from '../shaders/uv';

let game = new Game()


new Keyboard()

// createGround(game, 100, 100, new Vector3(0, 0, 0))

const segments = 400
const segmentsPerUnit = 1
const scale = new Vector3(segments / segmentsPerUnit, 20, segments / segmentsPerUnit)

const raceTrackMap = GenerateCanvasTrack(
    segments, .8,
    [
        { seed: 1700, noiseScale: 0.1 },
    ],
    [
        { seed: 17, noiseScale: .5 },
        // { seed: 1700, noiseScale },
        // { seed: 17000, noiseScale },
    ],
)

function createSubGrids(gridsize: number, heights: Float32Array<ArrayBufferLike>, row: number, col: number, segments: number, invresolution: number) {

    let subgridsize = Math.floor(gridsize / invresolution) + 1;

    let subgrid = new Float32Array(subgridsize * subgridsize).fill(0);

    for (let r = 0; r < subgridsize; r++) {
        for (let c = 0; c < subgridsize; c++) {

            let h = heights[
                row * segments * gridsize +
                r * invresolution * segments +
                col * gridsize +
                c * invresolution
            ];

            if (!h) {
                h = .5;
            }

            subgrid[r * subgridsize + c] = h;
        }
    }
    return { subgrid, subgridsize };
}

function createGrid(heights: Float32Array, segments: number, division: number, scale: Vector3) {

    const gridsize = segments / division

    for (let row = 0; row < division; row++) {
        for (let col = 0; col < division; col++) {

            let l = createSubGrids(gridsize, heights, row, col, segments, 4);
            let m = createSubGrids(gridsize, heights, row, col, segments, 2);
            let h = createSubGrids(gridsize, heights, row, col, segments, 1);

            createTerrain(game,
                l.subgrid, l.subgridsize,
                m.subgrid, m.subgridsize,
                h.subgrid, h.subgridsize,
                new Vector3(scale.x / division, scale.y, scale.z / division),
                new Vector3(row * (scale.x / division), scale.y / 2, col * (scale.z / division))
            )

        }
    }
}

createGrid(raceTrackMap.heights, segments, 10, scale)

// for (let i = 0; i < 40; i++) {
//     spawnRandomObject(game, new Vector3(s, 30, s));
// }

const car = CAR_TOY_CAR(game, new Vector3(scale.x / 2, scale.y, scale.z / 2));


game.SCENE.traverse((object) => {
    // Check if the object is a Mesh
    if (object instanceof Mesh) {
        const axesHelper = new AxesHelper(1.2);
        object.add(axesHelper); // Add AxesHelper to the Mesh
    }
});

game.animate((deltaTime: number) => {
    car.update(game, deltaTime);

    mapCanvasUpdate(raceTrackMap, car, scale, segments, 100)
})


{
    let points = []

    function getImageToHeight(v: Vector2): Vector3 {
        let sss = segments
        let x = v.x * (scale.x / sss)
        let z = v.y * (scale.z / sss)
        let y = (raceTrackMap.heights[Math.floor(v.y) + Math.floor(v.x) * sss] + 0.5) * scale.y + 1

        return new Vector3(x, y, z)
    }


    let extrudePoints: { pos: Vector3; xAxis: Vector3; yAxis: Vector3, zAxis: Vector3; }[] = []

    raceTrackMap.track.samples.forEach((d) => {
        let checkpointGeometry = new BoxGeometry(12, 5, .2)
        const checkpointMaterial = new MeshStandardMaterial({ color: Math.random() * 0xffffff });

        const checkpointMesh = new Mesh(checkpointGeometry, checkpointMaterial);
        const ax = new AxesHelper(10)
        checkpointMesh.add(ax)

        const pos = getImageToHeight(d.pos)

        checkpointMesh.position.set(pos.x, pos.y, pos.z)

        let baricadeGeometry = new BoxGeometry(.5, .5, .5)
        const baricadeMaterial = new MeshStandardMaterial({ color: 0xff0000 });

        const baricadeMeshForward = new Mesh(baricadeGeometry, baricadeMaterial);

        const baricadeMeshRight = new Mesh(baricadeGeometry, baricadeMaterial);
        const baricadeMeshLeft = new Mesh(baricadeGeometry, baricadeMaterial);

        const baricadeMeshTop = new Mesh(baricadeGeometry, baricadeMaterial);

        let t = d.dir.clone().rotateAround(new Vector2(0, 0), Math.PI / 2).multiplyScalar(5)

        const forwardPoint = getImageToHeight(d.pos.clone().add(d.dir.multiplyScalar(4)))
        const leftPoint = getImageToHeight(d.pos.clone().add(t))
        const rightPoint = getImageToHeight(d.pos.clone().add(t.multiplyScalar(-1)))

        const xAxis = rightPoint.clone().sub(pos).normalize()
        const zAxis = forwardPoint.clone().sub(pos).normalize()
        const yAxis = zAxis.clone().cross(xAxis).normalize()

        const topPoint = pos.clone().add(yAxis)

        orientMesh(checkpointMesh, xAxis, yAxis, zAxis)

        spawnSpheresInCircle(checkpointMesh, 6, 20, game.SCENE);

        points.push(new Vector3(pos.x, pos.y, pos.z))

        extrudePoints.push({
            pos: pos,
            xAxis: xAxis,
            yAxis: yAxis,
            zAxis: zAxis,
        })

        baricadeMeshForward.position.set(forwardPoint.x, forwardPoint.y, forwardPoint.z)
        baricadeMeshRight.position.set(rightPoint.x, rightPoint.y, rightPoint.z)
        baricadeMeshLeft.position.set(leftPoint.x, leftPoint.y, leftPoint.z)
        baricadeMeshTop.position.set(topPoint.x, topPoint.y, topPoint.z)

        game.SCENE.add(checkpointMesh)
        game.SCENE.add(baricadeMeshForward)
        game.SCENE.add(baricadeMeshRight)
        game.SCENE.add(baricadeMeshLeft)
        game.SCENE.add(baricadeMeshTop)
    })

    extrudeShapeAlongPoints(
        extrudePoints,
        [
            new Vector2(-5, 0), // Bottom-left
            new Vector2(5, 0),  // Bottom-right
            new Vector2(5, 1), // Top-right
            new Vector2(-5, 1), // Top-left
        ],
        game.SCENE
    );

    const tubeGeometry = new TubeGeometry(
        new CatmullRomCurve3(points), // Curve from points
        100, // Number of segments
        .1, // Radius
        4,   // Radial segments
        true // Closed
    );
    const tubeMaterial = new MeshBasicMaterial({ color: 0x00ff00 });
    const tube = new Mesh(tubeGeometry, tubeMaterial);
    game.SCENE.add(tube);
}

function orientMesh(mesh: Mesh, xAxis: Vector3, yAxis: Vector3, zAxis: Vector3,): void {
    const rotationMatrix = new Matrix4();
    rotationMatrix.makeBasis(xAxis, yAxis, zAxis);

    const quaternion = new Quaternion();
    quaternion.setFromRotationMatrix(rotationMatrix);

    mesh.quaternion.copy(quaternion);
}

function spawnSpheresInCircle(mesh: Mesh, radius: number, count: number, scene: Object3D): void {
    const angleStep = (2 * Math.PI) / count;

    const sphereGeometry = new SphereGeometry(.5, 8, 8);
    const sphereMaterial = new MeshBasicMaterial({ color: 0x00ff00 });

    const transformMatrix = new Matrix4();
    transformMatrix.compose(mesh.position, mesh.quaternion, mesh.scale);

    for (let i = 0; i < count; i++) {
        const angle = i * angleStep;

        const localPosition = new Vector3(
            radius * Math.cos(angle),
            radius * Math.sin(angle),
            0
        );

        const worldPosition = localPosition.applyMatrix4(transformMatrix);

        const sphere = new Mesh(sphereGeometry, sphereMaterial);
        sphere.position.copy(worldPosition);

        scene.add(sphere);
    }
}
function extrudeShapeAlongPoints(
    points: { pos: Vector3, xAxis: Vector3, yAxis: Vector3, zAxis: Vector3 }[],
    shape: Vector2[],
    scene: Scene
): void {
    const vertices: number[] = [];
    const indices: number[] = [];
    const uvs: number[] = [];

    const shapeLength = shape.length;

    for (let i = 0; i < points.length; i++) {
        const { pos, xAxis, yAxis, zAxis } = points[i];

        // Transform each point of the 2D shape into 3D space using the local frame
        for (let j = 0; j < shapeLength; j++) {
            const localPoint = shape[j];
            const transformedPoint = new Vector3()
                .copy(pos)
                .addScaledVector(xAxis, localPoint.x) // xDir scales X
                .addScaledVector(yAxis, localPoint.y); // yDir scales Y

            vertices.push(transformedPoint.x, transformedPoint.y, transformedPoint.z);

            // UV mapping: u = shape index / shapeLength, v = segment index / points.length
            uvs.push(j / (shapeLength - 1), i / (points.length - 1));
        }
    }

    // Create indices for triangles between consecutive segments
    for (let i = 0; i < points.length - 1; i++) {
        const segmentStart = i * shapeLength;
        const segmentEnd = (i + 1) * shapeLength;

        for (let j = 0; j < shapeLength - 1; j++) {
            const current = segmentStart + j;
            const next = segmentEnd + j;

            // Two triangles per quad
            indices.push(current, next, current + 1); // Triangle 1
            indices.push(current + 1, next, next + 1); // Triangle 2
        }
    }

    // Optionally close the loop
    if (true /* Set to true if the path is closed */) {
        const segmentStart = (points.length - 1) * shapeLength;
        const segmentEnd = 0;

        for (let j = 0; j < shapeLength - 1; j++) {
            const current = segmentStart + j;
            const next = segmentEnd + j;

            indices.push(current, next, current + 1); // Triangle 1
            indices.push(current + 1, next, next + 1); // Triangle 2
        }
    }

    // Create geometry
    const geometry = new BufferGeometry();
    geometry.setAttribute('position', new Float32BufferAttribute(vertices, 3));
    geometry.setAttribute('uv', new Float32BufferAttribute(uvs, 2));
    geometry.setIndex(indices);

    // Create material and mesh
    const material = new MeshStandardMaterial({ color: 0x333333, side: 2, wireframe: true });
    const mesh = new Mesh(geometry, material);

    // Add to the scene
    scene.add(mesh);
}

// Define the 2D shape (e.g., rectangle)
const shape = [
    new Vector2(-1, 0), // Bottom-left
    new Vector2(1, 0),  // Bottom-right
    new Vector2(1, 0.5), // Top-right
    new Vector2(-1, 0.5), // Top-left
];

// Define the points with local frames
const points = [
    {
        pos: new Vector3(-10, 0, 0),
        xAxis: new Vector3(0, 0, -1), // Right
        yAxis: new Vector3(0, 1, 0),  // Up
        zAxis: new Vector3(1, 0, 0),  // Forward
    },
    {
        pos: new Vector3(-5, 0, 5),
        xAxis: new Vector3(-0.707, 0, -0.707), // Right
        yAxis: new Vector3(0, 1, 0), // Up
        zAxis: new Vector3(0.707, 0, 0.707), // Forward
    },
    {
        pos: new Vector3(0, 0, 10),
        xAxis: new Vector3(-1, 0, 0), // Right
        yAxis: new Vector3(0, 1, 0), // Up
        zAxis: new Vector3(0, 0, 1), // Forward
    },
];

// Call the function to create the extruded mesh
extrudeShapeAlongPoints(points, shape, game.SCENE);
