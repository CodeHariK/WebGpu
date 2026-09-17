#ifndef FOLIO_TIME_H
#define FOLIO_TIME_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/callable.hpp>

namespace godot {

class Ticker;

/**
 * Folio port — Time  (registered as `FolioTime`)
 * ----------------------------------------------
 * Faithful port of folio-2025 `Game/Time.js`: the time-scale controller and
 * bullet-time (slow-motion) driver.
 *
 * It does NOT keep its own clock — it OWNS the `Ticker`'s `scale`. Normally the
 * scale sits at `default_scale` (2 -> world runs at 2x wall-clock). Calling
 * `activate_bullet_time()` eases the scale down toward `bullet_scale` (0.5) and
 * back out, animated by a 0..1 `progress` moved each tick.
 *
 * Runs at tick priority 0 (earliest, before physics-pre at 2) so the scaled
 * delta the rest of the frame consumes is already set.
 *
 * Renamed to `FolioTime` because Godot already ships a core `Time` singleton;
 * a class named `Time` in the `godot` namespace would collide.
 *
 * Not ported: folio also scales `gsap.globalTimeline.timeScale(...)` so tweens
 * slow with the world. Godot has no GSAP global timeline; when we port a tween
 * layer, mirror the scale here.
 */
class FolioTime : public Node {
	GDCLASS(FolioTime,
			Node)

private:
	double default_scale = 2.0;
	double _scale = 2.0;

	// Bullet-time state (slow-motion easing).
	bool bt_active = false;
	uint64_t bt_end_time_ms = 0;
	double bt_scale = 0.5; // scale target while active
	double bt_progress = 0.0; // 0 = normal, 1 = full slow-mo
	double bt_in_speed = 3.0; // progress ramp-in rate
	double bt_out_speed = 0.3; // progress ramp-out rate

	bool subscribed = false;
	bool _subscribe();

protected:
	static void _bind_methods();

public:
	FolioTime();
	~FolioTime();

	void _ready() override;

	// Advance one frame: eases progress and pushes the resulting scale to Ticker.
	void update();

	// Trigger / extend bullet time for `duration` seconds.
	void activate_bullet_time(double p_duration);

	void set_scale(double p_scale);
	double get_scale() const { return _scale; }

	void set_default_scale(double p_scale) { default_scale = p_scale; }
	double get_default_scale() const { return default_scale; }

	bool is_bullet_time_active() const { return bt_active; }
	double get_bullet_progress() const { return bt_progress; }
};

} // namespace godot

#endif // FOLIO_TIME_H
