using UnityEngine;
using System.Collections.Generic;

namespace CMF
{
    //This script is responsible for casting rays and spherecasts;
    //It is instantiated by the 'Mover' component at runtime;
    [System.Serializable]
    public class Sensor
    {
        #region References to attached components; Basic raycast parameters;
        private Transform tr;
        private Collider col;

        private Vector3 origin = Vector3.zero; //Starting point of (ray-)cast;
        public float castLength = 1f;
        public float sphereCastRadius = 0.2f;

        public enum CastDirection
        { //Enum describing local transform axes used as directions for raycasting;
            Forward,
            Right,
            Up,
            Backward,
            Left,
            Down
        }
        private CastDirection castDirection;

        [SerializeField] public enum CastType
        { //Enum describing different types of ground detection methods;
            Raycast,
            RaycastArray,
            Spherecast
        }
        public CastType castType = CastType.Raycast;
        #endregion

        #region Raycast hit information variables;
        private bool hasDetectedHit;
        private Vector3 hitPosition;
        private Vector3 hitNormal;
        private float hitDistance;
        private List<Collider> hitColliders = new List<Collider>();
        private List<Transform> hitTransforms = new List<Transform>();
        #endregion

        #region Spherecast settings;
        public bool calculateRealSurfaceNormal = false; //Cast an additional ray to get the true surface normal;
        public bool calculateRealDistance = false; //Cast an additional ray to get the true distance to the ground;
        private Vector3 backupNormal; //Backup normal used for specific edge cases when using spherecasts;
        #endregion

        #region Array raycast settings;
        public int arrayRayCount = 9; //Number of rays in every row;
        public int ArrayRows = 3; //Number of rows around the central ray;
        public bool offsetArrayRows = false; //Whether or not to offset every other row;
        private Vector3[] raycastArrayStartPositions; //Array containing all array raycast start positions (in local coordinates);
        #endregion

        #region misc
        public LayerMask layermask = 255;

        int ignoreRaycastLayer; //Layer number for 'Ignore Raycast' layer;
        private Collider[] ignoreList; //Optional list of colliders to ignore when raycasting;
        private int[] ignoreListLayers; //Array to store layers of colliders in ignore list;

        public bool isInDebugMode = false; //Whether to draw debug information (hit positions, hit normals...) in the editor;

        List<Vector3> arrayNormals = new List<Vector3>();
        List<Vector3> arrayPoints = new List<Vector3>();
        #endregion

        #region Getters

        //!Calculate a direction in world coordinates based on the local axes of this gameobject's transform component;
        Vector3 GetCastDirection()
        {
            switch (castDirection)
            {
                case CastDirection.Forward: 	return tr.forward;
                case CastDirection.Right: 		return tr.right;
                case CastDirection.Up: 			return tr.up;
                case CastDirection.Backward: 	return -tr.forward;
                case CastDirection.Left: 		return -tr.right;
                case CastDirection.Down: 		return -tr.up;
                default: 						return Vector3.one;
            }
        }

        //!Returns an array containing the starting positions of all array rays (in local coordinates) based on the input arguments;
        public static Vector3[] GetRaycastStartPositions(int sensorRows, int sensorRayCount, bool offsetRows, float sensorRadius)
        {
            List<Vector3> _posList = new List<Vector3>(); //Initialize list used to store the positions;
            _posList.Add(Vector3.zero); //Add central start position to the list;

            for (int i = 0; i < sensorRows; i++)
            {
                //Calculate radius for all positions on this row;
                float _rowRadius = (float)(i + 1) / sensorRows;
                int _sensorInRow = sensorRayCount * (i + 1);
                float _angleInRow = 360f / _sensorInRow;

                for (int j = 0; j < _sensorInRow; j++)
                {
                    float _angle = _angleInRow * j; //Calculate angle (in degrees) for this individual position;

                    if (offsetRows && i % 2 == 0) _angle += _angleInRow / 2f; //If 'offsetRows' is set to 'true', every other row is offset;

                    //Combine radius and angle into one position and add it to the list;
                    float _x = _rowRadius * Mathf.Cos(Mathf.Deg2Rad * _angle);
                    float _y = _rowRadius * Mathf.Sin(Mathf.Deg2Rad * _angle);
                    _posList.Add(new Vector3(_x, 0f, _y) * sensorRadius);
                }
            }
            return _posList.ToArray();
        }

        public bool HasDetectedHit() { return hasDetectedHit; } //Returns whether the sensor has hit something;
        public float GetHitDistance() { return hitDistance; } //Returns how far the raycast reached before hitting a collider;
        public Vector3 GetNormal() { return hitNormal; } //Returns the surface normal of the collider the raycast has hit;
        public Vector3 GetHitPosition() { return hitPosition; } //Returns the position in world coordinates where the raycast has hit a collider;
        public Collider GetHitCollider() { return hitColliders[0]; } //Returns a reference to the collider that was hit by the raycast;
        public Transform GetHitTransform() { return hitTransforms[0]; } //Returns a reference to the transform component attached to the collider that was hit by the raycast;

		#endregion

        #region Setters
        //Set the position for the raycast to start from; //The input vector '_origin' is converted to local coordinates;
        public void SetCastOrigin(Vector3 _origin)
        {
            if (tr == null) return;
            origin = tr.InverseTransformPoint(_origin);
        }

        //Set which axis of this gameobject's transform will be used as the direction for the raycast;
        public void SetCastDirection(CastDirection _direction)
        {
            if (tr == null) return;
            castDirection = _direction;
        }

        //Recalculate start positions for the raycast array;
        public void RecalibrateRaycastArrayPositions()
        {
            raycastArrayStartPositions = GetRaycastStartPositions(ArrayRows, arrayRayCount, offsetArrayRows, sphereCastRadius);
        }
        #endregion

        public Sensor(Transform _transform, Collider _collider)
        {
            tr = _transform;

            if (_collider == null) return;

            ignoreList = new Collider[1];
            ignoreList[0] = _collider; //Add collider to ignore list;

            ignoreRaycastLayer = LayerMask.NameToLayer("Ignore Raycast"); //Store "Ignore Raycast" layer number for later;

            ignoreListLayers = new int[ignoreList.Length]; //Setup array to store ignore list layers;
        }

        //! Reset all variables related to storing information on raycast hits;
        private void ResetFlags()
        {
            hasDetectedHit = false;
            hitPosition = Vector3.zero;
            hitNormal = -GetCastDirection();
            hitDistance = 0f;

            if (hitColliders.Count > 0) hitColliders.Clear();
            if (hitTransforms.Count > 0) hitTransforms.Clear();
        }

        #region Cast Methods

        //! Cast a ray (or sphere or array of rays) to check for colliders;
        public void Cast()
        {
            ResetFlags();

            //Calculate origin and direction of ray in world coordinates;
            Vector3 _worldDirection = GetCastDirection();
            Vector3 _worldOrigin = tr.TransformPoint(origin);

            if (ignoreListLayers.Length != ignoreList.Length)
            { //Check if ignore list length has been changed since last frame; If so, setup ignore layer array to fit new length;
                ignoreListLayers = new int[ignoreList.Length];
            }

            for (int i = 0; i < ignoreList.Length; i++)
            { //(Temporarily) move all objects in ignore list to 'Ignore Raycast' layer;
                ignoreListLayers[i] = ignoreList[i].gameObject.layer;
                ignoreList[i].gameObject.layer = ignoreRaycastLayer;
            }

            //Depending on the chosen mode of detection, call different functions to check for colliders;
            switch (castType)
            {
                case CastType.Raycast:
                    CastRay(_worldOrigin, _worldDirection);
                    break;
                case CastType.Spherecast:
                    CastSphere(_worldOrigin, _worldDirection);
                    break;
                case CastType.RaycastArray:
                    CastRayArray(_worldOrigin, _worldDirection);
                    break;
                default:
                    hasDetectedHit = false;
                    break;
            }

            for (int i = 0; i < ignoreList.Length; i++)
            { //Reset collider layers in ignoreList;
                ignoreList[i].gameObject.layer = ignoreListLayers[i];
            }
        }

        //Cast an array of rays into '_direction' and centered around '_origin';
        private void CastRayArray(Vector3 _origin, Vector3 _direction)
        {
            //Calculate origin and direction of ray in world coordinates;
            Vector3 _rayStartPosition = Vector3.zero;
            Vector3 rayDirection = GetCastDirection();

            //Clear results from last frame;
            arrayNormals.Clear();
            arrayPoints.Clear();

            RaycastHit _hit;

            //Cast array;
            for (int i = 0; i < raycastArrayStartPositions.Length; i++)
            {
                //Calculate ray start position;
                _rayStartPosition = _origin + tr.TransformDirection(raycastArrayStartPositions[i]);

                if (Physics.Raycast(_rayStartPosition, rayDirection, out _hit, castLength, layermask, QueryTriggerInteraction.Ignore))
                {
                    if (isInDebugMode)
                        Debug.DrawRay(_hit.point, _hit.normal, Color.red, Time.fixedDeltaTime * 1.01f);

                    hitColliders.Add(_hit.collider);
                    hitTransforms.Add(_hit.transform);
                    arrayNormals.Add(_hit.normal);
                    arrayPoints.Add(_hit.point);
                }
            }

            //Evaluate results;
            hasDetectedHit = (arrayPoints.Count > 0);

            if (hasDetectedHit)
            {
                //Calculate average surface normal;
                Vector3 _averageNormal = Vector3.zero;
                for (int i = 0, l = arrayNormals.Count; i < l; i++)
                {
                    _averageNormal += arrayNormals[i];
                }

                _averageNormal.Normalize();

                //Calculate average surface point;
                Vector3 _averagePoint = Vector3.zero;
                for (int i = 0; i < arrayPoints.Count; i++)
                {
                    _averagePoint += arrayPoints[i];
                }

                _averagePoint /= arrayPoints.Count;

                hitPosition = _averagePoint;
                hitNormal = _averageNormal;
                hitDistance = VectorMath.ProjectAonB(_origin - hitPosition, _direction).magnitude;
            }
        }

        //Cast a single ray into '_direction' from '_origin';
        private void CastRay(Vector3 _origin, Vector3 _direction)
        {
            RaycastHit _hit;
            hasDetectedHit = Physics.Raycast(_origin, _direction, out _hit, castLength, layermask, QueryTriggerInteraction.Ignore);

            if (hasDetectedHit)
            {
                hitPosition = _hit.point;
                hitNormal = _hit.normal;

                hitColliders.Add(_hit.collider);
                hitTransforms.Add(_hit.transform);

                hitDistance = _hit.distance;
            }
        }

        //Cast a sphere into '_direction' from '_origin';
        private void CastSphere(Vector3 _origin, Vector3 _direction)
        {
            RaycastHit _hit;
            hasDetectedHit = Physics.SphereCast(_origin, sphereCastRadius, _direction, out _hit, castLength - sphereCastRadius, layermask, QueryTriggerInteraction.Ignore);

            if (hasDetectedHit)
            {
                hitPosition = _hit.point;
                hitNormal = _hit.normal;
                hitColliders.Add(_hit.collider);
                hitTransforms.Add(_hit.transform);

                hitDistance = _hit.distance;

                hitDistance += sphereCastRadius;

                //Calculate real distance;
                if (calculateRealDistance)
                {
                    hitDistance = VectorMath.SignedMagProjectAonB(_origin - hitPosition, _direction);
                }

                Collider _col = hitColliders[0];

                //Calculate real surface normal by casting an additional raycast;
                if (calculateRealSurfaceNormal)
                {
                    if (_col.Raycast(new Ray(hitPosition - _direction, _direction), out _hit, 1.5f))
                    {
                        if (Vector3.Angle(_hit.normal, -_direction) >= 89f)
                            hitNormal = backupNormal;
                        else
                            hitNormal = _hit.normal;
                    }
                    else
                        hitNormal = backupNormal;

                    backupNormal = hitNormal;
                }
            }
        }

        #endregion

        //Draw debug information in editor (hit positions and ground surface normals);
        public void DrawDebug()
        {
            if (hasDetectedHit && isInDebugMode)
            {
                Debug.DrawRay(hitPosition, hitNormal, Color.red, Time.deltaTime);
                float _markerSize = 0.2f;
                Debug.DrawLine(hitPosition + Vector3.up * _markerSize, hitPosition - Vector3.up * _markerSize, Color.green, Time.deltaTime);
                Debug.DrawLine(hitPosition + Vector3.right * _markerSize, hitPosition - Vector3.right * _markerSize, Color.green, Time.deltaTime);
                Debug.DrawLine(hitPosition + Vector3.forward * _markerSize, hitPosition - Vector3.forward * _markerSize, Color.green, Time.deltaTime);
            }
        }
    }
}
