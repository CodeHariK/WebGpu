import { Mesh } from "./Mesh";

export class Renderer {
	public device: GPUDevice;
	public queue: GPUQueue;
	public format: GPUTextureFormat;

	private uniformBuffer: GPUBuffer;
	public uniformBindGroupLayout: GPUBindGroupLayout;
	private uniformBindGroup: GPUBindGroup;

	private depthTexture: GPUTexture | null = null;

	constructor(device: GPUDevice, format: GPUTextureFormat) {
		this.device = device;
		this.queue = device.queue;
		this.format = format;

		// 3 matrices (View, Projection, Model) each 64 bytes
		this.uniformBuffer = device.createBuffer({
			size: 64 * 3,
			usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
		});

		this.uniformBindGroupLayout = device.createBindGroupLayout({
			entries: [
				{
					binding: 0,
					visibility: GPUShaderStage.VERTEX,
					buffer: { type: "uniform" },
				},
			],
		});

		this.uniformBindGroup = device.createBindGroup({
			layout: this.uniformBindGroupLayout,
			entries: [
				{
					binding: 0,
					resource: { buffer: this.uniformBuffer },
				},
			],
		});
	}

	public updateUniforms(
		viewMatrix: Float32Array,
		projectionMatrix: Float32Array,
		modelMatrix: Float32Array,
	): void {
		this.device.queue.writeBuffer(this.uniformBuffer, 0, viewMatrix as any);
		this.device.queue.writeBuffer(this.uniformBuffer, 64, projectionMatrix as any);
		this.device.queue.writeBuffer(this.uniformBuffer, 128, modelMatrix as any);
	}

	public render(
		context: GPUCanvasContext,
		mesh: Mesh,
	): void {
		const commandEncoder = this.device.createCommandEncoder();
		const textureView = context.getCurrentTexture().createView();

		if (
			!this.depthTexture ||
			this.depthTexture.width !== context.canvas.width ||
			this.depthTexture.height !== context.canvas.height
		) {
			this.depthTexture = this.device.createTexture({
				size: [context.canvas.width, context.canvas.height],
				format: "depth24plus",
				usage: GPUTextureUsage.RENDER_ATTACHMENT,
			});
		}

		const renderPassDescriptor: GPURenderPassDescriptor = {
			colorAttachments: [
				{
					view: textureView,
					clearValue: [0.1, 0.1, 0.1, 1],
					loadOp: "clear",
					storeOp: "store",
				},
			],
			depthStencilAttachment: {
				view: this.depthTexture.createView(),
				depthClearValue: 1.0,
				depthLoadOp: "clear",
				depthStoreOp: "store",
			},
		};

		const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
		passEncoder.setBindGroup(0, this.uniformBindGroup);

		for (const primitive of mesh.primitives) {
			primitive.material.apply(passEncoder);
			passEncoder.setVertexBuffer(0, primitive.vertexBuffer);
			passEncoder.setIndexBuffer(primitive.indexBuffer, primitive.indexType);
			passEncoder.drawIndexed(primitive.indexCount);
		}

		passEncoder.end();
		this.device.queue.submit([commandEncoder.finish()]);
	}
}
