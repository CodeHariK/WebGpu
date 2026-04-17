#ifndef DEBUG_LINE_QUAD_H
#define DEBUG_LINE_QUAD_H

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

class DebugLineQuad : public MeshInstance3D {
	GDCLASS(DebugLineQuad, MeshInstance3D)

private:
	Vector3 start_pos;
	Vector3 end_pos;
	float thickness = 0.1f;
	Color color = Color(1, 1, 1, 1);

	Ref<ShaderMaterial> shader_material;
	Ref<QuadMesh> quad_mesh;

	void _update_geometry();

protected:
	static void _bind_methods();

public:
	DebugLineQuad();
	~DebugLineQuad();

	void _ready() override;

	void set_line(const Vector3 &p_start, const Vector3 &p_end, float p_thickness);
	void set_shader(const Ref<Shader> &p_shader);
	
	void set_color(const Color &p_color);
	Color get_color() const;

	void set_start_pos(const Vector3 &p_pos);
	Vector3 get_start_pos() const;

	void set_end_pos(const Vector3 &p_pos);
	Vector3 get_end_pos() const;

	void set_thickness(float p_thickness);
	float get_thickness() const;
};

} // namespace godot

#endif // DEBUG_LINE_QUAD_H
