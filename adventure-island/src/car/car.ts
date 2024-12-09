import * as THREE from 'three';
import * as RAPIER from '@dimforge/rapier3d';
import { Physics, rVecAdd, rVec, rVecMag, rVecString, rVecScale, rVecSub, rVecDot, rVecAddd, UNIT_YN, UNIT_Z, ZERO, UNIT_X, UNIT_XN, clamp, rQuat } from './physics';
import { Keyboard } from './keyboard';
import { AddLabel, createCheckbox, hudUpdate } from './ui';
import { CSS2DObject } from 'three/addons/renderers/CSS2DRenderer.js';
import { Game } from './game';

type WheelType = 'lefttop' | 'leftbottom' | 'righttop' | 'rightbottom'
type CarType = 'AWD' | 'RWD' | 'FWD'

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

    constructor(game: Game, type: WheelType, x: number, y: number, z: number, radius: number, canSteer: boolean, springCompression: number) {

        this.type = type
        this.origin = new THREE.Vector3(x, y, z)
        this.mesh = new THREE.Mesh(new THREE.SphereGeometry(radius / 6, 8, 8), new THREE.MeshStandardMaterial({ color: Math.random() * 0xffffff, wireframe: true }))

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

        this.label = AddLabel(this.type, this.contactMesh)

        game.SCENE.add(this.mesh)
        game.SCENE.add(this.raycastMesh)

        game.SCENE.add(this.velocityHelp)
        game.SCENE.add(this.disHelp)

        game.SCENE.add(this.forwardForceHelp)
        game.SCENE.add(this.suspensionForceHelp)
        game.SCENE.add(this.netForceHelp)
        game.SCENE.add(this.curvatureForceHelp)

        game.SCENE.add(this.contactMesh)
    }
}

export class Car {
    mesh: THREE.Mesh;
    rigidBody: RAPIER.RigidBody;

    wheels: Wheel[]

    velocityHelp: THREE.ArrowHelper

    mass: number

    accelerationForce: number;
    nitrousForce: number;
    decelerationForce: number;

    type: CarType = 'FWD'

    wheelRadius: number
    wheelGripTop: number
    wheelGripBottom: number
    steeringTorque: number

    wheelContact: number

    cameraType: 'TPS' | 'Orbit' = 'TPS'

    suspensionLength: number; // Maximum suspension travel
    suspensionStiffness: number; // Spring stiffness
    suspensionDamping: number; // Damping to reduce oscillation

    position: THREE.Vector3
    linearVelocity: THREE.Vector3 = ZERO()
    rotation: THREE.Quaternion

    private outerSteerAngle: number
    private innerSteerAngle: number
    private wheelRotation: number

    constructor(
        game: Game, position: THREE.Vector3,
        accelerationForce: number, nitrousForce: number, decelerationForce: number,
        type: CarType, mass: number,
        suspensionLength: number, suspensionStiffness: number, suspensionDamping: number,
        trackWidth: number, wheelBase: number, groundClearance: number, wheelRadius: number,
        carWidth: number, carLength: number, carHeight: number,
        wheelGripTop: number, wheelGripBottom: number, steeringTorque: number
    ) {
        // Create Rapier RigidBody
        const rigidBodyDesc = RAPIER.RigidBodyDesc.dynamic().setTranslation(position.x, position.y, position.z);
        this.rigidBody = game.WORLD.createRigidBody(rigidBodyDesc);

        // Add a Box Collider
        const colliderDesc = RAPIER.ColliderDesc
            .cuboid(carWidth / 2, carHeight / 2, carLength / 2);
        game.WORLD.createCollider(colliderDesc, this.rigidBody);

        let addWeight = Math.max(0, mass - this.rigidBody.mass())
        this.rigidBody.setAdditionalMass(addWeight, true)
        this.mass = this.rigidBody.mass() + addWeight

        this.accelerationForce = accelerationForce
        this.nitrousForce = nitrousForce
        this.decelerationForce = decelerationForce
        this.type = type

        this.wheelRadius = wheelRadius
        this.suspensionLength = suspensionLength
        this.suspensionStiffness = (this.mass * 10 * suspensionStiffness) / (this.suspensionLength * 4)
        this.suspensionDamping = this.suspensionStiffness * clamp(suspensionDamping, .1, .9)

        this.position = position

        this.wheelGripTop = wheelGripTop
        this.wheelGripBottom = wheelGripBottom
        this.steeringTorque = steeringTorque

        this.outerSteerAngle = Math.atan2(carLength, 10 + carWidth / 2)
        this.innerSteerAngle = Math.atan2(carLength, 10 - carWidth / 2)
        this.wheelRotation = 0

        let suspensionAnchor = wheelRadius - groundClearance + suspensionLength - carHeight / 2
        this.wheels = [
            new Wheel(game, 'righttop', trackWidth / 2, suspensionAnchor, wheelBase / 2, wheelRadius, true, this.suspensionLength),
            new Wheel(game, 'lefttop', -trackWidth / 2, suspensionAnchor, wheelBase / 2, wheelRadius, true, this.suspensionLength),
            new Wheel(game, 'rightbottom', trackWidth / 2, suspensionAnchor, -wheelBase / 2, wheelRadius, false, this.suspensionLength),
            new Wheel(game, 'leftbottom', -trackWidth / 2, suspensionAnchor, -wheelBase / 2, wheelRadius, false, this.suspensionLength),
        ]

        const carGeometry = new THREE.BoxGeometry(carWidth, carHeight, carLength);
        const carMaterial = new THREE.MeshStandardMaterial({ color: 0xff0000, wireframe: true });
        const carMesh = new THREE.Mesh(carGeometry, carMaterial);
        carMesh.castShadow = true;
        game.SCENE.add(carMesh);
        this.mesh = carMesh;

        this.velocityHelp = new THREE.ArrowHelper(); this.velocityHelp.setColor(0xffffff)
        game.SCENE.add(this.velocityHelp);

        createCheckbox('Orbit : ', this.cameraType == 'Orbit', () => {
            this.cameraType = this.cameraType == 'TPS' ? 'Orbit' : 'TPS'
            game.cameraUpdate(this.mesh, this.cameraType)
        })
    }

    private updateUI(game: Game, wheel: Wheel) {

        if (wheel.type === 'lefttop') {
            hudUpdate("Force : " + this.mass * 9.81)
            hudUpdate("Pos : " + rVecString(this.position))
            hudUpdate("Vel : " + rVecString(this.linearVelocity))
        }

        // wheel.label.element.innerHTML = ""
        // wheel.label.element.innerHTML += "<br>c: " + wheel.springCompression.toFixed(2)
        // wheel.label.element.innerHTML += "<br>d: " + wheel.dampingForceMag.toFixed(2)
        // wheel.label.element.innerHTML += "<br>v: " + rVecString(wheel.velocity)
        // wheel.label.element.innerHTML += "<br>r: " + THREE.MathUtils.radToDeg(wheel.steerAngle).toFixed(2)
        // wheel.label.element.innerHTML += "<br>s: " + rVecString(wheel.suspensionForce)
        // wheel.label.element.innerHTML += "<br>f: " + rVecString(wheel.forceForward)
        // wheel.label.element.innerHTML += "<br>c: " + rVecString(wheel.curvatureForce)
        // wheel.label.element.innerHTML += "<br>n: " + rVecString(wheel.netForce)

        // wheel.contactMesh.position.set(wheel.contactPoint.x, wheel.contactPoint.y, wheel.contactPoint.z)
        // wheel.contactMesh.setRotationFromQuaternion(this.rotation)

        // wheel.suspensionForceHelp.position.set(wheel.contactPoint.x, wheel.contactPoint.y + .1, wheel.contactPoint.z)
        // wheel.suspensionForceHelp.setLength(clamp(wheel.suspensionForce.length(), 0, 2))
        // wheel.suspensionForceHelp.setDirection(rVec(wheel.suspensionForce).normalize())

        // wheel.curvatureForceHelp.position.set(wheel.contactPoint.x, wheel.contactPoint.y + .1, wheel.contactPoint.z)
        // wheel.curvatureForceHelp.setLength(clamp(wheel.curvatureForce.length(), 0, 2))
        // wheel.curvatureForceHelp.setDirection(rVec(wheel.curvatureForce).normalize())

        // wheel.forwardForceHelp.position.set(wheel.contactPoint.x, wheel.contactPoint.y + .1, wheel.contactPoint.z)
        // wheel.forwardForceHelp.setLength(clamp(wheel.forceForward.length(), 0, 5))
        // wheel.forwardForceHelp.setDirection(rVec(wheel.forceForward).normalize())

        // this.velocityHelp.position.set(this.position.x, this.position.y, this.position.z)
        // this.velocityHelp.setLength(clamp(this.linearVelocity.length(), 0, 2))
        // this.velocityHelp.setDirection(rVec(this.linearVelocity).normalize())

        // wheel.velocityHelp.position.set(wheel.contactPoint.x, wheel.contactPoint.y + .1, wheel.contactPoint.z)
        // wheel.velocityHelp.setLength(clamp(wheel.velocity.length(), 0, 2))
        // wheel.velocityHelp.setDirection((rVec(wheel.velocity).normalize()))

        // let dis = rVecSub(wheel.contactPoint, this.position)
        // wheel.disHelp.position.set(this.position.x, this.position.y + .1, this.position.z)
        // wheel.disHelp.setLength(dis.length())
        // wheel.disHelp.setDirection(dis.normalize())

        // wheel.netForceHelp.position.set(wheel.contactPoint.x, wheel.contactPoint.y + .1, wheel.contactPoint.z)
        // wheel.netForceHelp.setLength(clamp(wheel.netForce.length(), 0, 2))
        // wheel.netForceHelp.setDirection(rVec(wheel.netForce).normalize())

        game.cameraUpdate(this.mesh, this.cameraType)
    }

    update(game: Game, deltaTime: number): void {

        // if (this.wheelContact >= 2) {
        this.handleKeyboardInput(deltaTime)
        // }

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

        this.wheelContact = 0

        // Rotate and position each wheel
        for (const wheel of this.wheels) {

            // Update wheel position relative to car body
            const suspensionAnchorPos = rVecAdd(
                this.position,
                rVec(wheel.origin).applyQuaternion(this.rotation)
            );

            let ray = new RAPIER.Ray(suspensionAnchorPos, Car_YN_Local_Down)
            const hit = game.WORLD.castRayAndGetNormal(
                ray,
                this.suspensionLength + this.wheelRadius,
                true,
                null, null, null, this.rigidBody);

            if (hit) {
                wheel.contactMesh.visible = true
                wheel.velocityHelp.visible = true

                this.wheelContact++

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
                    wheel.forceForward = ZERO()
                    if (
                        (this.type === 'FWD' && (wheel.type === 'lefttop' || wheel.type === 'righttop')) ||
                        (this.type === 'RWD' && (wheel.type === 'leftbottom' || wheel.type === 'rightbottom')) ||
                        (this.type === 'AWD')
                    ) {
                        if (Keyboard.keys.Up) {
                            wheel.forceForward = rVecScale(Car_Z_Local_Forward, Keyboard.keys.Nitrous ? this.nitrousForce : this.accelerationForce)
                        } else if (Keyboard.keys.Down) {
                            wheel.forceForward = rVecScale(Car_Z_Local_Forward, - this.decelerationForce)
                        }
                    }
                }

                {
                    wheel.contactPoint = rVec(ray.pointAt(hit.timeOfImpact))

                    wheel.velocity = Physics.linearVelocityAtWorldPoint(this.rigidBody, wheel.contactPoint)
                }

                wheel.curvatureForce = ZERO()
                if (!Keyboard.keys.BANANA) {
                    let slipMag = rVecDot(Car_XN_Local_Left, wheel.velocity)
                    let traction = 1// rVecDot(Car_Z_Local_Forward, rVec(wheel.velocity).normalize())
                    if ((wheel.type === 'lefttop' || wheel.type === 'righttop')) {
                        wheel.curvatureForce = rVecScale(Car_XN_Local_Left, this.wheelGripTop * slipMag * traction)
                    }
                    else if ((wheel.type === 'leftbottom' || wheel.type === 'rightbottom')) {
                        wheel.curvatureForce = rVecScale(Car_XN_Local_Left, this.wheelGripBottom * slipMag * traction)
                    }
                }

                {
                    // wheel.netForce = rVecAddd([wheel.suspensionForce, wheel.forceForward, wheel.curvatureForce])

                    // this.rigidBody.addForceAtPoint(wheel.netForce, wheel.contactPoint, true);
                }
                {
                    // wheel.netForce = rVecAddd([wheel.suspensionForce, wheel.curvatureForce])

                    // this.rigidBody.addForceAtPoint(wheel.netForce, wheel.contactPoint, true);
                    // this.rigidBody.addForceAtPoint(wheel.forceForward, wheelWorldPos, true);
                }
                {
                    wheel.netForce = rVecAddd([
                        wheel.suspensionForce,
                        wheel.forceForward,
                        wheel.curvatureForce])

                    this.rigidBody.addForceAtPoint(wheel.netForce, suspensionAnchorPos, true);
                }
                {
                    // wheel.netForce = rVecAddd([wheel.suspensionForce, wheel.forceForward, wheel.curvatureForce])

                    // this.rigidBody.addForceAtPoint(wheel.netForce, this.position, true);
                }

                {
                    let ll = rVecAdd(wheel.contactPoint, rVecScale(Car_YN_Local_Down, - this.wheelRadius))
                    wheel.mesh.position.set(ll.x, ll.y, ll.z);
                }

            } else {
                wheel.contactMesh.visible = false
                wheel.velocityHelp.visible = false

                wheel.mesh.position.set(suspensionAnchorPos.x, suspensionAnchorPos.y, suspensionAnchorPos.z)
            }

            {
                wheel.raycastMesh.position.set(suspensionAnchorPos.x, suspensionAnchorPos.y, suspensionAnchorPos.z)
                wheel.raycastMesh.setLength(this.suspensionLength + this.wheelRadius)
                wheel.raycastMesh.setDirection(Car_YN_Local_Down)
            }

            {
                wheel.mesh.setRotationFromQuaternion(this.rotation)
                wheel.mesh.rotateY(wheel.steerAngle)
                wheel.mesh.rotateX(this.wheelRotation);
            }
            this.updateUI(game, wheel)
        }

        {
            const currentUp = new THREE.Vector3(0, 1, 0).applyQuaternion(this.rotation);
            let angleX = Math.atan2(currentUp.z, currentUp.y); // Rotation in X-axis
            let angleZ = Math.atan2(currentUp.x, currentUp.y); // Rotation in Z-axis

            // console.log(
            //     angleX.toFixed(2),
            //     angleZ.toFixed(2),
            //     THREE.MathUtils.radToDeg(angleX).toFixed(2),
            //     THREE.MathUtils.radToDeg(angleZ).toFixed(2),
            //     this.airborne,
            //     rVecString(currentUp),
            // )

            const torqueX = -angleX * 30;
            const torqueZ = -angleZ * 1;

            this.rigidBody.applyTorqueImpulse({ x: torqueX, y: 0, z: torqueZ }, true);
        }

        {
            // if (Keyboard.keys.Left || Keyboard.keys.Right) {
            this.rigidBody.setLinearDamping(.4 + this.wheelContact / 2); // Simulate resistance to motion
            this.rigidBody.setAngularDamping(6 + this.wheelContact / 2); // Stabilize rotation
            // }
        }
    }

    handleKeyboardInput(deltaTime: number): void {

        let velocityMag = this.linearVelocity.length()
        let velocityRatio = (velocityMag) / (this.accelerationForce / this.mass)
        let steeringMultiplier = (1 - Math.pow(1 - 2.1 * velocityRatio, 2) / 2)
        let steeringTorque = this.steeringTorque * steeringMultiplier

        if ((Keyboard.keys.Left && Keyboard.keys.Up) || (Keyboard.keys.Right && Keyboard.keys.Down)) {
            this.rigidBody.applyTorqueImpulse(new RAPIER.Vector3(0, steeringTorque, 0), true);
        }
        else if ((Keyboard.keys.Left && Keyboard.keys.Down) || (Keyboard.keys.Right && Keyboard.keys.Up)) {
            this.rigidBody.applyTorqueImpulse(new RAPIER.Vector3(0, - steeringTorque, 0), true);
        }

        this.wheelRotation += velocityMag * deltaTime * ((Keyboard.keys.Up) ? 1 : (Keyboard.keys.Down ? - 1 : 0))

        // Adjust steering angle for front tires
        const steeringSpeed = 1 * deltaTime; // Steering angle increment

        for (const wheel of this.wheels) {
            // if (wheel.canSteer) {
            // Increase or decrease steering angle based on input
            if (Keyboard.keys.Left) {

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
            } else if (Keyboard.keys.Right) {

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
