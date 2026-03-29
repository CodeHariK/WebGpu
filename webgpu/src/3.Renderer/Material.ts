import shaderCode from "./Shader.wgsl?raw";

export class Material {
	public pipeline: GPURenderPipeline;
	public bindGroup: GPUBindGroup | null = null;
	public bindGroupLayout: GPUBindGroupLayout;

	constructor(
		device: GPUDevice,
		format: GPUTextureFormat,
		uniformBindGroupLayout: GPUBindGroupLayout,
		texture?: GPUTexture,
	) {

		const shaderModule = device.createShaderModule({
			code: shaderCode,
		});

		this.bindGroupLayout = device.createBindGroupLayout({
			entries: [
				{
					binding: 0,
					visibility: GPUShaderStage.FRAGMENT,
					sampler: {},
				},
				{
					binding: 1,
					visibility: GPUShaderStage.FRAGMENT,
					texture: {},
				},
			],
		});

		this.pipeline = device.createRenderPipeline({
			layout: device.createPipelineLayout({
				bindGroupLayouts: [uniformBindGroupLayout, this.bindGroupLayout],
			}),
			vertex: {
				module: shaderModule,
				entryPoint: "vertexMain",
				buffers: [
					{
						arrayStride: 8 * 4, // pos(3) + uv(2) + normal(3)
						attributes: [
							{ shaderLocation: 0, format: "float32x3", offset: 0 },
							{ shaderLocation: 1, format: "float32x2", offset: 3 * 4 },
							{ shaderLocation: 2, format: "float32x3", offset: 5 * 4 },
						],
					},
				],
			},
			fragment: {
				module: shaderModule,
				entryPoint: "fragmentMain",
				targets: [{ format }],
			},
			primitive: {
				topology: "triangle-list",
				cullMode: "back",
			},
			depthStencil: {
				format: "depth24plus",
				depthWriteEnabled: true,
				depthCompare: "less",
			},
		});

		if (texture) {
			this.updateTexture(device, texture);
		}
	}

	public updateTexture(device: GPUDevice, texture: GPUTexture): void {
		this.bindGroup = device.createBindGroup({
			layout: this.bindGroupLayout,
			entries: [
				{
					binding: 0,
					resource: device.createSampler({
						magFilter: "linear",
						minFilter: "linear",
					}),
				},
				{
					binding: 1,
					resource: texture.createView(),
				},
			],
		});
	}

	public apply(passEncoder: GPURenderPassEncoder): void {
		passEncoder.setPipeline(this.pipeline);
		if (this.bindGroup) {
			passEncoder.setBindGroup(1, this.bindGroup);
		}
	}
}
