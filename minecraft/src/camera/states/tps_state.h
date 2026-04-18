#ifndef CAMERA_STATE_TPS_H
#define CAMERA_STATE_TPS_H

#include "../camera_state.h"

namespace godot {

class CameraStateTPS : public CameraState {
public:
	virtual void enter(GameCamera *p_camera) override;
	virtual void update(GameCamera *p_camera, float p_delta) override;
};

} // namespace godot

#endif // CAMERA_STATE_TPS_H
