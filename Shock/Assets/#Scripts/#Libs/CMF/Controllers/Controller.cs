using UnityEngine;

namespace CMF
{
    //This abstract class is the base for all other controller components (such as 'AdvancedWalkerController');
    //It can be extended to create a custom controller class;
    public abstract class Controller : MonoBehaviour
    {

        //Getters;
        public abstract Vector3 GetSavedVelocity();
        public abstract Vector3 GetSavedMovementVelocity();
        public abstract bool IsGrounded();

        //Events;
        public delegate void CMFEvent(Vector3 v);
        public CMFEvent OnJump;
        public CMFEvent OnLand;
    }
}
