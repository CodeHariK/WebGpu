using UnityEngine;

namespace CMF
{
    //This script handles all physics, collision detection and ground detection;
    //It expects a movement velocity (via 'SetVelocity') every 'FixedUpdate' frame from an external script (like a controller script) to work;
    //It also provides several getter methods for important information (whether the mover is grounded, the current surface normal [...]);
    public class Mover : MonoBehaviour
    {
        #region References to attached components;
        Collider col;
        Rigidbody rig;
        Transform tr;
        Sensor sensor;

        BoxCollider boxCollider;
        SphereCollider sphereCollider;
        CapsuleCollider capsuleCollider;
		#endregion

        #region Collider variables
        [Header("Mover Options :")]
        [Range(0f, 1f)] [SerializeField] float stepHeightRatio = 0.25f;
        [Header("Collider Options :")]
        [SerializeField] float colliderHeight = 2f;
        [SerializeField] float colliderThickness = 1f;
        [SerializeField] Vector3 colliderOffset = Vector3.zero;
		#endregion

        #region Sensor variables
        [Header("Sensor Options :")]
        [SerializeField] public Sensor.CastType sensorType = Sensor.CastType.Raycast;
        private float sensorRadiusModifier = 0.8f;
        private int currentLayer;
        [SerializeField] bool isInDebugMode = false;
        [Header("Sensor Array Options :")]
        [SerializeField] [Range(1, 5)] int sensorArrayRows = 1;
        [SerializeField] [Range(3, 10)] int sensorArrayRayCount = 6;
        [SerializeField] bool sensorArrayRowsAreOffset = false;

        [HideInInspector] public Vector3[] raycastArrayPreviewPositions;

        //Ground detection variables;
        bool isGrounded = false;

        //Sensor range variables;
        bool IsUsingExtendedSensorRange = true;
        float baseSensorRange = 0f;

        //Current upwards (or downwards) velocity necessary to keep the correct distance to the ground;
        Vector3 currentGroundAdjustmentVelocity = Vector3.zero;
		#endregion

        #region Getters

        public Vector3 GetGroundNormal() { return sensor.GetNormal(); }

        public Vector3 GetGroundPoint() { return sensor.GetHitPosition(); }

        public Collider GetGroundCollider() { return sensor.GetHitCollider(); }

        Vector3 GetColliderCenter()
        { //Returns the collider's center in world coordinates;
            if (col == null) Setup();
            return col.bounds.center;
        }

        //Returns 'true' if mover is touching ground and the angle between hte 'up' vector and ground normal is not too steep (e.g., angle < slope_limit);
        public bool IsGrounded() { return isGrounded; }

		#endregion

        #region Setters

        public void SetVelocity(Vector3 _velocity) //Set mover velocity;
        { rig.velocity = _velocity + currentGroundAdjustmentVelocity; }

        public void ExtendSensorRange(bool _isExtended)
        { IsUsingExtendedSensorRange = _isExtended; }

        public void SetColliderHeight(float _newColliderHeight)
        {
            colliderHeight = _newColliderHeight;
            RecalculateColliderDimensions();
        }

        public void SetColliderThickness(float _newColliderThickness)
        {
            if (_newColliderThickness < 0f) _newColliderThickness = 0f;
            colliderThickness = _newColliderThickness;
            RecalculateColliderDimensions();
        }

        public void SetStepHeightRatio(float _newStepHeightRatio)
        {
            _newStepHeightRatio = Mathf.Clamp(_newStepHeightRatio, 0f, 1f);
            stepHeightRatio = _newStepHeightRatio;
            RecalculateColliderDimensions();
        }

		#endregion

        void Awake()
        {
            Setup();

            //Initialize sensor;
            sensor = new Sensor(this.tr, col);
            RecalculateColliderDimensions();
            RecalibrateSensor();
        }

        void Reset()
        {
            Setup();
        }

        void OnValidate()
        {
            //Recalculate collider dimensions;
            if (this.gameObject.activeInHierarchy)
                RecalculateColliderDimensions();

            //Recalculate raycast array preview positions;
            if (sensorType == Sensor.CastType.RaycastArray)
                raycastArrayPreviewPositions =
                    Sensor.GetRaycastStartPositions(sensorArrayRows, sensorArrayRayCount, sensorArrayRowsAreOffset, 1f);
        }

        //Setup references to components;
        void Setup()
        {
            tr = transform;
            col = GetComponent<Collider>();

            //If no collider is attached to this gameobject, add a collider;
            if (col == null)
            {
                tr.gameObject.AddComponent<CapsuleCollider>();
                col = GetComponent<Collider>();
            }

            rig = GetComponent<Rigidbody>();

            //If no rigidbody is attached to this gameobject, add a rigidbody;
            if (rig == null)
            {
                tr.gameObject.AddComponent<Rigidbody>();
                rig = GetComponent<Rigidbody>();
            }

            boxCollider = GetComponent<BoxCollider>();
            sphereCollider = GetComponent<SphereCollider>();
            capsuleCollider = GetComponent<CapsuleCollider>();

            //Freeze rigidbody rotation and disable rigidbody gravity;
            rig.freezeRotation = true;
            rig.useGravity = false;
        }

        void LateUpdate()
        {
            if (isInDebugMode) sensor.DrawDebug();
        }

        //Recalculate collider height/width/thickness;
        public void RecalculateColliderDimensions()
        {
            if (col == null)
            {
                Setup();
                if (col == null)
                {
                    Debug.LogWarning("There is no collider attached to " + this.gameObject.name + "!");
                    return;
                }
            }

            //Set collider dimensions based on collider variables;
            if (boxCollider)
            {
                Vector3 _size = Vector3.zero;
                _size.x = colliderThickness;
                _size.z = colliderThickness;

                boxCollider.center = colliderOffset * colliderHeight;

                _size.y = colliderHeight * (1f - stepHeightRatio);
                boxCollider.size = _size;

                boxCollider.center = boxCollider.center + new Vector3(0f, stepHeightRatio * colliderHeight / 2f, 0f);
            }
            else if (sphereCollider)
            {
                sphereCollider.radius = colliderHeight / 2f;
                sphereCollider.center = colliderOffset * colliderHeight;

                sphereCollider.center = sphereCollider.center + new Vector3(0f, stepHeightRatio * sphereCollider.radius, 0f);
                sphereCollider.radius *= (1f - stepHeightRatio);
            }
            else if (capsuleCollider)
            {
                capsuleCollider.height = colliderHeight;
                capsuleCollider.center = colliderOffset * colliderHeight;
                capsuleCollider.radius = colliderThickness / 2f;

                capsuleCollider.center = capsuleCollider.center + new Vector3(0f, stepHeightRatio * capsuleCollider.height / 2f, 0f);
                capsuleCollider.height *= (1f - stepHeightRatio);

                if (capsuleCollider.height / 2f < capsuleCollider.radius)
                    capsuleCollider.radius = capsuleCollider.height / 2f;
            }

            //Recalibrate sensor variables to fit new collider dimensions;
            if (sensor != null) RecalibrateSensor();
        }

        //Recalibrate sensor variables;
        void RecalibrateSensor()
        {
            //Set sensor ray origin and direction;
            sensor.SetCastOrigin(GetColliderCenter());
            sensor.SetCastDirection(Sensor.CastDirection.Down);

            //Calculate sensor layermask;
            RecalculateSensorLayerMask();

            //Set sensor cast type;
            sensor.castType = sensorType;

            //Calculate sensor radius/width;
            float _radius = colliderThickness / 2f * sensorRadiusModifier;

            //Multiply all sensor lengths with '_safetyDistanceFactor' to compensate for floating point errors;
            float _safetyDistanceFactor = 0.001f;

            //Fit collider height to sensor radius;
            if (boxCollider)
                _radius = Mathf.Clamp(_radius, _safetyDistanceFactor, (boxCollider.size.y / 2f) * (1f - _safetyDistanceFactor));
            else if (sphereCollider)
                _radius = Mathf.Clamp(_radius, _safetyDistanceFactor, sphereCollider.radius * (1f - _safetyDistanceFactor));
            else if (capsuleCollider)
                _radius = Mathf.Clamp(_radius, _safetyDistanceFactor, (capsuleCollider.height / 2f) * (1f - _safetyDistanceFactor));

            //Set sensor variables;

            //Set sensor radius;
            sensor.sphereCastRadius = _radius * tr.localScale.x;

            //Calculate and set sensor length;
            float _length = 0f;
            _length += colliderHeight * (1 + stepHeightRatio) * 0.5f;
            baseSensorRange = _length * (1 + _safetyDistanceFactor) * tr.localScale.x;
            sensor.castLength = _length * tr.localScale.x;

            //Set sensor array variables;
            sensor.ArrayRows = sensorArrayRows;
            sensor.arrayRayCount = sensorArrayRayCount;
            sensor.offsetArrayRows = sensorArrayRowsAreOffset;
            sensor.isInDebugMode = isInDebugMode;

            //Set sensor spherecast variables;
            sensor.calculateRealDistance = true;
            sensor.calculateRealSurfaceNormal = true;

            //Recalibrate sensor to the new values;
            sensor.RecalibrateRaycastArrayPositions();
        }

        //Recalculate sensor layermask based on current physics settings;
        void RecalculateSensorLayerMask()
        {
            int _layerMask = 0;
            int _objectLayer = this.gameObject.layer;

            for (int i = 0; i < 32; i++)
            { //Calculate layermask;
                if (!Physics.GetIgnoreLayerCollision(_objectLayer, i))
                    _layerMask = _layerMask | (1 << i);
            }

            if (_layerMask == (_layerMask | (1 << LayerMask.NameToLayer("Ignore Raycast"))))
            { //Make sure that the calculated layermask does not include the 'Ignore Raycast' layer;
                _layerMask ^= (1 << LayerMask.NameToLayer("Ignore Raycast"));
            }

            sensor.layermask = _layerMask; //Set sensor layermask;
            currentLayer = _objectLayer; //Save current layer;
        }

        //Check if mover is grounded;
        public void CheckForGround()
        {
            //Check if object layer has been changed since last frame;
            //If so, recalculate sensor layer mask;
            if (currentLayer != this.gameObject.layer) RecalculateSensorLayerMask();

			//Check if mover is grounded;
			//Store all relevant collision information for later;
			//Calculate necessary adjustment velocity to keep the correct distance to the ground;
			{
				//Reset ground adjustment velocity;
				currentGroundAdjustmentVelocity = Vector3.zero;

				//Set sensor length;
				if (IsUsingExtendedSensorRange)
					sensor.castLength = baseSensorRange + (colliderHeight * tr.localScale.x) * stepHeightRatio;
				else
					sensor.castLength = baseSensorRange;

				sensor.Cast();

				//If sensor has not detected anything, set flags and return;
				if (!sensor.HasDetectedHit())
				{
					isGrounded = false;
					return;
				}

				//Set flags for ground detection;
				isGrounded = true;

				//Get distance that sensor ray reached;
				float _distance = sensor.GetHitDistance();

				//Calculate how much mover needs to be moved up or down;
				float _upperLimit = ((colliderHeight * tr.localScale.x) * (1f - stepHeightRatio)) * 0.5f;
				float _middle = _upperLimit + (colliderHeight * tr.localScale.x) * stepHeightRatio;
				float _distanceToGo = _middle - _distance;

				//Set new ground adjustment velocity for the next frame;
				currentGroundAdjustmentVelocity = tr.up * (_distanceToGo / Time.fixedDeltaTime);
			}
        }
    }
}
