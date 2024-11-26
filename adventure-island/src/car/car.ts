import * as THREE from 'three';
import * as RAPIER from '@dimforge/rapier3d';
import { Physics, rVecAdd, rVec, rVecMag, rVecString, rVecScale } from './physics';
import { Keyboard } from './keyboard';
import { AddLabel } from './ui';
import { CSS2DObject } from 'three/addons/renderers/CSS2DRenderer.js';
import { Game } from './game';

type WheelType = 'lefttop' | 'leftbottom' | 'righttop' | 'rightbottom'
class Wheel {
    origin: THREE.Vector3
    mesh: THREE.Mesh
    raycastMesh: THREE.ArrowHelper
    velMesh: THREE.ArrowHelper
    contactMesh: THREE.Mesh
    canSteer: boolean
    steerAngle: number
    label: CSS2DObject

    type: WheelType

    springCompression: number

    constructor(type: WheelType, x: number, z: number, radius: number, canSteer: boolean, springCompression: number) {

        this.type = type
        this.origin = new THREE.Vector3(x, 0, z)
        this.mesh = new THREE.Mesh(new THREE.CylinderGeometry(radius, radius, .2, 24), new THREE.MeshStandardMaterial({ color: Math.random() * 0xffffff, wireframe: true }))
        this.raycastMesh = new THREE.ArrowHelper()
        this.velMesh = new THREE.ArrowHelper()
        this.velMesh.setColor(0xffffff)
        this.contactMesh = new THREE.Mesh(new THREE.BoxGeometry(.3, .3, .3, 2), new THREE.MeshStandardMaterial({ color: Math.random() * 0xffffff }))
        this.canSteer = canSteer
        this.steerAngle = 0

        this.springCompression = springCompression

        this.label = AddLabel(this.type, this.mesh)
    }
}

export class Car {
    mesh: THREE.Mesh;
    rigidBody: RAPIER.RigidBody;

    wheels: Wheel[]

    positionHUD: HTMLElement
    linearVelocityHUD: HTMLElement
    infoHUD: HTMLElement

    mass: number
    force = 2;

    wheelRadius: number
    wheelCircumference: number

    suspensionLength: number; // Maximum suspension travel
    suspensionStiffness: number; // Spring stiffness
    suspensionDamping: number; // Damping to reduce oscillation

    verticalTireDistanceFromCenter: number = 2
    horizontalTireDistanceFromCenter: number = 1.5
    turnRadius: number = 10
    outerSteerAngle: number
    innerSteerAngle: number

    constructor(game: Game, position: THREE.Vector3, force: number, wheelRadius: number, mass: number, suspensionLength: number, suspensionBounce: number, suspensionDamping: number) {

        this.createUI()

        // Create Rapier RigidBody
        const rigidBodyDesc = RAPIER.RigidBodyDesc.dynamic().setTranslation(position.x, position.y, position.z);
        this.rigidBody = game.WORLD.createRigidBody(rigidBodyDesc);

        // Add a Box Collider
        const colliderDesc = RAPIER.ColliderDesc.cuboid(1, .5, 2); // Dimensions match the car mesh
        game.WORLD.createCollider(colliderDesc, this.rigidBody);

        this.rigidBody.setLinearDamping(1);
        this.rigidBody.setAngularDamping(1);

        let addWeight = Math.max(0, mass - this.rigidBody.mass())
        this.rigidBody.setAdditionalMass(addWeight, true)
        this.mass = this.rigidBody.mass() + addWeight

        this.force = force
        this.wheelRadius = wheelRadius
        this.wheelCircumference = 2 * Math.PI * wheelRadius;
        this.suspensionLength = suspensionLength
        this.suspensionStiffness = (this.mass * 10 * suspensionBounce) / (this.suspensionLength * 4)
        this.suspensionDamping = this.suspensionStiffness * Math.min(.4, suspensionDamping)

        this.wheels = [
            new Wheel('righttop', this.horizontalTireDistanceFromCenter, this.verticalTireDistanceFromCenter, wheelRadius, true, this.suspensionLength),
            new Wheel('lefttop', -this.horizontalTireDistanceFromCenter, this.verticalTireDistanceFromCenter, wheelRadius, true, this.suspensionLength),
            new Wheel('rightbottom', this.horizontalTireDistanceFromCenter, -this.verticalTireDistanceFromCenter, wheelRadius, false, this.suspensionLength),
            new Wheel('leftbottom', -this.horizontalTireDistanceFromCenter, -this.verticalTireDistanceFromCenter, wheelRadius, false, this.suspensionLength),
        ]

        this.outerSteerAngle = Math.atan2(this.verticalTireDistanceFromCenter * 2, this.turnRadius + this.horizontalTireDistanceFromCenter)
        this.innerSteerAngle = Math.atan2(this.verticalTireDistanceFromCenter * 2, this.turnRadius - this.horizontalTireDistanceFromCenter)

        this.mesh = this.createCarMesh(game.SCENE);
    }

    private createUI() {

        this.infoHUD = document.createElement("span")
        this.positionHUD = document.createElement("span")
        this.linearVelocityHUD = document.createElement("span")

        document.getElementById("hud").append(...[this.infoHUD, this.positionHUD, this.linearVelocityHUD])
    }

    private createCarMesh(scene: THREE.Scene): THREE.Mesh {
        const carGeometry = new THREE.BoxGeometry(2, 1, 4);
        const carMaterial = new THREE.MeshStandardMaterial({ color: 0xff0000, wireframe: true });
        const carMesh = new THREE.Mesh(carGeometry, carMaterial);
        carMesh.castShadow = true;
        scene.add(carMesh);

        for (const wheel of this.wheels) {
            // wheel.mesh.rotateZ(Math.PI / 2)
            scene.add(wheel.mesh)
            scene.add(wheel.raycastMesh)
            scene.add(wheel.velMesh)
            scene.add(wheel.contactMesh)
        }
        return carMesh;
    }

    update(game: Game, deltaTime: number): void {

        this.handleKeyboardInput(deltaTime)

        // Sync mesh position/rotation with Rapier body
        const position = this.rigidBody.translation();
        const rotation = this.rigidBody.rotation();
        const linearVelocity = this.rigidBody.linvel()

        this.infoHUD.innerHTML = "Force : " + this.mass * 9.81
        this.positionHUD.innerHTML = "Pos : " + rVecString(position)
        this.linearVelocityHUD.innerHTML = "Vel : " + rVecString(linearVelocity)

        const carRotation = new THREE.Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);

        const downDirectionLocal = (new THREE.Vector3(0, -1, 0)).clone().applyQuaternion(carRotation);

        const forwardDirectionLocal = (new THREE.Vector3(0, 0, 1)).applyQuaternion(carRotation)
        const leftDirectionLocal = (new THREE.Vector3(-1, 0, 0)).applyQuaternion(carRotation)

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

            let ray = new RAPIER.Ray(wheelWorldPos, downDirectionLocal)
            const hit = game.WORLD.castRayAndGetNormal(ray, this.suspensionLength + this.wheelRadius, true);

            if (hit) {
                wheel.contactMesh.visible = true

                const contactPoint = ray.pointAt(hit.timeOfImpact)
                wheel.contactMesh.position.set(contactPoint.x, contactPoint.y, contactPoint.z)
                wheel.contactMesh.setRotationFromQuaternion(carRotation)

                let velocityAtContact = Physics.linearVelocityAtWorldPoint(this.rigidBody, contactPoint)
                wheel.velMesh.position.set(contactPoint.x, contactPoint.y, contactPoint.z)
                wheel.velMesh.setLength(velocityAtContact.length())
                wheel.velMesh.setDirection(rVec(velocityAtContact).normalize())

                // Suspension force (Hooke's law: F = -kx)
                let lastSpringCompression = wheel.springCompression
                wheel.springCompression = Math.min(this.suspensionLength, this.wheelRadius + this.suspensionLength - hit.timeOfImpact)
                let springVelocity = (lastSpringCompression - wheel.springCompression) / deltaTime

                const compressionForce = wheel.springCompression * this.suspensionStiffness

                // Damping force
                const dampingForce = springVelocity * this.suspensionDamping

                // Apply total force at the wheel position
                const suspensionForce = rVecScale(new THREE.Vector3(0, downDirectionLocal.y, 0), (- compressionForce + dampingForce))
                // const suspensionForce = rVecScale(new THREE.Vector3(0, -1, 0), - compressionForce + dampingForce)

                wheel.label.element.innerHTML += "<br>  c:" + wheel.springCompression.toFixed(2) + " <br> d:" + dampingForce.toFixed(2) + "  <br>  s:" + rVecString(suspensionForce) + "  <br>  v:" + rVecString(velocityAtContact)

                let forceForward = new THREE.Vector3(0, 0, 0)
                if (Keyboard.keys.ArrowUp) {
                    forceForward = rVecScale(forwardDirectionLocal, 1000)
                }
                if (Keyboard.keys.ArrowDown) {
                    forceForward = rVecScale(forwardDirectionLocal, - 1000)
                }

                let curvatureForce = rVecScale(leftDirectionLocal, velocityAtContact.x * 10)

                this.rigidBody.addForceAtPoint(rVecAdd(rVecAdd(suspensionForce, forceForward), curvatureForce), contactPoint, true);
            } else {
                wheel.contactMesh.visible = false
            }

            {
                let ll = rVecAdd(wheelWorldPos, rVecScale(downDirectionLocal, wheel.springCompression))
                wheel.mesh.position.set(ll.x, ll.y, ll.z);
                wheel.raycastMesh.position.set(wheelWorldPos.x, wheelWorldPos.y, wheelWorldPos.z)
                wheel.raycastMesh.setLength(this.suspensionLength + this.wheelRadius)
                wheel.raycastMesh.setDirection(downDirectionLocal)
            }

            // Wheel rotation variables
            const forwardDistance = rVecMag(linearVelocity) // Linear velocity (speed)
            wheel.mesh.setRotationFromQuaternion(carRotation)
            wheel.mesh.rotateZ(Math.PI / 2)
            wheel.mesh.rotateX(wheel.steerAngle);
            wheel.mesh.rotateY(forwardDistance / this.wheelRadius)
        }
    }

    handleKeyboardInput(deltaTime: number): void {
        // if (Keyboard.keys.ArrowUp) {
        //     // Physics.applyImpulse(this.rigidBody, new THREE.Vector3(0, 0, this.force))
        // }
        // if (Keyboard.keys.ArrowDown) {
        //     // Physics.applyImpulse(this.rigidBody, new THREE.Vector3(0, 0, -this.force))
        // }

        // if (Keyboard.keys.ArrowLeft) {
        //     this.rigidBody.applyTorqueImpulse(new THREE.Vector3(0, 40, 0), true)
        // }
        // if (Keyboard.keys.ArrowRight) {
        //     this.rigidBody.applyTorqueImpulse(new THREE.Vector3(0, -40, 0), true)
        // }

        // Adjust steering angle for front tires
        const steeringSpeed = 1 * deltaTime; // Steering angle increment

        for (const wheel of this.wheels) {
            wheel.label.element.innerHTML = ""
            // if (wheel.canSteer) {
            // Increase or decrease steering angle based on input
            if (Keyboard.keys.ArrowLeft) {
                if (wheel.type == 'lefttop') {
                    wheel.steerAngle = Math.min(wheel.steerAngle + steeringSpeed, this.outerSteerAngle);
                    wheel.label.element.innerHTML += " <br>  l:" + THREE.MathUtils.radToDeg(wheel.steerAngle).toFixed(2)
                }
                if (wheel.type == 'righttop') {
                    wheel.steerAngle = Math.min(wheel.steerAngle + steeringSpeed, this.innerSteerAngle);
                    wheel.label.element.innerHTML += " <br>  r:" + THREE.MathUtils.radToDeg(wheel.steerAngle).toFixed(2)
                }
                if (wheel.type == 'leftbottom') {
                    wheel.steerAngle = Math.max(wheel.steerAngle - steeringSpeed, - this.innerSteerAngle);
                    wheel.label.element.innerHTML += " <br>  l:" + THREE.MathUtils.radToDeg(wheel.steerAngle).toFixed(2)
                }
                if (wheel.type == 'rightbottom') {
                    wheel.steerAngle = Math.max(wheel.steerAngle - steeringSpeed, - this.outerSteerAngle);
                    wheel.label.element.innerHTML += " <br>  r:" + THREE.MathUtils.radToDeg(wheel.steerAngle).toFixed(2)
                }
            } else if (Keyboard.keys.ArrowRight) {
                if (wheel.type == 'lefttop') {
                    wheel.steerAngle = Math.max(wheel.steerAngle - steeringSpeed, - this.innerSteerAngle);
                    wheel.label.element.innerHTML += " <br>  l:" + THREE.MathUtils.radToDeg(wheel.steerAngle).toFixed(2)
                }
                if (wheel.type == 'righttop') {
                    wheel.steerAngle = Math.max(wheel.steerAngle - steeringSpeed, - this.outerSteerAngle);
                    wheel.label.element.innerHTML += " <br>  r:" + THREE.MathUtils.radToDeg(wheel.steerAngle).toFixed(2)
                }
                if (wheel.type == 'leftbottom') {
                    wheel.steerAngle = Math.min(wheel.steerAngle + steeringSpeed, this.outerSteerAngle);
                    wheel.label.element.innerHTML += " <br>  l:" + THREE.MathUtils.radToDeg(wheel.steerAngle).toFixed(2)
                }
                if (wheel.type == 'rightbottom') {
                    wheel.steerAngle = Math.min(wheel.steerAngle + steeringSpeed, this.innerSteerAngle);
                    wheel.label.element.innerHTML += " <br>  r:" + THREE.MathUtils.radToDeg(wheel.steerAngle).toFixed(2)
                }
            } else {
                wheel.steerAngle *= .9; // Smoothly return to center
            }
            // }
        }
    }
}
