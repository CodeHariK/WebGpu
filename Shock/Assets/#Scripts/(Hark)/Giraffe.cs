using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

using static Hat;

[System.Serializable] public struct _Grid
{
	public Vector2 side;//side Length
	public Vector2Int num;//number of inner rect
	public Vector2 wh;//Grid Width and Height
	public Vector2 thk;//Grid thickness
}

public enum cmdType
{
	line = 1,
	polygon = 2,
	stripe = 3,
}

[System.Serializable] public struct drawCmd
{
	public cmdType cmd;
	public Vector2[] points;
	public Vector2 pos;
	public bool smooth;
	public float thk;
	public bool loop;
	public bool hasCenter;
	public Color color;
}

[RequireComponent(typeof(CanvasRenderer))]
public class Giraffe : Graphic
{
	public _Grid Grid = new _Grid();

	public Vector2[] doints = new Vector2[10];
	public Vector2[] coints = new Vector2[5];

	public List<drawCmd> cmdList = new List<drawCmd>();

	VertexHelper vh;

    protected override void OnPopulateMesh(VertexHelper _vh)
    {
		vh = _vh;
        vh.Clear();
		cmdList.Clear();

		//Grid.wh.x = rectTransform.rect.width;
		//Grid.wh.y = rectTransform.rect.height;

/*
		drawPolygon(circlePoints(50, 4, 360, 0), false, Vector2.up * 300, new Color32(10, 1, 40, 140));

		makeGrid();

		drawStripe3(smoothCurve(coints, true, 0.1f), Vector2.right * 40, 1.0f, true, Color.blue);

		drawPolygon3(smoothCurve(coints, true, 0.1f), false, Vector2.one * 150, Color.yellow);
		drawPolygon3(circlePoints(20, 17, 360, 0), true, Vector2.right * 80, Color.red);
		drawPolygon3(circlePoints(20, 17, 180, 0), true, Vector2.right * 20, Color.red);
		
		drawStripe3(circlePoints(30, 17, 360, 0), Vector2.zero, 1.5f, true, Color.red);
		drawStripe3(circlePoints(30, 17, 360, 0), Vector2.right * 80, 1.5f, false, Color.blue);
		drawStripe3(smoothCurve(circlePoints(30, 17, 360, 0), true, 0.1f), Vector2.right * 160, 1.5f, true, Color.yellow);

		drawStripe3(doints, Vector2.up * 0, 3.0f, false, Color.white);
		drawStripe3(smoothCurve(doints, false, 0.2f), 		Vector2.zero, 3.0f, false, Color.green);
		drawPolygon3(smoothCurve(doints, true, 0.2f), false, Vector2.zero, new Color32(150, 1, 240, 40));
*/

		cmdList.Add(new drawCmd(){
			cmd 	= cmdType.stripe,
			color 	= Color.red,
			pos 	= Vector2.up * 100,
			points 	= doints,
			smooth 	= false,
			thk		= 1,
			loop 	= false
		});
		cmdList.Add(new drawCmd(){
			cmd 	= cmdType.polygon,
			color 	= new Color32(1,24,150,104),
			pos 	= Vector2.right * 100,
			points 	= doints,
			smooth 	= true,
			thk		= 1,
			loop 	= true
		});

		for(int i = 0, l = cmdList.Count; i < l; i++)
		{
			cmdProcess(cmdList[i]);
		}

		//System.Diagnostics.Stopwatch watch = new System.Diagnostics.Stopwatch();
		//float ms = 0;
/*
		int t = 10000000;
		Vector2[] joker = new Vector2[t];
		for(int i = 0; i < t; i++)
		{	
			//joker[i] = Random.insideUnitCircle * 50;
			joker[i] = new Vector2(Random.value, Random.value) * 50;
		}

		watch.Start();
		Vector2[] Boomc = DoomHull(joker);
		watch.Stop();
		Debug.Log("bomc " + (watch.ElapsedMilliseconds - ms) + "  " + Boomc.Length);
		drawStripe(Boomc, Vector2.zero, .5f, true, Color.white);
		ms = watch.ElapsedMilliseconds;
*/		
	}

	void cmdProcess(drawCmd dc)
	{
		if(dc.cmd == cmdType.line)
		{
			if(dc.smooth)
				drawLines(smoothCurve(dc.points, dc.loop, 0.1f), dc.thk, dc.pos, dc.color);	
			else
				drawLines(dc.points, dc.thk, dc.pos, dc.color);	
		}
		if(dc.cmd == cmdType.polygon)
		{
			if(dc.smooth)
				drawPolygon3(smoothCurve(dc.points, dc.loop, 0.1f), dc.hasCenter, dc.pos, dc.color);
			else
				drawPolygon3(dc.points, dc.hasCenter, dc.pos, dc.color);
		}
		if(dc.cmd == cmdType.stripe)
		{
			if(dc.smooth)
				drawStripe3(smoothCurve(dc.points, dc.loop, 0.1f), dc.pos, dc.thk, dc.loop, dc.color);
			else
				drawStripe3(dc.points, dc.pos, dc.thk, dc.loop, dc.color);
		}
	}

	void drawGrid()
	{
		if(!(Grid.side.x * Grid.num.x < 0.98f * Grid.wh.x))
		{
			Grid.side.x = 0.98f * (Grid.wh.x)/Grid.num.x;
		}
	
		if(!(Grid.side.y * Grid.num.y < 0.98f * Grid.wh.y))
		{
			Grid.side.y = 0.98f * (Grid.wh.y)/Grid.num.y;
		}

		Grid.thk.x = (Grid.wh.x - Grid.num.x * Grid.side.x)/(Grid.num.x + 1);
		Grid.thk.y = (Grid.wh.y - Grid.num.y * Grid.side.y)/(Grid.num.y + 1);

		for(int y = 0; y < (Grid.num.y + 1); y++)
		{
			drawLines(new Vector2[]{ new Vector2(0, y * (Grid.side.y + Grid.thk.y) + Grid.thk.y/2), 
									new Vector2(Grid.wh.x, y * (Grid.side.y + Grid.thk.y) + Grid.thk.y/2)}, 
						Grid.thk.y, Vector2.zero, Color.white);
		}
		for(int x = 0; x < (Grid.num.x + 1); x++)
		{
			drawLines(new Vector2[]{ 
									new Vector2(x * (Grid.side.x + Grid.thk.x) + Grid.thk.x/2, Grid.wh.y),
									new Vector2(x * (Grid.side.x + Grid.thk.x) + Grid.thk.x/2, 0) 
									},
						Grid.thk.x, Vector2.zero, Color.white);
		}

	}

	void drawPolygon(Vector2[] points, bool hasCenter, Vector2 pos, Color c)
	{
		var cv = vh.currentVertCount;

		UIVertex vert = new UIVertex();
		vert.color = c;

		Vector2 center = Vector2.zero;

		int l = points.Length;
		for(int n = 0; n < l; n++)
		{
			vert.position = points[n] + pos;
			vert.uv0 = Vector2.one;
			vh.AddVert(vert);

			center += points[n];
		}

		vert.position = hasCenter ? pos : ((center/l) + pos);
		vert.uv0 = Vector2.zero;
		vh.AddVert(vert);

		for(int n = 0; n < l ; n++)
		{
			vh.AddTriangle(cv + (n + 1)%l, cv + n, cv + l);
		}
	}

	void drawPolygon3(Vector2[] points, bool hasCenter, Vector2 pos, Color c)
	{
		var cv = vh.currentVertCount;

		List<UIVertex> velo = new List<UIVertex>();

		UIVertex vert = new UIVertex();
		vert.color = c;

		Vector2 center = Vector2.zero;

		int l = points.Length;
		for(int n = 0; n < l; n++)
		{
			vert.position = points[n] + pos;
			vert.uv0 = Vector2.one;
			vh.AddVert(vert);
			velo.Add(vert);

			center += points[n];
		}

		vert.position = hasCenter ? pos : ((center/l) + pos);
		vert.uv0 = Vector2.zero;
		vh.AddVert(vert);
		velo.Add(vert);

		for(int n = 0; n < l ; n++)
		{
			appendTriangle(velo, cv, (n + 1)%l, n, l);
		}
	}

	void appendTriangle(List<UIVertex> vBuf, int cv, int i0, int i1, int i2)
	{
		if(Cross2(vBuf[i1].position - vBuf[i0].position, vBuf[i2].position - vBuf[i0].position) > 0)
		{
			vh.AddTriangle(cv + i0, cv + i2, cv + i1);
		} else {
			vh.AddTriangle(cv + i0, cv + i1, cv + i2);
		}
	}

	void drawLines(Vector2[] points, float thk, Vector2 pos, Color c)
	{
		void drawQuad(Vector2[] q)
		{
			if(q.Length != 4) return;

			var i = vh.currentVertCount;

			UIVertex vert = new UIVertex();
			vert.color = c;

			vert.position = q[0] + pos;
			vert.uv0 = Vector2.zero;
			vh.AddVert(vert);

			vert.position = q[1] + pos;
			vert.uv0 = Vector2.zero;
			vh.AddVert(vert);

			vert.position = q[2] + pos;
			vert.uv0 = Vector2.one;
			vh.AddVert(vert);

			vert.position = q[3] + pos;
			vert.uv0 = Vector2.one;
			vh.AddVert(vert);

			vh.AddTriangle(i + 0, i + 1, i + 2);
			vh.AddTriangle(i + 2, i + 3, i + 0);
		}

		for(int i = 0, l = (points.Length - 1); i < l; i++)
		{
			drawQuad(new Vector2[]{
				eqLine(points[i + 0], 0, -thk, points[i + 0], points[i + 1]),
				eqLine(points[i + 0], 0,  thk, points[i + 0], points[i + 1]),
				eqLine(points[i + 1], 0,  thk, points[i + 0], points[i + 1]),
				eqLine(points[i + 1], 0, -thk, points[i + 0], points[i + 1])});
		}
	}

	void drawStripe(Vector2[] points, Vector2 pos, float thk, bool loop, Color c)
	{
		if(points.Length <=1) return;

		var cv = vh.currentVertCount;

        UIVertex vert = new UIVertex();
        vert.color = c;

		int l = points.Length;
		float lineLen = 0;
		for(int i = 0; i < (l-1); i++){ lineLen += (points[i + 1] - points[i]).magnitude; }

		Vector2 p0, pi0, p1, pi1;

		float dis = 0;

		void Local_addVert(int i)
		{
			Vector2 k_n = points[(i-1 + l)%l];
			Vector2 k = points[i];
			Vector2 k_p = points[(i+1 + l)%l];

			p0  = eqLine(k, 0,  thk, k_n, k);
			pi0 = eqLine(k, 0, -thk, k_n, k);
			p1  = eqLine(k, 0,  thk, k, k_p);
			pi1 = eqLine(k, 0, -thk, k, k_p);

			Vector2 sc0 = sinCos(k_n, k);
			Vector2 sc1 = sinCos(k, k_p);

			Vector2 I = lineIntersection(p0, sc0, p1, sc1) + pos;
			Vector2 Ii = lineIntersection(pi0, sc0, pi1, sc1) + pos;

			Vector2 p01 = (p0 + p1)/2 + pos;
			Vector2 pi01 = (pi0 + pi1)/2 + pos;

			vert.position = I;
			if(((I - k).magnitude/(p01 - k).magnitude) > 1.17) {
				//vert.position = p01;
			}
    	    vert.uv0 = new Vector2(dis/lineLen, 1);
			vh.AddVert(vert);

			vert.position = Ii;
			if(((Ii - k).magnitude/(pi01 - k).magnitude) > 1.17) {
				//vert.position = pi01;
			}
	        vert.uv0 = new Vector2(dis/lineLen, 0);
			vh.AddVert(vert);
		}

		if(!loop)
		{
			vert.position = eqLine(points[0], 0,  thk, points[0], points[1]) + pos;
			vert.uv0 = new Vector2(0, 1);
			vh.AddVert(vert);

			vert.position = eqLine(points[0], 0, -thk, points[0], points[1]) + pos;
			vert.uv0 = new Vector2(0, 0);
			vh.AddVert(vert);
		} else {
			Local_addVert(0);
		}

		for(int i = 1, n = loop ? l : (l - 1); i < n; i++)
		{
			dis += (points[i] - points[i-1]).magnitude;

			Local_addVert(i);

			int o = cv + 2 * i;
			vh.AddTriangle(o - 0, o - 1,	o - 2);
			vh.AddTriangle(o - 1, o - 0,	o + 1);
		}

		if(!loop)
		{
			vert.position = eqLine(points[l - 1], 0,  thk, points[l - 2], points[l - 1]) + pos;
			vert.uv0 = new Vector2(1, 1);
			vh.AddVert(vert);

			vert.position = eqLine(points[l - 1], 0, -thk, points[l - 2], points[l - 1]) + pos;
			vert.uv0 = new Vector2(1, 0);
			vh.AddVert(vert);

			int f = cv + 2 * (l - 1);

			vh.AddTriangle(f - 0, f - 1, 		f - 2);
			vh.AddTriangle(f - 1, f - 0, 		f + 1);
		} else {

			int f = cv + 2 * (l - 1);

			vh.AddTriangle(f + 1 , 	  cv,	   cv + 1);
			vh.AddTriangle(cv	 , f + 1, 			f);
		}

		//addCircle(points[0], thk, 8, 360, 0, c);
		//addCircle(points[l - 1], thk, 8, 360, 0, c);
	}

	void drawStripe3(Vector2[] points, Vector2 pos, float thk, bool loop, Color c)
	{
		if(points.Length <=1) return;

		List<UIVertex> velo = new List<UIVertex>();

		var cv = vh.currentVertCount;

        UIVertex vert = new UIVertex();
        vert.color = c;

		int l = points.Length;
		float lineLen = 0;
		for(int i = 0; i < (l-1); i++){ lineLen += (points[i + 1] - points[i]).magnitude; }

		Vector2 p0, pi0, p1, pi1;

		float dis = 0;

		void Local_addVert(int i)
		{
			Vector2 k_n = points[(i-1 + l)%l];
			Vector2 k = points[i];
			Vector2 k_p = points[(i+1 + l)%l];

			p0  = eqLine(k, 0,  thk, k_n, k);
			pi0 = eqLine(k, 0, -thk, k_n, k);
			p1  = eqLine(k, 0,  thk, k, k_p);
			pi1 = eqLine(k, 0, -thk, k, k_p);

			Vector2 sc0 = sinCos(k_n, k);
			Vector2 sc1 = sinCos(k, k_p);

			Vector2 I = lineIntersection(p0, sc0, p1, sc1) + pos;
			Vector2 Ii = lineIntersection(pi0, sc0, pi1, sc1) + pos;

			Vector2 p01 = (p0 + p1)/2 + pos;
			Vector2 pi01 = (pi0 + pi1)/2 + pos;

			vert.position = I;
			if(((I - k).magnitude/(p01 - k).magnitude) > 1.17) {
				//vert.position = p01;
			}
    	    vert.uv0 = new Vector2(dis/lineLen, 1);
			vh.AddVert(vert);
			velo.Add(vert);

			vert.position = Ii;
			if(((Ii - k).magnitude/(pi01 - k).magnitude) > 1.17) {
				//vert.position = pi01;
			}
	        vert.uv0 = new Vector2(dis/lineLen, 0);
			vh.AddVert(vert);
			velo.Add(vert);
		}

		if(!loop)
		{
			vert.position = eqLine(points[0], 0,  thk, points[0], points[1]) + pos;
			vert.uv0 = new Vector2(0, 1);
			vh.AddVert(vert);
			velo.Add(vert);

			vert.position = eqLine(points[0], 0, -thk, points[0], points[1]) + pos;
			vert.uv0 = new Vector2(0, 0);
			vh.AddVert(vert);
			velo.Add(vert);
		} else {
			Local_addVert(0);
		}

		for(int i = 1, n = loop ? l : (l - 1); i < n; i++)
		{
			dis += (points[i] - points[i-1]).magnitude;

			Local_addVert(i);

			int o = 2 * i;
			appendTriangle(velo, cv, o - 0, o - 1, o - 2);
			appendTriangle(velo, cv, o - 1, o - 0, o + 1);
		}

		if(!loop)
		{
			vert.position = eqLine(points[l - 1], 0,  thk, points[l - 2], points[l - 1]) + pos;
			vert.uv0 = new Vector2(1, 1);
			vh.AddVert(vert);
			velo.Add(vert);

			vert.position = eqLine(points[l - 1], 0, -thk, points[l - 2], points[l - 1]) + pos;
			vert.uv0 = new Vector2(1, 0);
			vh.AddVert(vert);
			velo.Add(vert);

			int f = 2 * (l - 1);
			appendTriangle(velo, cv, f - 0, f - 1, f - 2);
			appendTriangle(velo, cv, f - 1, f - 0, f + 1);
		} else {

			int f = 2 * (l - 1);
			appendTriangle(velo, cv, f + 1,		0, 1);
			appendTriangle(velo, cv, 0	, 	f + 1, f);
		}

		//addCircle(points[0], thk, 8, 360, 0, c);
		//addCircle(points[l - 1], thk, 8, 360, 0, c);
	}

/*
	Vector2[] DelTriangulation(Vector2[] Points)
	{

	}
*/

    protected override void OnRectTransformDimensionsChange()
    {
        base.OnRectTransformDimensionsChange();
        SetVerticesDirty();
        SetMaterialDirty();
    }
}