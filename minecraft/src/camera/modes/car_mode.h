#ifndef CAR_MODE_H
#define CAR_MODE_H

#include "camera_mode.h"

namespace godot {

class MCCarCameraMode : public CameraMode {
public:
	void enter(MCCamera *p_camera) override;
	void update(MCCamera *p_camera, float p_delta) override;
};

} // namespace godot

#endif // CAR_MODE_H
