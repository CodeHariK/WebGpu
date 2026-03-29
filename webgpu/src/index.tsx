/* @refresh reload */
import { render } from "solid-js/web";
import { createSignal, For } from "solid-js";
import "./index.css";
import Triangle from "./1.Triangle/triangle.tsx";
import Cube from "./2.Cube/cube.tsx";
import PlatformerDemo from "./3.Renderer/PlatformerDemo.tsx";

const EXAMPLES = [

	{
		id: "triangle",
		label: "Basic Triangle",
		component: Triangle,
	},
	{
		id: "cube",
		label: "Cube Example",
		component: Cube,
	},
	{
		id: "renderer",
		label: "Platformer Renderer",
		component: PlatformerDemo,
	},
];

function App() {
	const [activeTab, setActiveTab] = createSignal(EXAMPLES[0].id);

	const activeExample = () =>
		EXAMPLES.find((e) => e.id === activeTab()) || EXAMPLES[0];

	const isDoc = () =>
		activeTab() === "wgsl-syntax" ||
		activeTab() === "wgpu-dev" ||
		activeTab() === "wgpu-core";

	return (
		<div class="app-container">
			<aside class="sidebar">
				<header class="sidebar-header">
					<h2>WebGPU Lab</h2>
				</header>

				<nav class="tabs-container">
					<For each={EXAMPLES}>
						{(example) => (
							<button
								class="tab-button"
								classList={{ active: activeTab() === example.id }}
								onClick={() => setActiveTab(example.id)}
							>
								{example.label}
							</button>
						)}
					</For>
				</nav>
			</aside>

			<main
				class="main-view"
				style={{ "align-items": isDoc() ? "flex-start" : "center" }}
			>
				{(() => {
					const Component = activeExample().component;
					return <Component />;
				})()}
			</main>
		</div>
	);
}

const root = document.getElementById("root");
if (!root) throw new Error("Root element not found");
render(() => <App />, root);
