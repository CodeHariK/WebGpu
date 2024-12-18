using UnityEngine;

namespace CMF
{
	public static class VectorMath {

		//Calculate signed angle (ranging from -180 to +180) between '_vector_1' and '_vector_2';
		public static float GetAngle(Vector3 v1, Vector3 v2, Vector3 _planeNormal)
		{
			float _angle = Vector3.Angle(v1,v2);
			float _sign = Mathf.Sign(Vector3.Dot(_planeNormal,Vector3.Cross(v1,v2)));
			return _angle * _sign;
		}

		public static Vector3 ProjectAonB(Vector3 A, Vector3 B)
		{
			B.Normalize();
			return B * Vector3.Dot(A, B);
		}

		public static float SignedMagProjectAonB(Vector3 A, Vector3 B)
		{
			B.Normalize();				
			return Vector3.Dot(A, B);
		}
		
		public static Vector3 RejectAonB(Vector3 A, Vector3 B)
		{
			B.Normalize();				
			A -= B * Vector3.Dot(A, B);
			return A;
		}
		
		public static Vector3 RotateVectorOntoPlane(Vector3 _vector, Vector3 _planeNormal, Vector3 dir)
		{
			Quaternion _rotation = Quaternion.FromToRotation(dir, _planeNormal);

			_vector = _rotation * _vector;
			
			return _vector;
		}

		//Project a point onto a line defined by '_lineStartPosition' and '_lineDirection';
		public static Vector3 ProjectPointOntoLine(Vector3 _lineStartPosition, Vector3 _lineDirection, Vector3 _point)
		{		
			//Caclculate vector pointing from '_lineStartPosition' to '_point';
			Vector3 _projectLine = _point - _lineStartPosition;
	
			float dotProduct = Vector3.Dot(_projectLine, _lineDirection);
	
			return _lineStartPosition + _lineDirection * dotProduct;
		}
	}
}
