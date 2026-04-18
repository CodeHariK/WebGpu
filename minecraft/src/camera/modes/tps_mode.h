#ifndef MCTPSCAMERAMODE_H
#define MCTPSCAMERAMODE_H

#include "camera_mode.h"

namespace godot {

class MCTPSCameraMode : public CameraMode {
public:
	virtual void enter(MCCamera *p_camera) override;
	virtual void update(MCCamera *p_camera, float p_delta) override;
};

} // namespace godot

#endif // MCTPSCAMERAMODE_H
