import { createEffect, createSignal } from "solid-js";
import { mat4, vec3 } from "wgpu-matrix";
import { quitIfWebGPUNotAvailable } from "../util";

export default function Cube() {
	const [canvas, setCanvas] = createSignal<HTMLCanvasElement | null>(null);

	const matrixSize = 64; // size of one mat4x4 (16 floats * 4 bytes)
	const uniformBufferSize = 3 * matrixSize; // 0: model, 64: view, 128: projection

	createEffect(() => {
		const c = canvas();
		if (!c) return;
		setupWebGPU(c);
	});

	async function setupWebGPU(canvas: HTMLCanvasElement) {
		const adapter = await navigator.gpu?.requestAdapter({
			featureLevel: "compatibility",
		});
		const device = await adapter?.requestDevice();
		if (!device) return;

		quitIfWebGPUNotAvailable(adapter, device);

		const context = canvas.getContext("webgpu");
		if (!context) return;

		const devicePixelRatio = window.devicePixelRatio;
		canvas.width = canvas.clientWidth * devicePixelRatio;
		canvas.height = canvas.clientHeight * devicePixelRatio;
		const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

		context.configure({
			device,
			format: presentationFormat,
		});

		// Create a vertex buffer from the cube data.
		const verticesBuffer = device.createBuffer({
			size: cubeVertexArray.byteLength,
			usage: GPUBufferUsage.VERTEX,
			mappedAtCreation: true,
		});
		new Float32Array(verticesBuffer.getMappedRange()).set(cubeVertexArray);
		verticesBuffer.unmap();

		const pipeline = device.createRenderPipeline({
			layout: "auto",
			vertex: {
				module: device.createShaderModule({
					code: cubeVertWGSL,
				}),
				buffers: [
					{
						arrayStride: cubeVertexSize,
						attributes: [
							{
								// position
								shaderLocation: 0,
								offset: cubePositionOffset,
								format: "float32x4",
							},
							{
								// uv
								shaderLocation: 1,
								offset: cubeUVOffset,
								format: "float32x2",
							},
						],
					},
				],
			},
			fragment: {
				module: device.createShaderModule({
					code: cubeFragWGSL,
				}),
				targets: [
					{
						format: presentationFormat,
					},
				],
			},
			primitive: {
				topology: "triangle-list",
				cullMode: "back",
			},
			depthStencil: {
				depthWriteEnabled: true,
				depthCompare: "less",
				format: "depth24plus",
			},
		});

		const depthTexture = device.createTexture({
			size: [canvas.width, canvas.height],
			format: "depth24plus",
			usage: GPUTextureUsage.RENDER_ATTACHMENT,
		});

		const uniformBuffer = device.createBuffer({
			size: uniformBufferSize,
			usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
		});

		const uniformBindGroup = device.createBindGroup({
			layout: pipeline.getBindGroupLayout(0),
			entries: [
				{
					binding: 0,
					resource: {
						buffer: uniformBuffer,
					},
				},
			],
		});

		const renderPassDescriptor: GPURenderPassDescriptor = {
			colorAttachments: [
				{
					view: null as unknown as GPUTextureView,
					clearValue: [0.1, 0.1, 0.1, 1.0],
					loadOp: "clear",
					storeOp: "store",
				},
			],
			depthStencilAttachment: {
				view: depthTexture.createView(),
				depthClearValue: 1.0,
				depthLoadOp: "clear",
				depthStoreOp: "store",
			},
		};

		const aspect = canvas.width / canvas.height;
		const projectionMatrix = mat4.perspective(
			(2 * Math.PI) / 5,
			aspect,
			1,
			100.0,
		);
		const viewMatrix = mat4.translation(vec3.fromValues(0, 0, -5));
		const modelMatrix = mat4.create();

		// Write static matrices once
		device.queue.writeBuffer(
			uniformBuffer,
			128,
			(projectionMatrix as Float32Array).buffer,
		);
		device.queue.writeBuffer(
			uniformBuffer,
			64,
			(viewMatrix as Float32Array).buffer
		);

		function frame(device: GPUDevice) {
			const now = Date.now() / 1000;
			mat4.identity(modelMatrix);
			mat4.rotate(
				modelMatrix,
				vec3.fromValues(Math.sin(now), Math.cos(now), 0),
				1,
				modelMatrix,
			);

			device.queue.writeBuffer(
				uniformBuffer,
				0,
				(modelMatrix as Float32Array).buffer,
			);

			(
				renderPassDescriptor.colorAttachments as GPURenderPassColorAttachment[]
			)[0].view = context!.getCurrentTexture().createView();

			const commandEncoder = device.createCommandEncoder();
			const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
			passEncoder.setPipeline(pipeline);
			passEncoder.setVertexBuffer(0, verticesBuffer);
			passEncoder.setBindGroup(0, uniformBindGroup);
			passEncoder.draw(cubeVertexCount);
			passEncoder.end();
			device.queue.submit([commandEncoder.finish()]);

			requestAnimationFrame(() => frame(device));
		}

		requestAnimationFrame(() => frame(device));
	}

	return <canvas width={300} height={300} ref={setCanvas} />;
}

const cubeFragWGSL = `
@fragment
fn main(
    @location(0) fragUV: vec2f,
    @location(1) fragPosition: vec4f
) -> @location(0) vec4f {
    return fragPosition;
}
`;

const cubeVertWGSL = `
struct Uniforms {
    modelMatrix : mat4x4f,
    viewMatrix : mat4x4f,
    projectionMatrix : mat4x4f,
}
@binding(0) @group(0) var<uniform> uniforms : Uniforms;

struct VertexOutput {
    @builtin(position) Position : vec4f,
    @location(0) fragUV : vec2f,
    @location(1) fragPosition: vec4f,
}

@vertex
fn main(
    @location(0) position : vec4f,
    @location(1) uv : vec2f
) -> VertexOutput {
    var output : VertexOutput;
    // GPU side matrix multiplication: P * V * M * pos
    output.Position = uniforms.projectionMatrix * uniforms.viewMatrix * uniforms.modelMatrix * position;
    output.fragUV = uv;
    output.fragPosition = 0.5 * (position + vec4(1.0, 1.0, 1.0, 1.0));
    return output;
}
`;

const cubeVertexSize = 4 * 10;
const cubePositionOffset = 0;
const cubeUVOffset = 4 * 8;
const cubeVertexCount = 36;

// prettier-ignore
const cubeVertexArray = new Float32Array([
	// float4 position, float4 color, float2 uv,
	1, -1, 1, 1, 1, 0, 1, 1, 0, 1, -1, -1, 1, 1, 0, 0, 1, 1, 1, 1, -1, -1, -1, 1,
	0, 0, 0, 1, 1, 0, 1, -1, -1, 1, 1, 0, 0, 1, 0, 0, 1, -1, 1, 1, 1, 0, 1, 1, 0,
	1, -1, -1, -1, 1, 0, 0, 0, 1, 1, 0,

	1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, -1, 1, 1, 1, 0, 1, 1, 1, 1, 1, -1, -1, 1, 1,
	0, 0, 1, 1, 0, 1, 1, -1, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1,
	-1, -1, 1, 1, 0, 0, 1, 1, 0,

	-1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, 1, 1,
	1, 0, 1, 1, 0, -1, 1, -1, 1, 0, 1, 0, 1, 0, 0, -1, 1, 1, 1, 0, 1, 1, 1, 0, 1,
	1, 1, -1, 1, 1, 1, 0, 1, 1, 0,

	-1, -1, 1, 1, 0, 0, 1, 1, 0, 1, -1, 1, 1, 1, 0, 1, 1, 1, 1, 1, -1, 1, -1, 1,
	0, 1, 0, 1, 1, 0, -1, -1, -1, 1, 0, 0, 0, 1, 0, 0, -1, -1, 1, 1, 0, 0, 1, 1,
	0, 1, -1, 1, -1, 1, 0, 1, 0, 1, 1, 0,

	1, 1, 1, 1, 1, 1, 1, 1, 0, 1, -1, 1, 1, 1, 0, 1, 1, 1, 1, 1, -1, -1, 1, 1, 0,
	0, 1, 1, 1, 0, -1, -1, 1, 1, 0, 0, 1, 1, 1, 0, 1, -1, 1, 1, 1, 0, 1, 1, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 0, 1,

	1, -1, -1, 1, 1, 0, 0, 1, 0, 1, -1, -1, -1, 1, 0, 0, 0, 1, 1, 1, -1, 1, -1, 1,
	0, 1, 0, 1, 1, 0, 1, 1, -1, 1, 1, 1, 0, 1, 0, 0, 1, -1, -1, 1, 1, 0, 0, 1, 0,
	1, -1, 1, -1, 1, 0, 1, 0, 1, 1, 0,
]);
