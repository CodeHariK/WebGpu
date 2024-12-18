using UnityEngine;

namespace CMF
{
    //This abstract character input class serves as a base for all other character input classes;
    //The 'Controller' component will access this script at runtime to get input for the character's movement (and jumping);
    //By extending this class, it is possible to implement custom character input;
    public abstract class CharacterInput : MonoBehaviour
    {
        protected S_Input_Controls S_Input;

        protected UnityEngine.InputSystem.InputAction acMove;
        protected UnityEngine.InputSystem.InputAction acJump;

        //public abstract float GetHorizontalMovementInput();
        //public abstract float GetVerticalMovementInput();

        public abstract Vector2 GetMovementInput();

        public abstract bool IsJumpKeyPressed();

        void Awake()
        {
            S_Input = new S_Input_Controls();
            acMove = S_Input.PlayerKeyMap.Move;
            acJump = S_Input.PlayerKeyMap.Jump;
        }

        private void OnEnable()
        {
            S_Input.Enable();
        }

        private void OnDisable()
        {
            S_Input.Disable();
        }
    }
}
