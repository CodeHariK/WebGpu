import { mat4, vec3 } from "wgpu-matrix";
import type { Vec3, Mat4 } from "wgpu-matrix";

export class Camera {
	public position: Vec3 = vec3.create(0, 0, 5);
	public target: Vec3 = vec3.create(0, 0, 0);
	public up: Vec3 = vec3.create(0, 1, 0);

	public fov: number;
	public aspect: number;
	public near: number;
	public far: number;

	public viewMatrix: Mat4 = mat4.identity();
	public projectionMatrix: Mat4 = mat4.identity();

	constructor(
		fov: number = (45 * Math.PI) / 180,
		aspect: number = 1.0,
		near: number = 0.1,
		far: number = 100.0,
	) {
		this.fov = fov;
		this.aspect = aspect;
		this.near = near;
		this.far = far;

		this.updateMatrices();
	}

	public updateMatrices(): void {
		this.updateViewMatrix();
		this.updateProjectionMatrix();
	}

	public updateViewMatrix(): void {
		mat4.lookAt(this.position, this.target, this.up, this.viewMatrix);
	}

	public updateProjectionMatrix(): void {
		mat4.perspective(this.fov, this.aspect, this.near, this.far, this.projectionMatrix);
	}

	public lookAt(position: Vec3, target: Vec3, up: Vec3 = vec3.fromValues(0, 1, 0)): void {
		this.position = position;
		this.target = target;
		this.up = up;
		this.updateViewMatrix();
	}

	public setAspect(aspect: number): void {
		this.aspect = aspect;
		this.updateProjectionMatrix();
	}
}
