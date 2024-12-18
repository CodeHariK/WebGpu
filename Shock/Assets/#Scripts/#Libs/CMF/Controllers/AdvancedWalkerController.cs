using UnityEngine;

namespace CMF
{
    //Advanced walker controller script;
    //This controller is used as a basis for other controller types ('SidescrollerController');
    //Custom movement input can be implemented by creating a new script that inherits 'AdvancedWalkerController' and overriding the 'CalculateMovementDirection' function;
    public class AdvancedWalkerController : Controller
    {
        #region Variables
        protected Transform tr;
        protected Mover mover;
        protected CharacterInput characterInput;
        protected CeilingDetector ceilingDetector;
        [Tooltip("Optional camera transform used for calculating movement direction. If assigned, character movement will take camera view into account.")]
        public Transform cameraTransform;

        bool jumpInputIsLocked = false, jumpKeyWasPressed = false, jumpKeyWasLetGo = false, jumpKeyIsPressed = false;

        public float movementSpeed = 7f;
        public float airControlRate = 2f; //How fast the controller can change direction while in the air;
        public float jumpSpeed = 10f;
        public float jumpDuration = 0.2f;
        float currentJumpStartTime = 0f;
        public float airFriction = 0.5f; //'AirFriction' determines how fast the controller loses its momentum while in the air;
        public float groundFriction = 100f; //'GroundFriction' is used instead, if the controller is grounded;
        protected Vector3 momentum = Vector3.zero;
        Vector3 savedVelocity = Vector3.zero; //Saved velocity from last frame;
        Vector3 savedMovementVelocity = Vector3.zero; //Saved horizontal movement velocity from last frame;
        public float gravity = 30f; //Amount of downward gravity;
        [Tooltip("How fast the character will slide down steep slopes.")] public float slideGravity = 5f;
        public float slopeLimit = 80f; //Acceptable slope angle limit;
        [Tooltip("Whether to calculate and apply momentum relative to the controller's transform.")] public bool useLocalMomentum = false;

        public enum ControllerState
        { //Enum describing basic controller states; 
            Grounded,
            Sliding,
            Falling,
            Rising,
            Jumping
        }
        ControllerState currentControllerState = ControllerState.Falling;
        #endregion

		#region Getter and Setter

        //Returns 'true' if vertical momentum is above a small threshold;
        private bool IsRising()
        {
            //Setup threshold to check against; For most applications, a value of '0.001f' is recommended;
            return (VectorMath.SignedMagProjectAonB(GetMomentum(), tr.up) > 0.001f);
        }

        private bool IsFalling()
        {
            //Setup threshold to check against; For most applications, a value of '0.001f' is recommended;
            return (VectorMath.SignedMagProjectAonB(GetMomentum(), tr.up) < -0.001f);
        }

        //Returns true if angle between controller and ground normal is too big (> slope limit), i.e. ground is too steep;
        private bool IsGroundTooSteep()
        {
            if (!mover.IsGrounded()) return false;
            return (Vector3.Angle(mover.GetGroundNormal(), tr.up) > slopeLimit);
        }

        //Get last frame's velocity;
        public override Vector3 GetSavedVelocity() { return savedVelocity; }

        //Get last frame's movement velocity (momentum is ignored);
        public override Vector3 GetSavedMovementVelocity() { return savedMovementVelocity; }

        protected virtual bool IsJumpKeyPressed()
        {
            if (characterInput == null) return false;
            return characterInput.IsJumpKeyPressed();
        }

        public Vector3 GetMomentum()
        { //Get current momentum;
            if (useLocalMomentum) return tr.localToWorldMatrix * momentum;
            return momentum;
        }

        public override bool IsGrounded() //Returns 'true' if controller is grounded (or sliding down a slope);
        { return (currentControllerState == ControllerState.Grounded || currentControllerState == ControllerState.Sliding); }

        public bool IsSliding() { return (currentControllerState == ControllerState.Sliding); }

        //Add momentum to controller;
        public void AddMomentum(Vector3 _momentum)
        {
            if (useLocalMomentum) momentum = tr.localToWorldMatrix * momentum;
            momentum += _momentum;
            if (useLocalMomentum) momentum = tr.worldToLocalMatrix * momentum;
        }

        public void SetMomentum(Vector3 _newMomentum)
        { //Set controller momentum directly;
            if (useLocalMomentum) momentum = tr.worldToLocalMatrix * _newMomentum;
            else momentum = _newMomentum;
        }

		#endregion

        //Get references to all necessary components;
        void Awake()
        {
            mover = GetComponent<Mover>();
            tr = transform;
            characterInput = GetComponent<CharacterInput>();
            ceilingDetector = GetComponent<CeilingDetector>();

            if (characterInput == null)
                Debug.LogWarning("No character input script has been attached to this gameobject", this.gameObject);

            Setup();
        }

        //This function is called right after Awake(); It can be overridden by inheriting scripts;
        protected virtual void Setup()
        {
        }

        void Update()
        {
            { //Handle jump booleans for later use in FixedUpdate;
                bool _newJumpKeyPressedState = IsJumpKeyPressed();

                if (jumpKeyIsPressed == false && _newJumpKeyPressedState == true)
                    jumpKeyWasPressed = true;

                if (jumpKeyIsPressed == true && _newJumpKeyPressedState == false)
                {
                    jumpKeyWasLetGo = true;
                    jumpInputIsLocked = false;
                }

                jumpKeyIsPressed = _newJumpKeyPressedState;
            }
        }

        void FixedUpdate()
        {
            mover.CheckForGround(); //Check if mover is grounded;
            currentControllerState = DetermineControllerState();
            HandleMomentum(); //Apply friction and gravity to 'momentum';
            HandleJumping(); //Check if the player has initiated a jump;

            //Calculate movement velocity;
            Vector3 _velocity = Vector3.zero;
            if (currentControllerState == ControllerState.Grounded)
                _velocity = CalculateMovementVelocity();

            //If local momentum is used, transform momentum into world space first;
            Vector3 _worldMomentum = momentum;
            if (useLocalMomentum) _worldMomentum = tr.localToWorldMatrix * momentum;
            _velocity += _worldMomentum; //Add current momentum to velocity;

            //If player is grounded or sliding on a slope, extend mover's sensor range;
            //This enables the player to walk up/down stairs and slopes without losing ground contact;
            mover.ExtendSensorRange(IsGrounded());

            mover.SetVelocity(_velocity); //Set mover velocity;
            savedVelocity = _velocity; //Store velocity for next frame;
            savedMovementVelocity = CalculateMovementVelocity(); //Save controller movement velocity;

            //Reset jump key booleans;
            jumpKeyWasLetGo = false;
            jumpKeyWasPressed = false;

            if (ceilingDetector != null) ceilingDetector.ResetFlags();
        }

        //Calculate and return movement direction based on player input;
        //This function can be overridden by inheriting scripts to implement different player controls;
        protected virtual Vector3 CalculateMovementDirection()
        {
            //If no character input script is attached to this object, return;
            if (characterInput == null) return Vector3.zero;

            Vector3 _velocity = Vector3.zero;

            Vector2 _input = characterInput.GetMovementInput();

            if (cameraTransform == null)
            { //If no camera transform has been assigned, use the character's transform axes to calculate the movement direction;
                _velocity += tr.right * _input.x;
                _velocity += tr.forward * _input.y;
            }
            else
            {
                //If a camera transform has been assigned, use the assigned transform's axes for movement direction;
                //Project movement direction so movement stays parallel to the ground;
                _velocity += Vector3.ProjectOnPlane(cameraTransform.right, tr.up).normalized * _input.x;
                _velocity += Vector3.ProjectOnPlane(cameraTransform.forward, tr.up).normalized * _input.y;
            }

            //If necessary, clamp movement vector to magnitude of 1f;
            if (_velocity.magnitude > 1f) _velocity.Normalize();

            return _velocity;
        }

        //Calculate and return movement velocity based on player input, controller state, ground normal [...];
        protected virtual Vector3 CalculateMovementVelocity()
        {
            //Multiply (normalized) velocity with movement speed;
            return CalculateMovementDirection() * movementSpeed;
        }

        //Determine current controller state based on current momentum and whether the controller is grounded (or not);
        //Handle state transitions;
        ControllerState DetermineControllerState()
        {
            bool _isRising = IsRising();
            bool _isGroundedSliding = IsGroundTooSteep(); //Check if controller is sliding;

            //Grounded;
            if (currentControllerState == ControllerState.Grounded)
            {
                if (_isRising)
                {
                    OnGroundContactLost();
                    return ControllerState.Rising; //wasGrounded and now rising = rising
                }
                if (!mover.IsGrounded())
                {
                    OnGroundContactLost();
                    return ControllerState.Falling; //isnotGrounded, wasGrounded = falling off cliff
                }
                if (_isGroundedSliding)
                {
                    OnGroundContactLost();
                    return ControllerState.Sliding; //sliding
                }
                return ControllerState.Grounded;
            }

            //Falling;
            if (currentControllerState == ControllerState.Falling)
            {
                if (_isRising)
                {
                    return ControllerState.Rising;//wasFalling, nowRising (Like double jump) = rising
                }
                if (mover.IsGrounded() && !_isGroundedSliding)
                {
                    OnGroundContactRegained();
                    return ControllerState.Grounded;//wasFalling, nowGrounded and notSliding = grounded
                }
                if (_isGroundedSliding)
                {
                    return ControllerState.Sliding;//wasFalling, nowGrounded and Sliding = Sliding
                }
                return ControllerState.Falling;
            }

            //Sliding;
            if (currentControllerState == ControllerState.Sliding)
            {
                if (_isRising)
                {
                    OnGroundContactLost();
                    return ControllerState.Rising;
                }
                if (!mover.IsGrounded())
                {
                    OnGroundContactLost();
                    return ControllerState.Falling;
                }
                if (mover.IsGrounded() && !_isGroundedSliding)
                {
                    OnGroundContactRegained();
                    return ControllerState.Grounded;
                }
                return ControllerState.Sliding;
            }

            //Rising;
            if (currentControllerState == ControllerState.Rising)
            {
                if (!_isRising)
                {
                    if (mover.IsGrounded() && !_isGroundedSliding)
                    {
                        OnGroundContactRegained();
                        return ControllerState.Grounded;
                    }
                    if (_isGroundedSliding)
                    {
                        return ControllerState.Sliding;
                    }
                    if (!mover.IsGrounded())
                    {
                        return ControllerState.Falling;
                    }
                }

                //If a ceiling detector has been attached to this gameobject, check for ceiling hits;
                if (ceilingDetector != null)
                {
                    if (ceilingDetector.HitCeiling())
                    {
                        OnCeilingContact();
                        return ControllerState.Falling;
                    }
                }
                return ControllerState.Rising;
            }

            //Jumping;
            if (currentControllerState == ControllerState.Jumping)
            {
                //Check for jump timeout;
                if ((Time.time - currentJumpStartTime) > jumpDuration)
                    return ControllerState.Rising;

                //Check if jump key was let go;
                if (jumpKeyWasLetGo)
                    return ControllerState.Rising;

                //If a ceiling detector has been attached to this gameobject, check for ceiling hits;
                if (ceilingDetector != null)
                {
                    if (ceilingDetector.HitCeiling())
                    {
                        OnCeilingContact();
                        return ControllerState.Falling;
                    }
                }
                return ControllerState.Jumping;
            }

            return ControllerState.Falling;
        }

        //Check if player has initiated a jump;
        void HandleJumping()
        {
            if (currentControllerState == ControllerState.Grounded)
            {
                if ((jumpKeyIsPressed || jumpKeyWasPressed) && !jumpInputIsLocked)
                {
                    //Call events;
                    OnGroundContactLost();
                    OnJumpStart();

                    currentControllerState = ControllerState.Jumping;
                }
            }
        }

        //Apply friction to both vertical and horizontal momentum based on 'friction' and 'gravity';
        //Handle movement in the air; Handle sliding down steep slopes;
        void HandleMomentum()
        {
            if (useLocalMomentum) momentum = tr.localToWorldMatrix * momentum;
            Vector3 _verticalMomentum = Vector3.zero;
            Vector3 _horizontalMomentum = Vector3.zero;
            if (momentum != Vector3.zero)
            {
                _verticalMomentum = VectorMath.ProjectAonB(momentum, tr.up);
                _horizontalMomentum = momentum - _verticalMomentum;
            }
            _verticalMomentum -= tr.up * gravity * Time.deltaTime;

            //Remove any downward force if the controller is grounded;
            if (currentControllerState == ControllerState.Grounded && VectorMath.SignedMagProjectAonB(_verticalMomentum, tr.up) < 0f)
                _verticalMomentum = Vector3.zero;

            //Manipulate momentum to steer controller in the air (if controller is not grounded or sliding);
            if (!IsGrounded())
            {
                Vector3 _movementVelocity = CalculateMovementVelocity();

                //If controller has received additional momentum from somewhere else;
                if (_horizontalMomentum.magnitude > movementSpeed)
                {
                    //Prevent unwanted accumulation of speed in the direction of the current momentum;
                    if (VectorMath.SignedMagProjectAonB(_movementVelocity, _horizontalMomentum) > 0f)
                        _movementVelocity = VectorMath.RejectAonB(_movementVelocity, _horizontalMomentum);

                    //Lower air control slightly with a multiplier to add some 'weight' to any momentum applied to the controller;
                    float _airControlMultiplier = 0.25f;
                    _horizontalMomentum += _movementVelocity * Time.deltaTime * airControlRate * _airControlMultiplier;
                }
                //If controller has not received additional momentum;
                else
                {
                    //Clamp _horizontal velocity to prevent accumulation of speed;
                    _horizontalMomentum += _movementVelocity * Time.deltaTime * airControlRate;
                    _horizontalMomentum = Vector3.ClampMagnitude(_horizontalMomentum, movementSpeed);
                }
            }

            //Steer controller on slopes;
            if (currentControllerState == ControllerState.Sliding)
            {
                //Calculate vector pointing away from slope;
                Vector3 _pointDownVector = Vector3.ProjectOnPlane(mover.GetGroundNormal(), tr.up).normalized;

                //Calculate movement velocity;
                Vector3 _slopeMovementVelocity = CalculateMovementVelocity();
                //Remove all velocity that is pointing up the slope;
                _slopeMovementVelocity = VectorMath.RejectAonB(_slopeMovementVelocity, _pointDownVector);

                //Add movement velocity to momentum;
                _horizontalMomentum += _slopeMovementVelocity * Time.fixedDeltaTime;
            }

            //Apply friction to horizontal momentum based on whether the controller is grounded;
            if (currentControllerState == ControllerState.Grounded)
                _horizontalMomentum = Vector3.MoveTowards(_horizontalMomentum, Vector3.zero, groundFriction * Time.deltaTime);
            else
                _horizontalMomentum = Vector3.MoveTowards(_horizontalMomentum, Vector3.zero, airFriction * Time.deltaTime);

            //Add horizontal and vertical momentum back together;
            momentum = _horizontalMomentum + _verticalMomentum;

            //Additional momentum calculations for sliding;
            if (currentControllerState == ControllerState.Sliding)
            {
                //Project the current momentum onto the current ground normal if the controller is sliding down a slope;
                momentum = Vector3.ProjectOnPlane(momentum, mover.GetGroundNormal());

                //Remove any upwards momentum when sliding;
                if (VectorMath.SignedMagProjectAonB(momentum, tr.up) > 0f)
                    momentum = VectorMath.RejectAonB(momentum, tr.up);

                //Apply additional slide gravity;
                Vector3 _slideDirection = Vector3.ProjectOnPlane(-tr.up, mover.GetGroundNormal()).normalized;
                momentum += _slideDirection * slideGravity * Time.deltaTime;
            }

            //If controller is jumping, override vertical velocity with jumpSpeed;
            if (currentControllerState == ControllerState.Jumping)
            {
                momentum = VectorMath.RejectAonB(momentum, tr.up);
                momentum += tr.up * jumpSpeed;
            }

            if (useLocalMomentum) momentum = tr.worldToLocalMatrix * momentum;
        }

        //Events;

        //This function is called when the player has initiated a jump;
        void OnJumpStart()
        {
            //If local momentum is used, transform momentum into world coordinates first;
            if (useLocalMomentum) momentum = tr.localToWorldMatrix * momentum;

            momentum += tr.up * jumpSpeed; //Add jump force to momentum;

            currentJumpStartTime = Time.time; //Set jump start time;

            jumpInputIsLocked = true; //Lock jump input until jump key is released again;

            if (OnJump != null) OnJump(momentum);

            if (useLocalMomentum) momentum = tr.worldToLocalMatrix * momentum;
        }

        //This function is called when the controller has lost ground contact, i.e. is either falling or rising, or generally in the air;
        void OnGroundContactLost()
        {
            if (useLocalMomentum) momentum = tr.localToWorldMatrix * momentum;

            Vector3 _velocity = GetSavedMovementVelocity(); //Get current movement velocity;

            //Check if the controller has both momentum and a current movement velocity;
            if (_velocity.sqrMagnitude >= 0f && momentum.sqrMagnitude > 0f)
            {
                //Project momentum onto movement direction;
                Vector3 _projectedMomentum = Vector3.Project(momentum, _velocity.normalized);
                //Calculate dot product to determine whether momentum and movement are aligned;
                float _dot = VectorMath.SignedMagProjectAonB(_projectedMomentum.normalized, _velocity.normalized);

                //If current momentum is already pointing in the same direction as movement velocity,
                //Don't add further momentum (or limit movement velocity) to prevent unwanted speed accumulation;
                if (_projectedMomentum.sqrMagnitude >= _velocity.sqrMagnitude && _dot > 0f)
                    _velocity = Vector3.zero;
                else if (_dot > 0f)
                    _velocity -= _projectedMomentum;
            }

            momentum += _velocity; //Add movement velocity to momentum;

            if (useLocalMomentum) momentum = tr.worldToLocalMatrix * momentum;
        }

        //This function is called when the controller has landed on a surface after being in the air;
        void OnGroundContactRegained()
        {
            if (OnLand != null)
            {
                Vector3 _collisionVelocity = momentum;
                if (useLocalMomentum) _collisionVelocity = tr.localToWorldMatrix * _collisionVelocity;
                OnLand(_collisionVelocity);//Call every funciton attached to delegate
            }
        }

        //This function is called when the controller has collided with a ceiling while jumping or moving upwards;
        void OnCeilingContact()
        {
            if (useLocalMomentum) momentum = tr.localToWorldMatrix * momentum;
            momentum = VectorMath.RejectAonB(momentum, tr.up); //Remove all vertical parts of momentum;
            if (useLocalMomentum) momentum = tr.worldToLocalMatrix * momentum;
        }
    }
}
