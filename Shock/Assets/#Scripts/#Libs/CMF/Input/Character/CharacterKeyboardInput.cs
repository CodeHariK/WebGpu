using UnityEngine;

namespace CMF
{
    //This character movement input class is an example of how to get input from a keyboard to control the character;
    public class CharacterKeyboardInput : CharacterInput
    {
        public override Vector2 GetMovementInput()
        {
            Debug.Log(acMove.ReadValue<Vector2>());
            return acMove.ReadValue<Vector2>();
        }

        public override bool IsJumpKeyPressed()
        {
            return acJump.triggered;
        }
    }
}
