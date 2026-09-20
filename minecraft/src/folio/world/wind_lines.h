#ifndef FOLIO_WIND_LINES_H
#define FOLIO_WIND_LINES_H

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/templates/vector.hpp>

namespace godot {

/**
 * Folio port — FolioWindLines  (folio `World/WindLines.js`)
 * -----------------------------------------------
 * A small pool of wavy ribbon "gust" streaks that appear over the world on a
 * random interval, drift along the wind direction, and fade as a bulge sweeps
 * their length (see wind_line.gdshader). Spawn cadence is random (folio 0.3–2s);
 * each streak's lifetime shortens as wind picks up (folio duration =
 * remapClamp(weather.wind, 0,1, 8,2)).
 *
 * folio drives this with gsap tweens + setTimeout; the port advances the pool on
 * the shared ticker (order 10) and schedules spawns off elapsed time. Placement
 * uses the view's optimal-area centre + radius so streaks stay around the camera.
 *
 * All streaks share one ribbon mesh; each pool slot has its own ShaderMaterial so
 * its `progress` animates independently.
 */
class FolioWindLines : public Node3D {
	GDCLASS(FolioWindLines,
			Node3D)

private:
	// Tunables (folio uniforms / fields).
	int pool_size = 4;
	double interval_min = 0.3; // seconds (folio 300ms)
	double interval_max = 2.0; // seconds (folio 2000ms)
	double translation = 1.0; // drift distance along wind
	double thickness = 0.1;
	double spawn_height = 2.0; // folio mesh.position.y = 2

	struct Line {
		MeshInstance3D *mesh = nullptr;
		Ref<ShaderMaterial> material;
		bool active = false;
		double t_start = 0.0;
		double duration = 4.0;
		Vector3 start_pos;
		Vector3 end_pos;
	};

	Vector<Line> pool;
	Ref<ArrayMesh> ribbon;
	double next_spawn = 0.0;
	bool ready_done = false;

	Ref<ArrayMesh> _build_ribbon() const;
	void _display(double p_elapsed);

protected:
	static void _bind_methods();

public:
	FolioWindLines();
	~FolioWindLines();

	void _ready() override;
	void update(); // tick 10

	void set_pool_size(int p_n) { pool_size = p_n < 1 ? 1 : p_n; }
	int get_pool_size() const { return pool_size; }
	void set_thickness(double p_v);
	double get_thickness() const { return thickness; }
};

} // namespace godot

#endif // FOLIO_WIND_LINES_H
