#ifndef CAMERA_STATE_H
#define CAMERA_STATE_H

#include <godot_cpp/core/class_db.hpp>

namespace godot {

class GameCamera;

/**
 * Interface for specific camera behavior states.
 */
class CameraState {
public:
	virtual ~CameraState() {}

	virtual void enter(GameCamera *p_camera) {}
	virtual void exit(GameCamera *p_camera) {}

	virtual void update(GameCamera *p_camera, float p_delta) = 0;
};

} // namespace godot

#endif // CAMERA_STATE_H
