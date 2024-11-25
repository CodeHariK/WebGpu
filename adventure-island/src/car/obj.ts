import { World, RigidBody, ImpulseJointSet, RigidBodyDesc, JointData, Vector3, Quaternion } from "@dimforge/rapier3d";
import { rQuatMul, rVecAdd, rVecScale } from "./physics";

export class CarWithWheelJoints {
    private world: World;
    private impulseJoints: ImpulseJointSet;
    private chassisBody: RigidBody;
    private wheels: RigidBody[] = [];
    private wheelRadius: number;

    constructor(
        world: World,
        impulseJoints: ImpulseJointSet,
        chassisPosition: Vector3,
        wheelOffsets: Vector3[],
        wheelRadius: number
    ) {
        this.world = world;
        this.impulseJoints = impulseJoints;
        this.wheelRadius = wheelRadius;

        this.chassisBody = this.createChassis(chassisPosition);
        this.createWheels(wheelOffsets);
    }

    private createChassis(position: Vector3): RigidBody {
        const chassisDesc = RigidBodyDesc.dynamic().setTranslation(position.x, position.y, position.z);
        return this.world.createRigidBody(chassisDesc);
    }

    private createWheels(wheelOffsets: Vector3[]): void {
        wheelOffsets.forEach((offset) => {
            // Create a wheel rigidbody
            let o = rVecAdd(this.chassisBody.translation(), offset)
            const wheelDesc = RigidBodyDesc.dynamic().setTranslation(o.x, o.y, o.z)
            wheelDesc.mass = 1; // Adjust mass as needed
            const wheelBody = this.world.createRigidBody(wheelDesc);

            // Create joint data for the revolute joint
            const jointData = JointData.revolute(
                { x: offset.x, y: offset.y, z: offset.z }, // Anchor in chassis local space
                { x: 0, y: 0, z: 0 },                      // Anchor in wheel local space
                { x: 0, y: 0, z: 1 }                       // Axis of rotation
            );

            // Create the joint between the chassis and the wheel
            this.impulseJoints.createJoint(
                this.world.bodies,  // RigidBodySet containing all bodies
                jointData,          // The joint data
                this.chassisBody.handle, // Handle of the chassis
                wheelBody.handle,       // Handle of the wheel
                true // Wake up both bodies
            );

            this.wheels.push(wheelBody);
        });
    }

    public applyThrottle(force: number): void {
        // Apply forward force to each wheel
        this.wheels.forEach((wheel) => {
            const forwardForce = new Vector3(0, 0, -force); // Adjust direction based on car orientation
            wheel.resetForces(true)
            wheel.resetTorques(true)
            wheel.addForce(forwardForce, true);
        });
    }

    public applyBrake(brakeForce: number): void {
        // Reduce the angular velocity of the wheels to simulate braking
        this.wheels.forEach((wheel) => {
            const currentAngularVelocity = wheel.angvel();
            const brakingVelocity = rVecScale(currentAngularVelocity, 1 - brakeForce);
            wheel.setAngvel(brakingVelocity, true);
        });
    }

    // Change in the `steer` method:
    public steer(angle: number): void {
        // Rotate the front wheels for steering
        const frontWheels = this.wheels.slice(0, 2); // Assuming the first two are the front wheels
        frontWheels.forEach((wheel) => {
            const currentRotation = wheel.rotation();
            const steeringRotation = new Quaternion(0, Math.sin(angle / 2), 0, Math.cos(angle / 2)); // Y-axis rotation

            wheel.setRotation(rQuatMul(currentRotation, steeringRotation), true);
        });
    }

    public update(deltaTime: number): void {
        // Custom car dynamics updates (e.g., suspension, traction)
    }

    public getChassisBody(): RigidBody {
        return this.chassisBody;
    }

    public getWheels(): RigidBody[] {
        return this.wheels;
    }
}