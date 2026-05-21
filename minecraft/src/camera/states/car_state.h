#ifndef CAMERA_STATE_CAR_H
#define CAMERA_STATE_CAR_H

#include "../camera_state.h"
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class CameraStateCar : public CameraState {
private:
	Vector3 smoothed_pivot;
	Vector3 smoothed_velocity;
	bool first_frame = true;

public:
	void enter(GameCamera *p_camera) override;
	void update(GameCamera *p_camera, float p_delta) override;
};

} // namespace godot

#endif // CAMERA_STATE_CAR_H
