import { Keyboard } from './input';
import { Game } from './game';
import { AxesHelper, Mesh, Vector3, CatmullRomCurve3, MeshStandardMaterial, BoxGeometry, TubeGeometry, MeshBasicMaterial, Vector2, Matrix4, Quaternion, Object3D, SphereGeometry, Scene, BufferGeometry, Float32BufferAttribute, TextureLoader, LinearFilter, LinearMipMapLinearFilter, RepeatWrapping, MeshPhysicalMaterial, MeshMatcapMaterial } from 'three';
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
    function getImageToHeight(v: Vector2, above: number = .4): Vector3 {
        let sss = segments
        let x = v.x * (scale.x / sss)
        let z = v.y * (scale.z / sss)
        let y = (raceTrackMap.heights[Math.floor(v.y) + Math.floor(v.x) * sss] + 0.5) * scale.y + above

        return new Vector3(x, y, z)
    }

    let extrudePoints: { pos: Vector3; xAxis: Vector3; yAxis: Vector3 }[] = []

    raceTrackMap.track.samples.forEach((d) => {

        const pos = getImageToHeight(d.pos)

        let t = d.dir.clone().rotateAround(new Vector2(0, 0), Math.PI / 2).multiplyScalar(5)

        const forwardPoint = getImageToHeight(d.pos.clone().add(d.dir.multiplyScalar(4)))
        const leftPoint = getImageToHeight(d.pos.clone().add(t))
        const rightPoint = getImageToHeight(d.pos.clone().add(t.multiplyScalar(-1)))

        const xAxis = rightPoint.clone().sub(pos).normalize()
        const zAxis = forwardPoint.clone().sub(pos).normalize()
        const yAxis = zAxis.clone().cross(xAxis).normalize()

        const topPoint = pos.clone().add(yAxis)

        extrudePoints.push({
            pos: pos,
            xAxis: xAxis,
            yAxis: yAxis,
        })

        // {
        //     let checkpointGeometry = new BoxGeometry(8, 4, .2)
        //     const checkpointMaterial = new MeshStandardMaterial({ color: Math.random() * 0xffffff });
        //     const checkpointMesh = new Mesh(checkpointGeometry, checkpointMaterial);
        //     const ax = new AxesHelper(4)
        //     checkpointMesh.add(ax)

        //     let baricadeGeometry = new BoxGeometry(.5, .5, .5)
        //     const baricadeMaterial = new MeshStandardMaterial({ color: 0xff0000 });
        //     const baricadeMeshForward = new Mesh(baricadeGeometry, baricadeMaterial);
        //     const baricadeMeshRight = new Mesh(baricadeGeometry, baricadeMaterial);
        //     const baricadeMeshLeft = new Mesh(baricadeGeometry, baricadeMaterial);
        //     const baricadeMeshTop = new Mesh(baricadeGeometry, baricadeMaterial);

        //     baricadeMeshForward.position.set(forwardPoint.x, forwardPoint.y, forwardPoint.z)
        //     baricadeMeshRight.position.set(rightPoint.x, rightPoint.y, rightPoint.z)
        //     baricadeMeshLeft.position.set(leftPoint.x, leftPoint.y, leftPoint.z)
        //     baricadeMeshTop.position.set(topPoint.x, topPoint.y, topPoint.z)

        //     orientMesh(checkpointMesh, xAxis, yAxis, zAxis)
        //     checkpointMesh.position.set(pos.x, pos.y, pos.z)
        //     game.SCENE.add(checkpointMesh)
        //     game.SCENE.add(baricadeMeshForward)
        //     game.SCENE.add(baricadeMeshRight)
        //     game.SCENE.add(baricadeMeshLeft)
        //     game.SCENE.add(baricadeMeshTop)
        //     spawnSpheresInCircle(checkpointMesh, 6, 20, game.SCENE);
        // }
    })

    {
        let shape = []
        let n = 4
        let a = 2 * Math.PI / n
        for (let i = 0; i < n; i++) {
            shape.push(new Vector2(
                5 * Math.sin(i * a),
                20 + 5 * Math.cos(i * a))
            )
        }

        extrudeShapeAlongPoints(
            extrudePoints,
            shape.reverse(),
            game.SCENE,
            true, false
        );
    }
    {
        let shape = []
        let n = 4
        let a = 2 * Math.PI / n
        for (let i = 0; i < n; i++) {
            shape.push(new Vector2(
                5 * Math.sin(i * a),
                40 + 5 * Math.cos(i * a))
            )
        }

        extrudeShapeAlongPoints(
            extrudePoints,
            shape.reverse(),
            game.SCENE
        );
    }
    {
        let shape = []
        let n = 4
        let a = 2 * Math.PI / n
        for (let i = 0; i < n; i++) {
            shape.push(new Vector2(
                5 * Math.sin(i * a),
                60 + 5 * Math.cos(i * a))
            )
        }

        extrudeShapeAlongPoints(
            extrudePoints,
            shape.reverse(),
            game.SCENE,
            false,
        );
    }
    {
        extrudeShapeAlongPoints(
            extrudePoints,
            [
                new Vector2(6, 0),
                new Vector2(-6, 0),
            ],
            game.SCENE,
            false, false,
        );
    }
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
    points: { pos: Vector3, xAxis: Vector3, yAxis: Vector3 }[],
    shape: Vector2[],
    scene: Scene,
    loopClosed: boolean = true,
    shapeClosed: boolean = true,
): void {
    const vertices: number[] = [];
    const indices: number[] = [];
    const uvs: number[] = [];

    console.log('-----')

    let shapeLength = 0
    for (let i = 0; i < (shapeClosed ? shape.length : shape.length - 1); i++) {
        const current = shape[i];
        const next = shape[(i + 1) % shape.length];
        shapeLength += current.distanceTo(next)
        console.log(shapeLength)
    }

    if (shapeClosed) {
        shape.push(shape[0])
    }

    let pointsLength = 0
    for (let i = 0; i < (loopClosed ? points.length : points.length - 1); i++) {
        const current = points[i].pos
        const next = points[(i + 1) % points.length].pos
        pointsLength += current.distanceTo(next)
    }
    shapeLength = pointsLength / Math.floor(pointsLength / shapeLength)
    console.log("shapeLength", shapeLength, "pointsLength", pointsLength)

    let currentPointsLength = 0
    for (let p = 0; p < points.length; p++) {
        const { pos, xAxis, yAxis } = points[p];

        if (p > 0) {
            currentPointsLength += points[p - 1].pos.distanceTo(pos)
        }

        let currentShapeLength = 0
        // Transform each point of the 2D shape into 3D space using the local frame
        for (let j = 0; j < shape.length; j++) {

            const localPoint = shape[j];

            if (j > 0) {
                currentShapeLength += shape[j - 1].distanceTo(localPoint)
            }

            const transformedPoint = new Vector3()
                .copy(pos)
                .addScaledVector(xAxis, localPoint.x) // xDir scales X
                .addScaledVector(yAxis, localPoint.y); // yDir scales Y

            vertices.push(transformedPoint.x, transformedPoint.y, transformedPoint.z);

            uvs.push(
                currentShapeLength / shapeLength,
                currentPointsLength / shapeLength
            );
        }

    }

    function pushIndices(segmentStart: number, segmentEnd: number) {
        for (let j = 0; j < shape.length - 1; j++) {
            const current = segmentStart + j;
            const next = segmentEnd + j;

            indices.push(current, next, current + 1);
            indices.push(current + 1, next, next + 1);
        }

        if (shapeClosed) {
            const current = segmentStart + shape.length - 1;
            const next = segmentEnd + shape.length - 1;

            indices.push(current, next, segmentStart);
            indices.push(segmentStart, next, segmentEnd);
        }
    }

    // Create indices for triangles between consecutive segments
    for (let i = 0; i < points.length - 1; i++) {
        pushIndices(i * shape.length, (i + 1) * shape.length)
    }
    if (loopClosed) {
        pushIndices((points.length - 1) * shape.length, 0)
    }

    // Create geometry
    const geometry = new BufferGeometry();
    geometry.setAttribute('position', new Float32BufferAttribute(vertices, 3));
    geometry.setAttribute('uv', new Float32BufferAttribute(uvs, 2));
    geometry.setIndex(indices);
    geometry.computeVertexNormals()


    const textureLoader = new TextureLoader();
    const roadTexture = textureLoader.load('src/assets/UV_Grid.jpg');
    roadTexture.wrapS = RepeatWrapping;
    roadTexture.wrapT = RepeatWrapping;
    roadTexture.minFilter = LinearMipMapLinearFilter;
    roadTexture.magFilter = LinearFilter;

    let material = new MeshMatcapMaterial({
        // color: 0xd7b5a0,
        // side: THREE.DoubleSide,
        flatShading: true,
        // wireframe: true,
        map: roadTexture,
        // normalMap: normalMap,
    });

    const mesh = new Mesh(geometry, material);
    // const mesh = new Mesh(geometry, UV_Shader);

    // Add to the scene
    scene.add(mesh);
}
