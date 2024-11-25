import * as THREE from 'three';
import * as RAPIER from '@dimforge/rapier3d';
import { Physics, rVecAdd, rVec, rVecMag, rVecString, rVecMul, rVecScale } from './physics';
import { Keyboard } from './keyboard';

class Wheel {
    origin: THREE.Vector3
    mesh: THREE.Mesh
    raycastMesh: THREE.Mesh
    contactMesh: THREE.Mesh
    canSteer: boolean
    steerAngle: number

    springLength: number

    constructor(x: number, z: number, radius: number, canSteer: boolean) {
        this.origin = new THREE.Vector3(x, 0, z)
        this.mesh = new THREE.Mesh(new THREE.CylinderGeometry(radius, radius, .2, 24), new THREE.MeshStandardMaterial({ color: Math.random() * 0xffffff, wireframe: true }))
        this.raycastMesh = new THREE.Mesh(new THREE.BoxGeometry(.3, .3, .3, 2), new THREE.MeshStandardMaterial({ color: Math.random() * 0xffffff }))
        this.contactMesh = new THREE.Mesh(new THREE.BoxGeometry(.3, .3, .3, 2), new THREE.MeshStandardMaterial({ color: Math.random() * 0xffffff }))
        this.canSteer = canSteer
        this.steerAngle = 0

        this.springLength = 0
    }
}

export class Car {
    mesh: THREE.Mesh;
    rigidBody: RAPIER.RigidBody;

    wheels: Wheel[]

    positionHUD: HTMLElement
    linearVelocityHUD: HTMLElement
    suspensionHUD: HTMLElement
    infoHUD: HTMLElement

    mass = 10
    speed = 5;
    wheelRadius: number
    wheelCircumference: number
    suspension: number; // Maximum suspension travel
    suspensionStiffness: number; // Spring stiffness
    damping = 2; // Damping to reduce oscillation

    constructor(scene: THREE.Scene, position: THREE.Vector3, wheelRadius: number, suspension: number, bouncy: number) {

        this.createUI()

        // Create Rapier RigidBody
        const rigidBodyDesc = RAPIER.RigidBodyDesc.dynamic().setTranslation(position.x, position.y, position.z);
        this.rigidBody = Physics.World.createRigidBody(rigidBodyDesc);

        // Add a Box Collider
        const colliderDesc = RAPIER.ColliderDesc.cuboid(1, .5, 2); // Dimensions match the car mesh
        Physics.World.createCollider(colliderDesc, this.rigidBody);


        this.rigidBody.setLinearDamping(1);
        this.rigidBody.setAngularDamping(1);


        this.wheelRadius = wheelRadius
        this.wheelCircumference = 2 * Math.PI * wheelRadius;
        this.suspension = suspension
        this.suspensionStiffness = (this.mass * 10 * bouncy) / (this.suspension * 4)
        console.log(this.suspensionStiffness)
        this.wheels = [
            new Wheel(1.5, 2, wheelRadius, true),
            new Wheel(-1.5, 2, wheelRadius, true),
            new Wheel(1.5, -2, wheelRadius, false),
            new Wheel(-1.5, -2, wheelRadius, false),
        ]

        this.mesh = this.createCarMesh(scene);
        // this.mesh.userData = { name: "Car", rigidBody: this.rigidBody };;
    }

    private createUI() {

        this.infoHUD = document.createElement("span")
        this.positionHUD = document.createElement("span")
        this.linearVelocityHUD = document.createElement("span")
        this.suspensionHUD = document.createElement("span")

        document.getElementById("hud").append(...[this.infoHUD, this.positionHUD, this.linearVelocityHUD, this.suspensionHUD])
    }

    private createCarMesh(scene: THREE.Scene): THREE.Mesh {
        const carGeometry = new THREE.BoxGeometry(2, 1, 4);
        const carMaterial = new THREE.MeshStandardMaterial({ color: 0xff0000, wireframe: true });
        const carMesh = new THREE.Mesh(carGeometry, carMaterial);
        carMesh.castShadow = true;
        scene.add(carMesh);

        for (const wheel of this.wheels) {
            wheel.mesh.rotateZ(Math.PI / 2)
            scene.add(wheel.mesh)
            scene.add(wheel.raycastMesh)
            scene.add(wheel.contactMesh)
        }
        return carMesh;
    }

    update(deltaTime: number): void {

        this.handleKeyboardInput()

        // Sync mesh position/rotation with Rapier body
        const position = this.rigidBody.translation();
        const rotation = this.rigidBody.rotation();
        const linearVelocity = this.rigidBody.linvel()

        this.infoHUD.innerHTML = "Force : " + this.mass * 9.81
        this.positionHUD.innerHTML = "Pos : " + rVecString(position)
        this.linearVelocityHUD.innerHTML = "Vel : " + rVecString(linearVelocity)
        this.suspensionHUD.innerHTML = "Sus : "

        const carRotation = new THREE.Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);

        const carInverseRotation = new THREE.Quaternion(
            carRotation.x,
            carRotation.y,
            carRotation.z,
            carRotation.w
        ).invert();

        const rayDirectionWorld = new THREE.Vector3(0, -1, 0); // World space direction
        const rayDirectionLocal = rayDirectionWorld.clone().applyQuaternion(carInverseRotation);

        this.rigidBody.resetForces(true)
        this.rigidBody.resetTorques(true)

        // Update car body position and rotation
        this.mesh.position.set(position.x, position.y, position.z);
        this.mesh.quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);

        // this.rigidBody.addForce(new RAPIER.Vector3(0, -9.81 * this.mass, 0), true);

        // Rotate and position each wheel
        for (const wheel of this.wheels) {

            const rotatedWheelPosition = rVec(wheel.origin).applyQuaternion(carRotation);

            // Update wheel position relative to car body
            const wheelWorldPos = rVecAdd(position, rotatedWheelPosition);

            let ray = new RAPIER.Ray(wheelWorldPos, rayDirectionLocal)
            const hit = Physics.World.castRayAndGetNormal(ray, this.suspension + this.wheelRadius, true);

            if (hit) {

                const contactPoint = ray.pointAt(hit.timeOfImpact)
                wheel.contactMesh.position.set(contactPoint.x, contactPoint.y, contactPoint.z)

                // Suspension force (Hooke's law: F = -kx)
                // Calculate compression (distance to ground)
                let lastSpringLength = wheel.springLength
                const compressionDistance = this.wheelRadius + this.suspension - hit.timeOfImpact;
                wheel.springLength = Math.min(this.suspension, compressionDistance)
                let springVelocity = (lastSpringLength - wheel.springLength) / deltaTime

                const compressionForce = wheel.springLength * this.suspensionStiffness
                // const compressionForce = validCompression * this.suspensionStiffness
                this.suspensionHUD.innerHTML += "   (h:" + hit.timeOfImpact.toFixed(2) + " s:" + wheel.springLength.toFixed(2) + " c:" + " f:" + compressionForce.toFixed(2)

                // Damping force
                const dampingForce = springVelocity * this.damping
                this.suspensionHUD.innerHTML += "  d:" + springVelocity.toFixed(2) + ")  "

                // Apply total force at the wheel position
                const suspensionForce = rVecScale(new THREE.Vector3(0, rayDirectionLocal.y, 0), (- compressionForce + dampingForce))
                // const suspensionForce = rVecScale(new THREE.Vector3(0, -1, 0), - compressionForce + dampingForce)

                this.rigidBody.addForceAtPoint(suspensionForce, contactPoint, true);
            }

            {
                let ll = rVecAdd(wheelWorldPos, rVecScale(rayDirectionLocal, wheel.springLength))
                wheel.mesh.position.set(ll.x, ll.y, ll.z);
                wheel.raycastMesh.position.set(wheelWorldPos.x, wheelWorldPos.y, wheelWorldPos.z)
            }

            // Wheel rotation variables
            const forwardDistance = rVecMag(linearVelocity) // Linear velocity (speed)
            // wheel.mesh.rotation.x = (forwardDistance / this.wheelCircumference) * 2 * Math.PI; // Distance -> rotation;

            // // Apply steering rotation (if applicable) around the Y-axis
            // if (wheel.canSteer) {
            //     wheel.mesh.rotation.y = wheel.steerAngle; // `steerAngle` in radians
            // }
        }
    }

    handleKeyboardInput(): void {
        if (Keyboard.keys.ArrowUp) {
            Physics.applyImpulse(this.rigidBody, new THREE.Vector3(0, 0, this.speed))
        }
        if (Keyboard.keys.ArrowDown) {
            Physics.applyImpulse(this.rigidBody, new THREE.Vector3(0, 0, -this.speed))
        }

        // if (Keyboard.keys.ArrowLeft) {
        //     this.rigidBody.applyTorqueImpulse(new THREE.Vector3(0, 1, 0), true)
        // }
        // if (Keyboard.keys.ArrowRight) {
        //     this.rigidBody.applyTorqueImpulse(new THREE.Vector3(0, -1, 0), true)
        // }

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