#ifndef CAMERA_STATE_CAR_H
#define CAMERA_STATE_CAR_H

#include "../camera_state.h"

namespace godot {

class CameraStateCar : public CameraState {
public:
	void enter(GameCamera *p_camera) override;
	void update(GameCamera *p_camera, float p_delta) override;
};

} // namespace godot

#endif // CAMERA_STATE_CAR_H
