using UnityEngine;

namespace CMF
{
    //This character movement input class is an example of how to get input from a gamepad/joystick to control the character;
    //It comes with a dead zone threshold setting to bypass any unwanted joystick "jitter";
    public class CharacterJoystickInput : CharacterInput
    {
        public override Vector2 GetMovementInput()
        {
            return acMove.ReadValue<Vector2>();
        }

        public override bool IsJumpKeyPressed()
        {
            return acJump.triggered;
        }

    }
}
