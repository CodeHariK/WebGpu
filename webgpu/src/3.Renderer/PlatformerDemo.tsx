import { createSignal, onMount, onCleanup, For, Show } from "solid-js";
import { Renderer } from "./Renderer";
import { Loader } from "./Loader";
import { mat4, vec3 } from "wgpu-matrix";
import { Camera } from "./Camera";

const ASSETS = [
	"block-grass.glb",
	"character-oobi.glb",
	"character-oodi.glb",
	"character-ooli.glb",
	"chest.glb",
	"crate.glb",
	"barrel.glb",
	"coin-gold.glb",
	"tree-pine.glb",
	"flowers.glb",
	"rock.glb",
];

// We can also try to load all GLB files found in the directory
// For now, let's provide a searchable list or just a few key ones.

export default function PlatformerDemo() {
	let canvasRef: HTMLCanvasElement | undefined;
	const [activeAsset, setActiveAsset] = createSignal(ASSETS[0]);
	const [isLoading, setIsLoading] = createSignal(false);
	const [status, setStatus] = createSignal("");

	let renderer: Renderer | null = null;
	let loader: Loader | null = null;
	let currentMesh: any = null;
	let camera: Camera | null = null;

	let animationFrame: number;
	let rotation = 0;

	const init = async () => {
		if (!canvasRef) return;

		const adapter = await navigator.gpu.requestAdapter();
		const device = await adapter?.requestDevice();
		if (!device) {
			setStatus("WebGPU not supported");
			return;
		}

		const context = canvasRef.getContext("webgpu");
		const format = navigator.gpu.getPreferredCanvasFormat();
		context?.configure({ device, format, alphaMode: "premultiplied" });

		renderer = new Renderer(device, format);
		loader = new Loader(renderer);
		camera = new Camera((45 * Math.PI) / 180, canvasRef.width / canvasRef.height, 0.1, 100.0);
		camera.lookAt(vec3.fromValues(3, 3, 5), vec3.fromValues(0, 0.5, 0));

		loadAsset(activeAsset());
		requestAnimationFrame(frame);
	};

	const loadAsset = async (filename: string) => {
		if (!loader || !renderer) return;

		setIsLoading(true);
		setStatus(`Loading ${filename}...`);

		// Clean up old
		if (currentMesh) currentMesh.destroy();

		const result = await loader.loadGLB(`/assets/PlatformerKit/${filename}`);
		if (result) {
			currentMesh = result;
			setStatus(`Loaded ${filename}`);
		} else {
			setStatus(`Failed to load ${filename}`);
		}
		setIsLoading(false);
	};

	const frame = () => {
		if (!renderer || !canvasRef || !currentMesh) {
			animationFrame = requestAnimationFrame(frame);
			return;
		}

		const context = canvasRef.getContext("webgpu");
		if (!context) return;

		rotation += 0.01;

		const model = mat4.identity();
		mat4.rotateY(model, rotation, model);

		if (camera) {
			renderer.updateUniforms(
				camera.viewMatrix as Float32Array,
				camera.projectionMatrix as Float32Array,
				model as Float32Array,
			);
		}
		renderer.render(context, currentMesh);

		animationFrame = requestAnimationFrame(frame);
	};

	onMount(init);

	onCleanup(() => {
		cancelAnimationFrame(animationFrame);
	});

	return (
		<div class="platformer-demo">
			<canvas
				ref={canvasRef}
				width={800}
				height={600}
				style={{ width: "100%", height: "auto", "aspect-ratio": "4/3" }}
			/>
			<div class="controls">
				<div class="status-bar">
					<Show when={isLoading()}>
						<span class="loading-spinner">↻</span>
					</Show>
					{status()}
				</div>
				<div class="asset-grid">
					<For each={ASSETS}>
						{(asset) => (
							<button
								class="asset-button"
								classList={{ active: activeAsset() === asset }}
								onClick={() => {
									setActiveAsset(asset);
									loadAsset(asset);
								}}
							>
								{asset.replace(".glb", "")}
							</button>
						)}
					</For>
				</div>
			</div>

			<style>{`
        .platformer-demo {
          display: flex;
          flex-direction: column;
          gap: 1rem;
          padding: 1rem;
          background: #1e1e1e;
          border-radius: 12px;
          border: 1px solid #333;
        }
        .status-bar {
          font-size: 0.9rem;
          color: #aaa;
          margin-bottom: 0.5rem;
          display: flex;
          align-items: center;
          gap: 0.5rem;
        }
        .asset-grid {
          display: grid;
          grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
          gap: 0.5rem;
          max-height: 200px;
          overflow-y: auto;
          padding: 0.5rem;
          background: #121212;
          border-radius: 8px;
        }
        .asset-button {
          padding: 0.5rem;
          background: #2a2a2a;
          border: 1px solid #444;
          color: #eee;
          border-radius: 4px;
          cursor: pointer;
          font-size: 0.8rem;
          transition: all 0.2s;
          white-space: nowrap;
          overflow: hidden;
          text-overflow: ellipsis;
        }
        .asset-button:hover {
          background: #3a3a3a;
        }
        .asset-button.active {
          background: #646cff;
          border-color: #7b84ff;
          color: white;
        }
        .loading-spinner {
          display: inline-block;
          animation: spin 1s linear infinite;
        }
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
		</div>
	);
}
