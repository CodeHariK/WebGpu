import * as THREE from 'three';
import * as RAPIER from '@dimforge/rapier3d';
import { Physics, rVecAdd, rVec, rVecMag, rVecString, rVecScale, rVecSub, rVecDot, rVecAddd, UNIT_YN, UNIT_Z, ZERO, UNIT_X, UNIT_XN, clamp } from './physics';
import { Keyboard } from './keyboard';
import { AddLabel } from './ui';
import { CSS2DObject } from 'three/addons/renderers/CSS2DRenderer.js';
import { Game } from './game';

type WheelType = 'lefttop' | 'leftbottom' | 'righttop' | 'rightbottom'
class Wheel {
    origin: THREE.Vector3 = ZERO()

    velocity: THREE.Vector3 = ZERO()

    mesh: THREE.Mesh
    raycastMesh: THREE.ArrowHelper

    velocityHelp: THREE.ArrowHelper

    netForceHelp: THREE.ArrowHelper
    suspensionForceHelp: THREE.ArrowHelper
    forwardForceHelp: THREE.ArrowHelper
    curvatureForceHelp: THREE.ArrowHelper

    disHelp: THREE.ArrowHelper
    contactMesh: THREE.Mesh

    canSteer: boolean
    steerAngle: number
    label: CSS2DObject

    type: WheelType

    springCompression: number

    compressionForceMag: number = 0
    dampingForceMag: number = 0
    suspensionForce: THREE.Vector3 = ZERO()
    curvatureForce: THREE.Vector3 = ZERO()
    forceForward: THREE.Vector3 = ZERO()
    netForce: THREE.Vector3 = ZERO()

    contactPoint: THREE.Vector3 = ZERO()

    constructor(type: WheelType, x: number, z: number, radius: number, canSteer: boolean, springCompression: number) {

        this.type = type
        this.origin = new THREE.Vector3(x, 0, z)
        this.mesh = new THREE.Mesh(new THREE.CylinderGeometry(radius, radius, .2, 24), new THREE.MeshStandardMaterial({ color: Math.random() * 0xffffff, wireframe: true }))

        this.raycastMesh = new THREE.ArrowHelper(); this.raycastMesh.setColor(0x2200aa)
        this.velocityHelp = new THREE.ArrowHelper(); this.velocityHelp.setColor(0xaacc22)
        this.disHelp = new THREE.ArrowHelper(); this.disHelp.setColor(0x550055);

        this.forwardForceHelp = new THREE.ArrowHelper(); this.forwardForceHelp.setColor(0xff00ff)
        this.curvatureForceHelp = new THREE.ArrowHelper(); this.curvatureForceHelp.setColor(0xff0000)
        this.suspensionForceHelp = new THREE.ArrowHelper(); this.suspensionForceHelp.setColor(0x00ffff)
        this.netForceHelp = new THREE.ArrowHelper(); this.netForceHelp.setColor(0x00aaff)

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

    velocityHelp: THREE.ArrowHelper

    mass: number
    force = 100;

    wheelRadius: number
    wheelCircumference: number
    verticalWheelDistanceFromCenter: number
    horizontalWheelDistanceFromCenter: number
    turnRadius: number
    outerSteerAngle: number
    innerSteerAngle: number

    suspensionLength: number; // Maximum suspension travel
    suspensionStiffness: number; // Spring stiffness
    suspensionDamping: number; // Damping to reduce oscillation

    position: THREE.Vector3
    linearVelocity: THREE.Vector3
    rotation: THREE.Quaternion

    constructor(
        game: Game, position: THREE.Vector3, force: number, wheelRadius: number, mass: number,
        suspensionLength: number, suspensionStiffness: number, suspensionDamping: number,
        verticalTireDistanceFromCenter: number, horizontalTireDistanceFromCenter: number, turnRadius: number,
    ) {

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
        this.suspensionStiffness = (this.mass * 10 * suspensionStiffness) / (this.suspensionLength * 4)
        this.suspensionDamping = this.suspensionStiffness * Math.min(.4, suspensionDamping)

        this.position = position
        this.verticalWheelDistanceFromCenter = verticalTireDistanceFromCenter
        this.horizontalWheelDistanceFromCenter = horizontalTireDistanceFromCenter
        this.turnRadius = turnRadius

        this.wheels = [
            new Wheel('righttop', this.horizontalWheelDistanceFromCenter, this.verticalWheelDistanceFromCenter, wheelRadius, true, this.suspensionLength),
            new Wheel('lefttop', -this.horizontalWheelDistanceFromCenter, this.verticalWheelDistanceFromCenter, wheelRadius, true, this.suspensionLength),
            new Wheel('rightbottom', this.horizontalWheelDistanceFromCenter, -this.verticalWheelDistanceFromCenter, wheelRadius, false, this.suspensionLength),
            new Wheel('leftbottom', -this.horizontalWheelDistanceFromCenter, -this.verticalWheelDistanceFromCenter, wheelRadius, false, this.suspensionLength),
        ]

        this.outerSteerAngle = Math.atan2(this.verticalWheelDistanceFromCenter * 2, this.turnRadius + this.horizontalWheelDistanceFromCenter)
        this.innerSteerAngle = Math.atan2(this.verticalWheelDistanceFromCenter * 2, this.turnRadius - this.horizontalWheelDistanceFromCenter)

        this.mesh = this.createCarMesh(game.SCENE);
    }

    private createUI() {

        this.infoHUD = document.createElement("span")
        this.positionHUD = document.createElement("span")
        this.linearVelocityHUD = document.createElement("span")

        document.getElementById("hud").append(...[this.infoHUD, this.positionHUD, this.linearVelocityHUD])
    }

    private updateUI(wheel: Wheel) {

        this.infoHUD.innerHTML = "Force : " + this.mass * 9.81
        this.positionHUD.innerHTML = "Pos : " + rVecString(this.position)
        this.linearVelocityHUD.innerHTML = "Vel : " + rVecString(this.linearVelocity)

        wheel.label.element.innerHTML =
            "<br>c: " + wheel.springCompression.toFixed(2) +
            "<br>d: " + wheel.dampingForceMag.toFixed(2) +
            "<br>v: " + rVecString(wheel.velocity) +
            "<br>r: " + THREE.MathUtils.radToDeg(wheel.steerAngle).toFixed(2) +
            "<br>s: " + rVecString(wheel.suspensionForce) +
            "<br>f: " + rVecString(wheel.forceForward) +
            "<br>c: " + rVecString(wheel.curvatureForce) +
            "<br>n: " + rVecString(wheel.netForce)

        wheel.contactMesh.position.set(wheel.contactPoint.x, wheel.contactPoint.y, wheel.contactPoint.z)
        wheel.contactMesh.setRotationFromQuaternion(this.rotation)

        wheel.suspensionForceHelp.position.set(wheel.contactPoint.x, wheel.contactPoint.y + .1, wheel.contactPoint.z)
        wheel.suspensionForceHelp.setLength(Math.min(2, wheel.suspensionForce.length()))
        wheel.suspensionForceHelp.setDirection(rVec(wheel.suspensionForce).normalize())

        wheel.curvatureForceHelp.position.set(wheel.contactPoint.x, wheel.contactPoint.y + .1, wheel.contactPoint.z)
        wheel.curvatureForceHelp.setLength(clamp(wheel.curvatureForce.length(), 0, 2))
        wheel.curvatureForceHelp.setDirection(rVec(wheel.curvatureForce).normalize())

        wheel.forwardForceHelp.position.set(wheel.contactPoint.x, wheel.contactPoint.y + .1, wheel.contactPoint.z)
        wheel.forwardForceHelp.setLength(Math.min(2, wheel.forceForward.length()))
        wheel.forwardForceHelp.setDirection(rVec(wheel.forceForward).normalize())

        this.velocityHelp.position.set(this.position.x, this.position.y, this.position.z)
        this.velocityHelp.setLength(rVec(this.linearVelocity).length())
        this.velocityHelp.setDirection(rVec(this.linearVelocity).normalize())

        wheel.velocityHelp.position.set(wheel.contactPoint.x, wheel.contactPoint.y + .1, wheel.contactPoint.z)
        wheel.velocityHelp.setLength(Math.min(2, wheel.velocity.length()))
        wheel.velocityHelp.setDirection((rVec(wheel.velocity).normalize()))

        let dis = rVecSub(wheel.contactPoint, this.position)
        wheel.disHelp.position.set(this.position.x, this.position.y + .1, this.position.z)
        wheel.disHelp.setLength(dis.length())
        wheel.disHelp.setDirection(dis.normalize())

        wheel.netForceHelp.position.set(wheel.contactPoint.x, wheel.contactPoint.y + .1, wheel.contactPoint.z)
        wheel.netForceHelp.setLength(Math.min(2, wheel.netForce.length()))
        wheel.netForceHelp.setDirection(rVec(wheel.netForce).normalize())

    }

    private createCarMesh(scene: THREE.Scene): THREE.Mesh {
        const carGeometry = new THREE.BoxGeometry(2, 1, 4);
        const carMaterial = new THREE.MeshStandardMaterial({ color: 0xff0000, wireframe: true });
        const carMesh = new THREE.Mesh(carGeometry, carMaterial);
        carMesh.castShadow = true;
        scene.add(carMesh);

        this.velocityHelp = new THREE.ArrowHelper(); this.velocityHelp.setColor(0xffffff)
        scene.add(this.velocityHelp);

        for (const wheel of this.wheels) {
            scene.add(wheel.mesh)
            scene.add(wheel.raycastMesh)

            scene.add(wheel.velocityHelp)
            scene.add(wheel.disHelp)

            scene.add(wheel.forwardForceHelp)
            scene.add(wheel.suspensionForceHelp)
            scene.add(wheel.netForceHelp)
            scene.add(wheel.curvatureForceHelp)

            scene.add(wheel.contactMesh)
        }
        return carMesh;
    }

    update(game: Game, deltaTime: number): void {

        this.handleKeyboardInput(deltaTime)

        {
            // Sync mesh position/rotation with Rapier body
            this.position = rVec(this.rigidBody.translation());
            this.linearVelocity = rVec(this.rigidBody.linvel())
            const rotation = this.rigidBody.rotation();

            this.rotation = new THREE.Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
            // Update car body position and rotation
            this.mesh.position.set(this.position.x, this.position.y, this.position.z);
            this.mesh.quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);

        }

        const Car_YN_Local_Down = UNIT_YN().applyQuaternion(this.rotation);
        const Car_Z_Local_Forward = UNIT_Z().applyQuaternion(this.rotation)
        const Car_XN_Local_Left = UNIT_XN().applyQuaternion(this.rotation)

        this.rigidBody.resetForces(true)
        this.rigidBody.resetTorques(true)

        // Rotate and position each wheel
        for (const wheel of this.wheels) {

            const rotatedWheelPosition = rVec(wheel.origin).applyQuaternion(this.rotation);

            // Update wheel position relative to car body
            const wheelWorldPos = rVecAdd(this.position, rotatedWheelPosition);

            let ray = new RAPIER.Ray(wheelWorldPos, Car_YN_Local_Down)
            const hit = game.WORLD.castRayAndGetNormal(ray, this.suspensionLength + this.wheelRadius, true);

            if (hit) {
                wheel.contactMesh.visible = true
                wheel.velocityHelp.visible = true

                {
                    // Suspension force (Hooke's law: F = -kx)
                    let lastSpringCompression = wheel.springCompression
                    wheel.springCompression = Math.min(this.suspensionLength, this.wheelRadius + this.suspensionLength - hit.timeOfImpact)
                    let springVelocity = (lastSpringCompression - wheel.springCompression) / deltaTime
                    wheel.compressionForceMag = wheel.springCompression * this.suspensionStiffness
                    wheel.dampingForceMag = springVelocity * this.suspensionDamping
                    wheel.suspensionForce = rVecScale(new THREE.Vector3(0, Car_YN_Local_Down.y, 0), (- wheel.compressionForceMag + wheel.dampingForceMag))
                    // const suspensionForce = rVecScale(new THREE.Vector3(0, -1, 0), - compressionForce + dampingForce)
                }

                {
                    wheel.contactPoint = rVec(ray.pointAt(hit.timeOfImpact))

                    wheel.velocity = Physics.linearVelocityAtWorldPoint(this.rigidBody, wheel.contactPoint)

                    const rotationMatrix = new THREE.Matrix4();
                    rotationMatrix.makeRotationY(wheel.steerAngle)
                    let wheelForwardDir = rVec(Car_Z_Local_Forward).applyMatrix4(rotationMatrix).normalize()

                    wheel.forceForward = wheelForwardDir
                    let acc = this.force
                    if (wheel.canSteer) {
                        if (Keyboard.keys.ArrowUp) {
                            wheel.forceForward = rVecScale(wheelForwardDir, acc)
                        } else if (Keyboard.keys.ArrowDown) {
                            wheel.forceForward = rVecScale(wheelForwardDir, - acc)
                        }
                    }
                }

                {
                    wheel.curvatureForce = new THREE.Vector3(0, 0, 0)
                    if (Keyboard.keys.ArrowUp || Keyboard.keys.ArrowDown) {
                        wheel.curvatureForce = rVecScale(wheel.velocity, 200)
                    }

                    // wheel.curvatureForce = rVecScale(Car_XN_Local_Left, (wheel.velocity.length() > 1 ? 50 : 0) * rVecDot(Car_XN_Local_Left, wheel.velocity))
                }

                {
                    wheel.netForce = rVecAddd([wheel.suspensionForce, wheel.forceForward, wheel.curvatureForce])

                    this.rigidBody.addForceAtPoint(wheel.netForce, wheel.contactPoint, true);
                }
            } else {
                wheel.contactMesh.visible = false
                wheel.velocityHelp.visible = false
            }

            {
                let ll = rVecAdd(wheelWorldPos, rVecScale(Car_YN_Local_Down, wheel.springCompression))
                wheel.mesh.position.set(ll.x, ll.y, ll.z);
                wheel.raycastMesh.position.set(wheelWorldPos.x, wheelWorldPos.y, wheelWorldPos.z)
                wheel.raycastMesh.setLength(this.suspensionLength + this.wheelRadius)
                wheel.raycastMesh.setDirection(Car_YN_Local_Down)
            }

            // Wheel rotation variables
            const forwardDistance = rVecMag(this.linearVelocity) // Linear velocity (speed)
            wheel.mesh.setRotationFromQuaternion(this.rotation)
            wheel.mesh.rotateZ(Math.PI / 2)
            wheel.mesh.rotateX(wheel.steerAngle);
            // wheel.mesh.rotateY(+ 10 * forwardDistance / this.wheelRadius)

            this.updateUI(wheel)
        }

    }

    handleKeyboardInput(deltaTime: number): void {
        // Adjust steering angle for front tires
        const steeringSpeed = 1 * deltaTime; // Steering angle increment

        for (const wheel of this.wheels) {
            // if (wheel.canSteer) {
            // Increase or decrease steering angle based on input
            if (Keyboard.keys.ArrowLeft) {
                if (wheel.type == 'lefttop') {
                    wheel.steerAngle = Math.min(wheel.steerAngle + steeringSpeed, this.outerSteerAngle);
                }
                if (wheel.type == 'righttop') {
                    wheel.steerAngle = Math.min(wheel.steerAngle + steeringSpeed, this.innerSteerAngle);
                }
                // if (wheel.type == 'leftbottom') {
                //     wheel.steerAngle = Math.max(wheel.steerAngle - steeringSpeed, - this.innerSteerAngle);
                // }
                // if (wheel.type == 'rightbottom') {
                //     wheel.steerAngle = Math.max(wheel.steerAngle - steeringSpeed, - this.outerSteerAngle);
                // }
            } else if (Keyboard.keys.ArrowRight) {
                if (wheel.type == 'lefttop') {
                    wheel.steerAngle = Math.max(wheel.steerAngle - steeringSpeed, - this.innerSteerAngle);
                }
                if (wheel.type == 'righttop') {
                    wheel.steerAngle = Math.max(wheel.steerAngle - steeringSpeed, - this.outerSteerAngle);
                }
                // if (wheel.type == 'leftbottom') {
                //     wheel.steerAngle = Math.min(wheel.steerAngle + steeringSpeed, this.outerSteerAngle);
                // }
                // if (wheel.type == 'rightbottom') {
                //     wheel.steerAngle = Math.min(wheel.steerAngle + steeringSpeed, this.innerSteerAngle);
                // }
            } else {
                // Smoothly return to center
                if (steeringSpeed) {
                    if (wheel.steerAngle > 0) {
                        wheel.steerAngle = Math.max(0, wheel.steerAngle - steeringSpeed);
                    } else {
                        wheel.steerAngle = Math.min(0, wheel.steerAngle + steeringSpeed);
                    }

                }
            }
            // }
        }
    }
}
