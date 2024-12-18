using UnityEngine;

namespace CMF
{
    //This abstract camera input class serves as a base for all other camera input classes;
    //The 'CameraController' component will access this script at runtime to get input for the camera's rotation;
    //By extending this class, it is possible to implement custom camera input;
    public abstract class CameraInput : MonoBehaviour
    {
        protected S_Input_Controls S_Input;

        protected UnityEngine.InputSystem.InputAction acLook;

        public abstract Vector2 GetCameraInput();

        void Awake()
        {
            S_Input = new S_Input_Controls();
            acLook = S_Input.PlayerKeyMap.Look;
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
