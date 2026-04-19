#ifndef CAMERA_STATE_FIXED_H
#define CAMERA_STATE_FIXED_H

#include "../camera_state.h"

namespace godot {

class CameraStateFixed : public CameraState {
public:
	void enter(GameCamera *p_camera) override;
	void update(GameCamera *p_camera, float p_delta) override;
};

} // namespace godot

#endif // CAMERA_STATE_FIXED_H
