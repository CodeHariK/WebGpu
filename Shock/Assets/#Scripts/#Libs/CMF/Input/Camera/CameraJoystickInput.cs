using UnityEngine;

namespace CMF
{
    //This camera input class is an example of how to get input from joysticks/gamepads using Unity's default input system;
    //It also comes with a dead zone threshold setting to bypass any unwanted joystick "jitter";
    public class CameraJoystickInput : CameraInput
    {
        //Use this value to fine-tune mouse movement;
        //All mouse input will be multiplied by this value;
        public float mouseInputMultiplier = 0.01f;

        public override Vector2 GetCameraInput()
        {
            Vector2 i = acLook.ReadValue<Vector2>();

            //Since raw mouse input is already time-based, we need to correct for this before passing the input to the camera controller;
            if (Time.timeScale > 0f && Time.deltaTime > 0f)
            {
                i /= Time.deltaTime;
                i *= Time.timeScale;
            }
            else
                i = Vector2.zero;

            i *= mouseInputMultiplier;
            return i;
        }
    }
}

