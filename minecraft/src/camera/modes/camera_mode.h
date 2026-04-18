#ifndef CAMERA_MODE_H
#define CAMERA_MODE_H

#include <godot_cpp/core/class_db.hpp>

namespace godot {

class MCCamera;

/**
 * Interface for specific camera behavior states.
 */
class CameraMode {
public:
	virtual ~CameraMode() {}

	virtual void enter(MCCamera *p_camera) {}
	virtual void exit(MCCamera *p_camera) {}

	virtual void update(MCCamera *p_camera, float p_delta) = 0;
};

} // namespace godot

#endif // CAMERA_MODE_H
