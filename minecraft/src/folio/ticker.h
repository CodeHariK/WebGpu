#ifndef FOLIO_TICKER_H
#define FOLIO_TICKER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <vector>

namespace godot {

/**
 * Folio port — FolioTicker
 * -------------------
 * Faithful port of folio-2025 `Game/Ticker.js`: the single per-frame clock and
 * heartbeat that the whole engine hangs off.
 *
 * It owns time (`elapsed`/`delta`), a time `scale` (folio runs the world at 2x
 * wall-clock), a rolling `delta_average`, a frame-countdown scheduler (`wait`),
 * the shared shader-time globals, and one ordered `tick` dispatch.
 *
 * Design notes vs the JS original:
 *   - In folio the WebGPU renderer drives the clock (setAnimationLoop -> update).
 *     Here Godot's main loop drives it: `_process(delta)` funnels into `update()`
 *     from ONE place, preserving folio's single-entry determinism.
 *   - Subscribers register with an integer `order`; callbacks run ascending. This
 *     reproduces folio's implicit frame pipeline (physics-pre 2, physics-post 5,
 *     render 998, ...). Do NOT rely on Godot per-node process order for this.
 *   - The four time values are pushed to RenderingServer global shader uniforms
 *     so every ported shader shares one clock.
 */
/**
 * Why a FolioTicker at all, when Godot already ticks?
 * ----------------------------------------------
 * Godot calls `_process` (render/idle step) and `_physics_process` (fixed step)
 * automatically, so why funnel everything through one FolioTicker?
 *
 *  1. Deterministic CROSS-SYSTEM ordering. Godot only orders nodes coarsely
 *     (tree order + `process_priority`), per node, and idle vs physics live in
 *     separate callbacks. Folio needs one global, fine-grained pipeline where
 *     input -> physics-pre(2) -> physics step -> physics-post(5) -> gameplay(10)
 *     -> render(998) -> cleanup(2000) run in an exact, documented sequence,
 *     independent of where nodes sit in the scene tree. The FolioTicker owns that.
 *  2. One TIME AUTHORITY. A single place computes the clamped delta, the 2x world
 *     `scale`, the raw AND scaled timelines, the rolling delta average, and
 *     publishes the shader-time globals. Every system and shader reads the same
 *     numbers instead of each re-deriving time. (Godot has no built-in "world
 *     runs at 2x" separate from `Engine.time_scale`, which would also scale
 *     physics.)
 *  3. Non-Node systems can participate. Most folio "systems" are plain objects,
 *     not scene nodes; they subscribe to the tick without being in the tree, so
 *     the frame loop is decoupled from scene structure. Ported systems can be
 *     RefCounted/Object and register with the FolioTicker instead of each being a
 *     Node with its own `_process`.
 *  4. Frame-count scheduling. `wait(frames)` defers work by an exact number of
 *     frames (e.g. 1-3 frames after physics settles). Godot timers are seconds,
 *     `await` is coroutine-based; neither is a lightweight per-frame countdown.
 *  5. 1:1 fidelity. The source's behaviour (the physics pre/post split, render
 *     timing, the time scale) is defined by this single-tick contract; matching
 *     it is what makes the port faithful.
 *
 * Trade-off: you *could* instead lean on Godot built-ins (`process_priority`,
 * `_physics_process`). The FolioTicker is chosen for faithful porting AND the genuine
 * wins above. If deterministic physics becomes a priority, drive `update()` from
 * `_physics_process` rather than `_process`.
 */
class FolioTicker : public Node {
	GDCLASS(FolioTicker,
			Node)

public:
	// Named tick priorities (folio uses bare magic numbers; we document them).
	enum Priority {
		PRIORITY_DEFAULT = 1,
		PRIORITY_PHYSICS_PRE = 2,
		PRIORITY_PHYSICS_POST = 5,
		PRIORITY_RENDER = 998,
	};

private:
	struct TickSubscriber {
		int order = PRIORITY_DEFAULT;
		Callable callable;
	};

	struct Wait {
		int frames = 0;
		Callable callable;
	};

	// Time state (seconds).
	double elapsed = 0.0;
	double delta = 1.0 / 60.0;
	double max_delta = 1.0 / 30.0; // clamp big steps after a stall
	double scale = 2.0; // world runs at 2x wall-clock (folio parity)
	double delta_scaled = (1.0 / 60.0) * 2.0;
	double elapsed_scaled = 0.0;
	double delta_average = 1.0 / 60.0;

	std::vector<double> last_deltas; // rolling window, newest first
	int delta_average_count = 30;

	std::vector<TickSubscriber> tick_subscribers; // kept sorted ascending by order
	std::vector<Wait> waits;

	bool globals_registered = false;

	void _register_shader_globals();
	void _update_shader_globals();

	static FolioTicker *singleton;

protected:
	static void _bind_methods();

public:
	FolioTicker();
	~FolioTicker();

	void _ready() override;
	void _process(double p_delta) override;

	// Core: advance one frame with a raw (unclamped) frame delta in seconds.
	void update(double p_frame_delta);

	// Subscribe/unsubscribe a callback to the per-frame tick at a given order.
	void connect_tick(
			const Callable &p_callable,
			int p_order
	);
	void disconnect_tick(const Callable &p_callable);

	// Run a callback after N frames (frame-count scheduler, not seconds).
	void
	wait(int p_frames,
		 const Callable &p_callable);

	// Accessors.
	double get_elapsed() const { return elapsed; }
	double get_delta() const { return delta; }
	double get_delta_scaled() const { return delta_scaled; }
	double get_elapsed_scaled() const { return elapsed_scaled; }
	double get_delta_average() const { return delta_average; }

	void set_scale(double p_scale) { scale = p_scale; }
	double get_scale() const { return scale; }

	static FolioTicker *get_singleton() { return singleton; }
};

} // namespace godot

VARIANT_ENUM_CAST(godot::FolioTicker::Priority);

#endif // FOLIO_TICKER_H
