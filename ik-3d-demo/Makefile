# ik-3d-demo: run any demo scene.  make help  lists them.

GODOT := /Applications/Godot.app/Contents/MacOS/Godot
RUN   := $(GODOT) --path .

.DEFAULT_GOAL := help

# --- Editor ---
e: ## open the project in the Godot editor
	$(GODOT) -e --path .

# --- Limbs / IK ---
limbs: ## custom LimbChain vs Skeleton3D benchmark (Tab switches)
	$(RUN) --scene limbs/limb_demo.tscn
spider: ## procedural spider
	$(RUN) --scene spider/spider_demo.tscn
gait: ## clock-driven spider gait + footfall chart
	$(RUN) --scene gaitspider/gait_demo.tscn
hex: ## hex foot / pole picking prototype
	$(RUN) --scene hexspider/hex_demo.tscn
bat: ## bat on LimbChains
	$(RUN) --scene bat/bat_demo.tscn
flytrap: ## flytrap on FK LimbChains
	$(RUN) --scene plant/flytrap_demo.tscn
vacuum: ## ghost-vacuum tug of war
	$(RUN) --scene vacuum/vacuum_demo.tscn

# --- Softbody ---
rope: ## Verlet rope: 1 / 6 / 40 passes
	$(RUN) --scene softbody/rope_demo.tscn
chain: ## Kratos chain: throw, hook, pull crates / grapple a post
	$(RUN) --scene softbody/chain_demo.tscn

# --- Blades ---
blades: ## Blades of Chaos: Bezier blade paths + Bezier chains (no physics)
	$(RUN) --scene blades/blades_demo.tscn

# --- Bosses ---
orb: ## Orb Warden boss: six-legged laser sphere (WASD dodge, F laser)
	$(RUN) --scene orbwarden/orb_demo.tscn

# --- Look ---
mood: ## eight looks on one scene: spooky, candy, desert, autumn, cyberpunk, solarpunk, medieval, magical (, . switch)
	$(RUN) --scene mood/mood_demo.tscn

# --- AI ---
grid: ## layered tactical grid, A*, senses
	$(RUN) --scene ai/grid_demo.tscn
squad: ## squad roles and plans
	$(RUN) --scene ai/squad_demo.tscn
road: ## car player + road ambush
	$(RUN) --scene ai/road_demo.tscn

# --- Godot IK samples ---
adjusted: ## Godot's adjusted IK sample
	$(RUN) --scene scene/adjusted_ik_3d_demo.tscn
magnet: ## Godot's magnet IK sample
	$(RUN) --scene scene/magnet_ik_3d_demo.tscn
demo2: ## demo2
	$(RUN) --scene scene/demo2.tscn
view: ## view
	$(RUN) --scene scene/view.tscn
modifier: ## custom SkeletonModifier3D experiment
	$(RUN) --scene custom_modifier/node_3d.tscn

# --- Tools ---
import: ## rescan the project (registers new class_name scripts, reimports assets)
	$(GODOT) --headless --editor --quit --path .

help: ## list the targets
	@grep -E '^[a-z0-9_]+:.*## ' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*## "} {printf "  make %-10s %s\n", $$1, $$2}'

.PHONY: orb mood blades chain e limbs spider gait hex bat flytrap vacuum rope grid squad road adjusted magnet demo2 view modifier import help
