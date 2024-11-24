import * as THREE from 'three';
import * as RAPIER from '@dimforge/rapier3d';
import { Physics } from './physics';
import { Keyboard } from './keyboard';

class Wheel {
    origin: THREE.Vector3
    mesh: THREE.Mesh
    raycastMesh: THREE.Mesh
    contactMesh: THREE.Mesh
    canSteer: boolean
    steerAngle: number

    constructor(x: number, z: number, radius: number, canSteer: boolean) {
        this.origin = new THREE.Vector3(x, 0, z)
        this.mesh = new THREE.Mesh(new THREE.CylinderGeometry(radius, radius, .2, 24), new THREE.MeshStandardMaterial({ color: Math.random() * 0xffffff, wireframe: true }))
        this.raycastMesh = new THREE.Mesh(new THREE.BoxGeometry(.3, .3, .3, 2), new THREE.MeshStandardMaterial({ color: Math.random() * 0xffffff }))
        this.contactMesh = new THREE.Mesh(new THREE.BoxGeometry(.3, .3, .3, 2), new THREE.MeshStandardMaterial({ color: Math.random() * 0xffffff }))
        this.canSteer = canSteer
        this.steerAngle = 0
    }
}

export class Car {
    mesh: THREE.Mesh;
    rigidBody: RAPIER.RigidBody;

    wheels: Wheel[]

    mass = 1
    speed = 5;
    wheelRadius = .8
    wheelCircumference: number
    suspensionLength = 1; // Maximum suspension travel
    suspensionStiffness = 100; // Spring stiffness
    damping = 10; // Damping to reduce oscillation

    constructor(scene: THREE.Scene, wheelRadius: number) {

        this.wheelCircumference = 2 * Math.PI * this.wheelRadius;

        // Create Rapier RigidBody
        const rigidBodyDesc = RAPIER.RigidBodyDesc.dynamic().setTranslation(0, 1, 0);
        this.rigidBody = Physics.World.createRigidBody(rigidBodyDesc);

        // Add a Box Collider
        const colliderDesc = RAPIER.ColliderDesc.cuboid(1, 0.5, 2); // Dimensions match the car mesh
        Physics.World.createCollider(colliderDesc, this.rigidBody);

        this.wheelRadius = wheelRadius
        this.wheels = [
            new Wheel(1, 2, wheelRadius, true),
            new Wheel(-1, 2, wheelRadius, true),
            new Wheel(1, -2, wheelRadius, false),
            new Wheel(-1, -2, wheelRadius, false),
        ]

        this.mesh = this.createCarMesh(scene);
        this.mesh.userData = { name: "Car", rigidBody: this.rigidBody };;
    }

    private createCarMesh(scene: THREE.Scene): THREE.Mesh {
        const carGeometry = new THREE.BoxGeometry(2, 1, 4);
        const carMaterial = new THREE.MeshStandardMaterial({ color: 0xff0000, wireframe: true });
        const carMesh = new THREE.Mesh(carGeometry, carMaterial);
        carMesh.castShadow = true;
        carMesh.position.y = 60;
        scene.add(carMesh);

        for (const wheel of this.wheels) {
            wheel.mesh.rotateZ(Math.PI / 2)
            scene.add(wheel.mesh)
            scene.add(wheel.raycastMesh)
            scene.add(wheel.contactMesh)
        }
        return carMesh;
    }

    update(): void {
        // Sync mesh position/rotation with Rapier body
        const position = this.rigidBody.translation();
        const rotation = this.rigidBody.rotation();
        const carRotation = new THREE.Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);

        // Update car body position and rotation
        this.mesh.position.set(position.x, position.y, position.z);
        this.mesh.quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);

        // Wheel rotation variables
        const forwardDistance = Physics.toVec(this.rigidBody.linvel()).length(); // Linear velocity (speed)

        // Rotate and position each wheel
        for (const wheel of this.wheels) {
            let hello = wheel.origin

            const rotatedWheelPosition = wheel.origin.applyQuaternion(carRotation);

            // Update wheel position relative to car body
            const wheelWorldPos = Physics.add(position, rotatedWheelPosition);


            wheel.mesh.position.set(wheelWorldPos.x, wheelWorldPos.y, wheelWorldPos.z);

            // Apply rotation around the axle (local X-axis)
            wheel.mesh.rotation.x = (forwardDistance / this.wheelCircumference) * 2 * Math.PI; // Distance -> rotation;

            // Apply steering rotation (if applicable) around the Y-axis
            if (wheel.canSteer) {
                wheel.mesh.rotation.y = wheel.steerAngle; // `steerAngle` in radians
            }

            //------

            wheel.raycastMesh.position.set(wheelWorldPos.x, wheelWorldPos.y, wheelWorldPos.z)

            const ray = new RAPIER.Ray(Physics.fromVec(wheelWorldPos), new RAPIER.Vector3(0, -1, 0))
            const hit = Physics.World.castRay(ray, this.suspensionLength, true);

            if (hit) {

                const contactPoint = ray.pointAt(hit.timeOfImpact)

                // Calculate compression (distance to ground)
                const compression = this.suspensionLength * hit.timeOfImpact

                wheel.contactMesh.position.set(contactPoint.x, contactPoint.y, contactPoint.z)

                // Suspension force (Hooke's law: F = -kx)
                const force = compression * this.suspensionStiffness;

                // Damping force
                const relativeVelocity = Physics.linearVelocityAtWorldPoint(this.rigidBody, wheelWorldPos)
                const dampingForce = relativeVelocity.y * this.damping;

                // Apply total force at the wheel position
                const suspensionForce = new RAPIER.Vector3(0, force - dampingForce, 0);

                console.log(hit.timeOfImpact, compression, suspensionForce)

                this.rigidBody.addForceAtPoint(suspensionForce, position, true);

                this.rigidBody.addForce(new RAPIER.Vector3(0, -9.81 * this.mass, 0), true);
            }

            wheel.origin = hello
        }

    }


    handleKeyboardInput(): void {
        this.rigidBody.resetForces(true);
        this.rigidBody.resetTorques(true);

        if (Keyboard.keys.ArrowUp) {
            Physics.applyImpulse(this.rigidBody, new THREE.Vector3(0, 0, this.speed))
        }
        if (Keyboard.keys.ArrowDown) {
            Physics.applyImpulse(this.rigidBody, new THREE.Vector3(0, 0, -this.speed))
        }

        // Adjust steering angle for front tires
        const steeringSpeed = 0.05; // Steering angle increment
        const maxSteerAngle = Math.PI / 6; // Max steering angle (30 degrees)

        for (const wheel of this.wheels) {
            if (wheel.canSteer) {
                // Increase or decrease steering angle based on input
                if (Keyboard.keys.ArrowLeft) {
                    wheel.steerAngle = Math.min(wheel.steerAngle + steeringSpeed, maxSteerAngle);
                }
                if (Keyboard.keys.ArrowRight) {
                    wheel.steerAngle = Math.max(wheel.steerAngle - steeringSpeed, -maxSteerAngle);
                }

                // Apply the steering angle as a Y-axis rotation
                wheel.mesh.rotation.y = wheel.steerAngle;
            }
        }

        if (!Keyboard.keys.ArrowLeft && !Keyboard.keys.ArrowRight) {
            for (const wheel of this.wheels) {
                if (wheel.canSteer) {
                    wheel.steerAngle *= 0.9; // Smoothly return to center
                    wheel.mesh.rotation.y = wheel.steerAngle;
                }
            }
        }
    }
}
