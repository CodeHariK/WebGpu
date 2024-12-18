using UnityEngine.UI;
using UnityEngine;

public class VertBend : BaseMeshEffect
{
	public override void ModifyMesh(VertexHelper vh)
	{
		if(!IsActive()) return;

		int vertCount = vh.currentVertCount;

		var vert = new UIVertex();
		for (int v = 0; v < vertCount; v++)
		{
			vh.PopulateUIVertex(ref vert,v);

			vert.position.x += (UnityEngine.Random.value-0.5f) * 50f;
			vert.position.y += (UnityEngine.Random.value-0.5f) * 50f;
			vert.position.z += (UnityEngine.Random.value-0.5f) * 50f;

			vh.SetUIVertex(vert,v);
		}
	}

	public void Update()
	{
		var graphic = GetComponent<Graphic>();
		graphic.SetVerticesDirty();
	}
}