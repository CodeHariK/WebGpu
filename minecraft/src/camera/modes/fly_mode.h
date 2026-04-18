#ifndef FLY_MODE_H
#define FLY_MODE_H

#include "camera_mode.h"

namespace godot {

class MCFlyCameraMode : public CameraMode {
public:
	void enter(MCCamera *p_camera) override;
	void update(MCCamera *p_camera, float p_delta) override;
};

} // namespace godot

#endif // FLY_MODE_H
