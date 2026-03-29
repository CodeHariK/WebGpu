import { Material } from "./Material";

export interface MeshPrimitive {
	vertexBuffer: GPUBuffer;
	indexBuffer: GPUBuffer;
	indexCount: number;
	indexType: GPUIndexFormat;
	material: Material;
}

export class Mesh {
	public primitives: MeshPrimitive[] = [];

	constructor(primitives: MeshPrimitive[]) {
		this.primitives = primitives;
	}

	public destroy(): void {
		for (const primitive of this.primitives) {
			primitive.vertexBuffer.destroy();
			primitive.indexBuffer.destroy();
		}
	}
}
