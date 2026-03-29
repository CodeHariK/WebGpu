import { load } from "@loaders.gl/core";
import { GLTFLoader, postProcessGLTF } from "@loaders.gl/gltf";
import { Mesh } from "./Mesh";
import type { MeshPrimitive } from "./Mesh";
import { Material } from "./Material";
import { Renderer } from "./Renderer";

export class Loader {
	private device: GPUDevice;
	private renderer: Renderer;

	constructor(renderer: Renderer) {
		this.device = renderer.device;
		this.renderer = renderer;
	}

	public async loadGLB(url: string): Promise<Mesh | null> {
		try {
			// 1. Load and Post-process GLTF
			const gltfRaw = await load(url, GLTFLoader);
			const gltf = postProcessGLTF(gltfRaw);

			const meshPrimitives: MeshPrimitive[] = [];

			// 2. Iterate through meshes and primitives
			// For simplicity, we assume we want all primitives from all meshes in the scene
			for (const meshDef of gltf.meshes) {
				for (const primitiveDef of meshDef.primitives) {
					const attributes = primitiveDef.attributes;

					// Extract standard attributes
					const posAttr = attributes.POSITION;
					const uvAttr = attributes.TEXCOORD_0;
					const normAttr = attributes.NORMAL;

					if (!posAttr) continue;

					// 3. Interleave data for our shader (Pos:3, UV:2, Norm:3)
					const vertexCount = posAttr.count;
					const vertices = new Float32Array(vertexCount * 8);

					const posData = posAttr.value;
					const uvData = uvAttr?.value;
					const normData = normAttr?.value;

					for (let i = 0; i < vertexCount; i++) {
						// Position
						vertices[i * 8 + 0] = posData[i * 3 + 0];
						vertices[i * 8 + 1] = posData[i * 3 + 1];
						vertices[i * 8 + 2] = posData[i * 3 + 2];

						// UV
						if (uvData) {
							vertices[i * 8 + 3] = uvData[i * 2 + 0];
							vertices[i * 8 + 4] = uvData[i * 2 + 1];
						}

						// Normal
						if (normData) {
							vertices[i * 8 + 5] = normData[i * 3 + 0];
							vertices[i * 8 + 6] = normData[i * 3 + 1];
							vertices[i * 8 + 7] = normData[i * 3 + 2];
						}
					}

					// 4. Create Vertex Buffer
					const vertexBuffer = this.device.createBuffer({
						size: vertices.byteLength,
						usage: GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST,
						mappedAtCreation: true,
					});
					new Float32Array(vertexBuffer.getMappedRange()).set(vertices);
					vertexBuffer.unmap();

					// 5. Create Index Buffer
					let indices = primitiveDef.indices?.value;
					if (!indices) continue;

					// WebGPU only supports uint16 and uint32 indices. 
					// Many GLTF files use unit8 for optimization, we must convert them.
					let indexType: GPUIndexFormat = "uint32";
					if (indices instanceof Uint16Array) {
						indexType = "uint16";
					} else if (indices instanceof Uint8Array) {
						// Convert Uint8 to Uint16
						const newIndices = new Uint16Array(indices.length);
						newIndices.set(indices);
						indices = newIndices;
						indexType = "uint16";
					}

					const indexBuffer = this.device.createBuffer({
						size: indices.byteLength,
						usage: GPUBufferUsage.INDEX | GPUBufferUsage.COPY_DST,
						mappedAtCreation: true,
					});
					
					if (indices instanceof Uint16Array) {
						new Uint16Array(indexBuffer.getMappedRange()).set(indices);
					} else {
						new Uint32Array(indexBuffer.getMappedRange()).set(indices);
					}
					indexBuffer.unmap();

					// 6. Handle Material & Texture
					let texture: GPUTexture | undefined;
					const materialDef = primitiveDef.material;

					if (materialDef?.pbrMetallicRoughness?.baseColorTexture) {
						const textureDef =
							materialDef.pbrMetallicRoughness.baseColorTexture.texture;
						const imageSource = textureDef?.source?.image;

						if (imageSource) {
							texture = await this.createTextureFromImage(imageSource);
						}
					}

					const material = new Material(
						this.device,
						this.renderer.format,
						this.renderer.uniformBindGroupLayout,
						texture,
					);

					meshPrimitives.push({
						vertexBuffer,
						indexBuffer,
						indexCount: indices.length,
						indexType,
						material,
					});
				}
			}

			return new Mesh(meshPrimitives);
		} catch (e) {
			console.error(`Failed to load GLB: ${url}`, e);
			return null;
		}
	}

	private async createTextureFromImage(imageSource: any): Promise<GPUTexture> {
		const imageBitmap = await createImageBitmap(imageSource);

		const texture = this.device.createTexture({
			size: [imageBitmap.width, imageBitmap.height],
			format: "rgba8unorm",
			usage:
				GPUTextureUsage.TEXTURE_BINDING |
				GPUTextureUsage.COPY_DST |
				GPUTextureUsage.RENDER_ATTACHMENT,
		});

		this.device.queue.copyExternalImageToTexture(
			{ source: imageBitmap },
			{ texture },
			[imageBitmap.width, imageBitmap.height],
		);

		return texture;
	}
}
