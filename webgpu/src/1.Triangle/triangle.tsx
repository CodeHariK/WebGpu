import { createEffect, createSignal } from "solid-js";
import { quitIfWebGPUNotAvailable } from "../util";

const redFragWGSL = `
@fragment
fn main() -> @location(0) vec4f {
  return vec4(1.0, 0.0, 0.0, 1.0);
}
`;

const triangleVertWGSL = `
@vertex
fn main(
  @builtin(vertex_index) VertexIndex : u32
) -> @builtin(position) vec4f {
  var pos = array<vec2f, 3>(
    vec2(0.0, 0.5),
    vec2(-0.5, -0.5),
    vec2(0.5, -0.5)
  );

  return vec4f(pos[VertexIndex], 0.0, 1.0);
}
`;

function Triangle() {
	const [canvas, setCanvas] = createSignal<HTMLCanvasElement | null>(null);

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
		context.configure({ device, format: presentationFormat });

		const pipeline = device.createRenderPipeline({
			layout: "auto",
			vertex: {
				module: device.createShaderModule({
					code: triangleVertWGSL,
				}),
			},
			fragment: {
				module: device.createShaderModule({
					code: redFragWGSL,
				}),
				targets: [
					{
						format: presentationFormat,
					},
				],
			},
			primitive: {
				topology: "triangle-list",
			},
		});

		requestAnimationFrame(() => frame(device, context, pipeline));
	}

	function frame(
		device: GPUDevice,
		context: GPUCanvasContext,
		pipeline: GPURenderPipeline,
	) {
		const commandEncoder = device.createCommandEncoder();
		const textureView = context.getCurrentTexture().createView();

		const renderPassDescriptor: GPURenderPassDescriptor = {
			colorAttachments: [
				{
					view: textureView,
					clearValue: [0, 0, 0, 0], // Clear to transparent
					loadOp: "clear",
					storeOp: "store",
				},
			],
		};

		const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
		passEncoder.setPipeline(pipeline);
		passEncoder.draw(3);
		passEncoder.end();

		device.queue.submit([commandEncoder.finish()]);
		requestAnimationFrame(() => frame(device, context, pipeline));
	}

	return <canvas width={300} height={300} ref={setCanvas} />;
}

export default Triangle;
