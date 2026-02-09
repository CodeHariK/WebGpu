import * as THREE from 'three';
import * as RAPIER from '@dimforge/rapier3d';
import { Game } from './game';

export function createGround(game: Game, width: number, height: number, position: THREE.Vector3) {
    // Load the texture
    const textureLoader = new THREE.TextureLoader();
    const groundTexture = textureLoader.load('./src/assets/ground_grid.png');
    groundTexture.wrapS = THREE.RepeatWrapping; // Enable texture wrapping on S (horizontal) axis
    groundTexture.wrapT = THREE.RepeatWrapping; // Enable texture wrapping on T (vertical) axis
    groundTexture.repeat.set(50, 50); // Adjust the repeat to scale the texture
    const groundMaterial = new THREE.MeshToonMaterial({ map: groundTexture });

    // Create ground geometry and material
    const groundGeometry = new THREE.PlaneGeometry(width, height);

    const ground = new THREE.Mesh(groundGeometry, groundMaterial);
    ground.rotation.x = -Math.PI / 2;
    ground.receiveShadow = true;
    const groundColliderDesc = RAPIER.ColliderDesc.cuboid(width / 2, 0.1, height / 2).setTranslation(0, position.y - .1, 0);
    game.WORLD.createCollider(groundColliderDesc);
    game.SCENE.add(ground);

    ground.position.set(position.x, position.y, position.z)
}

export function spawnRandomObject(game: Game, scale: THREE.Vector3) {

    if (Math.random() < 0.2) {
        createRamp(game, 5, 5, 10, new THREE.Vector3((Math.random() - 0.5) * scale.x, 0, (Math.random() - 0.5) * scale.z));
        return
    }

    const objectGeometries = [
        new THREE.SphereGeometry(1, 32, 32),
        new THREE.BoxGeometry(2, 2, 2),
        new THREE.ConeGeometry(1, 2, 32),
    ];
    const geometry = objectGeometries[Math.floor(Math.random() * objectGeometries.length)];
    const material = new THREE.MeshStandardMaterial({ color: Math.random() * 0xffffff });
    const mesh = new THREE.Mesh(geometry, material);

    // Random spawn position above terrain
    mesh.position.set((Math.random() - 0.5) * scale.x, scale.y, (Math.random() - 0.5) * scale.y);
    mesh.castShadow = true;
    mesh.receiveShadow = true;

    game.SCENE.add(mesh);

    // Create RigidBody and Collider
    const bodyDesc = RAPIER.RigidBodyDesc.dynamic().setTranslation(
        mesh.position.x,
        mesh.position.y,
        mesh.position.z
    );
    const rigidBody = game.WORLD.createRigidBody(bodyDesc);

    let colliderDesc: RAPIER.ColliderDesc;
    if (geometry instanceof THREE.SphereGeometry) {
        colliderDesc = RAPIER.ColliderDesc.ball(1); // Sphere radius
    } else if (geometry instanceof THREE.BoxGeometry) {
        colliderDesc = RAPIER.ColliderDesc.cuboid(1, 1, 1); // Box half extents
    } else if (geometry instanceof THREE.ConeGeometry) {
        colliderDesc = RAPIER.ColliderDesc.cone(1, 1); // Cone height and radius
    }

    game.WORLD.createCollider(colliderDesc, rigidBody);

    mesh.userData = { name: "Obj", rigidBody: rigidBody };

    return { mesh, rigidBody };
}

function createRamp(game: Game, width: number, height: number, length: number, position: THREE.Vector3) {
    // Define the geometry
    const geometry = new THREE.BufferGeometry();

    // Define the vertices for a ramp
    const vertices = new Float32Array([
        // Base rectangle (bottom face)
        -width / 2, 0, length / 2,  // Bottom left
        width / 2, 0, length / 2,  // Bottom right
        -width / 2, 0, -length / 2, // Top left
        width / 2, 0, -length / 2, // Top right

        // Sloped face
        -width / 2, 0, length / 2,  // Bottom left
        width / 2, 0, length / 2,  // Bottom right
        -width / 2, height, -length / 2, // Top left
        width / 2, height, -length / 2  // Top right
    ]);

    // Define the faces (triangles) for the ramp
    const indices = [
        // Base face
        0, 1, 2,
        1, 3, 2,

        // Sloped face
        4, 6, 5,
        5, 6, 7,

        // Side faces
        0, 2, 4,
        2, 6, 4,

        1, 5, 3,
        3, 5, 7
    ];

    // Add the vertices and indices to the geometry
    geometry.setAttribute('position', new THREE.BufferAttribute(vertices, 3));
    geometry.setIndex(indices);
    geometry.computeVertexNormals(); // Calculate normals for proper lighting

    // Create a material
    const material = new THREE.MeshStandardMaterial({ color: 0x00ff00, side: THREE.DoubleSide });

    // Create the mesh
    const ramp = new THREE.Mesh(geometry, material);

    // Set the position of the ramp
    ramp.position.copy(position);

    const rampRigidBody = game.WORLD.createRigidBody(RAPIER.RigidBodyDesc.fixed().setTranslation(ramp.position.x, ramp.position.y, ramp.position.z).setCanSleep(false))
    const points = new Float32Array(ramp.geometry.attributes.position.array)
    const rampShape = (RAPIER.ColliderDesc.convexHull(points) as RAPIER.ColliderDesc).setMass(1).setRestitution(0.5)
    game.WORLD.createCollider(rampShape, rampRigidBody)

    game.SCENE.add(ramp);
}
